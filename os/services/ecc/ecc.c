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
 */

/**
 * \addtogroup crypto
 * @{
 *
 * \file
 * Adapter for uECC
 */

#include "lib/ecc.h"
#include "lib/csprng.h"
#include "lib/sha-256.h"
#include "uECC.h"

static struct pt protothread;
static const ecc_curve_t *ecc_curve;
static uECC_Curve uecc_curve;
static process_mutex_t mutex;

/*---------------------------------------------------------------------------*/
static int
csprng_adapter(uint8_t *dest, unsigned size)
{
  return csprng_rand(dest, size);
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  process_mutex_init(&mutex);
  uECC_set_rng(csprng_adapter);
}
/*---------------------------------------------------------------------------*/
static process_mutex_t *
get_mutex(void)
{
  return &mutex;
}
/*---------------------------------------------------------------------------*/
static int
enable(const ecc_curve_t *c)
{
  if(c == &ecc_curve_p_256) {
    uecc_curve = uECC_secp256r1();
  } else if(c == &ecc_curve_p_192) {
    uecc_curve = uECC_secp192r1();
  } else {
    process_mutex_unlock(&mutex);
    return 1;
  }
  ecc_curve = c;
  return 0;
}
/*---------------------------------------------------------------------------*/
static struct pt *
get_protothread(void)
{
  return &protothread;
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(validate_public_key(const uint8_t *public_key,
                                     int *const result))
{
  PT_BEGIN(&protothread);

  *result = !uECC_valid_public_key(public_key, uecc_curve);

  PT_END(&protothread);
}
/*---------------------------------------------------------------------------*/
static void
compress_public_key(const uint8_t *uncompressed_public_key,
                    uint8_t *compressed_public_key)
{
  uECC_compress(uncompressed_public_key, compressed_public_key, uecc_curve);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(decompress_public_key(const uint8_t *compressed_public_key,
                                       uint8_t *uncompressed_public_key,
                                       int *const result))
{
  PT_BEGIN(&protothread);

  uECC_decompress(compressed_public_key,
                  uncompressed_public_key,
                  uecc_curve);
  *result = 0;

  PT_END(&protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(sign(const uint8_t *message_hash,
                      const uint8_t *private_key,
                      uint8_t *signature,
                      int *const result))
{
  PT_BEGIN(&protothread);

  *result = !uECC_sign(private_key,
                       message_hash,
                       ecc_curve->bytes,
                       signature,
                       uecc_curve);

  PT_END(&protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(verify(const uint8_t *signature,
                        const uint8_t *message_hash,
                        const uint8_t *public_key,
                        int *const result))
{
  PT_BEGIN(&protothread);

  *result = !uECC_verify(public_key,
                         message_hash,
                         ecc_curve->bytes,
                         signature,
                         uecc_curve);

  PT_END(&protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(generate_key_pair(uint8_t *public_key,
                                   uint8_t *private_key,
                                   int *const result))
{
  PT_BEGIN(&protothread);

  *result = !uECC_make_key(public_key,
                           private_key,
                           uecc_curve);

  PT_END(&protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(generate_shared_secret(const uint8_t *public_key,
                                        const uint8_t *private_key,
                                        uint8_t *shared_secret,
                                        int *const result))
{
  PT_BEGIN(&protothread);

  *result = !uECC_shared_secret(public_key,
                                private_key,
                                shared_secret,
                                uecc_curve);

  PT_END(&protothread);
}
/*---------------------------------------------------------------------------*/
static void
disable(void)
{
  process_mutex_unlock(&mutex);
}
/*---------------------------------------------------------------------------*/
const struct ecc_driver ecc_driver = {
  init,
  get_mutex,
  enable,
  get_protothread,
  validate_public_key,
  compress_public_key,
  decompress_public_key,
  sign,
  verify,
  generate_key_pair,
  generate_shared_secret,
  disable
};
/*---------------------------------------------------------------------------*/

/** @} */
