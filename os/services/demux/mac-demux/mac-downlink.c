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

#include "net/mac/mac.h"
#include "net/packetbuf.h"

#if MAC_CONF_WITH_CSMA
extern const struct mac_driver csma_driver;
static const struct mac_driver *netstack_mac = &csma_driver;
#elif MAC_CONF_WITH_TSCH
extern const struct mac_driver tschmac_driver;
static const struct mac_driver *netstack_mac = &tschmac_driver;
#else
#error Support only CSMA and TSCH
#endif

#include <sys/log.h>

#define LOG_MODULE "demux-down"
#define LOG_LEVEL LOG_LEVEL_MAC

/*---------------------------------------------------------------------------*/
void
mac_downlink_init(void)
{
  LOG_INFO("Initialize %s for the downlink\n", netstack_mac->name);
  netstack_mac->init();
}
/*---------------------------------------------------------------------------*/
void
mac_downlink_send(mac_callback_t sent_callback, void *ptr)
{
  netstack_mac->send(sent_callback, ptr);
}
/*---------------------------------------------------------------------------*/
void
mac_downlink_input(void)
{
  /* input is expected to be called only from the radio driver */
  netstack_mac->input();
}
/*---------------------------------------------------------------------------*/
int
mac_downlink_on(void)
{
  return netstack_mac->on();
}
/*---------------------------------------------------------------------------*/
int
mac_downlink_off(void)
{
  return netstack_mac->off();
}
/*---------------------------------------------------------------------------*/
int
mac_downlink_max_payload(void)
{
  /* need to return a certain value for sixlowpan, header compression */
  return netstack_mac->max_payload();
}
/*---------------------------------------------------------------------------*/
