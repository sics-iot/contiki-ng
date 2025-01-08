/*
 * Copyright (c) 2021, Uppsala universitet.
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
 *
 */

/**
 * \file
 *         Demonstrates the compression and decompression of public keys.
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

#include "contiki.h"
#include "lib/ecc.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "compress"
#define LOG_LEVEL LOG_LEVEL_DBG

#define ECC_CURVE (&ecc_curve_p_256)
#define ECC_CURVE_SIZE (ECC_CURVE_P_256_SIZE)

PROCESS(compress_process, "compress_process");
AUTOSTART_PROCESSES(&compress_process);
static rtimer_clock_t t1, t2;

/*---------------------------------------------------------------------------*/
static uint64_t
get_milliseconds(void)
{
  uint64_t difference = t2 - t1;
  return (difference * 1000) / RTIMER_SECOND;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(compress_process, ev, data)
{
  int result;
  static uint8_t public_key[ECC_CURVE_SIZE * 2];
  static uint8_t private_key[ECC_CURVE_SIZE];
  static uint8_t compressed_public_key[ECC_CURVE_SIZE + 1];
  uint8_t uncompressed_public_key[ECC_CURVE_SIZE * 2];

  PROCESS_BEGIN();

  /* enable ECC driver */
  PROCESS_WAIT_UNTIL(process_mutex_try_lock(ECC.get_mutex()));
  if(ECC.enable(ECC_CURVE)) {
    LOG_ERR("enable failed\n");
    PROCESS_EXIT();
  }

  /* generate key pair */
  PROCESS_PT_SPAWN(ECC.get_protothread(),
                   ECC.generate_key_pair(public_key, private_key, &result));
  if(result) {
    LOG_ERR("generate_key_pair failed\n");
    PROCESS_EXIT();
  }

  /* compress & decompress */
  ECC.compress_public_key(public_key, compressed_public_key);
  t1 = RTIMER_NOW();
  PROCESS_PT_SPAWN(ECC.get_protothread(),
                   ECC.decompress_public_key(compressed_public_key,
                                             uncompressed_public_key,
                                             &result));
  t2 = RTIMER_NOW();
  if(result) {
    LOG_ERR("decompress_public_key failed\n");
    PROCESS_EXIT();
  }
  LOG_INFO("decompression took %" PRIu64 "ms\n", get_milliseconds());

  /* disable ECC driver */
  ECC.disable();

  /* check */
  if(memcmp(public_key, uncompressed_public_key, sizeof(public_key))) {
    LOG_ERR("unequal\n");
  } else {
    LOG_INFO("OK\n");
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
