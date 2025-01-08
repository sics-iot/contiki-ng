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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "contiki.h"
#include "unit-test.h"
#include "lib/ecc.h"
#include <stdio.h>
#include <string.h>

#define ECC_CURVE (&ecc_curve_p_256)
#define ECC_CURVE_SIZE (ECC_CURVE_P_256_SIZE)

PROCESS(test_process, "test");
AUTOSTART_PROCESSES(&test_process);

/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(ecc_compress, "Compress and decompress a public key");
UNIT_TEST(ecc_compress)
{
  int result;
  static uint8_t public_key[ECC_CURVE_SIZE * 2];
  static uint8_t private_key[ECC_CURVE_SIZE];
  static uint8_t compressed_public_key[ECC_CURVE_SIZE + 1];
  uint8_t uncompressed_public_key[ECC_CURVE_SIZE * 2];

  UNIT_TEST_BEGIN();

  /* enable ECC driver */
  PT_WAIT_UNTIL(&unit_test_pt, process_mutex_try_lock(ECC.get_mutex()));
  UNIT_TEST_ASSERT(!ECC.enable(ECC_CURVE));

  /* generate key pair */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.generate_key_pair(public_key, private_key, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* compress & decompress */
  ECC.compress_public_key(public_key, compressed_public_key);
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.decompress_public_key(compressed_public_key,
                                     uncompressed_public_key,
                                     &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* disable ECC driver */
  ECC.disable();

  /* check */
  UNIT_TEST_ASSERT(
    !memcmp(public_key, uncompressed_public_key, sizeof(public_key)));

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(ecc_ecdh, "ECDH");
UNIT_TEST(ecc_ecdh)
{
  int result;
  static uint8_t public_key_a[ECC_CURVE_SIZE * 2];
  static uint8_t private_key_a[ECC_CURVE_SIZE];
  static uint8_t public_key_b[ECC_CURVE_SIZE * 2];
  static uint8_t private_key_b[ECC_CURVE_SIZE];
  static uint8_t k_a[ECC_CURVE_SIZE];
  static uint8_t k_b[ECC_CURVE_SIZE];

  UNIT_TEST_BEGIN();

  /* enable ECC driver */
  PT_WAIT_UNTIL(&unit_test_pt, process_mutex_try_lock(ECC.get_mutex()));
  UNIT_TEST_ASSERT(!ECC.enable(ECC_CURVE));

  /* generate key pairs */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.generate_key_pair(public_key_a, private_key_a, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.generate_key_pair(public_key_b, private_key_b, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* validate public keys */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.validate_public_key(public_key_a, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.validate_public_key(public_key_b, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* run ECDH */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.generate_shared_secret(public_key_b,
                                      private_key_a,
                                      k_a,
                                      &result));
  if(result) {
    UNIT_TEST_FAIL();
  }
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.generate_shared_secret(public_key_a,
                                      private_key_b,
                                      k_b,
                                      &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* disable ECC driver */
  ECC.disable();

  /* check */
  UNIT_TEST_ASSERT(!memcmp(k_a, k_b, ECC_CURVE_SIZE));

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(ecc_ecdsa, "ECDSA");
UNIT_TEST(ecc_ecdsa)
{
  int result;
  static uint8_t hash[ECC_CURVE_SIZE] = { 0xFF };
  static uint8_t public_key[ECC_CURVE_SIZE * 2];
  static uint8_t private_key[ECC_CURVE_SIZE];
  static uint8_t signature[ECC_CURVE_SIZE * 2];

  UNIT_TEST_BEGIN();

  /* enable ECC driver */
  PT_WAIT_UNTIL(&unit_test_pt, process_mutex_try_lock(ECC.get_mutex()));
  UNIT_TEST_ASSERT(!ECC.enable(ECC_CURVE));

  /* generate key pair */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.generate_key_pair(public_key, private_key, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* validate public key */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.validate_public_key(public_key, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* sign */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.sign(hash, private_key, signature, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* verify */
  PT_SPAWN(&unit_test_pt,
           ECC.get_protothread(),
           ECC.verify(signature, hash, public_key, &result));
  if(result) {
    UNIT_TEST_FAIL();
  }

  /* disable ECC driver */
  ECC.disable();

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(test_process, ev, data)
{
  PROCESS_BEGIN();

  printf("Run unit-test\n");
  printf("---\n");

  UNIT_TEST_RUN(ecc_compress);
  UNIT_TEST_RUN(ecc_ecdh);
  UNIT_TEST_RUN(ecc_ecdsa);

  if(!UNIT_TEST_PASSED(ecc_compress)
      || !UNIT_TEST_PASSED(ecc_ecdh)
      || !UNIT_TEST_PASSED(ecc_ecdsa)) {
    printf("=check-me= FAILED\n");
    printf("---\n");
  }

  printf("=check-me= DONE\n");
  printf("---\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
