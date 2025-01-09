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

#include "net/netstack.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/routing/routing.h"

#include "sys/log.h"

#ifndef BUILD_WITH_RPL_BORDER_ROUTER
#error BUILD_WITH_RPL_BORDER_ROUTER required
#endif /* !BUILD_WITH_RPL_BORDER_ROUTER */

#define LOG_MODULE "6LBR"
#define LOG_LEVEL LOG_LEVEL_MAIN

void custom_shell_init(void);
static void rs_input(void);

UIP_ICMP6_HANDLER(rs_input_handler, ICMP6_RS,
                  UIP_ICMP6_HANDLER_CODE_ANY, rs_input);
PROCESS(standalone_rpl_border_router_process,
        "Standalone RPL Border Router Process");
AUTOSTART_PROCESSES(&standalone_rpl_border_router_process);

/*---------------------------------------------------------------------------*/
static void
rs_input(void)
{
  /*
   * We may receive a RS from the host OS which is running 6lbr via
   * the uplink interface. Let's ignore the RS.
   */
  LOG_INFO("Ignore RS from ");
  LOG_INFO_6ADDR(&UIP_IP_BUF->srcipaddr);
  LOG_INFO_("\n");
  uipbuf_clear();
}
/*---------------------------------------------------------------------------*/
void
rpl_border_router_init(void)
{
  /*
   * The prototype of rpl_border_router_init() is defined in
   * os/services/rpl-border-router/rpl-border-router.h, which is
   * included by contiki-main.c.
   */
  PROCESS_NAME(webserver_nogui_process);
  process_start(&webserver_nogui_process, NULL);

  custom_shell_init();
  uip_icmp6_register_input_handler(&rs_input_handler);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(standalone_rpl_border_router_process, ev, data)
{
  PROCESS_BEGIN();

  LOG_INFO("Start as RPL Root\n");
  NETSTACK_ROUTING.root_start();

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
