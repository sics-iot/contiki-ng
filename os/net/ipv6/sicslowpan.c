/*
 * Copyright (c) 2008, Swedish Institute of Computer Science.
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
 *         6LoWPAN implementation (RFC 4944 and RFC 6282)
 *
 * \author Adam Dunkels <adam@sics.se>
 * \author Nicolas Tsiftes <nvt@sics.se>
 * \author Niclas Finne <nfi@sics.se>
 * \author Mathilde Durvy <mdurvy@cisco.com>
 * \author Julien Abeille <jabeille@cisco.com>
 * \author Joakim Eriksson <joakime@sics.se>
 * \author Joel Hoglund <joel@sics.se>
 */

/**
 * \addtogroup sicslowpan
 * \ingroup uip
 * @{
 */

/**
 * FOR RFC 6282 COMPLIANCE TODO:
 * -Add compression options to UDP, currently only supports
 *  both ports compressed or both ports elided
 *
 * -Verify TC/FL compression works
 *
 * -Add stateless multicast option
 */

#include <string.h>

#include "contiki.h"
#include "dev/watchdog.h"
#include "net/link-stats.h"
#include "net/ipv6/uipopt.h"
#include "net/ipv6/tcpip.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uipbuf.h"
#include "net/ipv6/sicslowpan.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"

#include "net/routing/routing.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "6LoWPAN"
#define LOG_LEVEL LOG_LEVEL_6LOWPAN

#define GET16(ptr,index) (((uint16_t)((ptr)[(index)] << 8)) | ((ptr)[(index) + 1]))
#define SET16(ptr,index,value) do {     \
  (ptr)[(index)] = ((value) >> 8) & 0xff; \
  (ptr)[(index) + 1] = (value) & 0xff;    \
} while(0)

/** \name Pointers in the packetbuf buffer
 *  @{
 */
#define PACKETBUF_FRAG_PTR           (packetbuf_ptr)
#define PACKETBUF_FRAG_DISPATCH_SIZE 0   /* 16 bit */
#define PACKETBUF_FRAG_TAG           2   /* 16 bit */
#define PACKETBUF_FRAG_OFFSET        4   /* 8 bit */

/* define the buffer as a byte array */
#define PACKETBUF_IPHC_BUF              ((uint8_t *)(packetbuf_ptr + packetbuf_hdr_len))
#define PACKETBUF_PAYLOAD_END           ((uint8_t *)(packetbuf_ptr + mac_max_payload))

#define PACKETBUF_6LO_PTR            (packetbuf_ptr + packetbuf_hdr_len)
#define PACKETBUF_6LO_DISPATCH       0 /* 8 bit */
#define PACKETBUF_6LO_ENCODING       1 /* 8 bit */
#define PACKETBUF_6LO_TTL            2 /* 8 bit */

#define PACKETBUF_6LO_HC_UDP_PTR           (packetbuf_ptr + packetbuf_hdr_len)
#define PACKETBUF_6LO_HC_UDP_DISPATCH      0 /* 8 bit */
#define PACKETBUF_6LO_HC_UDP_HC1_ENCODING  1 /* 8 bit */
#define PACKETBUF_6LO_HC_UDP_UDP_ENCODING  2 /* 8 bit */
#define PACKETBUF_6LO_HC_UDP_TTL           3 /* 8 bit */
#define PACKETBUF_6LO_HC_UDP_PORTS         4 /* 8 bit */
#define PACKETBUF_6LO_HC_UDP_CHKSUM        5 /* 16 bit */

/** @} */

/** \name Pointers in the sicslowpan and uip buffer
 *  @{
 */

/* NOTE: In the multiple-reassembly context there is only room for the header / first fragment */
#define SICSLOWPAN_IP_BUF(buf)   ((struct uip_ip_hdr *)buf)
#define SICSLOWPAN_UDP_BUF(buf)  ((struct uip_udp_hdr *)&buf[UIP_IPH_LEN])
#define SICSLOWPAN_IPPAYLOAD_BUF(buf) (&buf[UIP_IPH_LEN])

#define UIP_IPPAYLOAD_BUF_POS(pos)         (&uip_buf[UIP_IPH_LEN + (pos)])
#define UIP_UDP_BUF_POS(pos)               ((struct uip_udp_hdr *)UIP_IPPAYLOAD_BUF_POS(pos))
#define UIP_EXT_HDR_LEN                    2

/** @} */

/* set this to zero if not compressing EXT_HDR - for backwards compatibility */
#ifdef SICSLOWPAN_CONF_COMPRESS_EXT_HDR
#define COMPRESS_EXT_HDR SICSLOWPAN_CONF_COMPRESS_EXT_HDR
#else
/* Compressing on by default - turn off to be compatible with older versions */
#define COMPRESS_EXT_HDR 1
#endif

#if COMPRESS_EXT_HDR
#define IS_COMPRESSABLE_PROTO(x) (x == UIP_PROTO_UDP               \
                                  || x == UIP_PROTO_HBHO           \
                                  || x == UIP_PROTO_DESTO          \
                                  || x == UIP_PROTO_ROUTING        \
                                  || x == UIP_PROTO_FRAG)
#else
#define IS_COMPRESSABLE_PROTO(x) (x == UIP_PROTO_UDP)
#endif /* COMPRESS_EXT_HDR */

/** \name General variables
 *  @{
 */

/**
 * A pointer to the packetbuf buffer.
 * We initialize it to the beginning of the packetbuf buffer, then
 * access different fields by updating the offset packetbuf_hdr_len.
 */
static uint8_t *packetbuf_ptr;

/**
 * packetbuf_hdr_len is the total length of (the processed) 6lowpan headers
 * (fragment headers, IPV6 or HC1, HC2, and HC1 and HC2 non compressed
 * fields).
 */
static uint8_t packetbuf_hdr_len;

/**
 * The length of the payload in the Packetbuf buffer.
 * The payload is what comes after the compressed or uncompressed
 * headers (can be the IP payload if the IP header only is compressed
 * or the UDP payload if the UDP header is also compressed)
 */
static int packetbuf_payload_len;

/**
 * uncomp_hdr_len is the length of the headers before compression (if HC2
 * is used this includes the UDP header in addition to the IP header).
 */
static uint8_t uncomp_hdr_len;

/**
 * mac_max_payload is the maimum payload space on the MAC frame.
 */
static int mac_max_payload;

/**
 * The current page (RFC 4944)
 */
static uint8_t curr_page;

/**
 * the result of the last transmitted fragment
 */
static int last_tx_status;
/** @} */

/* ----------------------------------------------------------------- */
/* Support for reassembling multiple packets                         */
/* ----------------------------------------------------------------- */

#if SICSLOWPAN_CONF_FRAG
static uint16_t my_tag;

/** The total length of the IPv6 packet in the sicslowpan_buf. */

/* This needs to be defined in NBR / Nodes depending on available RAM   */
/*   and expected reassembly requirements                               */
#ifdef SICSLOWPAN_CONF_FRAGMENT_BUFFERS
#define SICSLOWPAN_FRAGMENT_BUFFERS SICSLOWPAN_CONF_FRAGMENT_BUFFERS
#else
#define SICSLOWPAN_FRAGMENT_BUFFERS 12
#endif

/* REASS_CONTEXTS corresponds to the number of simultaneous
 * reassemblies that can be made. NOTE: the first buffer for each
 * reassembly is stored in the context since it can be larger than the
 * rest of the fragments due to header compression.
 **/
#ifdef SICSLOWPAN_CONF_REASS_CONTEXTS
#define SICSLOWPAN_REASS_CONTEXTS SICSLOWPAN_CONF_REASS_CONTEXTS
#else
#define SICSLOWPAN_REASS_CONTEXTS 2
#endif

/* The size of each fragment (IP payload) for the 6lowpan fragmentation */
#ifdef SICSLOWPAN_CONF_FRAGMENT_SIZE
#define SICSLOWPAN_FRAGMENT_SIZE SICSLOWPAN_CONF_FRAGMENT_SIZE
#else
/* The default fragment size (110 bytes for 127-2 bytes frames) */
#define SICSLOWPAN_FRAGMENT_SIZE (127 - 2 - 15)
#endif

/* Check the selected fragment size, since we use 8-bit integers to handle it. */
#if SICSLOWPAN_FRAGMENT_SIZE > 255
#error Too large SICSLOWPAN_FRAGMENT_SIZE set.
#endif

/* Assuming that the worst growth for uncompression is 38 bytes */
#define SICSLOWPAN_FIRST_FRAGMENT_SIZE (SICSLOWPAN_FRAGMENT_SIZE + 38)

/* all information needed for reassembly */
struct sicslowpan_frag_info {
  /** When reassembling, the source address of the fragments being merged */
  linkaddr_t sender;
  /** When reassembling, the tag in the fragments being merged. */
  uint16_t tag;
  /** Total length of the fragmented packet */
  uint16_t len;
  /** Current length of reassembled fragments */
  uint16_t reassembled_len;
  /** Reassembly %process %timer. */
  struct timer reass_timer;

  /** Fragment size of first fragment */
  uint16_t first_frag_len;
  /** First fragment - needs a larger buffer since the size is uncompressed size
   and we need to know total size to know when we have received last fragment. */
  uint8_t first_frag[SICSLOWPAN_FIRST_FRAGMENT_SIZE];
};

static struct sicslowpan_frag_info frag_info[SICSLOWPAN_REASS_CONTEXTS];

struct sicslowpan_frag_buf {
  /* the index of the frag_info */
  uint8_t index;
  /* Fragment offset */
  uint8_t offset;
  /* Length of this fragment (if zero this buffer is not allocated) */
  uint8_t len;
  uint8_t data[SICSLOWPAN_FRAGMENT_SIZE];
};

static struct sicslowpan_frag_buf frag_buf[SICSLOWPAN_FRAGMENT_BUFFERS];

