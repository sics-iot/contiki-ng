/*
 * Copyright (c) 2017, Inria.
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
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         The Contiki shell commands
 * \author
 *         Simon Duquennoy <simon.duquennoy@inria.fr>
 */

/**
 * \addtogroup shell
 * @{
 */

#include "contiki.h"
#include "shell.h"
#include "shell-commands.h"
#include "lib/list.h"
#include "sys/log.h"
#include "dev/watchdog.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uiplib.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/uip-ds6.h"
#if BUILD_WITH_RESOLV
#include "resolv.h"
#endif /* BUILD_WITH_RESOLV */
#if BUILD_WITH_HTTP_SOCKET
#include "http-socket.h"
#endif /* BUILD_WITH_HTTP_SOCKET */
#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
#endif /* MAC_CONF_WITH_TSCH */
#if MAC_CONF_WITH_CSMA
#include "net/mac/csma/csma.h"
#endif
#include "net/routing/routing.h"
#include "net/mac/llsec802154.h"

/* For RPL-specific commands */
#if ROUTING_CONF_RPL_LITE
#include "net/routing/rpl-lite/rpl.h"
#elif ROUTING_CONF_RPL_CLASSIC
#include "net/routing/rpl-classic/rpl.h"
#endif

#include <stdlib.h>

#define PING_TIMEOUT (5 * CLOCK_SECOND)

#if NETSTACK_CONF_WITH_IPV6
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static shell_output_func *curr_ping_output_func = NULL;
static struct process *curr_ping_process;
static uint8_t curr_ping_ttl;
static uint16_t curr_ping_datalen;
#endif /* NETSTACK_CONF_WITH_IPV6 */
#if TSCH_WITH_SIXTOP
static shell_command_6top_sub_cmd_t sixtop_sub_cmd = NULL;
#endif /* TSCH_WITH_SIXTOP */
static struct shell_command_set_t builtin_shell_command_set;
LIST(shell_command_sets);
#if NETSTACK_CONF_WITH_IPV6
/*---------------------------------------------------------------------------*/
static const char *
ds6_nbr_state_to_str(uint8_t state)
{
  switch(state) {
    case NBR_INCOMPLETE:
      return "Incomplete";
    case NBR_REACHABLE:
      return "Reachable";
    case NBR_STALE:
      return "Stale";
    case NBR_DELAY:
      return "Delay";
    case NBR_PROBE:
      return "Probe";
    default:
      return "Unknown";
  }
}
/*---------------------------------------------------------------------------*/
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data, uint16_t datalen)
{
  if(curr_ping_output_func != NULL) {
    curr_ping_output_func = NULL;
    curr_ping_ttl = ttl;
    curr_ping_datalen = datalen;
    process_poll(curr_ping_process);
  }
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_ping(struct pt *pt, shell_output_func output, char *args))
{
  static uip_ipaddr_t remote_addr;
  static struct etimer timeout_timer;
  char *next_args;

  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (remote IPv6) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "Destination IPv6 address is not specified\n");
    PT_EXIT(pt);
  } else if(uiplib_ipaddrconv(args, &remote_addr) == 0) {
    SHELL_OUTPUT(output, "Invalid IPv6 address: %s\n", args);
    PT_EXIT(pt);
  }

  SHELL_OUTPUT(output, "Pinging ");
  shell_output_6addr(output, &remote_addr);
  SHELL_OUTPUT(output, "\n");

  /* Send ping request */
  curr_ping_process = PROCESS_CURRENT();
  curr_ping_output_func = output;
  etimer_set(&timeout_timer, PING_TIMEOUT);
  uip_icmp6_send(&remote_addr, ICMP6_ECHO_REQUEST, 0, 4);
  PT_WAIT_UNTIL(pt, curr_ping_output_func == NULL || etimer_expired(&timeout_timer));

  if(curr_ping_output_func != NULL) {
    SHELL_OUTPUT(output, "Timeout\n");
    curr_ping_output_func = NULL;
  } else {
    SHELL_OUTPUT(output, "Received ping reply from ");
    shell_output_6addr(output, &remote_addr);
    SHELL_OUTPUT(output, ", len %u, ttl %u, delay %lu ms\n",
                 curr_ping_datalen, curr_ping_ttl,
                 (unsigned long)((1000 * (clock_time() - timeout_timer.timer.start)) / CLOCK_SECOND));
  }

  PT_END(pt);
}
#endif /* NETSTACK_CONF_WITH_IPV6 */

