/*
 * Copyright (C) 2024 Marcel Graber <marcel@clever.design>
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
 * \addtogroup nrf-clock Clock driver
 * @{
 *
 * \file
 *         Software clock implementation for the nRF.
 *
 * \author
 *         Marcel Graber <marcel@clever.design> (lowpower mode)
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "nrfx_config.h"
#include "nrfx_rtc.h"
#include "nrfx_clock.h"
#include "soc-rtc.h"

#if CLOCK_SIZE != 4
/* 64 bit variables may not be read atomically without extra handling */
#error CLOCK_CONF_SIZE must be 4 (32 bit)
#endif

#ifdef NRF_CLOCK_CONF_RTC_INSTANCE
#define NRF_CLOCK_RTC_INSTANCE NRF_CLOCK_CONF_RTC_INSTANCE
#else
#define NRF_CLOCK_RTC_INSTANCE 0
#endif

/*---------------------------------------------------------------------------*/
void
clock_update(void)
{
  if(etimer_pending() && !CLOCK_LT(clock_time(), etimer_next_expiration_time())) {
    etimer_request_poll();
  }
}
/*---------------------------------------------------------------------------*/
void
clock_init(void)
{
}
/*---------------------------------------------------------------------------*/
clock_time_t
clock_time(void)
{
  return soc_rtc_get_clock_ticks();
}
/*---------------------------------------------------------------------------*/
unsigned long
clock_seconds(void)
{
  return (unsigned long)(clock_time() / CLOCK_SECOND);
}
/*---------------------------------------------------------------------------*/
void
clock_wait(clock_time_t i)
{
  clock_time_t start = clock_time();
  while(clock_time() - start < i) {
    __WFE();
  }
}
/*---------------------------------------------------------------------------*/
void
clock_delay_usec(uint16_t dt)
{
  NRFX_DELAY_US(dt);
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
