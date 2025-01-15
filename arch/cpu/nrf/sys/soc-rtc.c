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
 * \addtogroup nrf-sys
 * @{
 *
 * \file
 * Implementation of the NRF RTC driver
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "clock.h"
#include "clock-arch.h"

#include "soc-rtc.h"

#include "etimer.h"
#include "lpm.h"
#include "rtimer.h"

#include "nrfx_clock.h"
#include "nrfx_rtc.h"

#include <stdbool.h>
#include <stdint.h>
#include "sys/log.h"
#define LOG_MODULE "RTC"
#define LOG_LEVEL LOG_LEVEL_INFO

/*---------------------------------------------------------------------------*/
#define COMPARE_INCREMENT (RTIMER_SECOND / CLOCK_SECOND)
#define MULTIPLE_256_MASK 0xFFFFFF00
/*---------------------------------------------------------------------------*/
#if CLOCK_SIZE != 4
#error CLOCK_CONF_SIZE must be 4 (32 bit)
#endif
#if RTIMER_SECOND != 32768
#error RTIMER_SECOND must be 32768 when using the NRF_RTC
#endif

#ifdef NRF_CLOCK_CONF_RTC_INSTANCE
#define NRF_CLOCK_RTC_INSTANCE NRF_CLOCK_CONF_RTC_INSTANCE
#else
#define NRF_CLOCK_RTC_INSTANCE 0
#endif

/*---------------------------------------------------------------------------*/
/**< RTC instance used for platform clock */
static const nrfx_rtc_t rtc = NRFX_RTC_INSTANCE(NRF_CLOCK_RTC_INSTANCE);
/*---------------------------------------------------------------------------*/
static rtimer_clock_t last_isr_time;
static clock_time_t rtc_max_clock_ticks;
static rtimer_clock_t rtc_max_rtimer_ticks;
static volatile uint32_t overflow;
/*---------------------------------------------------------------------------*/
static void
clock_handler(nrfx_clock_evt_type_t event)
{
  (void)event;
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Function for handling the RTC<instance> interrupts
 * @param int_type Type of interrupt to be handled
 */
static void
rtc_handler(nrfx_rtc_int_type_t int_type)
{
  if(int_type == SOC_RTC_SYSTEM_CH) {
    clock_update();
    uint32_t next = (nrfx_rtc_counter_get(&rtc) + COMPARE_INCREMENT) & MULTIPLE_256_MASK;
    nrfx_rtc_cc_set(&rtc, SOC_RTC_SYSTEM_CH, next, true);
  } else if(int_type == SOC_RTC_RTIMER_CH) {
    /* rtimer event */
    rtimer_run_next();
    /* We need to handle the compare event */
  } else if(int_type == NRFX_RTC_INT_OVERFLOW) {
    overflow++;
  }
  last_isr_time = soc_rtc_get_rtimer_ticks();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Function starting the internal LFCLK XTAL oscillator.
 */
static void
lfclk_config(void)
{
  nrfx_err_t err_code = nrfx_clock_init(clock_handler);

  if(err_code != NRFX_SUCCESS) {
    return;
  }

  nrfx_clock_enable();

  nrfx_clock_lfclk_start();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Function initialization and configuration of RTC driver instance.
 */
static void
rtc_config(void)
{
  /*Initialize RTC instance */
  nrfx_rtc_config_t config = NRFX_RTC_DEFAULT_CONFIG;
  config.prescaler = RTC_FREQ_TO_PRESCALER(32768); /* full speed... */
  config.interrupt_priority = 6;
  config.reliable = 0;

  nrfx_rtc_init(&rtc, &config, rtc_handler);
}
/*---------------------------------------------------------------------------*/
void
soc_rtc_init(void)
{
  uint32_t next;

  lfclk_config();
  rtc_config();

  rtc_max_clock_ticks = nrfx_rtc_max_ticks_get(&rtc) / COMPARE_INCREMENT;
  rtc_max_rtimer_ticks = nrfx_rtc_max_ticks_get(&rtc);
  /* lets handle the overflow  */
  nrfx_rtc_overflow_enable(&rtc, true);

  /* Tick not used */
  nrfx_rtc_tick_disable(&rtc);

  /*Power on RTC instance */
  nrfx_rtc_enable(&rtc);

  next = (nrfx_rtc_counter_get(&rtc) + COMPARE_INCREMENT) & MULTIPLE_256_MASK;
  nrfx_rtc_cc_set(&rtc, SOC_RTC_SYSTEM_CH, next, true);
}
/*---------------------------------------------------------------------------*/
void
soc_rtc_schedule_one_shot(uint32_t channel, rtimer_clock_t ticks)
{
  clock_time_t next = (clock_time_t)ticks; // % rtc_max_rtimer_ticks;
  if(channel == SOC_RTC_SYSTEM_CH) {
    /* system/etimeer tick will be triggered only every CLOCK_SECOND tick 1/128 */
    nrfx_rtc_cc_set(&rtc, channel, next & MULTIPLE_256_MASK, true);
  } else {
    /* rtimer schedules will be triggered exactley when they are required */
    nrfx_rtc_cc_set(&rtc, channel, next, true);
  }
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
soc_rtc_get_rtimer_ticks()
{
  /* RTC is a 24 bit counter, so we need to handle the overflow */
  return (rtc_max_rtimer_ticks * overflow) +
    (rtimer_clock_t)nrfx_rtc_counter_get(&rtc);
}
/*---------------------------------------------------------------------------*/
clock_time_t
soc_rtc_get_clock_ticks()
{
  return soc_rtc_get_rtimer_ticks() / COMPARE_INCREMENT;
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
soc_rtc_last_isr_time(void)
{
  return last_isr_time;
}
/*---------------------------------------------------------------------------*/
/** @} */
