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
 *     GPIO HAL implementation for the Gecko
 * \author
 *     Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "dev/gpio-hal.h"

#include "em_gpio.h"
#include "em_core.h"
#include "em_cmu.h"
#include "gpiointerrupt.h"
/*---------------------------------------------------------------------------*/
typedef struct {
  gpio_hal_port_t port;
  gpio_hal_pin_t pin;
  gpio_hal_pin_cfg_t cfg;
} gpio_hal_cache_t;
/*---------------------------------------------------------------------------*/
#define GPIO_HAL_CACHE_SIZE 32
/*---------------------------------------------------------------------------*/
static gpio_hal_cache_t gpio_hal_cache[GPIO_HAL_CACHE_SIZE] = { 0 };
/*---------------------------------------------------------------------------*/
static void
gpio_hal_arch_callback(uint8_t interrupt_no, void *ctx)
{
  (void)ctx;
  gpio_hal_event_handler(gpio_hal_cache[interrupt_no].port,
                         1 << gpio_hal_cache[interrupt_no].pin);
}
/*---------------------------------------------------------------------------*/
static uint8_t
gpio_hal_arch_cache_lookup(gpio_hal_port_t port, gpio_hal_pin_t pin)
{
  /* Logic extracted from GPIOINT_CallbackRegisterExt */
#if defined(_GPIO_EXTIPINSELL_MASK)
  uint32_t intToCheck;
  uint32_t intGroupStart = (pin & 0xFFC);

  for(uint8_t i = 0; i < 4; i++) {
    intToCheck = intGroupStart + ((pin + i) & 0x3);
    if(gpio_hal_cache[i].port == port &&
       gpio_hal_cache[i].pin == pin) {
      return (uint8_t)intToCheck;
    }
  }
#else
  if(gpioCallbacks[pin].callback == 0) {
    return (uint8_t)pin;
  }
#endif
  return INTERRUPT_UNAVAILABLE;
}
/*---------------------------------------------------------------------------*/
void
gpio_hal_arch_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);

  GPIOINT_Init();
}
/*---------------------------------------------------------------------------*/
void
gpio_hal_arch_port_pin_cfg_set(gpio_hal_port_t port, gpio_hal_pin_t pin,
                               gpio_hal_pin_cfg_t cfg)
{
  unsigned int interrupt;
  GPIO_Mode_TypeDef def;
  def = GPIO_PinModeGet(port, pin);

  GPIO_PinModeSet(port, pin, def, (cfg & GPIO_HAL_PIN_CFG_PULL_UP) ? 1 : 0);

  interrupt = GPIOINT_CallbackRegisterExt(pin,
                                          (GPIOINT_IrqCallbackPtrExt_t)gpio_hal_arch_callback,
                                          NULL);
  gpio_hal_cache[interrupt].port = port;
  gpio_hal_cache[interrupt].pin = pin;
  gpio_hal_cache[interrupt].cfg = cfg;
  GPIO_ExtIntConfig(port, pin, interrupt,
                    (cfg & GPIO_HAL_PIN_CFG_EDGE_RISING) != 0,
                    (cfg & GPIO_HAL_PIN_CFG_EDGE_FALLING) != 0,
                    (cfg & GPIO_HAL_PIN_CFG_INT_ENABLE) != 0);
}
/*---------------------------------------------------------------------------*/
gpio_hal_pin_cfg_t
gpio_hal_arch_port_pin_cfg_get(gpio_hal_port_t port, gpio_hal_pin_t pin)
{
  uint8_t index;

  index = gpio_hal_arch_cache_lookup(port, pin);
  if(index != INTERRUPT_UNAVAILABLE) {
    return gpio_hal_cache[index].cfg;
  }

  return 0;
}
/*---------------------------------------------------------------------------*/
void
gpio_hal_arch_port_interrupt_enable(gpio_hal_port_t port, gpio_hal_pin_t pin)
{
  uint8_t index;

  index = gpio_hal_arch_cache_lookup(port, pin);
  if(index != INTERRUPT_UNAVAILABLE) {
    GPIO_IntEnable(index);
  }
}
/*---------------------------------------------------------------------------*/
void
gpio_hal_arch_port_interrupt_disable(gpio_hal_port_t port, gpio_hal_pin_t pin)
{
  uint8_t index;

  index = gpio_hal_arch_cache_lookup(port, pin);
  if(index != INTERRUPT_UNAVAILABLE) {
    GPIO_IntDisable(index);
  }
}
/*---------------------------------------------------------------------------*/
void
gpio_hal_arch_port_set_pins(gpio_hal_port_t port, gpio_hal_pin_mask_t pins)
{
  uint32_t val;

  val = GPIO_PortOutGet(port);
  val |= pins;
  GPIO_PortOutSet(port, val);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