/*---------------------------------------------------------------------------*/
static int
clear_fragments(uint8_t frag_info_index)
{
  int i, clear_count;
  clear_count = 0;
  frag_info[frag_info_index].len = 0;
  for(i = 0; i < SICSLOWPAN_FRAGMENT_BUFFERS; i++) {
    if(frag_buf[i].len > 0 && frag_buf[i].index == frag_info_index) {
      /* deallocate the buffer */
      frag_buf[i].len = 0;
      clear_count++;
    }
  }
  return clear_count;
}
/*---------------------------------------------------------------------------*/
static int
timeout_fragments(int not_context)
{
  int i;
  int count = 0;
  for(i = 0; i < SICSLOWPAN_REASS_CONTEXTS; i++) {
    if(frag_info[i].len > 0 && i != not_context &&
       timer_expired(&frag_info[i].reass_timer)) {
      /* This context can be freed */
      count += clear_fragments(i);
    }
  }
  return count;
}
/*---------------------------------------------------------------------------*/
static int
store_fragment(uint8_t index, uint8_t offset)
{
  int i;
  int len;

  len = packetbuf_datalen() - packetbuf_hdr_len;

  if(len <= 0 || len > SICSLOWPAN_FRAGMENT_SIZE) {
    /* Unacceptable fragment size. */
    return -1;
  }

  for(i = 0; i < SICSLOWPAN_FRAGMENT_BUFFERS; i++) {
    if(frag_buf[i].len == 0) {
      /* copy over the data from packetbuf into the fragment buffer,
         and store offset and len */
      frag_buf[i].offset = offset; /* frag offset */
      frag_buf[i].len = len;
      frag_buf[i].index = index;
      memcpy(frag_buf[i].data, packetbuf_ptr + packetbuf_hdr_len, len);
      /* return the length of the stored fragment */
      return len;
    }
  }
  /* failed */
  return -1;
}
/*---------------------------------------------------------------------------*/
/* add a new fragment to the buffer */
static int8_t
add_fragment(uint16_t tag, uint16_t frag_size, uint8_t offset)
{
  int i;
  int len;
  int8_t found = -1;

  if(offset == 0) {
    /* This is a first fragment - check if we can add this */
    for(i = 0; i < SICSLOWPAN_REASS_CONTEXTS; i++) {
      /* clear all fragment info with expired timer to free all fragment buffers */
      if(frag_info[i].len > 0 && timer_expired(&frag_info[i].reass_timer)) {
        clear_fragments(i);
      }

      /* We use len as indication on used or not used */
      if(found < 0 && frag_info[i].len == 0) {
        /* We remember the first free fragment info but must continue
           the loop to free any other expired fragment buffers. */
        found = i;
      }
    }

    if(found < 0) {
      LOG_WARN("reassembly: failed to store new fragment session - tag: %d\n", tag);
      return -1;
    }

    /* Found a free fragment info to store data in */
    frag_info[found].len = frag_size;
    frag_info[found].tag = tag;
    linkaddr_copy(&frag_info[found].sender,
                  packetbuf_addr(PACKETBUF_ADDR_SENDER));
    timer_set(&frag_info[found].reass_timer, SICSLOWPAN_REASS_MAXAGE * CLOCK_SECOND / 16);
    /* first fragment can not be stored immediately but is moved into
       the buffer while uncompressing */
    return found;
  }

  /* This is a N-fragment - should find the info */
  for(i = 0; i < SICSLOWPAN_REASS_CONTEXTS; i++) {
    if(frag_info[i].tag == tag && frag_info[i].len > 0 &&
       linkaddr_cmp(&frag_info[i].sender, packetbuf_addr(PACKETBUF_ADDR_SENDER))) {
      /* Tag and Sender match - this must be the correct info to store in */
      found = i;
      break;
    }
  }

  if(found < 0) {
    /* no entry found for storing the new fragment */
    LOG_WARN("reassembly: failed to store N-fragment - could not find session - tag: %d offset: %d\n", tag, offset);
    return -1;
  }

  /* i is the index of the reassembly context */
  len = store_fragment(i, offset);
  if(len < 0 && timeout_fragments(i) > 0) {
    len = store_fragment(i, offset);
  }
  if(len > 0) {
    frag_info[i].reassembled_len += len;
    return i;
  } else {
    /* should we also clear all fragments since we failed to store
       this fragment? */
    LOG_WARN("reassembly: failed to store fragment - packet reassembly will fail tag:%d l\n", frag_info[i].tag);
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
/* Copy all the fragments that are associated with a specific context
   into uip */
static bool
copy_frags2uip(int context)
{
  int i;

  /* Check length fields before proceeding. */
  if(frag_info[context].len < frag_info[context].first_frag_len ||
     frag_info[context].len > sizeof(uip_buf)) {
    LOG_WARN("input: invalid total size of fragments\n");
    clear_fragments(context);
    return false;
  }

  /* Copy from the fragment context info buffer first */
  memcpy((uint8_t *)UIP_IP_BUF, (uint8_t *)frag_info[context].first_frag,
         frag_info[context].first_frag_len);

  /* Ensure that no previous data is used for reassembly in case of missing fragments. */
  memset((uint8_t *)UIP_IP_BUF + frag_info[context].first_frag_len, 0,
         frag_info[context].len - frag_info[context].first_frag_len);

  for(i = 0; i < SICSLOWPAN_FRAGMENT_BUFFERS; i++) {
    /* And also copy all matching fragments */
    if(frag_buf[i].len > 0 && frag_buf[i].index == context) {
      if(((size_t)frag_buf[i].offset << 3) + frag_buf[i].len > sizeof(uip_buf)) {
        LOG_WARN("input: invalid fragment offset\n");
        clear_fragments(context);
        return false;
      }
      memcpy((uint8_t *)UIP_IP_BUF + (uint16_t)(frag_buf[i].offset << 3),
             (uint8_t *)frag_buf[i].data, frag_buf[i].len);
    }
  }
  /* deallocate all the fragments for this context */
  clear_fragments(context);

  return true;
}
#endif /* SICSLOWPAN_CONF_FRAG */

/* -------------------------------------------------------------------------- */

/*-------------------------------------------------------------------------*/
/* Basic netstack sniffer */
/*-------------------------------------------------------------------------*/
static struct netstack_sniffer *callback = NULL;

void
netstack_sniffer_add(struct netstack_sniffer *s)
{
  callback = s;
}

void
netstack_sniffer_remove(struct netstack_sniffer *s)
{
  callback = NULL;
}

static void
set_packet_attrs(void)
{
  int c = 0;
  /* set protocol in NETWORK_ID */
  packetbuf_set_attr(PACKETBUF_ATTR_NETWORK_ID, UIP_IP_BUF->proto);

  /* assign values to the channel attribute (port or type + code) */
  if(UIP_IP_BUF->proto == UIP_PROTO_UDP) {
    c = UIP_UDP_BUF_POS(0)->srcport;
    if(UIP_UDP_BUF_POS(0)->destport < c) {
      c = UIP_UDP_BUF_POS(0)->destport;
    }
  } else if(UIP_IP_BUF->proto == UIP_PROTO_TCP) {
    c = UIP_TCP_BUF->srcport;
    if(UIP_TCP_BUF->destport < c) {
      c = UIP_TCP_BUF->destport;
    }
  } else if(UIP_IP_BUF->proto == UIP_PROTO_ICMP6) {
    c = UIP_ICMP_BUF->type << 8 | UIP_ICMP_BUF->icode;
  }

  packetbuf_set_attr(PACKETBUF_ATTR_CHANNEL, c);

/*   if(uip_ds6_is_my_addr(&UIP_IP_BUF->srcipaddr)) { */
/*     own = 1; */
/*   } */

}



#if SICSLOWPAN_COMPRESSION >= SICSLOWPAN_COMPRESSION_IPHC
/** \name variables specific to RFC 6282
 *  @{
 */

/** Addresses contexts for IPHC. */
#if SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0
static struct sicslowpan_addr_context
addr_contexts[SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS];
#endif

/** pointer to the byte where to write next inline field. */
static uint8_t *iphc_ptr;

/* Uncompression of linklocal */
/*   0 -> 16 bytes from packet  */
/*   1 -> 2 bytes from prefix - bunch of zeroes and 8 from packet */
/*   2 -> 2 bytes from prefix - 0000::00ff:fe00:XXXX from packet */
/*   3 -> 2 bytes from prefix - infer 8 bytes from lladdr */
/*   NOTE: => the uncompress function does change 0xf to 0x10 */
/*   NOTE: 0x00 => no-autoconfig => unspecified */
const uint8_t unc_llconf[] = {0x0f,0x28,0x22,0x20};

/* Uncompression of ctx-based */
/*   0 -> 0 bits from packet [unspecified / reserved] */
/*   1 -> 8 bytes from prefix - bunch of zeroes and 8 from packet */
/*   2 -> 8 bytes from prefix - 0000::00ff:fe00:XXXX + 2 from packet */
/*   3 -> 8 bytes from prefix - infer 8 bytes from lladdr */
const uint8_t unc_ctxconf[] = {0x00,0x88,0x82,0x80};

/* Uncompression of ctx-based */
/*   0 -> 0 bits from packet  */
/*   1 -> 2 bytes from prefix - bunch of zeroes 5 from packet */
/*   2 -> 2 bytes from prefix - zeroes + 3 from packet */
/*   3 -> 2 bytes from prefix - infer 1 bytes from lladdr */
const uint8_t unc_mxconf[] = {0x0f, 0x25, 0x23, 0x21};

/* Link local prefix */
const uint8_t llprefix[] = {0xfe, 0x80};

/* TTL uncompression values */
static const uint8_t ttl_values[] = {0, 1, 64, 255};

/** @} */
/*--------------------------------------------------------------------*/
/** \name IPHC related functions
 * @{                                                                 */
/*--------------------------------------------------------------------*/
/** \brief find the context corresponding to prefix ipaddr */
static struct sicslowpan_addr_context*
addr_context_lookup_by_prefix(uip_ipaddr_t *ipaddr)
{
/* Remove code to avoid warnings and save flash if no context is used */
#if SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0
  int i;
  for(i = 0; i < SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS; i++) {
    if((addr_contexts[i].used == 1) &&
       uip_ipaddr_prefixcmp(&addr_contexts[i].prefix, ipaddr, 64)) {
      return &addr_contexts[i];
    }
  }
#endif /* SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0 */
  return NULL;
}
/*--------------------------------------------------------------------*/
/** \brief find the context with the given number */
static struct sicslowpan_addr_context*
addr_context_lookup_by_number(uint8_t number)
{
/* Remove code to avoid warnings and save flash if no context is used */
#if SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0
  int i;
  for(i = 0; i < SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS; i++) {
    if((addr_contexts[i].used == 1) &&
       addr_contexts[i].number == number) {
      return &addr_contexts[i];
    }
  }
#endif /* SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0 */
  return NULL;
}
/*--------------------------------------------------------------------*/
static uint8_t
compress_addr_64(uint8_t bitpos, uip_ipaddr_t *ipaddr,
    const uip_lladdr_t *lladdr)
{
  if(uip_is_addr_mac_addr_based(ipaddr, lladdr)) {
    return 3 << bitpos; /* 0-bits */
  } else if(sicslowpan_is_iid_16_bit_compressable(ipaddr)) {
    /* compress IID to 16 bits xxxx::0000:00ff:fe00:XXXX */
    memcpy(iphc_ptr, &ipaddr->u16[7], 2);
    iphc_ptr += 2;
    return 2 << bitpos; /* 16-bits */
  } else {
    /* do not compress IID => xxxx::IID */
    memcpy(iphc_ptr, &ipaddr->u16[4], 8);
    iphc_ptr += 8;
    return 1 << bitpos; /* 64-bits */
  }
}

/*-------------------------------------------------------------------- */
/* Uncompress addresses based on a prefix and a postfix with zeroes in
 * between. If the postfix is zero in length it will use the link address
 * to configure the IP address (autoconf style).
 * pref_post_count takes a byte where the first nibble specify prefix count
 * and the second postfix count (NOTE: 15/0xf => 16 bytes copy).
 */
static bool
uncompress_addr(uip_ipaddr_t *ipaddr, uint8_t const prefix[],
                uint8_t pref_post_count, uip_lladdr_t *lladdr)
{
  uint8_t prefcount = pref_post_count >> 4;
  uint8_t postcount = pref_post_count & 0x0f;
  /* full nibble 15 => 16 */
  prefcount = prefcount == 15 ? 16 : prefcount;
  postcount = postcount == 15 ? 16 : postcount;

  LOG_DBG("uncompression: address %d %d ", prefcount, postcount);

  if(prefix != NULL) {
    memcpy(ipaddr, prefix, prefcount);
  }
  if(prefcount + postcount < 16) {
    memset(&ipaddr->u8[prefcount], 0, 16 - (prefcount + postcount));
  }
  if(postcount > 0) {
    if((iphc_ptr - packetbuf_ptr) + postcount > packetbuf_datalen()) {
      LOG_WARN("Insufficient packet data to decompress IP address\n");
      return false;
    }

    memcpy(&ipaddr->u8[16 - postcount], iphc_ptr, postcount);
    if(postcount == 2 && prefcount < 11) {
      /* 16 bits uncompression => 0000:00ff:fe00:XXXX */
      ipaddr->u8[11] = 0xff;
      ipaddr->u8[12] = 0xfe;
    }
    iphc_ptr += postcount;
  } else if (prefcount > 0) {
    /* no IID based configuration if no prefix and no data => unspec */
    uip_ds6_set_addr_iid(ipaddr, lladdr);
  }

  LOG_DBG_6ADDR(ipaddr);
  LOG_DBG_("\n");
  return true;
}

/*--------------------------------------------------------------------*/
/**
 * \brief Compress IP/UDP header
 *
 * This function is called by the 6lowpan code to create a compressed
 * 6lowpan packet in the packetbuf buffer from a full IPv6 packet in the
 * uip_buf buffer.
 *
 *
 * IPHC (RFC 6282)\n
 * http://tools.ietf.org/html/
 *
 * \note We do not support ISA100_UDP header compression
 *
 * For LOWPAN_UDP compression, we either compress both ports or none.
 * General format with LOWPAN_UDP compression is
 * \verbatim
 *                      1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |0|1|1|TF |N|HLI|C|S|SAM|M|D|DAM| SCI   | DCI   | comp. IPv6 hdr|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | compressed IPv6 fields .....                                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | LOWPAN_UDP    | non compressed UDP fields ...                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | L4 data ...                                                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 * \note The context number 00 is reserved for the link local prefix.
 * For unicast addresses, if we cannot compress the prefix, we neither
 * compress the IID.
 * \return 1 if success, else 0
 */
static int
compress_hdr_iphc(void)
{
  uint8_t tmp, iphc0, iphc1, *next_hdr, *next_nhc;
  int ext_hdr_len;
  struct uip_udp_hdr *udp_buf;

  if(LOG_DBG_ENABLED) {
    uint16_t ndx;
    LOG_DBG("compression: before (%d): ", UIP_IP_BUF->len[1]);
    for(ndx = 0; ndx < UIP_IP_BUF->len[1] + 40; ndx++) {
      uint8_t data = ((uint8_t *) (UIP_IP_BUF))[ndx];
      LOG_DBG_("%02x", data);
    }
    LOG_DBG_("\n");
  }

/* Macro used only internally, during header compression. Checks if there
 * is sufficient space in packetbuf before writing any further. */
#define CHECK_BUFFER_SPACE(writelen) do { \
  if(iphc_ptr + (writelen) >= PACKETBUF_PAYLOAD_END) { \
    LOG_WARN("Not enough packetbuf space to compress header (%u bytes, %u left). Aborting.\n", \
                (unsigned)(writelen), (unsigned)(PACKETBUF_PAYLOAD_END - iphc_ptr)); \
    return 0; \
  } \
} while(0);

  iphc_ptr = PACKETBUF_IPHC_BUF + 2;

  /* Check if there is enough space for the compressed IPv6 header, in the
   * worst case (least compressed case). Extension headers and transport
   * layer will be checked when they are compressed. */
  CHECK_BUFFER_SPACE(38);

  /*
   * As we copy some bit-length fields, in the IPHC encoding bytes,
   * we sometimes use |=
   * If the field is 0, and the current bit value in memory is 1,
   * this does not work. We therefore reset the IPHC encoding here
   */

  iphc0 = SICSLOWPAN_DISPATCH_IPHC;
  iphc1 = 0;
  PACKETBUF_IPHC_BUF[2] = 0; /* might not be used - but needs to be cleared */

  /*
   * Address handling needs to be made first since it might
   * cause an extra byte with [ SCI | DCI ]
   *
   */


  /* check if dest context exists (for allocating third byte) */
  struct sicslowpan_addr_context *source_context =
      addr_context_lookup_by_prefix(&UIP_IP_BUF->srcipaddr);
  struct sicslowpan_addr_context *destination_context =
      addr_context_lookup_by_prefix(&UIP_IP_BUF->destipaddr);
  if(source_context || destination_context) {
    /* set context flag and increase iphc_ptr */
    LOG_DBG("compression: dest or src ipaddr - setting CID\n");
    iphc1 |= SICSLOWPAN_IPHC_CID;
    iphc_ptr++;
  }

  /*
   * Traffic class, flow label
   * If flow label is 0, compress it. If traffic class is 0, compress it
   * We have to process both in the same time as the offset of traffic class
   * depends on the presence of version and flow label
   */

  /* IPHC format of tc is ECN | DSCP , original is DSCP | ECN */

  tmp = (UIP_IP_BUF->vtc << 4) | (UIP_IP_BUF->tcflow >> 4);
  tmp = ((tmp & 0x03) << 6) | (tmp >> 2);

  if(((UIP_IP_BUF->tcflow & 0x0F) == 0) &&
     (UIP_IP_BUF->flow == 0)) {
    /* flow label can be compressed */
    iphc0 |= SICSLOWPAN_IPHC_FL_C;
    if(((UIP_IP_BUF->vtc & 0x0F) == 0) &&
       ((UIP_IP_BUF->tcflow & 0xF0) == 0)) {
      /* compress (elide) all */
      iphc0 |= SICSLOWPAN_IPHC_TC_C;
    } else {
      /* compress only the flow label */
     *iphc_ptr = tmp;
      iphc_ptr += 1;
    }
  } else {
    /* Flow label cannot be compressed */
    if(((UIP_IP_BUF->vtc & 0x0F) == 0) &&
       ((UIP_IP_BUF->tcflow & 0xF0) == 0)) {
      /* compress only traffic class */
      iphc0 |= SICSLOWPAN_IPHC_TC_C;
      *iphc_ptr = (tmp & 0xc0) |
        (UIP_IP_BUF->tcflow & 0x0F);
      memcpy(iphc_ptr + 1, &UIP_IP_BUF->flow, 2);
      iphc_ptr += 3;
    } else {
      /* compress nothing */
      memcpy(iphc_ptr, &UIP_IP_BUF->vtc, 4);
      /* but replace the top byte with the new ECN | DSCP format*/
      *iphc_ptr = tmp;
      iphc_ptr += 4;
   }
  }

  /* Note that the payload length is always compressed */

  /* Next header. We compress it is compressable. */
  if(IS_COMPRESSABLE_PROTO(UIP_IP_BUF->proto)) {
    iphc0 |= SICSLOWPAN_IPHC_NH_C;
  }

  /* Add proto header unless it is compressed */
  if((iphc0 & SICSLOWPAN_IPHC_NH_C) == 0) {
    *iphc_ptr = UIP_IP_BUF->proto;
    iphc_ptr += 1;
  }

  /*
   * Hop limit
   * if 1: compress, encoding is 01
   * if 64: compress, encoding is 10
   * if 255: compress, encoding is 11
   * else do not compress
   */
  switch(UIP_IP_BUF->ttl) {
    case 1:
      iphc0 |= SICSLOWPAN_IPHC_TTL_1;
      break;
    case 64:
      iphc0 |= SICSLOWPAN_IPHC_TTL_64;
      break;
    case 255:
      iphc0 |= SICSLOWPAN_IPHC_TTL_255;
      break;
    default:
      *iphc_ptr = UIP_IP_BUF->ttl;
      iphc_ptr += 1;
      break;
  }

  /* source address - cannot be multicast */
  if(uip_is_addr_unspecified(&UIP_IP_BUF->srcipaddr)) {
    LOG_DBG("compression: addr unspecified - setting SAC\n");
    iphc1 |= SICSLOWPAN_IPHC_SAC;
    iphc1 |= SICSLOWPAN_IPHC_SAM_00;
  } else if(source_context) {
    /* elide the prefix - indicate by CID and set context + SAC */
    LOG_DBG("compression: src with context - setting CID & SAC ctx: %d\n",
           source_context->number);
    iphc1 |= SICSLOWPAN_IPHC_CID | SICSLOWPAN_IPHC_SAC;
    PACKETBUF_IPHC_BUF[2] |= source_context->number << 4;
    /* compession compare with this nodes address (source) */

    iphc1 |= compress_addr_64(SICSLOWPAN_IPHC_SAM_BIT,
                              &UIP_IP_BUF->srcipaddr, &uip_lladdr);
    /* No context found for this address */
  } else if(uip_is_addr_linklocal(&UIP_IP_BUF->srcipaddr) &&
            UIP_IP_BUF->destipaddr.u16[1] == 0 &&
            UIP_IP_BUF->destipaddr.u16[2] == 0 &&
            UIP_IP_BUF->destipaddr.u16[3] == 0) {
    iphc1 |= compress_addr_64(SICSLOWPAN_IPHC_SAM_BIT,
                              &UIP_IP_BUF->srcipaddr, &uip_lladdr);
  } else {
    /* send the full address => SAC = 0, SAM = 00 */
    iphc1 |= SICSLOWPAN_IPHC_SAM_00; /* 128-bits */
    memcpy(iphc_ptr, &UIP_IP_BUF->srcipaddr.u16[0], 16);
    iphc_ptr += 16;
  }

  /* dest address*/
  if(uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
    /* Address is multicast, try to compress */
    iphc1 |= SICSLOWPAN_IPHC_M;
    if(sicslowpan_is_mcast_addr_compressable8(&UIP_IP_BUF->destipaddr)) {
      iphc1 |= SICSLOWPAN_IPHC_DAM_11;
      /* use last byte */
      *iphc_ptr = UIP_IP_BUF->destipaddr.u8[15];
      iphc_ptr += 1;
    } else if(sicslowpan_is_mcast_addr_compressable32(&UIP_IP_BUF->destipaddr)) {
      iphc1 |= SICSLOWPAN_IPHC_DAM_10;
      /* second byte + the last three */
      *iphc_ptr = UIP_IP_BUF->destipaddr.u8[1];
      memcpy(iphc_ptr + 1, &UIP_IP_BUF->destipaddr.u8[13], 3);
      iphc_ptr += 4;
    } else if(sicslowpan_is_mcast_addr_compressable48(&UIP_IP_BUF->destipaddr)) {
      iphc1 |= SICSLOWPAN_IPHC_DAM_01;
      /* second byte + the last five */
      *iphc_ptr = UIP_IP_BUF->destipaddr.u8[1];
      memcpy(iphc_ptr + 1, &UIP_IP_BUF->destipaddr.u8[11], 5);
      iphc_ptr += 6;
    } else {
      iphc1 |= SICSLOWPAN_IPHC_DAM_00;
      /* full address */
      memcpy(iphc_ptr, &UIP_IP_BUF->destipaddr.u8[0], 16);
      iphc_ptr += 16;
    }
  } else {
    /* Address is unicast, try to compress */
    if(destination_context) {
      /* elide the prefix */
      iphc1 |= SICSLOWPAN_IPHC_DAC;
      PACKETBUF_IPHC_BUF[2] |= destination_context->number;
      /* compession compare with link adress (destination) */

      iphc1 |= compress_addr_64(SICSLOWPAN_IPHC_DAM_BIT,
          &UIP_IP_BUF->destipaddr,
          (const uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
      /* No context found for this address */
    } else if(uip_is_addr_linklocal(&UIP_IP_BUF->destipaddr) &&
              UIP_IP_BUF->destipaddr.u16[1] == 0 &&
              UIP_IP_BUF->destipaddr.u16[2] == 0 &&
              UIP_IP_BUF->destipaddr.u16[3] == 0) {
      iphc1 |= compress_addr_64(SICSLOWPAN_IPHC_DAM_BIT,
          &UIP_IP_BUF->destipaddr,
          (const uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
    } else {
      /* send the full address */
      iphc1 |= SICSLOWPAN_IPHC_DAM_00; /* 128-bits */
      memcpy(iphc_ptr, &UIP_IP_BUF->destipaddr.u16[0], 16);
      iphc_ptr += 16;
    }
  }

  /* Start of ext hdr compression or UDP compression */
  /* pick out the next-header position */
  next_hdr = &UIP_IP_BUF->proto;
  next_nhc = iphc_ptr; /* here we set the next header is compressed. */
  ext_hdr_len = 0;
  /* reserve the write place of this next header position */
  LOG_DBG("compression: first header: %d\n", *next_hdr);
  while(next_hdr != NULL && IS_COMPRESSABLE_PROTO(*next_hdr)) {
    LOG_DBG("compression: next header: %d\n", *next_hdr);
    int proto = -1; /* used for the specific ext hdr */
    /* UDP and EXT header compression */
    switch(*next_hdr) {
    case UIP_PROTO_HBHO:
      proto = SICSLOWPAN_NHC_ETX_HDR_HBHO;
    case UIP_PROTO_ROUTING:
      proto = proto == -1 ? SICSLOWPAN_NHC_ETX_HDR_ROUTING : proto;
    case UIP_PROTO_FRAG:
      proto = proto == -1 ? SICSLOWPAN_NHC_ETX_HDR_FRAG : proto;
    case UIP_PROTO_DESTO:
      /* Handle the header here! */
      {
        struct uip_ext_hdr *ext_hdr =
          (struct uip_ext_hdr *) UIP_IPPAYLOAD_BUF_POS(ext_hdr_len);
        int len;
        proto = proto == -1 ? SICSLOWPAN_NHC_ETX_HDR_DESTO : proto;
        /* Len is defined to be in octets from the length byte */
        len = (ext_hdr->len << 3) + 8;
        LOG_DBG("compression: next header %d (len:%d)\n", *next_hdr, len);
        /* pick up the next header */
        next_hdr = &ext_hdr->next;
        /* If the next header is not compressable we need to reserve the
           NHC byte extra - before the next header here. This is due to
           next not being elided in that case. */
        if(!IS_COMPRESSABLE_PROTO(*next_hdr)) {
          CHECK_BUFFER_SPACE(1);
          iphc_ptr++;
          LOG_DBG("compression: keeping the next header in this ext hdr: %d\n",
                 ext_hdr->next);
        }
        /* copy the ext-hdr into the hc06 buffer */
        CHECK_BUFFER_SPACE(len);
        memcpy(iphc_ptr, ext_hdr, len);
        /* modify the len to octets */
        ext_hdr = (struct uip_ext_hdr *) iphc_ptr;
        ext_hdr->len = len - 2; /* Len should be in bytes from len byte*/
        ext_hdr_len += len;
        iphc_ptr += len;
        uncomp_hdr_len += len;

        /* Write this next header - with its NHC header - including flag
           to tell if next header is elided in this one also- */
        *next_nhc = SICSLOWPAN_NHC_EXT_HDR |
          (IS_COMPRESSABLE_PROTO(*next_hdr) ? SICSLOWPAN_NHC_BIT : 0) |
          (proto << 1);
        /* update the position of the next header */
        next_nhc = iphc_ptr;
      }
      break;
    case UIP_PROTO_UDP:
      /* allocate a byte for the next header posision as UDP has no next */
      iphc_ptr++;
      udp_buf = UIP_UDP_BUF_POS(ext_hdr_len);
      LOG_DBG("compression: inlined UDP ports on send side: %x, %x\n",
             UIP_HTONS(udp_buf->srcport), UIP_HTONS(udp_buf->destport));
      /* Mask out the last 4 bits can be used as a mask */
      if(((UIP_HTONS(udp_buf->srcport) & 0xfff0) == SICSLOWPAN_UDP_4_BIT_PORT_MIN) &&
         ((UIP_HTONS(udp_buf->destport) & 0xfff0) == SICSLOWPAN_UDP_4_BIT_PORT_MIN)) {
        /* we can compress 12 bits of both source and dest */
        *next_nhc = SICSLOWPAN_NHC_UDP_CS_P_11;
        LOG_DBG("IPHC: remove 12 b of both source & dest with prefix 0xFOB\n");
        CHECK_BUFFER_SPACE(1);
        *iphc_ptr =
          (uint8_t)((UIP_HTONS(udp_buf->srcport) -
                     SICSLOWPAN_UDP_4_BIT_PORT_MIN) << 4) +
          (uint8_t)((UIP_HTONS(udp_buf->destport) -
                     SICSLOWPAN_UDP_4_BIT_PORT_MIN));
        iphc_ptr += 1;
      } else if((UIP_HTONS(udp_buf->destport) & 0xff00) == SICSLOWPAN_UDP_8_BIT_PORT_MIN) {
        /* we can compress 8 bits of dest, leave source. */
        *next_nhc = SICSLOWPAN_NHC_UDP_CS_P_01;
        LOG_DBG("IPHC: leave source, remove 8 bits of dest with prefix 0xF0\n");
        CHECK_BUFFER_SPACE(3);
        memcpy(iphc_ptr, &udp_buf->srcport, 2);
        *(iphc_ptr + 2) =
          (uint8_t)((UIP_HTONS(udp_buf->destport) -
                     SICSLOWPAN_UDP_8_BIT_PORT_MIN));
        iphc_ptr += 3;
      } else if((UIP_HTONS(udp_buf->srcport) & 0xff00) == SICSLOWPAN_UDP_8_BIT_PORT_MIN) {
        /* we can compress 8 bits of src, leave dest. Copy compressed port */
        *next_nhc = SICSLOWPAN_NHC_UDP_CS_P_10;
        LOG_DBG("IPHC: remove 8 bits of source with prefix 0xF0, leave dest. hch: %i\n", *next_nhc);
        CHECK_BUFFER_SPACE(3);
        *iphc_ptr =
          (uint8_t)((UIP_HTONS(udp_buf->srcport) -
                     SICSLOWPAN_UDP_8_BIT_PORT_MIN));
        memcpy(iphc_ptr + 1, &udp_buf->destport, 2);
        iphc_ptr += 3;
      } else {
        /* we cannot compress. Copy uncompressed ports, full checksum  */
        *next_nhc = SICSLOWPAN_NHC_UDP_CS_P_00;
        LOG_DBG("IPHC: cannot compress UDP headers\n");
        CHECK_BUFFER_SPACE(4);
        memcpy(iphc_ptr, &udp_buf->srcport, 4);
        iphc_ptr += 4;
      }
      /* always inline the checksum  */
      CHECK_BUFFER_SPACE(2);
      memcpy(iphc_ptr, &udp_buf->udpchksum, 2);
      iphc_ptr += 2;
      uncomp_hdr_len += UIP_UDPH_LEN;
      /* this is the final header. */
      next_hdr = NULL;
      break;
    default:
      LOG_ERR("compression: could not handle compression of header");
    }
  }
  if(next_hdr != NULL) {
    /* Last header could not be compressed - we assume that this is then OK!*/
    /* as the last EXT_HDR should be "uncompressed" and have the next there */
    LOG_DBG("compression: last header could is not compressed: %d\n", *next_hdr);
  }
  /* before the packetbuf_hdr_len operation */
  PACKETBUF_IPHC_BUF[0] = iphc0;
  PACKETBUF_IPHC_BUF[1] = iphc1;

  if(LOG_DBG_ENABLED) {
    uint16_t ndx;
    LOG_DBG("compression: after (%d): ", (int)(iphc_ptr - packetbuf_ptr));
    for(ndx = 0; ndx < iphc_ptr - packetbuf_ptr; ndx++) {
      uint8_t data = ((uint8_t *) packetbuf_ptr)[ndx];
      LOG_DBG_("%02x", data);
    }
    LOG_DBG_("\n");
  }

  packetbuf_hdr_len = iphc_ptr - packetbuf_ptr;

  return 1;
}

/*--------------------------------------------------------------------*/
/**
 * \brief Uncompress IPHC (i.e., IPHC and LOWPAN_UDP) headers and put
 * them in sicslowpan_buf
 *
 * This function is called by the input function when the dispatch is
 * IPHC.
 * We %process the packet in the packetbuf buffer, uncompress the header
 * fields, and copy the result in the sicslowpan buffer.
 * At the end of the decompression, packetbuf_hdr_len and uncompressed_hdr_len
 * are set to the appropriate values
 *
 * \param buf Pointer to the buffer to uncompress the packet into.
 * \param buf_size The size of the buffer to uncompress the packet into.
 * \param ip_len Equal to 0 if the packet is not a fragment (IP length
 * is then inferred from the L2 length), non 0 if the packet is a 1st
 * fragment.
 * \return A boolean value indicating whether the uncompression succeeded.
 */
static bool
uncompress_hdr_iphc(uint8_t *buf, uint16_t buf_size, uint16_t ip_len)
{
  uint8_t tmp, iphc0, iphc1, nhc;
  struct uip_ext_hdr *exthdr;
  uint8_t* last_nextheader;
  uint8_t* ip_payload;
  uint8_t ext_hdr_len = 0;
  uint16_t cmpr_len;

/* Macro used only internally, during header uncompression. Checks if there
 * is sufficient space in packetbuf before reading any further. */
#define CHECK_READ_SPACE(readlen) \
  if((iphc_ptr - packetbuf_ptr) + (readlen) > cmpr_len) { \
    LOG_WARN("Not enough packetbuf space to decompress header (%u bytes, %u left). Aborting.\n", \
             (unsigned)(readlen), (unsigned)(cmpr_len - (iphc_ptr - packetbuf_ptr))); \
    return false; \
  }

  /* at least two byte will be used for the encoding */
  cmpr_len = packetbuf_datalen();
  if(cmpr_len < packetbuf_hdr_len + 2) {
    return false;
  }
  iphc_ptr = packetbuf_ptr + packetbuf_hdr_len + 2;

  iphc0 = PACKETBUF_IPHC_BUF[0];
  iphc1 = PACKETBUF_IPHC_BUF[1];

  /* another if the CID flag is set */
  if(iphc1 & SICSLOWPAN_IPHC_CID) {
    LOG_DBG("uncompression: CID flag set - increase header with one\n");
    iphc_ptr++;
  }

  /* Traffic class and flow label */
    if((iphc0 & SICSLOWPAN_IPHC_FL_C) == 0) {
      /* Flow label are carried inline */
      if((iphc0 & SICSLOWPAN_IPHC_TC_C) == 0) {
        /* Traffic class is carried inline */
        CHECK_READ_SPACE(4);
        memcpy(&SICSLOWPAN_IP_BUF(buf)->tcflow, iphc_ptr + 1, 3);
        tmp = *iphc_ptr;
        iphc_ptr += 4;
        /* IPHC format of tc is ECN | DSCP , original is DSCP | ECN */
        /* set version, pick highest DSCP bits and set in vtc */
        SICSLOWPAN_IP_BUF(buf)->vtc = 0x60 | ((tmp >> 2) & 0x0f);
        /* ECN rolled down two steps + lowest DSCP bits at top two bits */
        SICSLOWPAN_IP_BUF(buf)->tcflow = ((tmp >> 2) & 0x30) | (tmp << 6) |
          (SICSLOWPAN_IP_BUF(buf)->tcflow & 0x0f);
      } else {
        /* Traffic class is compressed (set version and no TC)*/
        SICSLOWPAN_IP_BUF(buf)->vtc = 0x60;
        /* highest flow label bits + ECN bits */
        CHECK_READ_SPACE(3);
        SICSLOWPAN_IP_BUF(buf)->tcflow = (*iphc_ptr & 0x0F) | 
          ((*iphc_ptr >> 2) & 0x30);
        memcpy(&SICSLOWPAN_IP_BUF(buf)->flow, iphc_ptr + 1, 2);
        iphc_ptr += 3;
      }
    } else {
      /* Version is always 6! */
      /* Version and flow label are compressed */
      if((iphc0 & SICSLOWPAN_IPHC_TC_C) == 0) {
        /* Traffic class is inline */
        CHECK_READ_SPACE(1);
        SICSLOWPAN_IP_BUF(buf)->vtc = 0x60 | ((*iphc_ptr >> 2) & 0x0f);
        SICSLOWPAN_IP_BUF(buf)->tcflow = ((*iphc_ptr << 6) & 0xC0) | ((*iphc_ptr >> 2) & 0x30);
        SICSLOWPAN_IP_BUF(buf)->flow = 0;
        iphc_ptr += 1;
      } else {
        /* Traffic class is compressed */
        SICSLOWPAN_IP_BUF(buf)->vtc = 0x60;
        SICSLOWPAN_IP_BUF(buf)->tcflow = 0;
        SICSLOWPAN_IP_BUF(buf)->flow = 0;
      }
    }

  /* Next Header */
  if((iphc0 & SICSLOWPAN_IPHC_NH_C) == 0) {
    /* Next header is carried inline */
    CHECK_READ_SPACE(1);
    SICSLOWPAN_IP_BUF(buf)->proto = *iphc_ptr;
    LOG_DBG("uncompression: next header inline: %d\n", SICSLOWPAN_IP_BUF(buf)->proto);
    iphc_ptr += 1;
  }

  /* Hop limit */
  if((iphc0 & 0x03) != SICSLOWPAN_IPHC_TTL_I) {
    SICSLOWPAN_IP_BUF(buf)->ttl = ttl_values[iphc0 & 0x03];
  } else {
    CHECK_READ_SPACE(1);
    SICSLOWPAN_IP_BUF(buf)->ttl = *iphc_ptr;
    iphc_ptr += 1;
  }

  /* put the source address compression mode SAM in the tmp var */
  tmp = ((iphc1 & SICSLOWPAN_IPHC_SAM_11) >> SICSLOWPAN_IPHC_SAM_BIT) & 0x03;

  /* context based compression */
  if(iphc1 & SICSLOWPAN_IPHC_SAC) {
    uint8_t sci = (iphc1 & SICSLOWPAN_IPHC_CID) ?
      PACKETBUF_IPHC_BUF[2] >> 4 : 0;

    /* Source address - check context != NULL only if SAM bits are != 0*/
    struct sicslowpan_addr_context *source_context;
    if (tmp != 0) {
      source_context = addr_context_lookup_by_number(sci);
      if(!source_context) {
        LOG_ERR("uncompression: error source context not found\n");
        return false;
      }
    } else {
      source_context = NULL;
    }
    /* if tmp == 0 we do not have a context and therefore no prefix */
    if(!uncompress_addr(&SICSLOWPAN_IP_BUF(buf)->srcipaddr,
                        source_context ? source_context->prefix : NULL,
                        unc_ctxconf[tmp],
                        (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER))) {
      return false;
    }
  } else {
    /* no compression and link local */
    if(!uncompress_addr(&SICSLOWPAN_IP_BUF(buf)->srcipaddr, llprefix,
                        unc_llconf[tmp],
                        (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER))) {
      return false;
    }
  }

  /* Destination address */
  /* put the destination address compression mode into tmp */
  tmp = ((iphc1 & SICSLOWPAN_IPHC_DAM_11) >> SICSLOWPAN_IPHC_DAM_BIT) & 0x03;

  /* multicast compression */
  if(iphc1 & SICSLOWPAN_IPHC_M) {
    /* context based multicast compression */
    if(iphc1 & SICSLOWPAN_IPHC_DAC) {
      /* TODO: implement this */
    } else {
      /* non-context based multicast compression - */
      /* DAM_00: 128 bits  */
      /* DAM_01:  48 bits FFXX::00XX:XXXX:XXXX */
      /* DAM_10:  32 bits FFXX::00XX:XXXX */
      /* DAM_11:   8 bits FF02::00XX */
      uint8_t prefix[] = {0xff, 0x02};
      if(tmp > 0 && tmp < 3) {
        CHECK_READ_SPACE(1);
        prefix[1] = *iphc_ptr;
        iphc_ptr++;
      }

      if(!uncompress_addr(&SICSLOWPAN_IP_BUF(buf)->destipaddr, prefix,
                          unc_mxconf[tmp], NULL)) {
        return false;
      }
    }
  } else {
    /* no multicast */
    /* Context based */
    if(iphc1 & SICSLOWPAN_IPHC_DAC) {
      uint8_t dci = (iphc1 & SICSLOWPAN_IPHC_CID) ? PACKETBUF_IPHC_BUF[2] & 0x0f : 0;
      struct sicslowpan_addr_context *destination_context =
          addr_context_lookup_by_number(dci);

      /* all valid cases below need the context! */
      if(!destination_context) {
        LOG_ERR("uncompression: error destination context not found\n");
        return false;
      }
      if(!uncompress_addr(&SICSLOWPAN_IP_BUF(buf)->destipaddr,
                          destination_context->prefix,
                          unc_ctxconf[tmp],
                          (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_RECEIVER))) {
        return false;
      }
    } else {
      /* not context based => link local M = 0, DAC = 0 - same as SAC */
      if(!uncompress_addr(&SICSLOWPAN_IP_BUF(buf)->destipaddr, llprefix,
                          unc_llconf[tmp],
                          (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_RECEIVER))) {
        return false;
      }
    }
  }
  uncomp_hdr_len += UIP_IPH_LEN;

  /* Next header processing - continued */
  nhc = iphc0 & SICSLOWPAN_IPHC_NH_C;
  /* The next header is compressed, NHC is following */
  last_nextheader =  &SICSLOWPAN_IP_BUF(buf)->proto;
  ip_payload = SICSLOWPAN_IPPAYLOAD_BUF(buf);

  CHECK_READ_SPACE(1);
  while(nhc && (*iphc_ptr & SICSLOWPAN_NHC_MASK) == SICSLOWPAN_NHC_EXT_HDR) {
    uint8_t eid = (*iphc_ptr & 0x0e) >> 1;
    /* next header compression flag */
    uint8_t nh = (*iphc_ptr & 0x01);
    uint8_t next = 0;
    uint8_t len;
    uint8_t proto;

    nhc = nh;

    iphc_ptr++;
    CHECK_READ_SPACE(1);
    if(!nh) {
      next = *iphc_ptr;
      iphc_ptr++;
      LOG_DBG("uncompression: next header is inlined. Next: %d\n", next);
    }
    CHECK_READ_SPACE(1);
    len = *iphc_ptr;
    iphc_ptr++;

    LOG_DBG("uncompression: found ext header id: %d next: %d len: %d\n", eid, next, len);
    switch(eid) {
    case SICSLOWPAN_NHC_ETX_HDR_HBHO:
      proto = UIP_PROTO_HBHO;
      break;
    case SICSLOWPAN_NHC_ETX_HDR_ROUTING:
      proto = UIP_PROTO_ROUTING;
      break;
    case SICSLOWPAN_NHC_ETX_HDR_FRAG:
      proto = UIP_PROTO_FRAG;
      break;
    case SICSLOWPAN_NHC_ETX_HDR_DESTO:
      proto = UIP_PROTO_DESTO;
      break;
    default:
      LOG_DBG("uncompression: error unsupported ext header\n");
      return false;
    }
    *last_nextheader = proto;

    /* Check that there is enough room to write the extension header. */
    if((ip_payload - buf) + UIP_EXT_HDR_LEN + len > buf_size) {
      LOG_WARN("uncompression: cannot write ext header beyond target buffer\n");
      return false;
    }

    /* uncompress the extension header */
    exthdr = (struct uip_ext_hdr *)ip_payload;
    exthdr->len = (UIP_EXT_HDR_LEN + len) / 8;
    if(exthdr->len == 0) {
      LOG_WARN("Extension header length is below 8\n");
      return false;
    }
    exthdr->len--;
    exthdr->next = next;
    last_nextheader = &exthdr->next;

    /* The loop condition needs to read one byte after the next len bytes in the buffer. */
    CHECK_READ_SPACE(len + 1);
    memcpy((uint8_t *)exthdr + UIP_EXT_HDR_LEN, iphc_ptr, len);
    iphc_ptr += len;

    uncomp_hdr_len += (exthdr->len + 1) * 8;
    ip_payload += (exthdr->len + 1) * 8;
    ext_hdr_len += (exthdr->len + 1) * 8;

    LOG_DBG("uncompression: %d len: %d exthdr len: %d (calc: %d)\n",
            proto, len, exthdr->len, (exthdr->len + 1) * 8);
  }

  /* The next header is compressed, NHC is following */
  CHECK_READ_SPACE(1);
  if(nhc && (*iphc_ptr & SICSLOWPAN_NHC_UDP_MASK) == SICSLOWPAN_NHC_UDP_ID) {
    struct uip_udp_hdr *udp_buf;
    uint16_t udp_len;
    uint8_t checksum_compressed;

    /* Check that there is enough room to write the UDP header. */
    if((ip_payload - buf) + UIP_UDPH_LEN > buf_size) {
      LOG_WARN("uncompression: cannot write UDP header beyond target buffer\n");
      return false;
    }

    udp_buf = (struct uip_udp_hdr *)ip_payload;
    *last_nextheader = UIP_PROTO_UDP;
    checksum_compressed = *iphc_ptr & SICSLOWPAN_NHC_UDP_CHECKSUMC;
    LOG_DBG("uncompression: incoming header value: %i\n", *iphc_ptr);
    switch(*iphc_ptr & SICSLOWPAN_NHC_UDP_CS_P_11) {
    case SICSLOWPAN_NHC_UDP_CS_P_00:
      /* 1 byte for NHC, 4 byte for ports, 2 bytes chksum */
      CHECK_READ_SPACE(5);
      memcpy(&udp_buf->srcport, iphc_ptr + 1, 2);
      memcpy(&udp_buf->destport, iphc_ptr + 3, 2);
      LOG_DBG("uncompression: UDP ports (ptr+5): %x, %x\n",
             UIP_HTONS(udp_buf->srcport),
             UIP_HTONS(udp_buf->destport));
      iphc_ptr += 5;
      break;

    case SICSLOWPAN_NHC_UDP_CS_P_01:
      /* 1 byte for NHC + source 16bit inline, dest = 0xF0 + 8 bit inline */
      LOG_DBG("uncompression: destination address\n");
      CHECK_READ_SPACE(4);
      memcpy(&udp_buf->srcport, iphc_ptr + 1, 2);
      udp_buf->destport = UIP_HTONS(SICSLOWPAN_UDP_8_BIT_PORT_MIN + (*(iphc_ptr + 3)));
      LOG_DBG("uncompression: UDP ports (ptr+4): %x, %x\n",
             UIP_HTONS(udp_buf->srcport), UIP_HTONS(udp_buf->destport));
      iphc_ptr += 4;
      break;

    case SICSLOWPAN_NHC_UDP_CS_P_10:
      /* 1 byte for NHC + source = 0xF0 + 8bit inline, dest = 16 bit inline*/
      LOG_DBG("uncompression: source address\n");
      CHECK_READ_SPACE(4);
      udp_buf->srcport = UIP_HTONS(SICSLOWPAN_UDP_8_BIT_PORT_MIN +
                                   (*(iphc_ptr + 1)));
      memcpy(&udp_buf->destport, iphc_ptr + 2, 2);
      LOG_DBG("uncompression: UDP ports (ptr+4): %x, %x\n",
             UIP_HTONS(udp_buf->srcport), UIP_HTONS(udp_buf->destport));
      iphc_ptr += 4;
      break;

    case SICSLOWPAN_NHC_UDP_CS_P_11:
      /* 1 byte for NHC, 1 byte for ports */
      CHECK_READ_SPACE(2);
      udp_buf->srcport = UIP_HTONS(SICSLOWPAN_UDP_4_BIT_PORT_MIN +
                                   (*(iphc_ptr + 1) >> 4));
      udp_buf->destport = UIP_HTONS(SICSLOWPAN_UDP_4_BIT_PORT_MIN +
                                    ((*(iphc_ptr + 1)) & 0x0F));
      LOG_DBG("uncompression: UDP ports (ptr+2): %x, %x\n",
             UIP_HTONS(udp_buf->srcport), UIP_HTONS(udp_buf->destport));

      iphc_ptr += 2;
      break;
    default:
      LOG_DBG("uncompression: error unsupported UDP compression\n");
      return false;
    }
    if(!checksum_compressed) { /* has_checksum, default  */
      CHECK_READ_SPACE(2);
      memcpy(&udp_buf->udpchksum, iphc_ptr, 2);
      iphc_ptr += 2;
      LOG_DBG("uncompression: checksum included\n");
    } else {
      LOG_DBG("uncompression: checksum *NOT* included\n");
    }

    /* length field in UDP header (8 byte header + payload) */
    udp_len = 8 + packetbuf_datalen() - (iphc_ptr - packetbuf_ptr);
    udp_buf->udplen = UIP_HTONS(ip_len == 0 ? udp_len :
                                ip_len - UIP_IPH_LEN - ext_hdr_len);
    LOG_DBG("uncompression: UDP length: %u (ext: %u) ip_len: %d udp_len: %d\n",
           UIP_HTONS(udp_buf->udplen), ext_hdr_len, ip_len, udp_len);

    uncomp_hdr_len += UIP_UDPH_LEN;
  }

  packetbuf_hdr_len = iphc_ptr - packetbuf_ptr;

  /* IP length field. */
  if(ip_len == 0) {
    int len = packetbuf_datalen() - packetbuf_hdr_len + uncomp_hdr_len - UIP_IPH_LEN;
    LOG_DBG("uncompression: IP payload length: %d. %u - %u + %u - %u\n", len,
           packetbuf_datalen(), packetbuf_hdr_len, uncomp_hdr_len, UIP_IPH_LEN);

    /* This is not a fragmented packet */
    SICSLOWPAN_IP_BUF(buf)->len[0] = len >> 8;
    SICSLOWPAN_IP_BUF(buf)->len[1] = len & 0x00FF;
  } else {
    /* This is a 1st fragment */
    SICSLOWPAN_IP_BUF(buf)->len[0] = (ip_len - UIP_IPH_LEN) >> 8;
    SICSLOWPAN_IP_BUF(buf)->len[1] = (ip_len - UIP_IPH_LEN) & 0x00FF;
  }

  return true;
}
/** @} */
#endif /* SICSLOWPAN_COMPRESSION >= SICSLOWPAN_COMPRESSION_IPHC */

#if SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_6LORH
/*--------------------------------------------------------------------*/
/**
 * \brief Adds Paging dispatch byte
 */
static void
add_paging_dispatch(uint8_t page)
{
  /* Add paging dispatch to Page 1 */
  PACKETBUF_6LO_PTR[PACKETBUF_6LO_DISPATCH] = SICSLOWPAN_DISPATCH_PAGING | (page & 0x0f);
  packetbuf_hdr_len++;
}
/*--------------------------------------------------------------------*/
/**
 * \brief Adds 6lorh headers before IPHC
 */
static void
add_6lorh_hdr(void)
{
  /* 6LoRH is not implemented yet */
}
#endif /* SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_6LORH */

/*--------------------------------------------------------------------*/
/**
 * \brief Digest 6lorh headers before IPHC
 */
static void
digest_paging_dispatch(void)
{
  /* Is this a paging dispatch? */
  if((PACKETBUF_6LO_PTR[PACKETBUF_6LO_DISPATCH] & SICSLOWPAN_DISPATCH_PAGING_MASK) == SICSLOWPAN_DISPATCH_PAGING) {
    /* Parse page number */
    curr_page = PACKETBUF_6LO_PTR[PACKETBUF_6LO_DISPATCH] & 0x0f;
    packetbuf_hdr_len++;
  }
}
/*--------------------------------------------------------------------*/
/**
 * \brief Digest 6lorh headers before IPHC
 */
static void
digest_6lorh_hdr(void)
{
  /* 6LoRH is not implemented yet */
}
/*--------------------------------------------------------------------*/
/** \name IPv6 dispatch "compression" function
 * @{                                                                 */
/*--------------------------------------------------------------------*/
/* \brief Packets "Compression" when only IPv6 dispatch is used
 *
 * There is no compression in this case, all fields are sent
 * inline. We just add the IPv6 dispatch byte before the packet.
 * \verbatim
 * 0               1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | IPv6 Dsp      | IPv6 header and payload ...
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * \endverbatim
 */
#if SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_IPV6
static void
compress_hdr_ipv6(void)
{
  *packetbuf_ptr = SICSLOWPAN_DISPATCH_IPV6;
  packetbuf_hdr_len += SICSLOWPAN_IPV6_HDR_LEN;
  memcpy(packetbuf_ptr + packetbuf_hdr_len, UIP_IP_BUF, UIP_IPH_LEN);
  packetbuf_hdr_len += UIP_IPH_LEN;
}
#endif /* SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_IPV6 */
/** @} */

/*--------------------------------------------------------------------*/
/** \name Input/output functions common to all compression schemes
 * @{                                                                 */
/*--------------------------------------------------------------------*/
/**
 * Callback function for the MAC packet sent callback
 */
static void
packet_sent(void *ptr, int status, int transmissions)
{
  const linkaddr_t *dest;

  if(callback != NULL) {
    callback->output_callback(status);
  }
  last_tx_status = status;

  /* What follows only applies to unicast */
  dest = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  if(linkaddr_cmp(dest, &linkaddr_null)) {
    return;
  }

  /* Update neighbor link statistics */
  link_stats_packet_sent(dest, status, transmissions);

  /* Call routing protocol link callback */
  NETSTACK_ROUTING.link_callback(dest, status, transmissions);

  /* DS6 callback, used for UIP_DS6_LL_NUD */
  uip_ds6_link_callback(status, transmissions);
}
/*--------------------------------------------------------------------*/
/**
 * \brief This function is called by the 6lowpan code to send out a
 * packet.
 */
static void
send_packet(void)
{
  /* Provide a callback function to receive the result of
     a packet transmission. */
  NETSTACK_MAC.send(&packet_sent, NULL);

  /* If we are sending multiple packets in a row, we need to let the
     watchdog know that we are still alive. */
  watchdog_periodic();
}
#if SICSLOWPAN_CONF_FRAG
/*--------------------------------------------------------------------*/
/**
 * \brief This function is called by the 6lowpan code to copy a fragment's
 * payload from uIP and send it down the stack.
 * \param uip_offset the offset in the uIP buffer where to copy the payload from
 * \param dest the link layer destination address of the packet
 * \return 1 if success, 0 otherwise
 */
static int
fragment_copy_payload_and_send(uint16_t uip_offset)
{
  struct queuebuf *q;

  /* Now copy fragment payload from uip_buf */
  memcpy(packetbuf_ptr + packetbuf_hdr_len,
         (uint8_t *)UIP_IP_BUF + uip_offset, packetbuf_payload_len);
  packetbuf_set_datalen(packetbuf_payload_len + packetbuf_hdr_len);

  /* Backup packetbuf to queuebuf. Enables preserving attributes for all framgnets */
  q = queuebuf_new_from_packetbuf();
  if(q == NULL) {
    LOG_WARN("output: could not allocate queuebuf, dropping fragment\n");
    return 0;
  }

  /* Send fragment */
  send_packet();

  /* Restore packetbuf from queuebuf */
  queuebuf_to_packetbuf(q);
  queuebuf_free(q);

  /* Check tx result. */
  if((last_tx_status == MAC_TX_COLLISION) ||
     (last_tx_status >= MAC_TX_ERR)) {
    LOG_ERR("output: error in fragment tx, dropping subsequent fragments.\n");
    return 0;
  }
  return 1;
}
#endif /* SICSLOWPAN_CONF_FRAG */
/*--------------------------------------------------------------------*/
/** \brief Take an IP packet and format it to be sent on an 802.15.4
 *  network using 6lowpan.
 *  \param localdest The MAC address of the destination
 *
 *  The IP packet is initially in uip_buf. Its header is compressed
 *  and if necessary it is fragmented. The resulting
 *  packet/fragments are put in packetbuf and delivered to the 802.15.4
 *  MAC.
 */
static uint8_t
output(const linkaddr_t *localdest)
{
  int frag_needed;

  /* init */
  uncomp_hdr_len = UIP_IPH_LEN;
  packetbuf_hdr_len = 0;

  /* reset packetbuf buffer */
  packetbuf_clear();
  packetbuf_ptr = packetbuf_dataptr();

  if(callback) {
    /* call the attribution when the callback comes, but set attributes
       here ! */
    set_packet_attrs();
  }

  LOG_INFO("output: sending IPv6 packet with len %d\n", uip_len);

  /* copy over the retransmission count from uipbuf attributes */
  packetbuf_set_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS,
                     uipbuf_get_attr(UIPBUF_ATTR_MAX_MAC_TRANSMISSIONS));

  /* Copy destination address to packetbuf */
  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER,
      localdest ? localdest : &linkaddr_null);

#if LLSEC802154_USES_AUX_HEADER
  /* copy LLSEC level */
  packetbuf_set_attr(PACKETBUF_ATTR_SECURITY_LEVEL,
    uipbuf_get_attr(UIPBUF_ATTR_LLSEC_LEVEL));
#if LLSEC802154_USES_EXPLICIT_KEYS
  packetbuf_set_attr(PACKETBUF_ATTR_KEY_INDEX,
    uipbuf_get_attr(UIPBUF_ATTR_LLSEC_KEY_ID));
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
#endif /*  LLSEC802154_USES_AUX_HEADER */

  /* Calculate NETSTACK_FRAMER's header length, that will be added in the NETSTACK_MAC */
  mac_max_payload = NETSTACK_MAC.max_payload();

  if(mac_max_payload <= 0) {
  /* Framing failed, drop packet */
    LOG_WARN("output: failed to calculate payload size - dropping packet\n");
    return 0;
  }

  /* Try to compress the headers */
#if SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_IPV6
  compress_hdr_ipv6();
#endif /* SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_IPV6 */
#if SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_6LORH
  /* Add 6LoRH headers before IPHC. Only needed on routed traffic
  (non link-local). */
  if(!uip_is_addr_linklocal(&UIP_IP_BUF->destipaddr)) {
    add_paging_dispatch(1);
    add_6lorh_hdr();
  }
#endif /* SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_6LORH */
#if SICSLOWPAN_COMPRESSION >= SICSLOWPAN_COMPRESSION_IPHC
  if(compress_hdr_iphc() == 0) {
    /* Warning should already be issued by function above */
    return 0;
  }
#endif /* SICSLOWPAN_COMPRESSION >= SICSLOWPAN_COMPRESSION_IPHC */

  /* Use the mac_max_payload to understand what is the max payload in a MAC
   * packet. We calculate it here only to make a better decision of whether
   * the outgoing packet needs to be fragmented or not. */

  frag_needed = (int)uip_len - (int)uncomp_hdr_len + (int)packetbuf_hdr_len > mac_max_payload;
  LOG_INFO("output: header len %d -> %d, total len %d -> %d, MAC max payload %d, frag_needed %d\n",
            uncomp_hdr_len, packetbuf_hdr_len,
            uip_len, uip_len - uncomp_hdr_len + packetbuf_hdr_len,
            mac_max_payload, frag_needed);

  if(frag_needed) {
#if SICSLOWPAN_CONF_FRAG
    /* Number of bytes processed. */
    uint16_t processed_ip_out_len;
    uint16_t frag_tag;
    int curr_frag = 0;

    /*
     * The outbound IPv6 packet is too large to fit into a single 15.4
     * packet, so we fragment it into multiple packets and send them.
     * The first fragment contains frag1 dispatch, then IPv6/IPHC/HC_UDP
     * dispatchs/headers and IPv6 payload (with len multiple of 8 bytes).
     * The subsequent fragments contain the FRAGN dispatch and more of the
     * IPv6 payload (still multiple of 8 bytes, except for the last fragment)
     */
     /* Total IPv6 payload */
    int total_payload = (uip_len - uncomp_hdr_len);
    /* IPv6 payload that goes to first fragment */
    int frag1_payload = (mac_max_payload - packetbuf_hdr_len - SICSLOWPAN_FRAG1_HDR_LEN) & 0xfffffff8;
    /* max IPv6 payload in each FRAGN. Must be multiple of 8 bytes */
    int fragn_max_payload = (mac_max_payload - SICSLOWPAN_FRAGN_HDR_LEN) & 0xfffffff8;
    /* max IPv6 payload in the last fragment. Needs not be multiple of 8 bytes */
    int last_fragn_max_payload = mac_max_payload - SICSLOWPAN_FRAGN_HDR_LEN;
    /* sum of all IPv6 payload that goes to non-first and non-last fragments */
    int middle_fragn_total_payload = MAX(total_payload - frag1_payload - last_fragn_max_payload, 0);
    /* Ceiling of: 2 + middle_fragn_total_payload / fragn_max_payload */
    unsigned fragment_count = 2;
    if(middle_fragn_total_payload > 0) {
      fragment_count += 1 + (middle_fragn_total_payload - 1) / fragn_max_payload;
    }

    size_t free_bufs = queuebuf_numfree();
    LOG_INFO("output: fragmentation needed. fragments: %u, free queuebufs: %zu\n",
      fragment_count, free_bufs);

    /* Keep one queuebuf in reserve for certain protocol implementations
       at other layers. */
    size_t needed_bufs = fragment_count + 1;
    if(free_bufs < needed_bufs) {
      LOG_WARN("output: dropping packet, not enough free bufs (needed: %zu, free: %zu)\n",
        needed_bufs, free_bufs);
      return 0;
    }

    if(frag1_payload < 0) {
      /* The current implementation requires that all headers fit in the first
       * fragment. Here is a corner case where the header did fit packetbuf
       * but do no longer fit after truncating for a length multiple of 8. */
      LOG_WARN("output: compressed header does not fit first fragment\n");
      return 0;
    }

    /* Reset last tx status -- MAC layers most often call packet_sent asynchrously */
    last_tx_status = MAC_TX_OK;
    /* Update fragment tag */
    frag_tag = my_tag++;

    /* Move IPHC/IPv6 header to make room for FRAG1 header */
    memmove(packetbuf_ptr + SICSLOWPAN_FRAG1_HDR_LEN, packetbuf_ptr, packetbuf_hdr_len);
    packetbuf_hdr_len += SICSLOWPAN_FRAG1_HDR_LEN;

    /* Set FRAG1 header */
    SET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_DISPATCH_SIZE,
          ((SICSLOWPAN_DISPATCH_FRAG1 << 8) | uip_len));
    SET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_TAG, frag_tag);

    /* Set frag1 payload len. Was already caulcated earlier as frag1_payload */
    packetbuf_payload_len = frag1_payload;

    /* Copy payload from uIP and send fragment */
    /* Send fragment */
    LOG_INFO("output: fragment %d/%d (tag %d, payload %d)\n",
             curr_frag + 1, fragment_count,
             frag_tag, packetbuf_payload_len);
    if(fragment_copy_payload_and_send(uncomp_hdr_len) == 0) {
      return 0;
    }

    /* Now prepare for subsequent fragments. */

    /* FRAGN header: tag was already set at FRAG1. Now set dispatch for all FRAGN */
    packetbuf_hdr_len = SICSLOWPAN_FRAGN_HDR_LEN;
    SET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_DISPATCH_SIZE,
          ((SICSLOWPAN_DISPATCH_FRAGN << 8) | uip_len));

    /* Keep track of the total length of data sent */
    processed_ip_out_len = uncomp_hdr_len + packetbuf_payload_len;

    /* Create and send subsequent fragments. */
    while(processed_ip_out_len < uip_len) {
      curr_frag++;
      /* FRAGN header: set offset for this fragment */
      PACKETBUF_FRAG_PTR[PACKETBUF_FRAG_OFFSET] = processed_ip_out_len >> 3;

      /* Calculate fragment len */
      if(uip_len - processed_ip_out_len > last_fragn_max_payload) {
        /* Not last fragment, send max FRAGN payload */
        packetbuf_payload_len = fragn_max_payload;
      } else {
        /* last fragment */
        packetbuf_payload_len = uip_len - processed_ip_out_len;
      }

      /* Copy payload from uIP and send fragment */
      /* Send fragment */
      LOG_INFO("output: fragment %d/%d (tag %d, payload %d, offset %d)\n",
               curr_frag + 1, fragment_count,
               frag_tag, packetbuf_payload_len, processed_ip_out_len);
      if(fragment_copy_payload_and_send(processed_ip_out_len) == 0) {
        return 0;
      }

      processed_ip_out_len += packetbuf_payload_len;
    }
#else /* SICSLOWPAN_CONF_FRAG */
    LOG_ERR("output: Packet too large to be sent without fragmentation support; dropping packet\n");
    return 0;
#endif /* SICSLOWPAN_CONF_FRAG */
  } else {
    /*
     * The packet does not need to be fragmented
     * copy "payload" and send
     */

   if(uip_len < uncomp_hdr_len) {
     LOG_ERR("output: uip_len is smaller than uncomp_hdr_len (%d < %d)",
             (int)uip_len, (int)uncomp_hdr_len);
     return 0;
    }

    memcpy(packetbuf_ptr + packetbuf_hdr_len, (uint8_t *)UIP_IP_BUF + uncomp_hdr_len,
           uip_len - uncomp_hdr_len);
    packetbuf_set_datalen(uip_len - uncomp_hdr_len + packetbuf_hdr_len);
    send_packet();
  }
  return 1;
}