#if ROUTING_CONF_RPL_LITE
/*---------------------------------------------------------------------------*/
static const char *
rpl_state_to_str(enum rpl_dag_state state)
{
  switch(state) {
    case DAG_INITIALIZED:
      return "Initialized";
    case DAG_JOINED:
      return "Joined";
    case DAG_REACHABLE:
      return "Reachable";
    case DAG_POISONING:
      return "Poisoning";
    default:
      return "Unknown";
  }
}
/*---------------------------------------------------------------------------*/
static const char *
rpl_mop_to_str(int mop)
{
  switch(mop) {
    case RPL_MOP_NO_DOWNWARD_ROUTES:
      return "No downward routes";
    case RPL_MOP_NON_STORING:
      return "Non-storing";
    case RPL_MOP_STORING_NO_MULTICAST:
      return "Storing";
    case RPL_MOP_STORING_MULTICAST:
      return "Storing+multicast";
    default:
      return "Unknown";
  }
}
/*---------------------------------------------------------------------------*/
static const char *
rpl_ocp_to_str(int ocp)
{
  switch(ocp) {
    case RPL_OCP_OF0:
      return "OF0";
    case RPL_OCP_MRHOF:
      return "MRHOF";
    default:
      return "Unknown";
  }
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_rpl_nbr(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  if(!curr_instance.used || rpl_neighbor_count() == 0) {
    SHELL_OUTPUT(output, "RPL neighbors: none\n");
  } else {
    rpl_nbr_t *nbr = nbr_table_head(rpl_neighbors);
    SHELL_OUTPUT(output, "RPL neighbors:\n");
    while(nbr != NULL) {
      char buf[120];
      rpl_neighbor_snprint(buf, sizeof(buf), nbr);
      SHELL_OUTPUT(output, "%s\n", buf);
      nbr = nbr_table_next(rpl_neighbors, nbr);
    }
  }

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_rpl_status(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "RPL status:\n");
  if(!curr_instance.used) {
    SHELL_OUTPUT(output, "-- Instance: None\n");
  } else {
    SHELL_OUTPUT(output, "-- Instance: %u\n", curr_instance.instance_id);
    if(NETSTACK_ROUTING.node_is_root()) {
      SHELL_OUTPUT(output, "-- DAG root\n");
    } else {
      SHELL_OUTPUT(output, "-- DAG node\n");
    }
    SHELL_OUTPUT(output, "-- DAG: ");
    shell_output_6addr(output, &curr_instance.dag.dag_id);
    SHELL_OUTPUT(output, ", version %u\n", curr_instance.dag.version);
    SHELL_OUTPUT(output, "-- Prefix: ");
    shell_output_6addr(output, &curr_instance.dag.prefix_info.prefix);
    SHELL_OUTPUT(output, "/%u\n", curr_instance.dag.prefix_info.length);
    SHELL_OUTPUT(output, "-- MOP: %s\n", rpl_mop_to_str(curr_instance.mop));
    SHELL_OUTPUT(output, "-- OF: %s\n", rpl_ocp_to_str(curr_instance.of->ocp));
    SHELL_OUTPUT(output, "-- Hop rank increment: %u\n", curr_instance.min_hoprankinc);
    SHELL_OUTPUT(output, "-- Default lifetime: %lu seconds\n", RPL_LIFETIME(curr_instance.default_lifetime));

    SHELL_OUTPUT(output, "-- State: %s\n", rpl_state_to_str(curr_instance.dag.state));
    SHELL_OUTPUT(output, "-- Preferred parent: ");
    if(curr_instance.dag.preferred_parent) {
      shell_output_6addr(output, rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent));
      SHELL_OUTPUT(output, " (last DTSN: %u)\n", curr_instance.dag.preferred_parent->dtsn);
    } else {
      SHELL_OUTPUT(output, "None\n");
    }
    SHELL_OUTPUT(output, "-- Rank: %u\n", curr_instance.dag.rank);
    SHELL_OUTPUT(output, "-- Lowest rank: %u (%u)\n", curr_instance.dag.lowest_rank, curr_instance.max_rankinc);
    SHELL_OUTPUT(output, "-- DTSN out: %u\n", curr_instance.dtsn_out);
    SHELL_OUTPUT(output, "-- DAO sequence: last sent %u, last acked %u\n",
        curr_instance.dag.dao_last_seqno, curr_instance.dag.dao_last_acked_seqno);
    SHELL_OUTPUT(output, "-- Trickle timer: current %u, min %u, max %u, redundancy %u\n",
      curr_instance.dag.dio_intcurrent, curr_instance.dio_intmin,
      curr_instance.dio_intmin + curr_instance.dio_intdoubl, curr_instance.dio_redundancy);

  }

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_rpl_refresh_routes(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "Triggering routes refresh\n");
  rpl_refresh_routes("Shell");

  PT_END(pt);
}
#endif /* ROUTING_CONF_RPL_LITE */
/*---------------------------------------------------------------------------*/
static void
shell_output_log_levels(shell_output_func output)
{
  int i = 0;
  SHELL_OUTPUT(output, "Log levels:\n");
  while(all_modules[i].name != NULL) {
    SHELL_OUTPUT(output, "-- %-10s: %u (%s)\n",
      all_modules[i].name,
      *all_modules[i].curr_log_level,
      log_level_to_str(*all_modules[i].curr_log_level));
    i++;
  }
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_log(struct pt *pt, shell_output_func output, char *args))
{
  static int prev_level;
  static int level;
  char *next_args;
  char *ptr;
  char *module;

  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get and parse argument: module name */
  SHELL_ARGS_NEXT(args, next_args);
  module = args;
  if(module == NULL) {
    SHELL_OUTPUT(output, "Module is not specified\n");
    PT_EXIT(pt);
  }
  prev_level = log_get_level(module);
  if(module == NULL || (strcmp("all", module) && prev_level == -1)) {
    SHELL_OUTPUT(output, "Invalid first argument: %s\n", module)
    shell_output_log_levels(output);
    PT_EXIT(pt);
  }

  /* Get and parse argument: log level */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    level = -1;
  } else {
    level = (int)strtol(args, &ptr, 10);
  }
  if((level == 0 && args == ptr)
    || level < LOG_LEVEL_NONE || level > LOG_LEVEL_DBG) {
    SHELL_OUTPUT(output, "Invalid second argument: %s\n", args);
    PT_EXIT(pt);
  }

  /* Set log level */
  if(level != prev_level) {
    log_set_level(module, level);
#if MAC_CONF_WITH_TSCH && TSCH_LOG_PER_SLOT
    if(!strcmp(module, "mac") || !strcmp(module, "all")) {
      if(level >= LOG_LEVEL_DBG) {
        tsch_log_init();
        SHELL_OUTPUT(output, "TSCH logging started\n");
      } else {
        tsch_log_stop();
        SHELL_OUTPUT(output, "TSCH logging stopped\n");
      }
    }
#endif /* MAC_CONF_WITH_TSCH && TSCH_LOG_PER_SLOT */
  }

  shell_output_log_levels(output);

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_help(struct pt *pt, shell_output_func output, char *args))
{
  struct shell_command_set_t *set;
  const struct shell_command_t *cmd;
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "Available commands:\n");
  /* Note: we explicitly don't expend any code space to deal with shadowing */
  for(set = list_head(shell_command_sets); set != NULL; set = list_item_next(set)) {
    for(cmd = set->commands; cmd->name != NULL; ++cmd) {
      if(cmd->help == NULL) {
        /* nothing to print */
      } else {
        SHELL_OUTPUT(output, "%s\n", cmd->help);
      }
    }
  }

  PT_END(pt);
}
#if UIP_CONF_IPV6_RPL
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_rpl_set_root(struct pt *pt, shell_output_func output, char *args))
{
  static int is_on;
  static uip_ipaddr_t prefix;
  char *next_args;

  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get first arg (0/1) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "On-flag (0 or 1) is not specified\n");
    PT_EXIT(pt);
  }

  if(!strcmp(args, "1")) {
    is_on = 1;
  } else if(!strcmp(args, "0")) {
    is_on = 0;
  } else {
    SHELL_OUTPUT(output, "Invalid argument: %s\n", args);
    PT_EXIT(pt);
  }

  /* Get first second arg (prefix) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args != NULL) {
    if(uiplib_ipaddrconv(args, &prefix) == 0) {
      SHELL_OUTPUT(output, "Invalid Prefix: %s\n", args);
      PT_EXIT(pt);
    }
  } else {
    const uip_ipaddr_t *default_prefix = uip_ds6_default_prefix();
    uip_ip6addr_copy(&prefix, default_prefix);
  }

  if(is_on) {
    if(!NETSTACK_ROUTING.node_is_root()) {
      SHELL_OUTPUT(output, "Setting as DAG root with prefix ");
      shell_output_6addr(output, &prefix);
      SHELL_OUTPUT(output, "/64\n");
      NETSTACK_ROUTING.root_set_prefix(&prefix, NULL);
      NETSTACK_ROUTING.root_start();
    } else {
      SHELL_OUTPUT(output, "Node is already a DAG root\n");
    }
  } else {
    if(NETSTACK_ROUTING.node_is_root()) {
      SHELL_OUTPUT(output, "Setting as non-root node: leaving DAG\n");
      NETSTACK_ROUTING.leave_network();
    } else {
      SHELL_OUTPUT(output, "Node is not a DAG root\n");
    }
  }

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_rpl_global_repair(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "Triggering routing global repair\n");
  NETSTACK_ROUTING.global_repair("Shell");

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_rpl_local_repair(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "Triggering routing local repair\n");
  NETSTACK_ROUTING.local_repair("Shell");

  PT_END(pt);
}
#endif /* UIP_CONF_IPV6_RPL */
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_macaddr(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "Node MAC address: ");
  shell_output_lladdr(output, &linkaddr_node_addr);
  SHELL_OUTPUT(output, "\n");

  PT_END(pt);
}
#if NETSTACK_CONF_WITH_IPV6
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_ipaddr(struct pt *pt, shell_output_func output, char *args))
{
  int i;
  uint8_t state;

  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "Node IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      SHELL_OUTPUT(output, "-- ");
      shell_output_6addr(output, &uip_ds6_if.addr_list[i].ipaddr);
      SHELL_OUTPUT(output, "\n");
    }
  }

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_ip_neighbors(struct pt *pt, shell_output_func output, char *args))
{
  uip_ds6_nbr_t *nbr;

  PT_BEGIN(pt);

  nbr = uip_ds6_nbr_head();
  if(nbr == NULL) {
    SHELL_OUTPUT(output, "Node IPv6 neighbors: none\n");
    PT_EXIT(pt);
  }

  SHELL_OUTPUT(output, "Node IPv6 neighbors:\n");
  while(nbr != NULL) {
    SHELL_OUTPUT(output, "-- ");
    shell_output_6addr(output, uip_ds6_nbr_get_ipaddr(nbr));
    SHELL_OUTPUT(output, " <-> ");
    shell_output_lladdr(output, (linkaddr_t *)uip_ds6_nbr_get_ll(nbr));
    SHELL_OUTPUT(output, ", router %u, state %s ",
      nbr->isrouter, ds6_nbr_state_to_str(nbr->state));
    SHELL_OUTPUT(output, "\n");
    nbr = uip_ds6_nbr_next(nbr);
  }

  PT_END(pt);

}
#endif /* NETSTACK_CONF_WITH_IPV6 */
#if MAC_CONF_WITH_TSCH
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_tsch_set_coordinator(struct pt *pt, shell_output_func output, char *args))
{
  static int is_on;
  static int is_secured;
  char *next_args;

  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  /* Get first arg (0/1) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "On-flag (0 or 1) is not specified\n");
    PT_EXIT(pt);
  }

  if(!strcmp(args, "1")) {
    is_on = 1;
  } else if(!strcmp(args, "0")) {
    is_on = 0;
  } else {
    SHELL_OUTPUT(output, "Invalid first argument: %s\n", args);
    PT_EXIT(pt);
  }

  /* Get first second arg (prefix) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args != NULL) {
    if(!strcmp(args, "1")) {
#if LLSEC802154_ENABLED
      is_secured = 1;
#else /* LLSEC802154_ENABLED */
      SHELL_OUTPUT(output, "Security is not compiled in.\n");
      is_secured = 0;
#endif /* LLSEC802154_ENABLED */
    } else if(!strcmp(args, "0")) {
      is_secured = 0;
    } else {
      SHELL_OUTPUT(output, "Invalid second argument: %s\n", args);
      PT_EXIT(pt);
    }
  } else {
    is_secured = 0;
  }

  SHELL_OUTPUT(output, "Setting as TSCH %s (%s)\n",
    is_on ? "coordinator" : "non-coordinator", is_secured ? "secured" : "non-secured");

  tsch_set_pan_secured(is_secured);
  tsch_set_coordinator(is_on);

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_tsch_status(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);

  SHELL_OUTPUT(output, "TSCH status:\n");

  SHELL_OUTPUT(output, "-- Is coordinator: %u\n", tsch_is_coordinator);
  SHELL_OUTPUT(output, "-- Is associated: %u\n", tsch_is_associated);
  if(tsch_is_associated) {
    struct tsch_neighbor *n = tsch_queue_get_time_source();
    SHELL_OUTPUT(output, "-- PAN ID: 0x%x\n", frame802154_get_pan_id());
    SHELL_OUTPUT(output, "-- Is PAN secured: %u\n", tsch_is_pan_secured);
    SHELL_OUTPUT(output, "-- Join priority: %u\n", tsch_join_priority);
    SHELL_OUTPUT(output, "-- Time source: ");
    if(n != NULL) {
      shell_output_lladdr(output, tsch_queue_get_nbr_address(n));
      SHELL_OUTPUT(output, "\n");
    } else {
      SHELL_OUTPUT(output, "none\n");
    }
    SHELL_OUTPUT(output, "-- Last synchronized: %lu seconds ago\n",
                 (unsigned long)((clock_time() - tsch_last_sync_time) / CLOCK_SECOND));
    SHELL_OUTPUT(output, "-- Drift w.r.t. coordinator: %ld ppm\n",
                 tsch_adaptive_timesync_get_drift_ppm());
    SHELL_OUTPUT(output, "-- Network uptime: %lu seconds\n",
                 (unsigned long)(tsch_get_network_uptime_ticks() / CLOCK_SECOND));
  }

  PT_END(pt);
}
#endif /* MAC_CONF_WITH_TSCH */
#if NETSTACK_CONF_WITH_IPV6
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_routes(struct pt *pt, shell_output_func output, char *args))
{
  uip_ds6_defrt_t *default_route;

  PT_BEGIN(pt);

  /* Our default route */
  SHELL_OUTPUT(output, "Default route:\n");
  default_route = uip_ds6_defrt_lookup(uip_ds6_defrt_choose());
  if(default_route != NULL) {
    SHELL_OUTPUT(output, "-- ");
    shell_output_6addr(output, &default_route->ipaddr);
    if(default_route->lifetime.interval != 0) {
      SHELL_OUTPUT(output, " (lifetime: %lu seconds)\n", (unsigned long)default_route->lifetime.interval);
    } else {
      SHELL_OUTPUT(output, " (lifetime: infinite)\n");
    }
  } else {
    SHELL_OUTPUT(output, "-- None\n");
  }

#if UIP_CONF_IPV6_RPL
  if(uip_sr_num_nodes() > 0) {
    uip_sr_node_t *link;
    /* Our routing links */
    SHELL_OUTPUT(output, "Routing links (%u in total):\n", uip_sr_num_nodes());
    link = uip_sr_node_head();
    while(link != NULL) {
      char buf[100];
      uip_sr_link_snprint(buf, sizeof(buf), link);
      SHELL_OUTPUT(output, "-- %s\n", buf);
      link = uip_sr_node_next(link);
    }
  } else {
    SHELL_OUTPUT(output, "No routing links\n");
  }
#endif /* UIP_CONF_IPV6_RPL */

#if (UIP_MAX_ROUTES != 0)
  if(uip_ds6_route_num_routes() > 0) {
    uip_ds6_route_t *route;
    /* Our routing entries */
    SHELL_OUTPUT(output, "Routing entries (%u in total):\n", uip_ds6_route_num_routes());
    route = uip_ds6_route_head();
    while(route != NULL) {
      SHELL_OUTPUT(output, "-- ");
      shell_output_6addr(output, &route->ipaddr);
      SHELL_OUTPUT(output, " via ");
      shell_output_6addr(output, uip_ds6_route_nexthop(route));
      if((unsigned long)route->state.lifetime != 0xFFFFFFFF) {
        SHELL_OUTPUT(output, " (lifetime: %lu seconds)\n", (unsigned long)route->state.lifetime);
      } else {
        SHELL_OUTPUT(output, " (lifetime: infinite)\n");
      }
      route = uip_ds6_route_next(route);
    }
  } else {
    SHELL_OUTPUT(output, "No routing entries\n");
  }
#endif /* (UIP_MAX_ROUTES != 0) */

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
#if BUILD_WITH_RESOLV
static
PT_THREAD(cmd_resolv(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);
  static struct etimer timeout_timer;
  static int count, ret;
  char *next_args;
  static uip_ipaddr_t *remote_addr = NULL;
  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (remote hostname) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "Destination host is not specified\n");
    PT_EXIT(pt);
  } else {
    ret = resolv_lookup(args, &remote_addr);
    if(ret == RESOLV_STATUS_UNCACHED || ret == RESOLV_STATUS_RESOLVING) {
      SHELL_OUTPUT(output, "Looking up IPv6 address for host: %s\n", args);
      if(ret != RESOLV_STATUS_RESOLVING) {
        resolv_query(args);
      }
      /* Poll 10 times for resolve results (5 seconds max)*/
      for(count = 0; count < 10; count++) {
        etimer_set(&timeout_timer, CLOCK_SECOND / 2);
        PT_WAIT_UNTIL(pt, etimer_expired(&timeout_timer));
        printf("resoliving again...\n");
        if((ret = resolv_lookup(args, &remote_addr)) != RESOLV_STATUS_RESOLVING) {
          break;
        }
      }
    }
    if(ret == RESOLV_STATUS_NOT_FOUND) {
      SHELL_OUTPUT(output, "Did not find IPv6 address for host: %s\n", args);
    } else if(ret == RESOLV_STATUS_CACHED) {
      SHELL_OUTPUT(output, "Found IPv6 address for host: %s => ", args);
      shell_output_6addr(output, remote_addr);
      SHELL_OUTPUT(output, "\n");
    }
  }
  PT_END(pt);
}
#endif /* BUILD_WITH_RESOLV */
/*---------------------------------------------------------------------------*/
#if BUILD_WITH_HTTP_SOCKET
static struct http_socket s;
static int bytes_received = 0;

