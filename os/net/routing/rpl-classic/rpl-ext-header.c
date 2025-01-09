/*
 * Copyright (c) 2009, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * \file
 *         Management of extension headers for ContikiRPL.
 *
 * \author Vincent Brillault <vincent.brillault@imag.fr>,
 *         Joakim Eriksson <joakime@sics.se>,
 *         Niclas Finne <nfi@sics.se>,
 *         Nicolas Tsiftes <nvt@sics.se>.
 */

/**
 * \addtogroup uip
 * @{
 */

#include "net/routing/routing.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/tcpip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-sr.h"
#include "net/routing/rpl-classic/rpl-private.h"
#include "net/packetbuf.h"

#include "sys/log.h"

#define LOG_MODULE "RPL"
#define LOG_LEVEL LOG_LEVEL_RPL

#include <limits.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
int
rpl_ext_header_hbh_update(uint8_t *ext_buf, int opt_offset)
{
  rpl_instance_t *instance;
  int down;
  uint16_t sender_rank;
  uint8_t sender_closer;
  uip_ds6_route_t *route;
  rpl_parent_t *sender;

  /* RFC 6553: "This option has an alignment requirement of 2n." */
  if(opt_offset < 0 || opt_offset & 1) {
    LOG_ERR("Invalid RPL option offset: %d\n", opt_offset);
    return 0;
  }

  struct uip_hbho_hdr *hbh_hdr = (struct uip_hbho_hdr *)ext_buf;
  struct uip_ext_hdr_opt_rpl *rpl_opt = (struct uip_ext_hdr_opt_rpl *)(ext_buf + opt_offset);

  if(hbh_hdr->len != ((RPL_HOP_BY_HOP_LEN - 8) / 8)
     || rpl_opt->opt_type != UIP_EXT_HDR_OPT_RPL
     || rpl_opt->opt_len != RPL_HDR_OPT_LEN) {

    LOG_ERR("Hop-by-hop extension header has wrong size or type (%u %u %u)\n",
            hbh_hdr->len, rpl_opt->opt_type, rpl_opt->opt_len);
    return 0; /* Drop */
  }

  instance = rpl_get_instance(rpl_opt->instance);
  if(instance == NULL) {
    LOG_ERR("Unknown instance: %u\n", rpl_opt->instance);
    return 0;
  }

  if(rpl_opt->flags & RPL_HDR_OPT_FWD_ERR) {
    LOG_ERR("Forward error!\n");
    /*
     * We should try to repair it by removing the neighbor that caused
     * the packet to be forwarded in the first place. We drop any
     * routes that go through the neighbor that sent the packet to us.
     */
    if(RPL_IS_STORING(instance)) {
      route = uip_ds6_route_lookup(&UIP_IP_BUF->destipaddr);
      if(route != NULL) {
        uip_ds6_route_rm(route);
      }
    }
    RPL_STAT(rpl_stats.forward_errors++);
    /* Trigger DAO retransmission. */
    rpl_reset_dio_timer(instance);
    /* Drop the packet because it is not routable. */
    return 0;
  }

  if(!instance->current_dag->joined) {
    LOG_ERR("No DAG in the instance\n");
    return 0;
  }
  down = 0;
  if(rpl_opt->flags & RPL_HDR_OPT_DOWN) {
    down = 1;
  }

  sender_rank = UIP_HTONS(rpl_opt->senderrank);
  sender = nbr_table_get_from_lladdr(rpl_parents, packetbuf_addr(PACKETBUF_ADDR_SENDER));

  if(sender != NULL && (rpl_opt->flags & RPL_HDR_OPT_RANK_ERR)) {
    /* A rank error was signaled -- attempt to repair it by updating
       the sender's rank from the ext header. */
    sender->rank = sender_rank;
    if(RPL_IS_NON_STORING(instance)) {
      /*
       * Select DAG and preferred parent only in non-storing mode. In
       * storing mode, a parent switch would result in an immediate
       * no-path DAO transmission, dropping the current incoming
       * packet.
       */
      rpl_select_dag(instance, sender);
    }
  }

  sender_closer = sender_rank < instance->current_dag->rank;


  LOG_DBG("Packet going %s, sender%s closer (%d < %d)\n",
          down == 1 ? "down" : "up",
          sender_closer ? "" : " not",
          sender_rank,
          instance->current_dag->rank);

  if((down && !sender_closer) || (!down && sender_closer)) {
    LOG_WARN("Loop detected - senderrank: %d my-rank: %d sender_closer: %d\n",
             sender_rank, instance->current_dag->rank,
             sender_closer);
    /* Attempt to repair the loop by sending a unicast DIO back to the
       sender so that it gets a fresh update of our rank. */
    if(sender != NULL) {
      instance->unicast_dio_target = sender;
      rpl_schedule_unicast_dio_immediately(instance);
    }

    if(rpl_opt->flags & RPL_HDR_OPT_RANK_ERR) {
      RPL_STAT(rpl_stats.loop_errors++);
      LOG_ERR("Rank error signaled in RPL option!\n");
      /* Packet must be dropped and dio trickle timer reset, see
         RFC6550 - 11.2.2.2 */
      rpl_reset_dio_timer(instance);
      return 0;
    }

    LOG_WARN("One inconsistency along path is tolerated\n");
    RPL_STAT(rpl_stats.loop_warnings++);
    rpl_opt->flags |= RPL_HDR_OPT_RANK_ERR;
    return 1;
  }

  LOG_DBG("Rank OK\n");
  return 1;
}
/*---------------------------------------------------------------------------*/
int
rpl_ext_header_srh_get_next_hop(uip_ipaddr_t *ipaddr)
{
#if RPL_WITH_NON_STORING
  struct uip_routing_hdr *rh_header;
  rpl_dag_t *dag;
  uip_sr_node_t *dest_node;
  uip_sr_node_t *root_node;

  /* Look for the routing ext header. */
  rh_header = (struct uip_routing_hdr *)uipbuf_search_header(uip_buf, uip_len, UIP_PROTO_ROUTING);

  dag = rpl_get_dag(&UIP_IP_BUF->destipaddr);
  root_node = uip_sr_get_node(dag, &dag->dag_id);
  dest_node = uip_sr_get_node(dag, &UIP_IP_BUF->destipaddr);

  if((rh_header != NULL && rh_header->routing_type == RPL_RH_TYPE_SRH) ||
     (dest_node != NULL && root_node != NULL &&
      dest_node->parent == root_node)) {
    /*
     * A routing header was found or the packet was destined for a
     * direct child of the root. The next hop should already be copied
     * as the IPv6 destination address, via
     * rpl_ext_header_srh_update. We turn this address into a
     * link-local address to enable forwarding to next hop.
     */
    uip_ipaddr_copy(ipaddr, &UIP_IP_BUF->destipaddr);
    uip_create_linklocal_prefix(ipaddr);
    return 1;
  }

  return 0;
#else /* RPL_WITH_NON_STORING */
  return 0; /* SRH not found */
#endif /* RPL_WITH_NON_STORING */
}
/*---------------------------------------------------------------------------*/
#if RPL_WITH_NON_STORING
static bool
srh_is_valid(struct uip_routing_hdr *rh_header,
             struct uip_rpl_srh_hdr *srh_header)
{
  uip_ipaddr_t hop_addr;
  uip_ipaddr_copy(&hop_addr, &UIP_IP_BUF->destipaddr);