/*--------------------------------------------------------------------*/
/** \brief Process a received 6lowpan packet.
 *
 *  The 6lowpan packet is put in packetbuf by the MAC. If its a frag1 or
 *  a non-fragmented packet we first uncompress the IP header. The
 *  6lowpan payload and possibly the uncompressed IP header are then
 *  copied in siclowpan_buf. If the IP packet is complete it is copied
 *  to uip_buf and the IP layer is called.
 *
 * \note We do not check for overlapping sicslowpan fragments
 * (it is a SHALL in the RFC 4944 and should never happen)
 */
static void
input(void)
{
  /* size of the IP packet (read from fragment) */
  uint16_t frag_size = 0;
  /* offset of the fragment in the IP packet */
  uint8_t frag_offset = 0;
  uint8_t *buffer;
  uint16_t buffer_size;

#if SICSLOWPAN_CONF_FRAG
  uint8_t is_fragment = 0;
  int8_t frag_context = 0;

  /* tag of the fragment */
  uint16_t frag_tag = 0;
  uint8_t first_fragment = 0, last_fragment = 0;
#endif /*SICSLOWPAN_CONF_FRAG*/

  /* Update link statistics */
  link_stats_input_callback(packetbuf_addr(PACKETBUF_ADDR_SENDER));

  /* init */
  uncomp_hdr_len = 0;
  packetbuf_hdr_len = 0;

  /* The MAC puts the 15.4 payload inside the packetbuf data buffer */
  packetbuf_ptr = packetbuf_dataptr();

  if(packetbuf_datalen() == 0) {
    LOG_WARN("input: empty packet\n");
    return;
  }

  /* Clear uipbuf and set default attributes */
  uipbuf_clear();

  /* This is default uip_buf since we assume that this is not fragmented */
  buffer = (uint8_t *)UIP_IP_BUF;
  buffer_size = UIP_BUFSIZE;

  /* Save the RSSI and LQI of the incoming packet in case the upper layer will
     want to query us for it later. */
  uipbuf_set_attr(UIPBUF_ATTR_RSSI, packetbuf_attr(PACKETBUF_ATTR_RSSI));
  uipbuf_set_attr(UIPBUF_ATTR_LINK_QUALITY, packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY));


