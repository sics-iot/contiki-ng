/***************************************************************************//**
 * @file
 * @brief
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
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

#ifndef SL_RAIL_UTIL_DMA_CONFIG_H
#define SL_RAIL_UTIL_DMA_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

// <h> DMA Configuration
// <e SL_RAIL_UTIL_DMA_ENABLE> Allocate DMA channel to RAIL (to decrease channel config switching time)
// <i> Default: 1
#define SL_RAIL_UTIL_DMA_ENABLE  1
// <q SL_RAIL_UTIL_DMA_DMADRV_ENABLE> Use DMA Driver (i.e., auto-select DMA channel)
// <i> Default: 1
#define SL_RAIL_UTIL_DMA_DMADRV_ENABLE  1
// <o SL_RAIL_UTIL_DMA_CHANNEL> Use Specific DMA Channel (if DMA driver not used)
// <0-16:1>
// <i> Default: 0
#define SL_RAIL_UTIL_DMA_CHANNEL  0
// </e>
// </h>

// <<< end of configuration section >>>

#endif // SL_RAIL_UTIL_DMA_CONFIG_H
