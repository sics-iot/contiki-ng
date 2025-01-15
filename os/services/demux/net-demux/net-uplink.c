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

/*
 * The uplink of the standalone RPL border router is implemented with
 * the TUN I/F.
 */

#include "contiki.h"

#include "net/netstack.h"

#include "net-downlink.h"

#include "sys/log.h"

#define LOG_MODULE "demux-up"
#define LOG_LEVEL LOG_LEVEL_IPV6

extern const struct network_driver tun6_net_driver;

bool net_downlink_is_peer_linkaddr(const linkaddr_t *linkaddr);

/*---------------------------------------------------------------------------*/
void
net_uplink_init(void)
{
  LOG_INFO("Initialize TUN for the uplink\n");
  tun6_net_driver.init();
}
/*---------------------------------------------------------------------------*/
void
net_uplink_output(const linkaddr_t *localdest)
{
  const linkaddr_t *localdest_null = NULL;
  (void)tun6_net_driver.output(localdest_null);
}
/*---------------------------------------------------------------------------*/
bool
net_uplink_is_peer_linkaddr(const linkaddr_t *linkaddr)
{
  return linkaddr != NULL && net_downlink_is_peer_linkaddr(linkaddr) == false;
}
/*---------------------------------------------------------------------------*/