#if SICSLOWPAN_CONF_FRAG

  /*
   * Since we don't support the mesh and broadcast header, the first header
   * we look for is the fragmentation header
   */
  switch((GET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_DISPATCH_SIZE) >> 8) & SICSLOWPAN_DISPATCH_FRAG_MASK) {
    case SICSLOWPAN_DISPATCH_FRAG1:
      frag_offset = 0;
      frag_size = GET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_DISPATCH_SIZE) & 0x07ff;
      frag_tag = GET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_TAG);
      packetbuf_hdr_len += SICSLOWPAN_FRAG1_HDR_LEN;
      first_fragment = 1;
      is_fragment = 1;

      LOG_INFO("input: received first element of a fragmented packet (tag %d, len %d)\n",
             frag_tag, frag_size);

      /* Add the fragment to the fragmentation context */
      frag_context = add_fragment(frag_tag, frag_size, frag_offset);

      if(frag_context == -1) {
        LOG_ERR("input: failed to allocate new reassembly context\n");
        return;
      }

      buffer = frag_info[frag_context].first_frag;
      buffer_size = SICSLOWPAN_FIRST_FRAGMENT_SIZE;
      break;
    case SICSLOWPAN_DISPATCH_FRAGN:
      /*
       * set offset, tag, size
       * Offset is in units of 8 bytes
       */
      frag_offset = PACKETBUF_FRAG_PTR[PACKETBUF_FRAG_OFFSET];
      frag_tag = GET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_TAG);
      frag_size = GET16(PACKETBUF_FRAG_PTR, PACKETBUF_FRAG_DISPATCH_SIZE) & 0x07ff;
      packetbuf_hdr_len += SICSLOWPAN_FRAGN_HDR_LEN;

      /* Add the fragment to the fragmentation context (this will also
         copy the payload) */
      frag_context = add_fragment(frag_tag, frag_size, frag_offset);

      if(frag_context == -1) {
        LOG_ERR("input: reassembly context not found (tag %d)\n", frag_tag);
        return;
      }

      /* Ok - add_fragment will store the fragment automatically - so
         we should not store more */
      buffer = NULL;

      if(frag_info[frag_context].reassembled_len >= frag_size) {
        last_fragment = 1;
      }
      is_fragment = 1;
      break;
    default:
      break;
  }

  if(is_fragment && !first_fragment) {
    /* this is a FRAGN, skip the header compression dispatch section */
    goto copypayload;
  }
