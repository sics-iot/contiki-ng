/*
 * Copyright (C) 2024 Marcel Graber <marcel@clever.design>
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
 * For LowPower setup, we use RTC instead of TIMER as the basis for all clocks and
 * timers
 *
 * We use two RTC channels. Channel 0 is used by the rtimer sub-system. Channel 1 is used by the system clock and the LPM module.
 *
 * The RTC runs in all power modes except 'shutdown'
 *
 * \file
 *         Header file for the nRF rtimer (LowPower) driver
 * \author
 *         Marcel Graber <marcel@clever.design>
 *
 */
#ifndef SOC_RTC_H_
#define SOC_RTC_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "rtimer.h"

#include <stdint.h>
#include "nrfx_rtc.h"

#define SOC_RTC_SYSTEM_CH NRFX_RTC_INT_COMPARE0
#define SOC_RTC_RTIMER_CH NRFX_RTC_INT_COMPARE1

/*---------------------------------------------------------------------------*/
/**
 * \brief Initialise the RTC module
 *
 * This timer configures RTC channels.
 *
 * This function must be called before clock_init() and rtimer_init()
 */
void soc_rtc_init(void);

/**
 * \brief Schedule an RTC one-shot compare event
 * \param channel The RTC channel to use
 * \param ref_time The time when the event will be fired.
 *
 * Channel RTC_CH0 is reserved for the rtimer. RTC_CH1 is reserved
 * for the system clock.
 *
 * User applications should not use this function. User applications should
 * instead use Contiki's timer-related libraries
 */
void soc_rtc_schedule_one_shot(uint32_t channel, rtimer_clock_t ref_time);

/**
 * \brief Getting th ticks of the RTIMER since startup
 * \return The actual ticks of the clock
 *
 * The rtimer ticks per seconds are defined in the macro RTIMER_SECOND
 */
rtimer_clock_t soc_rtc_get_rtimer_ticks(void);

/**
 * \brief Getting th ticks of the CLOCK since startup
 * \return The actual ticks of the clock
 *
 * The clock ticks per seccons are defined in the macro CLOCK_SECOND
 */
clock_time_t soc_rtc_get_clock_ticks(void);

/**
 * \brief Helper Function in lpm.c to calculate the next wakeup time
 * \return The last time the RTC interrupt was fired
 */
rtimer_clock_t soc_rtc_last_isr_time(void);
/*---------------------------------------------------------------------------*/
#endif /* SOC_RTC_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
