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
 * \addtogroup gecko-dev Device drivers
 * @{
 *
 * \addtogroup gecko-watchdog Watchdog driver
 * @{
 *
 * \file
 *         Watchdog implementation for the gecko2.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *         Yevgen Gyl <yevgen.gyl@tii.ae>  
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "em_device.h"
#include "em_wdog.h"
#include "em_cmu.h"
#include "em_rmu.h"
/*---------------------------------------------------------------------------*/
void
watchdog_init(void)
{
  /* Enable LE interface */
#if !defined(_SILICON_LABS_32B_SERIES_2)
  CMU_ClockEnable(cmuClock_HFLE, true);
  CMU_OscillatorEnable(cmuOsc_LFRCO, true, true);
#endif

#if defined(_SILICON_LABS_32B_SERIES_2_CONFIG_8)
  CMU_ClockEnable(cmuClock_WDOG0, true);
#endif

  /* Make sure FULL reset is used on WDOG timeout */
#if defined(_RMU_CTRL_WDOGRMODE_MASK)
  RMU_ResetControl(rmuResetWdog, rmuResetModeFull);
#endif

  WDOG_Init_TypeDef init = WDOG_INIT_DEFAULT;
  init.enable = false;

#if defined(_WDOG_CTRL_CLKSEL_MASK)
  init.clkSel = wdogClkSelLFRCO;
#else
  /* Series 2 devices select watchdog oscillator with the CMU. */
  CMU_ClockSelectSet(cmuClock_WDOG0, cmuSelect_LFRCO);
#endif

  WDOGn_Init(WDOG0, &init);
}
/*---------------------------------------------------------------------------*/
void
watchdog_start(void)
{
  WDOGn_Enable(WDOG0, true); 
}
/*---------------------------------------------------------------------------*/
void
watchdog_periodic(void)
{
#if defined(_CMU_HFBUSCLKEN0_LE_MASK)
  if((CMU->HFBUSCLKEN0 & _CMU_HFBUSCLKEN0_LE_MASK) != 0) {
    WDOGn_Feed(DEFAULT_WDOG);
  }
#elif defined(_CMU_CLKEN0_WDOG0_MASK)
  if((CMU->CLKEN0 & _CMU_CLKEN0_WDOG0_MASK) != 0) {
    WDOGn_Feed(WDOG0);
  }
#else
  WDOGn_Feed(WDOG0);
#endif
}
/*---------------------------------------------------------------------------*/
void
watchdog_reboot(void)
{
  watchdog_start();

  while(1);
}
/**
 * @}
 * @}
 * @}
 */