#endif /* SICSLOWPAN_CONF_FRAG */

  /* First, process 6LoRH headers */
  curr_page = 0;
  digest_paging_dispatch();
  if(curr_page == 1) {
    LOG_INFO("input: page 1, 6LoRH\n");
    digest_6lorh_hdr();
  } else if (curr_page > 1) {
    LOG_ERR("input: page %u not supported\n", curr_page);
    return;
  }

  /* Process next dispatch and headers */
  if(SICSLOWPAN_COMPRESSION > SICSLOWPAN_COMPRESSION_IPV6 &&
     (PACKETBUF_6LO_PTR[PACKETBUF_6LO_DISPATCH] & SICSLOWPAN_DISPATCH_IPHC_MASK) == SICSLOWPAN_DISPATCH_IPHC) {
    LOG_DBG("uncompression: IPHC dispatch\n");
    if(uncompress_hdr_iphc(buffer, buffer_size, frag_size) == false) {
      LOG_ERR("input: failed to decompress IPHC packet\n");
      return;
    }
  } else if(PACKETBUF_6LO_PTR[PACKETBUF_6LO_DISPATCH] == SICSLOWPAN_DISPATCH_IPV6) {
    LOG_DBG("uncompression: IPV6 dispatch\n");
    packetbuf_hdr_len += SICSLOWPAN_IPV6_HDR_LEN;

    /* Put uncompressed IP header in sicslowpan_buf. */
    memcpy(buffer, packetbuf_ptr + packetbuf_hdr_len, UIP_IPH_LEN);

    /* Update uncomp_hdr_len and packetbuf_hdr_len. */
    packetbuf_hdr_len += UIP_IPH_LEN;
    uncomp_hdr_len += UIP_IPH_LEN;
  } else {
    LOG_ERR("uncompression: unknown dispatch: 0x%02x, or IPHC disabled\n",
             PACKETBUF_6LO_PTR[PACKETBUF_6LO_DISPATCH] & SICSLOWPAN_DISPATCH_IPHC_MASK);
    return;
  }

