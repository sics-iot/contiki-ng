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
 * \addtogroup gecko-uart UART driver
 * @{
 *
 * \file
 *         UART implementation for the gecko.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"

#include "uartdrv.h"
#include "sl_uartdrv_instances.h"
/*---------------------------------------------------------------------------*/
static int (*input_handler)(unsigned char c) = NULL;
static uint8_t uarte_buffer;
/*---------------------------------------------------------------------------*/
static void
receive_callback(UARTDRV_HandleData_t *handle, Ecode_t transferStatus,
                 uint8_t *data, UARTDRV_Count_t transferCount)
{
  UARTDRV_Count_t i;
  if(transferStatus == ECODE_EMDRV_UARTDRV_OK &&
     input_handler != NULL) {
    for(i = 0; i < transferCount; i++) {
      input_handler(data[i]);
    }
    UARTDRV_Receive(sl_uartdrv_usart_vcom_handle, &uarte_buffer, sizeof(uarte_buffer), receive_callback);
  }
}
/*---------------------------------------------------------------------------*/
void
uart_write(unsigned char *s, unsigned int len)
{
  UARTDRV_ForceTransmit(sl_uartdrv_usart_vcom_handle, s, len);
}
/*---------------------------------------------------------------------------*/
void
uart_set_input(int (*input)(unsigned char c))
{
  input_handler = input;

  if(input) {
    UARTDRV_Receive(sl_uartdrv_usart_vcom_handle, &uarte_buffer, sizeof(uarte_buffer), receive_callback);
  }
}
/*---------------------------------------------------------------------------*/
void
uart_init(void)
{
  sl_uartdrv_init_instances();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
