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

/**
 * \addtogroup gecko
 * @{
 *
 * \addtogroup gecko-dev Device drivers
 * @{
 *
 * \addtogroup gecko-gpio GPIO HAL driver
 * @{
 *
 * \file
 *     GPIO HAL header file for the gecko
 * \author
 *     Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#ifndef GPIO_HAL_ARCH_H_
#define GPIO_HAL_ARCH_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "em_gpio.h"
/*---------------------------------------------------------------------------*/
#define gpio_hal_arch_pin_set_input(port, pin) \
  GPIO_PinModeSet(port, pin, gpioModeInput, 0u)
#define gpio_hal_arch_pin_set_output(port, pin) \
  GPIO_PinModeSet(port, pin, gpioModePushPull, 0u)
/*---------------------------------------------------------------------------*/
#define gpio_hal_arch_set_pin(port, pin) \
  GPIO_PinOutSet(port, pin)
#define gpio_hal_arch_clear_pin(port, pin) \
  GPIO_PinOutClear(port, pin)
#define gpio_hal_arch_toggle_pin(port, pin) \
  GPIO_PinOutToggle(port, pin)
#define gpio_hal_arch_write_pin(port, pin, v) \
  GPIO_PortOutSetVal(port, v, pin)
#define gpio_hal_arch_read_pin(port, pin) \
  GPIO_PinInGet(port, pin)
#define gpio_hal_arch_write_pins(port, pins, value) \
  GPIO_PortOutSetVal(port, value, pins)
#define gpio_hal_arch_read_pins(port, pins) \
  (GPIO_PortOutGet(port) & (pins))
#define gpio_hal_arch_toggle_pins(port, pins) \
  GPIO_PortOutToggle(port, pins)
#define gpio_hal_arch_clear_pins(port, pins) \
  GPIO_PortOutClear(port, pins)
/*---------------------------------------------------------------------------*/
#endif /* GPIO_HAL_ARCH_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