#if SICSLOWPAN_CONF_FRAG
 copypayload:
#endif /*SICSLOWPAN_CONF_FRAG*/
  /*
   * copy "payload" from the packetbuf buffer to the sicslowpan_buf
   * if this is a first fragment or not fragmented packet,
   * we have already copied the compressed headers, uncomp_hdr_len
   * and packetbuf_hdr_len are non 0, frag_offset is.
   * If this is a subsequent fragment, this is the contrary.
   */
  if(packetbuf_datalen() < packetbuf_hdr_len) {
    LOG_ERR("input: packet dropped due to header > total packet\n");
    return;
  }
  packetbuf_payload_len = packetbuf_datalen() - packetbuf_hdr_len;

#if SICSLOWPAN_CONF_FRAG
  if(is_fragment) {
    LOG_INFO("input: fragment (tag %d, payload %d, offset %d) -- %u %u\n",
         frag_tag, packetbuf_payload_len, frag_offset << 3, packetbuf_datalen(), packetbuf_hdr_len);
  }
#endif /*SICSLOWPAN_CONF_FRAG*/

  /* Sanity-check size of incoming packet to avoid buffer overflow */
  {
    unsigned int req_size = uncomp_hdr_len + (uint16_t)(frag_offset << 3)
        + packetbuf_payload_len;
    if(req_size > sizeof(uip_buf)) {
#if SICSLOWPAN_CONF_FRAG
      LOG_ERR(
          "input: packet and fragment context %u dropped, minimum required IP_BUF size: %d+%d+%d=%u (current size: %u)\n",
          frag_context,
          uncomp_hdr_len, (uint16_t)(frag_offset << 3),
          packetbuf_payload_len, req_size, (unsigned)sizeof(uip_buf));
      /* Discard all fragments for this contex, as reassembling this particular fragment would
       * cause an overflow in uipbuf */
      clear_fragments(frag_context);
#endif /* SICSLOWPAN_CONF_FRAG */
      return;
    }
  }

  /* copy the payload if buffer is non-null - which is only the case with first fragment
     or packets that are non fragmented */
  if(buffer != NULL) {
    if(uncomp_hdr_len + packetbuf_payload_len > buffer_size) {
      LOG_ERR("input: cannot copy the payload into the buffer\n");
      return;
    }
    memcpy((uint8_t *)buffer + uncomp_hdr_len, packetbuf_ptr + packetbuf_hdr_len, packetbuf_payload_len);
  }

  /* update processed_ip_in_len if fragment, sicslowpan_len otherwise */

