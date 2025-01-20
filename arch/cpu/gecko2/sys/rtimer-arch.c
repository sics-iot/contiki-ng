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
 * \addtogroup gecko-rtimer Rtimer driver
 * @{
 *
 * \file
 *         Implementation of the architecture dependent rtimer functions for the Gecko
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "sl_sleeptimer.h"

#include "rtimer-arch.h"
/*---------------------------------------------------------------------------*/
static sl_sleeptimer_timer_handle_t rtimer_timer;
/*---------------------------------------------------------------------------*/
static void
on_rtimer_timeout(sl_sleeptimer_timer_handle_t *handle,
                  void *data)
{
  (void)handle;
  (void)data;
  rtimer_run_next();
}
/*---------------------------------------------------------------------------*/
void
rtimer_arch_init(void)
{
  sl_sleeptimer_init();
}
/*---------------------------------------------------------------------------*/
void
rtimer_arch_schedule(rtimer_clock_t t)
{
  uint32_t now = sl_sleeptimer_get_tick_count();
  uint32_t timeout = t - now;

  /* t is an absolute time */
  /* sl_sleeptimer_start_timer takes a timeout */
  /* Check for overflow and compensate */
  if(timeout > 0x80000000UL) {
    timeout -= 0x80000000UL;
  }

  sl_sleeptimer_start_timer(&rtimer_timer,
                            timeout,
                            on_rtimer_timeout, NULL,
                            0,
                            0);
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
rtimer_arch_now()
{
  return sl_sleeptimer_get_tick_count();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
