/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
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
 * \addtogroup cc26xx-platforms
 * @{
 *
 * \defgroup cc26xx-srf-tag SmartRF+CC13xx/CC26xx EM, SensorTags and LaunchPads
 *
 * This platform supports a number of different boards:
 * - A standard TI SmartRF06EB with a CC26xx EM mounted on it
 * - A standard TI SmartRF06EB with a CC1310 EM mounted on it
 * - The TI CC2650 SensorTag
 * - The TI CC1350 SensorTag
 * - The TI CC2650 LaunchPad
 * - The TI CC1310 LaunchPad
 * - The TI CC1350 LaunchPad
 * @{
 */
#include "ti-lib.h"
#include "contiki.h"
#include "contiki-net.h"
#include "lpm.h"
#include "dev/leds.h"
#include "dev/gpio-hal.h"
#include "dev/oscillators.h"
#include "ieee-addr.h"
#include "ble-addr.h"
#include "vims.h"
#include "dev/cc26xx-uart.h"
#include "dev/soc-rtc.h"
#include "dev/serial-line.h"
#include "dev/soc-trng.h"
#include "rf-core/rf-core.h"
#include "sys_ctrl.h"
#include "uart.h"
#include "sys/clock.h"
#include "sys/rtimer.h"
#include "sys/node-id.h"
#include "sys/platform.h"
#include "lib/random.h"
#include "lib/sensors.h"
#include "button-sensor.h"
#include "dev/serial-line.h"
#include "dev/button-hal.h"
#include "net/mac/framer/frame802154.h"
#include "board-peripherals.h"

#include "driverlib/driverlib_release.h"

#include <stdio.h>
/*---------------------------------------------------------------------------*/
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "CC26x0/CC13x0"
#define LOG_LEVEL LOG_LEVEL_MAIN
/*---------------------------------------------------------------------------*/
/** \brief Board specific iniatialisation */
void board_init(void);
/*---------------------------------------------------------------------------*/
#ifdef BOARD_CONF_HAS_SENSORS
#define BOARD_HAS_SENSORS BOARD_CONF_HAS_SENSORS
#else
#define BOARD_HAS_SENSORS 1
#endif
/*---------------------------------------------------------------------------*/
static void
fade(leds_mask_t l)
{
  volatile int i;
  int k, j;
  for(k = 0; k < 800; ++k) {
    j = k > 400 ? 800 - k : k;

    leds_on(l);
    for(i = 0; i < j; ++i) {
      __asm("nop");
    }
    leds_off(l);
    for(i = 0; i < 400 - j; ++i) {
      __asm("nop");
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
set_rf_params(void)
{
  uint8_t ext_addr[8];

#if MAC_CONF_WITH_BLE
  ble_eui64_addr_cpy_to((uint8_t *)&ext_addr);
  NETSTACK_RADIO.set_object(RADIO_PARAM_64BIT_ADDR, ext_addr, 8);
#else
  uint16_t short_addr;
  ieee_addr_cpy_to(ext_addr, 8);

  short_addr = ext_addr[7];
  short_addr |= ext_addr[6] << 8;

  NETSTACK_RADIO.set_value(RADIO_PARAM_PAN_ID, IEEE802154_PANID);
  NETSTACK_RADIO.set_value(RADIO_PARAM_16BIT_ADDR, short_addr);
  NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, IEEE802154_DEFAULT_CHANNEL);
  NETSTACK_RADIO.set_object(RADIO_PARAM_64BIT_ADDR, ext_addr, 8);
#endif
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_one()
{
  /* Enable flash cache and prefetch. */
  ti_lib_vims_mode_set(VIMS_BASE, VIMS_MODE_ENABLED);
  ti_lib_vims_configure(VIMS_BASE, true, true);

  ti_lib_int_master_disable();

  /* Set the LF XOSC as the LF system clock source */
  oscillators_select_lf_xosc();

  lpm_init();

  board_init();

  gpio_hal_init();

  leds_init();
  fade(LEDS_RED);

  /*
   * Disable I/O pad sleep mode and open I/O latches in the AON IOC interface
   * This is only relevant when returning from shutdown (which is what froze
   * latches in the first place. Before doing these things though, we should
   * allow software to first regain control of pins
   */
  ti_lib_aon_ioc_freeze_disable();
  HWREG(AON_SYSCTL_BASE + AON_SYSCTL_O_SLEEPCTL) = 1;
  ti_lib_sys_ctrl_aon_sync();

  ti_lib_int_enable(INT_AON_GPIO_EDGE);
  ti_lib_int_master_enable();

  soc_rtc_init();
  fade(LEDS_YELLOW);
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_two()
{
  soc_trng_init();

  /* Use TRNG to seed random lib. */
  random_init((uint16_t)soc_trng_rand_synchronous());

  /* Character I/O Initialisation */
#if TI_UART_CONF_ENABLE
  cc26xx_uart_init();
#endif

  serial_line_init();

#if BUILD_WITH_SHELL
  cc26xx_uart_set_input(serial_line_input_byte);
#endif

  /* Populate linkaddr_node_addr */
#if MAC_CONF_WITH_BLE
  uint8_t ext_addr[8];
  ble_eui64_addr_cpy_to((uint8_t *)&ext_addr);
  memcpy(&linkaddr_node_addr, &ext_addr[8 - LINKADDR_SIZE], LINKADDR_SIZE);
#else
  ieee_addr_cpy_to(linkaddr_node_addr.u8, LINKADDR_SIZE);
#endif

  button_hal_init();

  fade(LEDS_GREEN);
}
/*---------------------------------------------------------------------------*/
void
platform_init_stage_three()
{
  radio_value_t chan = 0;
  radio_value_t pan = 0;

  set_rf_params();

  NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &chan);

  LOG_DBG("With DriverLib v%u.%u\n", DRIVERLIB_RELEASE_GROUP,
          DRIVERLIB_RELEASE_BUILD);
  LOG_INFO(BOARD_STRING "\n");
  LOG_DBG("IEEE 802.15.4: %s, Sub-GHz: %s, BLE: %s, Prop: %s\n",
          ti_lib_chipinfo_supports_ieee_802_15_4() == true ? "Yes" : "No",
          ti_lib_chipinfo_chip_family_is_cc13xx() == true ? "Yes" : "No",
          ti_lib_chipinfo_supports_ble() == true ? "Yes" : "No",
          ti_lib_chipinfo_supports_proprietary() == true ? "Yes" : "No");
  LOG_INFO(" RF: Channel %d", chan);

  if(NETSTACK_RADIO.get_value(RADIO_PARAM_PAN_ID, &pan) == RADIO_RESULT_OK) {
    LOG_INFO_(", PANID 0x%04X", pan);
  }
  LOG_INFO_("\n");

#if BOARD_HAS_SENSORS
  process_start(&sensors_process, NULL);
#endif

  fade(LEDS_ORANGE);
}
/*---------------------------------------------------------------------------*/
void
platform_idle()
{
  /* Drop to some low power mode */
  lpm_drop();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
