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

/* Network-layer demultiplexer */

#include "contiki.h"

#include "net/netstack.h"

#include "net-downlink.h"
#include "net-uplink.h"

#include "sys/log.h"

#define LOG_MODULE "demux"
#define LOG_LEVEL LOG_LEVEL_IPV6

/*---------------------------------------------------------------------------*/
static void
init(void)
{
  net_uplink_init();
  net_downlink_init();
}
/*---------------------------------------------------------------------------*/
static void
input(void)
{
  /* shouldn't be called */
  LOG_ERR("net-demux.input() is UNEXPECTEDLY called.\n");
}
/*---------------------------------------------------------------------------*/
static uint8_t
output(const linkaddr_t *localdest)
{
  LOG_DBG("Route a packet for ");
  LOG_DBG_LLADDR(localdest);
  if(localdest != NULL && net_uplink_is_peer_linkaddr(localdest)) {
    /* route to the uplink only unicast frames destined to the peer */
    LOG_DBG_(" to the uplink\n");
    net_uplink_output(localdest);
  } else {
    /* the rest including broadcast frames are routed to the downlink */
    LOG_DBG_(" to the downlink\n");
    net_downlink_output(localdest);
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
const struct network_driver network_demultiplexer_driver = {
  "net-demux",
  init,
  input,
  output
};
/*---------------------------------------------------------------------------*/
