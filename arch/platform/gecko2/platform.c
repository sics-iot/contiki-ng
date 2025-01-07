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
 * \addtogroup gecko-platforms
 * @{
 *
 * \file
 *      Platform implementation for gecko
 * \author
 *      Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "em_chip.h"
#include "sl_device_init_nvic.h"
#include "sl_board_init.h"
#include "sl_device_init_dcdc.h"
#include "sl_device_init_hfxo.h"
#include "sl_device_init_lfxo.h"
#include "sl_device_init_clocks.h"
#include "sl_device_init_emu.h"
#include "sl_board_control.h"
#include "sl_power_manager.h"
#include "sl_sleeptimer.h"

#include "dev/gpio-hal.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include "lib/random.h"
#include "dev/uart-arch.h"
#include "sys/linkaddr-arch.h"
/*---------------------------------------------------------------------------*/
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "GECKO2"
#define LOG_LEVEL LOG_LEVEL_MAIN
/*---------------------------------------------------------------------------*/
static void
sl_platform_init_stage_one(void)
{
  CHIP_Init();
  sl_device_init_nvic();
  sl_board_preinit();
  sl_device_init_dcdc();
  sl_device_init_hfxo();
  sl_device_init_lfxo();
  sl_device_init_clocks();
  sl_device_init_emu();
  sl_board_init();
  sl_power_manager_init();
}
/*---------------------------------------------------------------------------*/
static void
sl_platform_init_stage_two(void)
{
  sl_board_configure_vcom();
  sl_sleeptimer_init();
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_one(void)
{
  sl_platform_init_stage_one();

  gpio_hal_init();
  leds_init();
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_two(void)
{
  sl_platform_init_stage_two();

  button_hal_init();

  random_init(0x5678);

  uart_init();
  serial_line_init();

#if BUILD_WITH_SHELL
  uart_set_input(serial_line_input_byte);
#endif /* BUILD_WITH_SHELL */

  populate_link_address();
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_three(void)
{
}
/*---------------------------------------------------------------------------*/
void
platform_idle()
{
  sl_power_manager_sleep();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
