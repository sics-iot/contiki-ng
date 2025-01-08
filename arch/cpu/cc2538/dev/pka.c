/*
 * Original file:
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *
 * Port to Contiki:
 * Copyright (c) 2014 Andreas Dröscher <contiki@anticat.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
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
 * \addtogroup cc2538-pka
 * @{
 *
 * \file
 * Implementation of the cc2538 PKA engine driver
 */
#include "contiki.h"
#include "dev/pka.h"
#include "dev/sys-ctrl.h"
#include "dev/nvic.h"
#include "lpm.h"
#include "reg.h"

#include <stdbool.h>
#include <stdint.h>

static volatile struct process *notification_process;
/*---------------------------------------------------------------------------*/
/** \brief The PKA engine ISR
 *
 *        This is the interrupt service routine for the PKA engine.
 *
 *        This ISR is called at worst from PM0, so lpm_exit() does not need
 *        to be called.
 */
void
pka_isr(void)
{
  NVIC_ClearPendingIRQ(PKA_IRQn);
  NVIC_DisableIRQ(PKA_IRQn);

  if(notification_process != NULL) {
    process_poll((struct process *)notification_process);
    notification_process = NULL;
  }
}
/*---------------------------------------------------------------------------*/
static bool
permit_pm1(void)
{
  return (REG(PKA_FUNCTION) & PKA_FUNCTION_RUN) == 0;
}
/*---------------------------------------------------------------------------*/
void
pka_init(void)
{
  volatile int i;

  lpm_register_peripheral(permit_pm1);

  pka_enable();

  /* Reset the PKA engine */
  REG(SYS_CTRL_SRSEC)   |= SYS_CTRL_SRSEC_PKA;
  for(i = 0; i < 16; i++) {
    REG(SYS_CTRL_SRSEC) &= ~SYS_CTRL_SRSEC_PKA;
  }
}
/*---------------------------------------------------------------------------*/
void
pka_enable(void)
{
  /* Enable the clock for the PKA engine */
  REG(SYS_CTRL_RCGCSEC) |= SYS_CTRL_RCGCSEC_PKA;
  REG(SYS_CTRL_SCGCSEC) |= SYS_CTRL_SCGCSEC_PKA;
  REG(SYS_CTRL_DCGCSEC) |= SYS_CTRL_DCGCSEC_PKA;
}
/*---------------------------------------------------------------------------*/
void
pka_disable(void)
{
  /* Gate the clock for the PKA engine */
  REG(SYS_CTRL_RCGCSEC) &= ~SYS_CTRL_RCGCSEC_PKA;
  REG(SYS_CTRL_SCGCSEC) &= ~SYS_CTRL_SCGCSEC_PKA;
  REG(SYS_CTRL_DCGCSEC) &= ~SYS_CTRL_DCGCSEC_PKA;
}
/*---------------------------------------------------------------------------*/
uint8_t
pka_check_status(void)
{
  return (REG(PKA_FUNCTION) & PKA_FUNCTION_RUN) == 0;
}
/*---------------------------------------------------------------------------*/
void
pka_register_process_notification(struct process *p)
{
  notification_process = p;
}
/*---------------------------------------------------------------------------*/
void
pka_run_function(uint32_t function)
{
  pka_register_process_notification(process_current);
  REG(PKA_FUNCTION) = PKA_FUNCTION_RUN | function;
  NVIC_ClearPendingIRQ(PKA_IRQn);
  NVIC_EnableIRQ(PKA_IRQn);
}
/*---------------------------------------------------------------------------*/
void
pka_little_endian_to_pka_ram(const uint32_t *words,
                             size_t num_words,
                             uintptr_t offset)
{
  offset *= sizeof(uint32_t);
  offset += PKA_RAM_BASE;
  for(size_t i = 0; i < num_words; i++) {
    REG(offset + sizeof(uint32_t) * i) = words[i];
  }
}
/*---------------------------------------------------------------------------*/
void
pka_word_to_pka_ram(uint32_t word, uintptr_t offset)
{
  offset *= sizeof(uint32_t);
  offset += PKA_RAM_BASE;
  REG(offset) = word;
}
/*---------------------------------------------------------------------------*/
uint32_t
pka_word_from_pka_ram(uintptr_t offset)
{
  offset *= sizeof(uint32_t);
  offset += PKA_RAM_BASE;
  return REG(offset);
}
/*---------------------------------------------------------------------------*/
void
pka_big_endian_to_pka_ram(const uint8_t *bytes,
                          size_t num_bytes,
                          uintptr_t offset)
{
  offset *= sizeof(uint32_t);
  offset += PKA_RAM_BASE;
  while(num_bytes) {
    uint32_t word = bytes[--num_bytes];
    word |= bytes[--num_bytes] << 8;
    word |= bytes[--num_bytes] << 16;
    word |= bytes[--num_bytes] << 24;
    REG(offset) = word;
    offset += sizeof(uint32_t);
  }
}
/*---------------------------------------------------------------------------*/
void
pka_big_endian_from_pka_ram(uint8_t *bytes,
                            size_t num_words,
                            uintptr_t offset)
{
  offset *= sizeof(uint32_t);
  offset += PKA_RAM_BASE;
  size_t remaining_bytes = num_words * sizeof(uint32_t);
  while(remaining_bytes) {
    uint32_t word = REG(offset);
    bytes[--remaining_bytes] = word;
    bytes[--remaining_bytes] = word >> 8;
    bytes[--remaining_bytes] = word >> 16;
    bytes[--remaining_bytes] = word >> 24;
    offset += sizeof(uint32_t);
  }
}
/*---------------------------------------------------------------------------*/

/** @} */
