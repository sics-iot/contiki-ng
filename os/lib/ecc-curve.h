/*
 * Original file:
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
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
 * \addtogroup crypto
 * @{
 *
 * \file
 * NIST curves for various key sizes
 */
#ifndef ECC_CURVE_H_
#define ECC_CURVE_H_

#include <stddef.h>
#include <stdint.h>

/** Parameters of an ECC curve in little-endian word order. */
typedef struct {
  /** Name of the curve. */
  const char *name;

  /** Size of the curve in 32-bit words. */
  const size_t words;

  /** Size of the curve in bytes. */
  const size_t bytes;

  /** The prime that defines the field of the curve. */
  const uint32_t *p;

  /** Precomputed value of p + 1. */
  const uint32_t *p_plus_one;

  /** Length of the binary representation of p + 1. */
  const size_t binary_length_of_p_plus_one;

  /** Order of the curve. */
  const uint32_t *n;

  /** Length of the binary representation of n. */
  const size_t binary_length_of_n;

  /** Coefficient a of the equation. */
  const uint32_t *a;

  /** Coefficient b of the equation. */
  const uint32_t *b;

  /** x coordinate of the generator point. */
  const uint32_t *x;

  /** y coordinate of the generator point. */
  const uint32_t *y;
} ecc_curve_t;

/*
 * NIST P-256, X9.62 prime256v1, secp256r1
 */
extern ecc_curve_t ecc_curve_p_256;
#define ECC_CURVE_P_256_SIZE (32)

/*
 * NIST P-192, X9.62 prime192v1
 */
extern ecc_curve_t ecc_curve_p_192;
#define ECC_CURVE_P_192_SIZE (24)

#endif /* ECC_CURVE_H_ */

/**
 * @}
 */