  uint8_t segments_left = rh_header->seg_left;
  uint8_t ext_len = rh_header->len * 8 + 8;
  uint8_t cmpri = srh_header->cmpr >> 4;
  uint8_t cmpre = srh_header->cmpr & 0x0f;
  uint8_t padding = srh_header->pad >> 4;
  uint8_t path_len = ((ext_len - padding - RPL_RH_LEN - RPL_SRH_LEN - (16 - cmpre)) / (16 - cmpri)) + 1;

  bool prev_hop_is_my_addr = false;
  uint8_t my_addr_count = 0;
  for(uint8_t i = path_len - segments_left; i < path_len; i++) {
    uint8_t cmpr = segments_left == 1 ? cmpre : cmpri;
    ptrdiff_t rh_offset = (uint8_t *)rh_header - uip_buf;
    size_t addr_offset = RPL_RH_LEN + RPL_SRH_LEN + (i * (16 - cmpri));

    if(rh_offset + addr_offset + 16 - cmpr > UIP_BUFSIZE) {
      return false;
    }

    uint8_t *addr_ptr = (uint8_t *)rh_header + addr_offset;
    memcpy((uint8_t *)&hop_addr + cmpr, addr_ptr, 16 - cmpr);

    LOG_DBG("Processing SRH hop %u, IP addr ", i);
    LOG_DBG_6ADDR(&hop_addr);
    LOG_DBG_("\n");

    /*
     * RFC 6554 states that "To detect loops in the SRH, a router MUST
     * determine if the SRH includes multiple addresses assigned to
     * any interface on that router. If such addresses appear more
     * than once and are separated by at least one address not
     * assigned to that router, the router MUST drop the packet."
     *
     * We add further checks of unacceptable next hop addresses.
     */
    if(uip_ds6_is_my_addr(&hop_addr) ||
       uip_ds6_is_my_maddr(&hop_addr)) {
      my_addr_count++;
      if(!prev_hop_is_my_addr && my_addr_count > 1) {
        LOG_WARN("SRH contains a loop\n");
        return false;
      }
      prev_hop_is_my_addr = true;
    } else {
      prev_hop_is_my_addr = false;
    }

    if(uip_is_addr_mcast(&hop_addr) ||
       uip_is_addr_unspecified(&hop_addr) ||
       uip_is_addr_loopback(&hop_addr)) {
      LOG_WARN("SRH contains an invalid next hop address\n");
      return false;
    }
  }