#if SICSLOWPAN_CONF_FRAG
  if(frag_size > 0) {
    /* Add the size of the header only for the first fragment. */
    if(first_fragment != 0) {
      frag_info[frag_context].reassembled_len = uncomp_hdr_len + packetbuf_payload_len;
      frag_info[frag_context].first_frag_len = uncomp_hdr_len + packetbuf_payload_len;
    }
    /* For the last fragment, we are OK if there is extrenous bytes at
       the end of the packet. */
    if(last_fragment != 0) {
      frag_info[frag_context].reassembled_len = frag_size;
      /* copy to uip */
      if(!copy_frags2uip(frag_context)) {
        return;
      }
    }
  }

  /*
   * If we have a full IP packet in sicslowpan_buf, deliver it to
   * the IP stack
   */
  if(!is_fragment || last_fragment) {
    /* packet is in uip already - just set length */
    if(is_fragment != 0 && last_fragment != 0) {
      uip_len = frag_size;
    } else {
      uip_len = packetbuf_payload_len + uncomp_hdr_len;
    }
#else
    uip_len = packetbuf_payload_len + uncomp_hdr_len;
#endif /* SICSLOWPAN_CONF_FRAG */
    LOG_INFO("input: received IPv6 packet with len %d\n",
             uip_len);

    if(LOG_DBG_ENABLED) {
      uint16_t ndx;
      LOG_DBG("uncompression: after (%u):", UIP_IP_BUF->len[1]);
      for (ndx = 0; ndx < UIP_IP_BUF->len[1] + 40; ndx++) {
        uint8_t data = ((uint8_t *) (UIP_IP_BUF))[ndx];
        LOG_DBG_("%02x", data);
      }
      LOG_DBG_("\n");
    }

    /* if callback is set then set attributes and call */
    if(callback) {
      set_packet_attrs();
      callback->input_callback();
    }

#if LLSEC802154_USES_AUX_HEADER
    /*
     * Assuming that the last packet in packetbuf is containing
     *  the LLSEC state so that it can be copied to uipbuf.
     */
    uipbuf_set_attr(UIPBUF_ATTR_LLSEC_LEVEL,
      packetbuf_attr(PACKETBUF_ATTR_SECURITY_LEVEL));
#if LLSEC802154_USES_EXPLICIT_KEYS
    uipbuf_set_attr(UIPBUF_ATTR_LLSEC_KEY_ID,
      packetbuf_attr(PACKETBUF_ATTR_KEY_INDEX));
#endif /* LLSEC802154_USES_EXPLICIT_KEYS */
#endif /*  LLSEC802154_USES_AUX_HEADER */

    tcpip_input();
#if SICSLOWPAN_CONF_FRAG
  }
