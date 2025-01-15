/*
 * Copyright (c) 2022, Andreas Urke
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
 */

/**
 * \file
 *         Unit tests for the small PRNG library.
 * \author
 *         Andreas Urke
 */

#include "contiki.h"
#include "lib/smallprng.h"

#include "unit-test/unit-test.h"

#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
PROCESS(test_smallprng_process, "Test small-PRNG library");
AUTOSTART_PROCESSES(&test_smallprng_process);
/*---------------------------------------------------------------------------*/
#define RAND_BUF_SIZE 128
/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(output_sanity, "Basic sanity of the output");
UNIT_TEST(output_sanity)
{
  UNIT_TEST_BEGIN();

  smallprng_t smallprng1 = {0};
  uint32_t seed = 1;

  /* Test 1: Verify all output is not identical */
  smallprng_init(&smallprng1, seed);
  uint32_t rand_output1[RAND_BUF_SIZE] = {0};
  for(int i = 0; i < RAND_BUF_SIZE; i++) {
    rand_output1[i] = smallprng_rand(&smallprng1);
  }

  bool identical = true;
  for(int i = 1; i < RAND_BUF_SIZE; i++) {
    if(rand_output1[i-1] != rand_output1[i]) {
      identical = false;
      break;
    }
  }

  UNIT_TEST_ASSERT(!identical);


  /* Test 2: Verify output is identical for identical seed */
  smallprng_t smallprng2 = {0};
  smallprng_init(&smallprng2, seed);
  uint32_t rand_output2[RAND_BUF_SIZE] = {0};
  for(int i = 0; i < RAND_BUF_SIZE; i++) {
    rand_output2[i] = smallprng_rand(&smallprng2);
  }

  UNIT_TEST_ASSERT(!memcmp(rand_output1, rand_output2, RAND_BUF_SIZE));


  /* Test 3: Verify output is different for different seed */
  seed = 2;
  smallprng_init(&smallprng2, seed);
  for(int i = 0; i < RAND_BUF_SIZE; i++) {
    rand_output2[i] = smallprng_rand(&smallprng2);
  }

  UNIT_TEST_ASSERT(memcmp(rand_output1, rand_output2, RAND_BUF_SIZE));

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(validate_output, "Validate output against python impl.");
UNIT_TEST(validate_output)
{
  UNIT_TEST_BEGIN();

  smallprng_t smallprng = {0};
  uint32_t seed = 1;

  smallprng_init(&smallprng, seed);

  /*
   * Verify output matches open-source Python implementation
   * https://gist.github.com/tjoneslo/2c3b472f641bab4069b7
   */
  uint32_t rand_output = smallprng_rand(&smallprng);
  UNIT_TEST_ASSERT(rand_output == 2723230452);

  rand_output = smallprng_rand(&smallprng);
  UNIT_TEST_ASSERT(rand_output == 519702369);

  rand_output = smallprng_rand(&smallprng);
  UNIT_TEST_ASSERT(rand_output == 858478259);

  rand_output = smallprng_rand(&smallprng);
  UNIT_TEST_ASSERT(rand_output == 3517897607);

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(test_smallprng_process, ev, data)
{
  PROCESS_BEGIN();

  printf("Run unit-test\n");
  printf("---\n");

  UNIT_TEST_RUN(output_sanity);
  UNIT_TEST_RUN(validate_output);

  if(!UNIT_TEST_PASSED(output_sanity) ||
     !UNIT_TEST_PASSED(validate_output)) {
    printf("=check-me= FAILED\n");
    printf("---\n");
  }

  printf("=check-me= DONE\n");
  printf("---\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
