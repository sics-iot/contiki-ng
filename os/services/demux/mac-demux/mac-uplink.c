/*
 * Copyright (c) 2019, Inria.
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* The uplink is implemented with the SLIP I/F. */

#include "contiki.h"

#include "net/linkaddr.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/ipv6/sicslowpan.h"

#include "dev/slip.h"

#include "sys/log.h"

#define LOG_MODULE "demux-up"
#define LOG_LEVEL LOG_LEVEL_MAC

#ifdef MAC_UPLINK_PEER_MAC_ADDR
static const uint8_t peer_mac_addr[] = MAC_UPLINK_PEER_MAC_ADDR;
#else
#error MAC_UPLINK_PEER_MAC_ADDR needs to be specified
#endif /* MAC_UPLINK_PEER_MAC_ADDR */

/*---------------------------------------------------------------------------*/
static void
input_callback(void)
{
  /*
   * uip_buf has received bytes, which need to be stored in packetbuf
   * so that sicslowpan can process the received packet.
   */
  if(uip_len > PACKETBUF_SIZE) {
    LOG_ERR("Drop a received packet; too big (%u bytes)\n", uip_len);
    uipbuf_clear();
  } else {
    LOG_DBG("Receive a received packet (%u bytes)\n", uip_len);
    /* move the contents in uip_buf to packetbuf */
    packetbuf_clear();
    memcpy(packetbuf_hdrptr(), uip_buf, uip_len);
    packetbuf_set_datalen(uip_len);
    packetbuf_set_addr(PACKETBUF_ADDR_SENDER,
                       (const linkaddr_t *)peer_mac_addr);
    /*
     * the receiver address attribute is referred when the input is a
     * unicast frame
     */
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr);
    uipbuf_clear();
    sicslowpan_driver.input();
  }
}
/*---------------------------------------------------------------------------*/
void
mac_uplink_init(void)
{
  LOG_INFO("Initialize SLIP for the uplink\n");
  slip_arch_init();
  slip_set_input_callback(input_callback);
  process_start(&slip_process, NULL);
}
/*---------------------------------------------------------------------------*/
void
mac_uplink_send(mac_callback_t sent_callback, void *ptr)
{
  if(packetbuf_totlen() > sizeof(uip_buf)) {
    LOG_ERR("Drop the sending packet; too big (%u bytes)\n",
            packetbuf_totlen());
    sent_callback(ptr, MAC_TX_ERR_FATAL, 0);
  } else {
    slip_write(packetbuf_hdrptr(), packetbuf_totlen());
    sent_callback(ptr, MAC_TX_OK, 1);
  }
}
/*---------------------------------------------------------------------------*/
bool
mac_uplink_is_peer_linkaddr(const linkaddr_t *linkaddr)
{
  return linkaddr_cmp(linkaddr, (const linkaddr_t *)peer_mac_addr);
}
/*---------------------------------------------------------------------------*/
