/*
 * Copyright (c) 2024, Konrad-Felix Krentz.
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
 *         Benchmarks AES-128 drivers.
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

#include "contiki.h"
#include "lib/aes-128.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "speed-test"
#define LOG_LEVEL LOG_LEVEL_DBG

PROCESS(speed_test_process, "speed_test_process");
AUTOSTART_PROCESSES(&speed_test_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(speed_test_process, ev, data)
{
  const uint8_t key[AES_128_KEY_LENGTH] = { 0x00, 0x01, 0x02, 0x03,
                                            0x04, 0x05, 0x06, 0x07,
                                            0x08, 0x09, 0x0A, 0x0B,
                                            0x0C, 0x0D, 0x0E, 0x0F };
  uint8_t block[AES_128_BLOCK_SIZE] = { 0x00, 0x11, 0x22, 0x33,
                                        0x44, 0x55, 0x66, 0x77,
                                        0x88, 0x99, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF };

  PROCESS_BEGIN();

  rtimer_clock_t t1 = RTIMER_NOW();
  for(unsigned i = 0; i < 1000; i++) {
    AES_128.set_key(key);
    AES_128.encrypt(block);
  }
  rtimer_clock_t t2 = RTIMER_NOW();
  LOG_INFO("%" PRIu64 "ms\n", (((uint64_t)(t2 - t1)) * 1000) / RTIMER_SECOND);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