#endif /* SICSLOWPAN_CONF_FRAG */
}
/** @} */

/*--------------------------------------------------------------------*/
/* \brief 6lowpan init function (called by the MAC layer)             */
/*--------------------------------------------------------------------*/
void
sicslowpan_init(void)
{

#if SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_IPHC
/* Preinitialize any address contexts for better header compression
 * (Saves up to 13 bytes per 6lowpan packet)
 * The platform contiki-conf.h file can override this using e.g.
 * #define SICSLOWPAN_CONF_ADDR_CONTEXT_0 {addr_contexts[0].prefix[0]=0xbb;addr_contexts[0].prefix[1]=0xbb;}
 */
#if SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0
  addr_contexts[0].used   = 1;
  addr_contexts[0].number = 0;
#ifdef SICSLOWPAN_CONF_ADDR_CONTEXT_0
  SICSLOWPAN_CONF_ADDR_CONTEXT_0;
#else
  addr_contexts[0].prefix[0] = UIP_DS6_DEFAULT_PREFIX_0;
  addr_contexts[0].prefix[1] = UIP_DS6_DEFAULT_PREFIX_1;
#endif
#endif /* SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 0 */

#if SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 1
  {
    int i;
    for(i = 1; i < SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS; i++) {
#ifdef SICSLOWPAN_CONF_ADDR_CONTEXT_1
      if (i==1) {
        addr_contexts[1].used   = 1;
        addr_contexts[1].number = 1;
        SICSLOWPAN_CONF_ADDR_CONTEXT_1;
#ifdef SICSLOWPAN_CONF_ADDR_CONTEXT_2
      } else if (i==2) {
        addr_contexts[2].used   = 1;
        addr_contexts[2].number = 2;
        SICSLOWPAN_CONF_ADDR_CONTEXT_2;
#endif
      } else {
        addr_contexts[i].used = 0;
      }
#else
      addr_contexts[i].used = 0;
#endif /* SICSLOWPAN_CONF_ADDR_CONTEXT_1 */
    }
  }
#endif /* SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS > 1 */

#endif /* SICSLOWPAN_COMPRESSION == SICSLOWPAN_COMPRESSION_IPHC */
}
/*--------------------------------------------------------------------*/
const struct network_driver sicslowpan_driver = {
  "sicslowpan",
  sicslowpan_init,
  input,
  output
};
/*--------------------------------------------------------------------*/
/** @} */