  return true;
}
#endif /* RPL_WITH_NON_STORING */
/*---------------------------------------------------------------------------*/
int
rpl_ext_header_srh_update(void)
{
#if RPL_WITH_NON_STORING
  struct uip_routing_hdr *rh_header;
  struct uip_rpl_srh_hdr *srh_header;

  /* Look for routing ext header */
  rh_header = (struct uip_routing_hdr *)uipbuf_search_header(uip_buf, uip_len,
                                                             UIP_PROTO_ROUTING);

  if(rh_header != NULL && rh_header->routing_type == RPL_RH_TYPE_SRH) {
    /* SRH found, now look for next hop */
    uint8_t cmpri, cmpre;
    uint8_t ext_len;
    uint8_t padding;
    uint8_t path_len;
    uint8_t segments_left;
    uip_ipaddr_t current_dest_addr;

    srh_header = (struct uip_rpl_srh_hdr *)(((uint8_t *)rh_header) + RPL_RH_LEN);
    segments_left = rh_header->seg_left;
    ext_len = rh_header->len * 8 + 8;
    cmpri = srh_header->cmpr >> 4;
    cmpre = srh_header->cmpr & 0x0f;
    padding = srh_header->pad >> 4;
    path_len = ((ext_len - padding - RPL_RH_LEN - RPL_SRH_LEN - (16 - cmpre)) / (16 - cmpri)) + 1;
    (void)path_len;

    LOG_DBG("read SRH, path len %u, segments left %u, Cmpri %u, Cmpre %u, ext len %u (padding %u)\n",
            path_len, segments_left, cmpri, cmpre, ext_len, padding);

    if(segments_left == 0) {
      /* We are the final destination, do nothing. */
    } else if(segments_left > path_len) {
      /* Discard the packet because of a parameter problem. */
      LOG_ERR("SRH with too many segments left (%u > %u)\n",
              segments_left, path_len);
      return 0;
    } else {
      if(!srh_is_valid(rh_header, srh_header)) {
        LOG_ERR("Invalid SRH hop sequence\n");
        return 0;
      }

      /* The index of the next address to be visited. */
      uint8_t i = path_len - segments_left;
      uint8_t cmpr = segments_left == 1 ? cmpre : cmpri;
      ptrdiff_t rh_offset = (uint8_t *)rh_header - uip_buf;
      size_t addr_offset = RPL_RH_LEN + RPL_SRH_LEN + (i * (16 - cmpri));

      if(rh_offset + addr_offset + 16 - cmpr > UIP_BUFSIZE) {
        LOG_ERR("Invalid SRH address pointer\n");
        return 0;
      }

      uint8_t *addr_ptr = ((uint8_t *)rh_header) + addr_offset;

      /* As per RFC6554: swap the IPv6 destination address and address[i]. */

      /* First, copy the current IPv6 destination address. */
      uip_ipaddr_copy(&current_dest_addr, &UIP_IP_BUF->destipaddr);
      /* Second, update the IPv6 destination address with addresses[i]. */
      memcpy(((uint8_t *)&UIP_IP_BUF->destipaddr) + cmpr, addr_ptr, 16 - cmpr);
      /* Third, write current_dest_addr to addresses[i]. */
      memcpy(addr_ptr, ((uint8_t *)&current_dest_addr) + cmpr, 16 - cmpr);

      /* Update segments left field */
      rh_header->seg_left--;

      LOG_INFO("SRH next hop ");
      LOG_INFO_6ADDR(&UIP_IP_BUF->destipaddr);
      LOG_INFO_("\n");
    }
    return 1;
  }

  return 0;
#else /* RPL_WITH_NON_STORING */
  return 0; /* SRH not found */
#endif /* RPL_WITH_NON_STORING */
}
/*---------------------------------------------------------------------------*/
static int
count_matching_bytes(const void *p1, const void *p2, size_t n)
{
  for(size_t i = 0; i < n; i++) {
    if(((uint8_t *)p1)[i] != ((uint8_t *)p2)[i]) {
      return i;
    }
  }
  return n;
}
/*---------------------------------------------------------------------------*/
static int
insert_srh_header(void)
{
  /* Implementation of RFC6554. */
  uint8_t path_len;
  uint8_t ext_len;
  uint8_t cmpri, cmpre; /* ComprI and ComprE fields of the RPL Source Routing Header. */
  uint8_t *hop_ptr;
  uint8_t padding;
  uip_sr_node_t *dest_node;
  uip_sr_node_t *root_node;
  uip_sr_node_t *node;
  rpl_dag_t *dag;
  uip_ipaddr_t node_addr;

  /* Always insert the SRH as the first extension header. */
  struct uip_routing_hdr *rh_hdr = (struct uip_routing_hdr *)UIP_IP_PAYLOAD(0);
  struct uip_rpl_srh_hdr *srh_hdr = (struct uip_rpl_srh_hdr *)(UIP_IP_PAYLOAD(0) + RPL_RH_LEN);

  LOG_INFO("SRH creating source routing header with destination ");
  LOG_INFO_6ADDR(&UIP_IP_BUF->destipaddr);
  LOG_INFO_("\n");

  /* Construct source route. We do not do this recursively to keep the
     runtime stack usage constant. */

  /* Get link of the destination and root. */
  dag = rpl_get_dag(&UIP_IP_BUF->destipaddr);
  if(dag == NULL) {
    LOG_ERR("SRH DAG not found\n");
    return 0;
  }

  dest_node = uip_sr_get_node(dag, &UIP_IP_BUF->destipaddr);
  if(dest_node == NULL) {
    /* The destination was not found, skip SRH insertion. */
    return 1;
  }

  root_node = uip_sr_get_node(dag, &dag->dag_id);
  if(root_node == NULL) {
    LOG_ERR("SRH root node not found\n");
    return 0;
  }

  if(!uip_sr_is_addr_reachable(dag, &UIP_IP_BUF->destipaddr)) {
    LOG_ERR("SRH no path found to destination\n");
    return 0;
  }

  /* Compute path length and compression factors. (We use cmpri == cmpre.) */
  path_len = 0;
  node = dest_node->parent;
  /* For simplicity, we use cmpri = cmpre. */
  cmpri = 15;
  cmpre = 15;

  if(node == root_node) {
    LOG_DBG("SRH no need to insert SRH\n");
    return 1;
  }

  while(node != NULL && node != root_node) {

    NETSTACK_ROUTING.get_sr_node_ipaddr(&node_addr, node);

    /* How many bytes in common between all nodes in the path? */
    cmpri = MIN(cmpri, count_matching_bytes(&node_addr, &UIP_IP_BUF->destipaddr, 16));
    cmpre = cmpri;

    LOG_DBG("SRH Hop ");
    LOG_DBG_6ADDR(&node_addr);
    LOG_DBG_("\n");
    node = node->parent;
    path_len++;
  }

  /* Extension header length:
     fixed headers + (n - 1) * (16 - ComprI) + (16 - ComprE). */
  ext_len = RPL_RH_LEN + RPL_SRH_LEN
    + (path_len - 1) * (16 - cmpre)
    + (16 - cmpri);

  padding = ext_len % 8 == 0 ? 0 : (8 - (ext_len % 8));
  ext_len += padding;

  LOG_DBG("SRH Path len: %u, ComprI %u, ComprE %u, ext len %u (padding %u)\n",
          path_len, cmpri, cmpre, ext_len, padding);

  /* Check if there is enough space to store the extension header. */
  if(uip_len + ext_len > UIP_LINK_MTU) {
    LOG_ERR("Too long packet: impossible to add SRH (%u bytes)\n", ext_len);
    return 0;
  }

  /* Move existing ext headers and payload ext_len further. */
  memmove(uip_buf + UIP_IPH_LEN + uip_ext_len + ext_len,
          uip_buf + UIP_IPH_LEN + uip_ext_len, uip_len - UIP_IPH_LEN);
  memset(uip_buf + UIP_IPH_LEN + uip_ext_len, 0, ext_len);

  /* Insert source routing header (as first ext header). */
  rh_hdr->next = UIP_IP_BUF->proto;
  UIP_IP_BUF->proto = UIP_PROTO_ROUTING;

  /* Initialize IPv6 Routing Header. */
  rh_hdr->len = (ext_len - 8) / 8;
  rh_hdr->routing_type = RPL_RH_TYPE_SRH;
  rh_hdr->seg_left = path_len;

  /* Initialize RPL Source Routing Header. */
  srh_hdr->cmpr = (cmpri << 4) + cmpre;
  srh_hdr->pad = padding << 4;

  /* Initialize the addresses field (the actual source route).
     From last to first. */
  node = dest_node;

  /* Pointer where to write the next hop compressed address. */
  hop_ptr = ((uint8_t *)rh_hdr) + ext_len - padding;

  while(node != NULL && node->parent != root_node) {
    NETSTACK_ROUTING.get_sr_node_ipaddr(&node_addr, node);

    hop_ptr -= (16 - cmpri);
    memcpy(hop_ptr, ((uint8_t *)&node_addr) + cmpri, 16 - cmpri);

    node = node->parent;
  }

  /* The next hop (i.e. node whose parent is the root) is placed as
     the current IPv6 destination. */
  NETSTACK_ROUTING.get_sr_node_ipaddr(&node_addr, node);
  uip_ipaddr_copy(&UIP_IP_BUF->destipaddr, &node_addr);

  /* Update the IPv6 length field. */
  uipbuf_add_ext_hdr(ext_len);
  uipbuf_set_len_field(UIP_IP_BUF, uip_len - UIP_IPH_LEN);

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
update_hbh_header(void)
{
  rpl_instance_t *instance;
  rpl_parent_t *parent;
  struct uip_hbho_hdr *hbh_hdr = (struct uip_hbho_hdr *)UIP_IP_PAYLOAD(0);
  struct uip_ext_hdr_opt_rpl *rpl_opt = (struct uip_ext_hdr_opt_rpl *)(UIP_IP_PAYLOAD(0) + 2);

  if(UIP_IP_BUF->proto == UIP_PROTO_HBHO &&
     rpl_opt->opt_type == UIP_EXT_HDR_OPT_RPL) {
    if(hbh_hdr->len != ((RPL_HOP_BY_HOP_LEN - 8) / 8) ||
       rpl_opt->opt_len != RPL_HDR_OPT_LEN) {

      LOG_ERR("Hop-by-hop extension header has wrong size (%u)\n",
              rpl_opt->opt_len);
      return 0; /* Drop */
    }

    instance = rpl_get_instance(rpl_opt->instance);
    if(instance == NULL || !instance->used || !instance->current_dag->joined) {
      LOG_ERR("Unable to add/update hop-by-hop extension header: incorrect instance\n");
      return 0; /* Drop */
    }

    LOG_INFO("Updating RPL option\n");
    /* Update sender rank and instance, will update flags next. */
    rpl_opt->senderrank = UIP_HTONS(instance->current_dag->rank);
    rpl_opt->instance = instance->instance_id;

    if(RPL_IS_STORING(instance)) {
      /* In non-storing mode, downwards traffic does not have the HBH option. */

      /*
       * Check the direction of the down flag, as per Section
       * 11.2.2.3, which states that if a packet is going down it
       * should in general not go back up again. If this happens, a
       * RPL_HDR_OPT_FWD_ERR should be flagged.
       */
      if(rpl_opt->flags & RPL_HDR_OPT_DOWN) {
        if(uip_ds6_route_lookup(&UIP_IP_BUF->destipaddr) == NULL) {
          rpl_opt->flags |= RPL_HDR_OPT_FWD_ERR;
          LOG_WARN("RPL forwarding error\n");
          /* We should send back the packet to the originating parent,
             but it is not feasible yet, so we send a No-Path DAO instead. */
          LOG_WARN("RPL generate No-Path DAO\n");
          parent = rpl_get_parent((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
          if(parent != NULL) {
            rpl_schedule_unicast_dao_immediately(instance, parent,
                                                 &UIP_IP_BUF->destipaddr,
                                                 RPL_ZERO_LIFETIME);
          }
          /* Drop packet. */
          return 0;
        }
      } else {
        /*
         * Set the down extension flag correctly as described in
         * Section 11.2 of RFC6550. If the packet progresses along a
         * DAO route, the down flag should be set.
         */
        if(uip_ds6_route_lookup(&UIP_IP_BUF->destipaddr) == NULL) {
          /* No route was found, so this packet will go towards the
             RPL root. If so, we should not set the down flag. */
          rpl_opt->flags &= ~RPL_HDR_OPT_DOWN;
          LOG_DBG("RPL option going up\n");
        } else {
          /* A DAO route was found so we set the down flag. */
          rpl_opt->flags |= RPL_HDR_OPT_DOWN;
          LOG_DBG("RPL option going down\n");
        }
      }
    }
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
insert_hbh_header(const rpl_instance_t *instance)
{
  struct uip_hbho_hdr *hbh_hdr = (struct uip_hbho_hdr *)UIP_IP_PAYLOAD(0);
  struct uip_ext_hdr_opt_rpl *rpl_opt = (struct uip_ext_hdr_opt_rpl *)(UIP_IP_PAYLOAD(2));

  /* Insert hop-by-hop header. */
  LOG_DBG("Creating hop-by-hop option\n");
  if(uip_len + RPL_HOP_BY_HOP_LEN > UIP_LINK_MTU) {
    LOG_ERR("Too long packet: impossible to add hop-by-hop option\n");
    return 0;
  }

  /* Move existing ext headers and payload RPL_HOP_BY_HOP_LEN further. */
  memmove(UIP_IP_PAYLOAD(RPL_HOP_BY_HOP_LEN), UIP_IP_PAYLOAD(0),
          uip_len - UIP_IPH_LEN);
  memset(UIP_IP_PAYLOAD(0), 0, RPL_HOP_BY_HOP_LEN);

  /* Insert the HBH header as the first ext header. */
  hbh_hdr->next = UIP_IP_BUF->proto;
  UIP_IP_BUF->proto = UIP_PROTO_HBHO;

  /* Initialize the HBH option. */
  hbh_hdr->len = (RPL_HOP_BY_HOP_LEN - 8) / 8;
  rpl_opt->opt_type = UIP_EXT_HDR_OPT_RPL;
  rpl_opt->opt_len = RPL_HDR_OPT_LEN;
  rpl_opt->flags = 0;
  rpl_opt->senderrank = UIP_HTONS(instance->current_dag->rank);
  rpl_opt->instance = instance->instance_id;

  uipbuf_add_ext_hdr(RPL_HOP_BY_HOP_LEN);
  uipbuf_set_len_field(UIP_IP_BUF, uip_len - UIP_IPH_LEN);

  /* Update the header before returning. */
  return update_hbh_header();
}
/*---------------------------------------------------------------------------*/
bool
rpl_ext_header_remove(void)
{
  uint8_t *prev_proto_ptr;
  uint8_t protocol;
  uint16_t ext_len;
  uint8_t *next_header;
  struct uip_ext_hdr *ext_ptr;
  struct uip_ext_hdr_opt *opt_ptr;

  next_header = uipbuf_get_next_header(uip_buf, uip_len, &protocol, true);
  if(next_header == NULL) {
    return true;
  }

  ext_ptr = (struct uip_ext_hdr *)next_header;
  prev_proto_ptr = &UIP_IP_BUF->proto;

  while(uip_is_proto_ext_hdr(protocol)) {
    opt_ptr = (struct uip_ext_hdr_opt *)(next_header + 2);
    if(protocol == UIP_PROTO_ROUTING ||
       (protocol == UIP_PROTO_HBHO && opt_ptr->type == UIP_EXT_HDR_OPT_RPL)) {
      /* Remove ext header */
      *prev_proto_ptr = ext_ptr->next;
      ext_len = ext_ptr->len * 8 + 8;
      if(uipbuf_add_ext_hdr(-ext_len) == false) {
        return false;
      }

      /* Update length field and rest of packet to the "left". */
      uipbuf_set_len_field(UIP_IP_BUF, uip_len - UIP_IPH_LEN);
      if(uip_len <= next_header - uip_buf) {
        /* No more data to move. */
        return false;
      }
      memmove(next_header, next_header + ext_len,
              uip_len - (next_header - uip_buf));

      /* Update loop variables. */
      protocol = *prev_proto_ptr;
    } else {
      /* Move to the ext hdr. */
      next_header = uipbuf_get_next_header(next_header,
                                           uip_len - (next_header - uip_buf),
                                           &protocol, false);
      if(next_header == NULL) {
        /* Processing finished. */
        break;
      }
      ext_ptr = (struct uip_ext_hdr *)next_header;
      prev_proto_ptr = &ext_ptr->next;
    }
  }

  return true;
}
/*---------------------------------------------------------------------------*/
int
rpl_ext_header_update(void)
{
  if(default_instance == NULL || default_instance->current_dag == NULL ||
     uip_is_addr_linklocal(&UIP_IP_BUF->destipaddr) ||
     uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
    return 1;
  }

  if(default_instance->current_dag->rank == ROOT_RANK(default_instance)) {
    /* At the root, remove headers if any, and insert SRH or HBH.
       (SRH is inserted only if the destination is in the DODAG.) */
    rpl_ext_header_remove();
    if(rpl_get_dag(&UIP_IP_BUF->destipaddr) != NULL) {
      /* dest is in a DODAG; the packet is going down. */
      if(RPL_IS_NON_STORING(default_instance)) {
        return insert_srh_header();
      } else {
        if(uip_ds6_route_lookup(&UIP_IP_BUF->destipaddr) != NULL) {
          /* the destination is in the DAG */
          return insert_hbh_header(default_instance);
        } else {
          /* no need to add the hop-by-hop option */
          return 1;
        }
      }
    } else {
      /* dest is outside of DODAGs; no ext header is needed. */
      return 1;
    }
  } else {
    if(uip_ds6_is_my_addr(&UIP_IP_BUF->srcipaddr)
       && UIP_IP_BUF->ttl == uip_ds6_if.cur_hop_limit) {
      /*
       * Insert a HBH option at the source. Checking the address is
       * insufficient because in non-storing mode, a packet may go up
       * and then down the same path again.
       */
      return insert_hbh_header(default_instance);
    } else {
      /* Update HBH option at forwarders. */
      return update_hbh_header();
    }
  }
}
/** @}*/
