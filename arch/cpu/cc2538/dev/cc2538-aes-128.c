/*
 * Copyright (c) 2015, Hasso-Plattner-Institut.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \addtogroup cc2538-aes-128
 * @{
 *
 * \file
 *         Implementation of the AES-128 driver for the CC2538 SoC
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

#include "dev/cc2538-aes-128.h"
#include "dev/aes.h"
#include "dev/sys-ctrl.h"
#include "lib/assert.h"
#include <stdbool.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "cc2538-aes-128"
#define LOG_LEVEL LOG_LEVEL_NONE

/*---------------------------------------------------------------------------*/
static void
set_key(const uint8_t *key)
{
  bool was_crypto_enabled = CRYPTO_IS_ENABLED();
  if(!was_crypto_enabled) {
    crypto_enable();
  }

  /* all previous interrupts should have been acknowledged */
  assert(!REG(AES_CTRL_INT_STAT));

  /* set up AES interrupts */
  REG(AES_CTRL_INT_CFG) = AES_CTRL_INT_CFG_LEVEL;
  REG(AES_CTRL_INT_EN) = AES_CTRL_INT_EN_RESULT_AV;

  /* enable DMA path to the key store module */
  REG(AES_CTRL_ALG_SEL) = AES_CTRL_ALG_SEL_KEYSTORE;

  /* configure key store module (area, size) - note that setting
   * AES_KEY_STORE_SIZE to AES_KEY_STORE_SIZE_KEY_SIZE_128 is unnecessary
   * because AES_KEY_STORE_SIZE_KEY_SIZE_128 is the reset value. Moreover,
   * writing AES_KEY_STORE_SIZE would clear all other loaded keys. */
  /* clear key to write */
  REG(AES_KEY_STORE_WRITTEN_AREA) = 1 << CC2538_AES_128_KEY_AREA;
  /* enable key to write */
  REG(AES_KEY_STORE_WRITE_AREA) = 1 << CC2538_AES_128_KEY_AREA;

  /* configure DMAC */
  REG(AES_DMAC_CH0_CTRL) = AES_DMAC_CH_CTRL_EN; /* enable DMA channel 0 */
  uint64_t aligned_key[AES_128_KEY_LENGTH / sizeof(uint64_t)];
  memcpy(aligned_key, key, AES_128_KEY_LENGTH);
  /* set base address of the aligned key in external memory */
  REG(AES_DMAC_CH0_EXTADDR) = (uintptr_t)aligned_key;
  /* total key length in bytes (e.g. 16 for 1 x 128-bit key) */
  REG(AES_DMAC_CH0_DMALENGTH) = AES_128_KEY_LENGTH;

  /* wait for completion */
  while(!(REG(AES_CTRL_INT_STAT) & AES_CTRL_INT_STAT_RESULT_AV));

  /* acknowledge the interrupt */
  REG(AES_CTRL_INT_CLR) = AES_CTRL_INT_CLR_RESULT_AV;

  /* check for absence of errors in DMA and key store */
  uint32_t errors = REG(AES_CTRL_INT_STAT)
      & (AES_CTRL_INT_STAT_DMA_BUS_ERR | AES_CTRL_INT_STAT_KEY_ST_WR_ERR);
  if(errors) {
    LOG_ERR("error at line %d\n", __LINE__);
    /* clear errors */
    REG(AES_CTRL_INT_CLR) = errors;
    goto exit;
  }

  /* check that key was written */
  if(!(REG(AES_KEY_STORE_WRITTEN_AREA)
      & (1 << CC2538_AES_128_KEY_AREA))) {
    LOG_ERR("error at line %d\n", __LINE__);
    goto exit;
  }

exit:
  /* all interrupts should have been acknowledged */
  assert(!REG(AES_CTRL_INT_STAT));

  /* disable master control/DMA clock */
  REG(AES_CTRL_ALG_SEL) = 0;
  if(!was_crypto_enabled) {
    crypto_disable();
  }
}
/*---------------------------------------------------------------------------*/
static void
encrypt(uint8_t *plaintext_and_result)
{
  bool was_crypto_enabled = CRYPTO_IS_ENABLED();
  if(!was_crypto_enabled) {
    crypto_enable();
  }

  /* all previous interrupts should have been acknowledged */
  assert(!REG(AES_CTRL_INT_STAT));

  /* set up AES interrupts */
  REG(AES_CTRL_INT_CFG) = AES_CTRL_INT_CFG_LEVEL;
  REG(AES_CTRL_INT_EN) = AES_CTRL_INT_EN_RESULT_AV;

  /* enable the DMA path to the AES engine */
  REG(AES_CTRL_ALG_SEL) = AES_CTRL_ALG_SEL_AES;

  /* configure the key store to provide pre-loaded AES key */
  REG(AES_KEY_STORE_READ_AREA) = CC2538_AES_128_KEY_AREA;

  /* wait until the key is loaded to the AES module */
  while(REG(AES_KEY_STORE_READ_AREA) & AES_KEY_STORE_READ_AREA_BUSY);

  /* check if the key was loaded without errors */
  if(REG(AES_CTRL_INT_STAT) & AES_CTRL_INT_STAT_KEY_ST_RD_ERR) {
    LOG_ERR("error at line %d\n", __LINE__);
    /* clear error */
    REG(AES_CTRL_INT_CLR) = AES_CTRL_INT_STAT_KEY_ST_RD_ERR;
    goto exit;
  }

  /* configure AES engine */
  REG(AES_AES_CTRL) = AES_AES_CTRL_DIRECTION_ENCRYPT;
  REG(AES_AES_C_LENGTH_0) = AES_128_BLOCK_SIZE; /* write length of the message (lo) */
  REG(AES_AES_C_LENGTH_1) = 0; /* write length of the message (hi) */

  /* configure DMAC */
  /* enable DMA channel 0 */
  REG(AES_DMAC_CH0_CTRL) = AES_DMAC_CH_CTRL_EN;
  /* base address of the input data in external memory */
  REG(AES_DMAC_CH0_EXTADDR) = (uintptr_t)plaintext_and_result;
  /* length of the input data to be transferred */
  REG(AES_DMAC_CH0_DMALENGTH) = AES_128_BLOCK_SIZE;
  /* enable DMA channel 1 */
  REG(AES_DMAC_CH1_CTRL) = AES_DMAC_CH_CTRL_EN;
  /* base address of the output data in external memory */
  REG(AES_DMAC_CH1_EXTADDR) = (uintptr_t)plaintext_and_result;
  /* length of the output data to be transferred */
  REG(AES_DMAC_CH1_DMALENGTH) = AES_128_BLOCK_SIZE;

  /* wait for completion */
  while(!(REG(AES_CTRL_INT_STAT) & AES_CTRL_INT_STAT_RESULT_AV));

  /* acknowledge the interrupt */
  REG(AES_CTRL_INT_CLR) = AES_CTRL_INT_CLR_RESULT_AV;

  /* check for errors in DMA and key store */
  uint32_t errors = REG(AES_CTRL_INT_STAT)
      & (AES_CTRL_INT_STAT_DMA_BUS_ERR | AES_CTRL_INT_STAT_KEY_ST_RD_ERR);
  if(errors) {
    LOG_ERR("error at line %d\n", __LINE__);
    /* clear errors */
    REG(AES_CTRL_INT_CLR) = errors;
    goto exit;
  }

exit:
  /* all interrupts should have been acknowledged */
  assert(!REG(AES_CTRL_INT_STAT));

  /* disable master control/DMA clock */
  REG(AES_CTRL_ALG_SEL) = 0;
  if(!was_crypto_enabled) {
    crypto_disable();
  }
}
/*---------------------------------------------------------------------------*/
const struct aes_128_driver cc2538_aes_128_driver = {
  set_key,
  encrypt
};
/*---------------------------------------------------------------------------*/

/** @} */
