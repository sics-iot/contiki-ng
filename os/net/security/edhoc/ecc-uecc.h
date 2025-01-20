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
 *         ecc-uecc headers
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 */
#ifndef _ECC_UECC_H_
#define _ECC_UECC_H_

#include <stdint.h>
#include "edhoc-config.h"
#include "edhoc-log.h"

#include "lib/random.h"
#include "sys/rtimer.h"
#include "sys/pt.h"
#include <uECC.h>
#include <string.h>
#include <stdio.h>
#include "ecc-common.h"

typedef struct ecc_curve {
  uECC_Curve curve;
} ecc_curve_t;

/**
 * \brief Generate an ECC key pair using the uECC library
 * \param key The ECC key structure where the generated public and private keys will be stored
 * \param curve The ECC curve information used for key generation
 * \return A status code indicating success (0) or failure (non-zero)
 *
 * This function generates an ECC key pair (private and public keys) using the uECC library and the specified ECC curve.
 * The private key is stored in the `key->private_key`, and the public key is split into x and y coordinates and stored in
 * `key->public.x` and `key->public.y`, respectively.
 */
uint8_t uecc_generate_key(ecc_key_t *key, ecc_curve_t curve);

/**
 * \brief Generate IKM using ECC shared secret
 * \param gx_in The x-coordinate of the ECC public point
 * \param gy_in The y-coordinate of the ECC public point
 * \param private_key The private key used for ECC shared secret calculation
 * \param ikm Output buffer where the generated IKM will be stored
 * \param crv The ECC curve information used for the operation
 * \return true for success or false for failure
 *
 * This function generates the IKM by performing ECC point multiplication
 * using the public point coordinates (gx, gy) and the private key. The public point is first uncompressed,
 * and the ECC shared secret is computed using the uECC library. The result is stored in the output buffer `ikm`.
 */
bool uecc_generate_ikm(const uint8_t *gx_in, const uint8_t *gy_in,
                       const uint8_t *private_key, uint8_t *ikm,
                       ecc_curve_t crv);

/**
 * \brief Generate random bytes for cryptographic operations
 * \param dest Output buffer where the random bytes will be stored
 * \param size The number of random bytes to generate
 * \return Always returns 1 to indicate success
 *
 * This function generates `size` random bytes using the system's random number generator and stores
 * them in the output buffer `dest`.
 */
/* static int RNG(uint8_t *dest, unsigned size); */

#endif
