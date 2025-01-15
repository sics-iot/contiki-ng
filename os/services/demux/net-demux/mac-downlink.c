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

#include "contiki.h"

#include "net/mac/mac.h"
#include "net/packetbuf.h"
#include "net/ipv6/sicslowpan.h"

void net_downlink_slip_send(mac_callback_t sent_callback, void *ptr);

/*
 * MAX_PAYLOAD_LEN shouldn't be too large for 6LoWPAN
 * fragmentation. The following definitions for
 * SICSLOWPAN_FRAGMENT_SIZE are copied from sicslowpan.c.
 */
#ifdef SICSLOWPAN_CONF_FRAGMENT_SIZE
#define SICSLOWPAN_FRAGMENT_SIZE SICSLOWPAN_CONF_FRAGMENT_SIZE
#else
#define SICSLOWPAN_FRAGMENT_SIZE (127 - 2 - 15)
#endif
#define MAC_DOWNLINK_MAX_PAYLOAD_LEN  \
  (SICSLOWPAN_FRAGMENT_SIZE + SICSLOWPAN_FRAGN_HDR_LEN)

/*---------------------------------------------------------------------------*/
static void
init(void)
{
}
/*---------------------------------------------------------------------------*/
static void
send(mac_callback_t sent_callback, void *ptr)
{
  /*
   * NETSTACK_MAC.send() is expected to be called only in sixlowpan.c
   * for packets to the downlink.
   */
  net_downlink_slip_send(sent_callback, ptr);
}
/*---------------------------------------------------------------------------*/
static void
input(void)
{
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
max_payload(void)
{
  return MAC_DOWNLINK_MAX_PAYLOAD_LEN;
}
/*---------------------------------------------------------------------------*/

const struct mac_driver mac_downlink_driver = {
  "mac_dummy",
  init,
  send,
  input,
  on,
  off,
  max_payload,
};
/*---------------------------------------------------------------------------*/
