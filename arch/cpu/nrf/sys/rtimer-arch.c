/*
 * Copyright (c) 2024, Marcel Graber <marcel@clever.design>
 * Copyright (c) 2020, Toshiba BRIL
 * Copyright (C) 2020 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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
 * \addtogroup nrf
 * @{
 *
 * \addtogroup nrf-sys System drivers
 * @{
 *
 * \addtogroup nrf-rtimer Rtimer driver
 * @{
 *
 * \file
 *         Implementation of the architecture dependent rtimer functions
 *         for the nRF
 * \author
 *         Marcel Graber <marcel@clever.design>
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "sys/rtimer.h"
#include "sys/soc-rtc.h"

/*---------------------------------------------------------------------------*/
static volatile rtimer_clock_t next_trigger = 0;
/*---------------------------------------------------------------------------*/
void
rtimer_arch_init(void)
{
  /* good place to initialize the RTC, used as base tick for rtimer */
  soc_rtc_init();
}
/*---------------------------------------------------------------------------*/
/**
 *
 * This function schedules a one-shot event with the RTC.
 *
 */
void
rtimer_arch_schedule(rtimer_clock_t t)
{
  /* Convert the rtimer tick value to a value suitable for the RTC */
  soc_rtc_schedule_one_shot(SOC_RTC_RTIMER_CH, (clock_time_t)t);
  next_trigger = t;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Returns the current real-time clock time
 * \return The current rtimer time in ticks
 *
 * The value is read from the RTC counter and converted to a number of
 * rtimer ticks
 *
 */
rtimer_clock_t
rtimer_arch_now()
{
  return soc_rtc_get_rtimer_ticks();
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Returns the next rtimer time
 * \return The next rtimer time in ticks or 0 if no event is scheduled
 *
 *
 */
rtimer_clock_t
rtimer_arch_next()
{
  return RTIMER_CLOCK_LT(RTIMER_NOW(), (next_trigger)) ? next_trigger : 0;
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