static void
http_callback(struct http_socket *s, void *ptr,
         http_socket_event_t e,
         const uint8_t *data, uint16_t datalen)
{
  if(e == HTTP_SOCKET_ERR) {
    printf("HTTP socket error\n");
  } else if(e == HTTP_SOCKET_TIMEDOUT) {
    printf("HTTP socket error: timed out\n");
  } else if(e == HTTP_SOCKET_ABORTED) {
    printf("HTTP socket error: aborted\n");
  } else if(e == HTTP_SOCKET_HOSTNAME_NOT_FOUND) {
    printf("HTTP socket error: hostname not found\n");
  } else if(e == HTTP_SOCKET_CLOSED) {
    printf("HTTP socket closed, %d bytes received\n", bytes_received);
  } else if(e == HTTP_SOCKET_DATA) {
    int i;
    if(bytes_received == 0) {
      printf("HTTP socket received data, total expects:%d\n", (int) s->header.content_length);
    }

    bytes_received += datalen;
    for(i = 0; i < datalen; i++) {
      printf("%c", data[i]);
    }
  }
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_wget(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);
  char *next_args;
  SHELL_ARGS_INIT(args, next_args);

  /* Get argument (remote hostname and url (http://host/url) */
  SHELL_ARGS_NEXT(args, next_args);
  if(args == NULL) {
    SHELL_OUTPUT(output, "URL is not specified\n");
    PT_EXIT(pt);
  } else {
    bytes_received = 0;
    SHELL_OUTPUT(output, "Fetching web page at %s\n", args);
    http_socket_init(&s);
    http_socket_get(&s, args, 0, 0,
                    http_callback, NULL);
  }

  PT_END(pt);
}
#endif /* BUILD_WITH_HTTP_SOCKET */
/*---------------------------------------------------------------------------*/
#endif /* NETSTACK_CONF_WITH_IPV6 */
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_reboot(struct pt *pt, shell_output_func output, char *args))
{
  PT_BEGIN(pt);
  SHELL_OUTPUT(output, "rebooting\n");
  watchdog_reboot();
  PT_END(pt);
}
#if MAC_CONF_WITH_TSCH
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_tsch_schedule(struct pt *pt, shell_output_func output, char *args))
{
  struct tsch_slotframe *sf;

  PT_BEGIN(pt);

  if(tsch_is_locked()) {
    PT_EXIT(pt);
  }

  sf = tsch_schedule_slotframe_head();

  if(sf == NULL) {
    SHELL_OUTPUT(output, "TSCH schedule: no slotframe\n");
  } else {
    SHELL_OUTPUT(output, "TSCH schedule:\n");
    while(sf != NULL) {
      struct tsch_link *l = list_head(sf->links_list);

      SHELL_OUTPUT(output, "-- Slotframe: handle %u, size %u, links:\n", sf->handle, sf->size.val);

      while(l != NULL) {
        SHELL_OUTPUT(output, "---- Options %02x, type %u, timeslot %u, channel offset %u, address ",
               l->link_options, l->link_type, l->timeslot, l->channel_offset);
        shell_output_lladdr(output, &l->addr);
        SHELL_OUTPUT(output, "\n");
        l = list_item_next(l);
      }

      sf = tsch_schedule_slotframe_next(sf);
    }
  }
  PT_END(pt);
}
#endif /* MAC_CONF_WITH_TSCH */
/*---------------------------------------------------------------------------*/
#if TSCH_WITH_SIXTOP
void
shell_commands_set_6top_sub_cmd(shell_command_6top_sub_cmd_t sub_cmd)
{
  sixtop_sub_cmd = sub_cmd;
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_6top(struct pt *pt, shell_output_func output, char *args))
{
  char *next_args;

  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  if(sixtop_sub_cmd == NULL) {
    SHELL_OUTPUT(output, "6top command is unavailable:\n");
  } else {
    SHELL_OUTPUT(output, "6top: ");
    sixtop_sub_cmd(output, args);
  }
  SHELL_ARGS_NEXT(args, next_args);

  PT_END(pt);
}
#endif /* TSCH_WITH_SIXTOP */
/*---------------------------------------------------------------------------*/
#if LLSEC802154_ENABLED
static
PT_THREAD(cmd_llsec_setlv(struct pt *pt, shell_output_func output, char *args))
{

  PT_BEGIN(pt);

  if(args == NULL) {
    SHELL_OUTPUT(output, "Default LLSEC level is %d\n",
                 uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
    PT_EXIT(pt);
  } else {
    int lv = atoi(args);
    if(lv < 0 || lv > 7) {
      SHELL_OUTPUT(output, "Illegal LLSEC Level %d\n", lv);
      PT_EXIT(pt);
    } else {
      uipbuf_set_default_attr(UIPBUF_ATTR_LLSEC_LEVEL, lv);
      uipbuf_clear_attr();
      SHELL_OUTPUT(output, "LLSEC default level set %d\n", lv);
    }
  }

  PT_END(pt);
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(cmd_llsec_setkey(struct pt *pt, shell_output_func output, char *args))
{
  char *next_args;

  PT_BEGIN(pt);

  SHELL_ARGS_INIT(args, next_args);

  if(args == NULL) {
    SHELL_OUTPUT(output, "Provide an index and a 16-char string for the key\n");
    PT_EXIT(pt);
  } else {
    int key;
    SHELL_ARGS_NEXT(args, next_args);
    if(args == NULL) {
      SHELL_OUTPUT(output, "Key index is not specified\n");
      PT_EXIT(pt);
    }
    key = atoi(args);
    if(key < 0) {
      SHELL_OUTPUT(output, "Illegal LLSEC Key index %d\n", key);
      PT_EXIT(pt);
    } else {
#if MAC_CONF_WITH_CSMA
      /* Get next arg (key-string) */
      SHELL_ARGS_NEXT(args, next_args);
      if(args == NULL) {
        SHELL_OUTPUT(output, "Provide both an index and a key\n");
      } else if(strlen(args) == 16) {
        csma_security_set_key(key, (const uint8_t *) args);
        SHELL_OUTPUT(output, "Set key for index %d\n", key);
      } else {
        SHELL_OUTPUT(output, "Wrong length of key: '%s' (%d)\n", args, strlen(args));
      }
#else
      SHELL_OUTPUT(output, "Set key not supported.\n");
      PT_EXIT(pt);
#endif
    }
  }
  PT_END(pt);
}
#endif /* LLSEC802154_ENABLED */
/*---------------------------------------------------------------------------*/
void
shell_commands_init(void)
{
  list_init(shell_command_sets);
  list_add(shell_command_sets, &builtin_shell_command_set);
#if NETSTACK_CONF_WITH_IPV6
  /* Set up Ping Reply callback */
  uip_icmp6_echo_reply_callback_add(&echo_reply_notification,
                                    echo_reply_handler);
#endif /* NETSTACK_CONF_WITH_IPV6 */
}
/*---------------------------------------------------------------------------*/
void
shell_command_set_register(struct shell_command_set_t *set)
{
  list_push(shell_command_sets, set);
}
/*---------------------------------------------------------------------------*/
int
shell_command_set_deregister(struct shell_command_set_t *set)
{
  if(!list_contains(shell_command_sets, set)) {
    return !0;
  }
  list_remove(shell_command_sets, set);
  return 0;
}
/*---------------------------------------------------------------------------*/
const struct shell_command_t *
shell_command_lookup(const char *name)
{
  struct shell_command_set_t *set;
  const struct shell_command_t *cmd;

  for(set = list_head(shell_command_sets);
      set != NULL;
      set = list_item_next(set)) {
    for(cmd = set->commands; cmd->name != NULL; ++cmd) {
      if(!strcmp(cmd->name, name)) {
        return cmd;
      }
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
const struct shell_command_t builtin_shell_commands[] = {
  { "help",                 cmd_help,                 "'> help': Shows this help" },
  { "reboot",               cmd_reboot,               "'> reboot': Reboot the board by watchdog_reboot()" },
  { "log",                  cmd_log,                  "'> log module level': Sets log level (0--4) for a given module (or \"all\"). For module \"mac\", level 4 also enables per-slot logging." },
  { "mac-addr",             cmd_macaddr,               "'> mac-addr': Shows the node's MAC address" },
#if NETSTACK_CONF_WITH_IPV6
  { "ip-addr",              cmd_ipaddr,               "'> ip-addr': Shows all IPv6 addresses" },
  { "ip-nbr",               cmd_ip_neighbors,         "'> ip-nbr': Shows all IPv6 neighbors" },
  { "ping",                 cmd_ping,                 "'> ping addr': Pings the IPv6 address 'addr'" },
  { "routes",               cmd_routes,               "'> routes': Shows the route entries" },
#if BUILD_WITH_RESOLV
  { "nslookup",             cmd_resolv,               "'> nslookup': Lookup IPv6 address of host" },
#endif /* BUILD_WITH_RESOLV */
#if BUILD_WITH_HTTP_SOCKET
  { "wget",                 cmd_wget,                 "'> wget url': get content of URL (only http)." },
#endif /* BUILD_WITH_HTTP_SOCKET */
#endif /* NETSTACK_CONF_WITH_IPV6 */
#if UIP_CONF_IPV6_RPL
  { "rpl-set-root",         cmd_rpl_set_root,         "'> rpl-set-root 0/1 [prefix]': Sets node as root (1) or not (0). A /64 prefix can be optionally specified." },
  { "rpl-local-repair",     cmd_rpl_local_repair,     "'> rpl-local-repair': Triggers a RPL local repair" },
#if ROUTING_CONF_RPL_LITE
  { "rpl-refresh-routes",   cmd_rpl_refresh_routes,   "'> rpl-refresh-routes': Refreshes all routes through a DTSN increment" },
  { "rpl-status",           cmd_rpl_status,           "'> rpl-status': Shows a summary of the current RPL state" },
  { "rpl-nbr",              cmd_rpl_nbr,              "'> rpl-nbr': Shows the RPL neighbor table" },
#endif /* ROUTING_CONF_RPL_LITE */
  { "rpl-global-repair",    cmd_rpl_global_repair,    "'> rpl-global-repair': Triggers a RPL global repair" },
#endif /* UIP_CONF_IPV6_RPL */
#if MAC_CONF_WITH_TSCH
  { "tsch-set-coordinator", cmd_tsch_set_coordinator, "'> tsch-set-coordinator 0/1 [0/1]': Sets node as coordinator (1) or not (0). Second, optional parameter: enable (1) or disable (0) security." },
  { "tsch-schedule",        cmd_tsch_schedule,        "'> tsch-schedule': Shows the current TSCH schedule" },
  { "tsch-status",          cmd_tsch_status,          "'> tsch-status': Shows a summary of the current TSCH state" },
#endif /* MAC_CONF_WITH_TSCH */
#if TSCH_WITH_SIXTOP
  { "6top",                 cmd_6top,                 "'> 6top help': Shows 6top command usage" },
#endif /* TSCH_WITH_SIXTOP */
#if LLSEC802154_ENABLED
  { "llsec-set-level", cmd_llsec_setlv, "'> llsec-set-level <lv>': Set the level of link layer security (show if no lv argument)"},
  { "llsec-set-key", cmd_llsec_setkey, "'> llsec-set-key <id> <key>': Set the key of link layer security"},
#endif /* LLSEC802154_ENABLED */
  { NULL, NULL, NULL },
};

static struct shell_command_set_t builtin_shell_command_set = {
  .next = NULL,
  .commands = builtin_shell_commands,
};
/** @} */
