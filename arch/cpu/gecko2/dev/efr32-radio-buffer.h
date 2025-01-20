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
#include "rail.h"

#define RAIL_FIFO_SIZE      (128)

typedef struct {
  RAIL_Time_t timestamp;
  int16_t rssi;
  int16_t lqi;
  uint8_t len;
  uint8_t buf[RAIL_FIFO_SIZE];
} rx_buffer_t;

int has_packet(void);
rx_buffer_t * get_full_rx_buf(void);
rx_buffer_t * get_empty_rx_buf(void);
void free_rx_buf(rx_buffer_t *rx_buf);

/*---------------------------------------------------------------------------*/
#endif /* GPIO_HAL_ARCH_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
