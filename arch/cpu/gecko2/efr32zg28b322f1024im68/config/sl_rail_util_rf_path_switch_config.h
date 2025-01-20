/***************************************************************************//**
 * @file sl_rail_util_rf_path_switch_config.h
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2023 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#ifndef SL_RAIL_UTIL_RF_PATH_SWITCH_CONFIG_H
#define SL_RAIL_UTIL_RF_PATH_SWITCH_CONFIG_H

#include "em_gpio.h"

#define SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_DISABLE (0U)
#define SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_COMBINE (1U)

// <<< Use Configuration Wizard in Context Menu >>>

// <h> RF Path Switch Configuration
// <o SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_MODE> RFPATH Switch Radio Active Mode
// <SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_DISABLE=> Do not AND RACL_ACTIVE PRS signal with GPIO outputs.
// <SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_COMBINE=> AND RACL_ACTIVE PRS signal with GPIO outputs.
// <i> Default: SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_COMBINE
#define SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_MODE SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_COMBINE
// </h>

// <<< end of configuration section >>>

// <<< sl:start pin_tool >>>

// <gpio label="SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE"> SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE
// $[GPIO_SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE]
#ifndef SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_PORT
#define SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_PORT gpioPortD
#endif
#ifndef SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_PIN
#define SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE_PIN 2
#endif
// [GPIO_SL_RAIL_UTIL_RF_PATH_SWITCH_RADIO_ACTIVE]$

// <gpio label="SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL"> SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL
// $[GPIO_SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL]
#ifndef SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL_PORT
#define SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL_PORT gpioPortC
#endif
#ifndef SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL_PIN 
#define SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL_PIN  0
#endif
// [GPIO_SL_RAIL_UTIL_RF_PATH_SWITCH_CONTROL]$

// <<< sl:end pin_tool >>>
#endif
