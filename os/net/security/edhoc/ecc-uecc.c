/*
 * Copyright (c) 2024, RISE Research Institutes of Sweden AB (RISE), Stockholm, Sweden
 * Copyright (c) 2020, Industrial Systems Institute (ISI), Patras, Greece
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
 */

/**
 * \file
 *         ecc-uecc interface the uecc SW library
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 */

#include "contiki.h"
#include "dev/watchdog.h"
#include "ecc-uecc.h"

/*----------------------------------------------------------------------------*/
#if EDHOC_ECC == EDHOC_ECC_UECC
static int
RNG(uint8_t *dest, unsigned size)
{
  while(size) {
    uint8_t val = (uint8_t)random_rand();
    *dest = val;
    ++dest;
    --size;
  }
  /* TODO: it would be a good idea to hash the resulting random data using SHA-256 or similar. */
  return 1;
}
#endif /* EDHOC_ECC == EDHOC_ECC_UECC */
/*----------------------------------------------------------------------------*/
uint8_t
uecc_generate_key(ecc_key_t *key, ecc_curve_t curve)
{
  int er = 0;
  watchdog_periodic();
  uECC_set_rng(&RNG);
  uint8_t public_key[64];
  watchdog_periodic();
  er = uECC_make_key(public_key, key->priv, curve.curve);
  watchdog_periodic();
  memcpy(key->pub.x, public_key, 32);
  memcpy(key->pub.y, public_key + 32, 32);
  watchdog_periodic();
  return er;
}

/*----------------------------------------------------------------------------*//*TODO: Check further */
bool
uecc_generate_ikm(const uint8_t *gx_in, const uint8_t *gy_in,
                  const uint8_t *private_key, uint8_t *ikm, ecc_curve_t crv)
{
  uint8_t compressed[ECC_KEY_LEN + 1];
  static uint8_t pub[2 * ECC_KEY_LEN];
  uint8_t *gy;

  /* Prepare the compressed format (first byte is 0x03, followed by gx) */
  compressed[0] = 0x03;
  memcpy(compressed + 1, gx_in, ECC_KEY_LEN);    /* Use gx_in for input gx */

  /* Decompress */
  uECC_decompress(compressed, pub, crv.curve);
  gy = pub + ECC_KEY_LEN;

  /* Now prepare the public key using gx and gy */
  uint8_t public[2 * ECC_KEY_LEN];
  memcpy(public, gx_in, ECC_KEY_LEN);
  memcpy(public + ECC_KEY_LEN, gy, ECC_KEY_LEN);

  watchdog_periodic();
  bool er = uECC_shared_secret(public, private_key, ikm, crv.curve) != 0;
  watchdog_periodic();

  return er;
}
/*----------------------------------------------------------------------------*/
