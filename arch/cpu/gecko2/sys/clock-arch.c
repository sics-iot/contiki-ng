/*
 * Copyright (C) 2022 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup gecko
 * @{
 *
 * \addtogroup gecko-sys System drivers
 * @{
 *
 * \addtogroup gecko-clock Clock driver
 * @{
 *
 * \file
 *         Software clock implementation for the gecko.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "sl_sleeptimer.h"
/*---------------------------------------------------------------------------*/
static volatile uint32_t ticks;
static sl_sleeptimer_timer_handle_t periodic_timer;
/*---------------------------------------------------------------------------*/
static void
on_periodic_timeout(sl_sleeptimer_timer_handle_t *handle,
                    void *data)
{
  ticks++;
  if(etimer_pending()) {
    etimer_request_poll();
  }
}
/*---------------------------------------------------------------------------*/
static void
delay_callback(sl_sleeptimer_timer_handle_t *handle,
               void *data)
{
  volatile bool *wait_flag = (bool *)data;

  (void)handle;  /* Unused parameter. */

  *wait_flag = false;
}
/*---------------------------------------------------------------------------*/
void
clock_init(void)
{
  uint32_t timer_frequency;

  sl_sleeptimer_init();
  timer_frequency = sl_sleeptimer_get_timer_frequency();
  ticks = 0;
  sl_sleeptimer_start_periodic_timer(&periodic_timer,
                                     timer_frequency / CLOCK_CONF_SECOND,
                                     on_periodic_timeout, NULL,
                                     0,
                                     SL_SLEEPTIMER_NO_HIGH_PRECISION_HF_CLOCKS_REQUIRED_FLAG);
}
/*---------------------------------------------------------------------------*/
clock_time_t
clock_time(void)
{
  return (clock_time_t)(ticks & 0xFFFFFFFF);
}
/*---------------------------------------------------------------------------*/
unsigned long
clock_seconds(void)
{
  return (unsigned long)ticks / CLOCK_CONF_SECOND;
}
/*---------------------------------------------------------------------------*/
void
clock_wait(clock_time_t i)
{
  clock_time_t start;
  start = clock_time();
  while(clock_time() - start < (clock_time_t)i) {
    __WFE();
  }
}
/*---------------------------------------------------------------------------*/
void
clock_delay_usec(uint16_t dt)
{
  volatile bool wait = true;
  sl_status_t error_code;
  sl_sleeptimer_timer_handle_t delay_timer;
  uint32_t timer_frequency = sl_sleeptimer_get_timer_frequency();
  uint32_t delay = (uint32_t)((((uint32_t)dt * timer_frequency) + 999999) / 1000000L);

  error_code = sl_sleeptimer_start_timer(&delay_timer,
                                         delay,
                                         delay_callback,
                                         (void *)&wait,
                                         0,
                                         0);
  if(error_code == SL_STATUS_OK) {
    while(wait) {  /* Active delay loop. */
    }
  }
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Obsolete delay function but we implement it here since some code
 * still uses it
 */
void
clock_delay(unsigned int i)
{
  clock_delay_usec(i);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
