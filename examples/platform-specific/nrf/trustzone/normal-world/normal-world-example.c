/*
 * Copyright (c) 2022 RISE Research Institutes of Sweden
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Authors: John Kanwar <johnkanwar@hotmail.com>
 *          Niclas Finne <niclas.finne@ri.se>
 *          Nicolas Tsiftes <nicolas.tsiftes@ri.se>
 */

#include <stdio.h>
#include <string.h>

#include "contiki.h"
#include "dev/button-hal.h"

#include <nrfx.h>

PROCESS(normal_world_process, "Normal world process");
AUTOSTART_PROCESSES(&normal_world_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(normal_world_process, ev, data)
{
  static struct etimer et;
  static unsigned long iteration;

  PROCESS_BEGIN();

  printf("Non-secure world\n");

#if TEST_SECURE_FAULT
  int *sec_retp = (int *)0x2003ff64;
  printf("sec ret = %d\n", *sec_retp);
#endif

  etimer_set(&et, CLOCK_SECOND * 10);
  while(true) {
    PROCESS_WAIT_EVENT();
    if(ev == button_hal_press_event) {
      button_hal_button_t *btn = data;
      printf("Press event (%s)\n", BUTTON_HAL_GET_DESCRIPTION(btn));
    } else if(ev == button_hal_release_event) {
      button_hal_button_t *btn = data;
      printf("Release event (%s)\n", BUTTON_HAL_GET_DESCRIPTION(btn));

      /*
       * Run a secure fault test by pressing user button (Button 1 on
       * nRF5340DK) for more than 10 seconds.
       */
      if(btn->unique_id == BUTTON_HAL_ID_USER_BUTTON &&
         btn->press_duration_events >= 10) {
        printf("%s pressed for %u secs. Running fault test.\n\n",
               BUTTON_HAL_GET_DESCRIPTION(btn),
               btn->press_duration_events);
        /*
         * Read memory in the secure world. This should trigger a
         * secure fault and reboot the device.
         */
        int *sec_retp = (int *)0x2003ff64;
        printf("sec ret = %d\n", *sec_retp);
      }
    } else if(ev == button_hal_periodic_event) {
      button_hal_button_t *btn = data;
      printf("Periodic event, %u seconds (%s)\n", btn->press_duration_events,
             BUTTON_HAL_GET_DESCRIPTION(btn));
    }

    if(etimer_expired(&et)) {
      printf("  Iteration %lu at %lu\n",
             ++iteration, (unsigned long)clock_time());
      etimer_reset(&et);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
