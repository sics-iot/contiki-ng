/*
 * Copyright (c) 2014, Institute for Pervasive Computing, ETH Zurich.
 * All rights reserved.
 *
 * Author: Andreas Dröscher <contiki@anticat.ch>
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
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS "AS IS" AND
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
 * \addtogroup crypto
 * @{
 */
#include "lib/ecc-curve.h"

/* [NIST P-256, X9.62 prime256v1, secp256r1] */
static const uint32_t nist_p_256_p[] = {
  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
  0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF
};
static const uint32_t nist_p_256_p_plus_one[] = {
  0x00000000, 0x00000000, 0x00000000, 0x00000001,
  0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF
};
static const uint32_t nist_p_256_n[] = {
  0xFC632551, 0xF3B9CAC2, 0xA7179E84, 0xBCE6FAAD,
  0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF
};
static const uint32_t nist_p_256_a[] = {
  0xFFFFFFFC, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000,
  0x00000000, 0x00000000, 0x00000001, 0xFFFFFFFF
};
static const uint32_t nist_p_256_b[] = {
  0x27D2604B, 0x3BCE3C3E, 0xCC53B0F6, 0x651D06B0,
  0x769886BC, 0xB3EBBD55, 0xAA3A93E7, 0x5AC635D8
};
static const uint32_t nist_p_256_x[] = {
  0xD898C296, 0xF4A13945, 0x2DEB33A0, 0x77037D81,
  0x63A440F2, 0xF8BCE6E5, 0xE12C4247, 0x6B17D1F2
};
static const uint32_t nist_p_256_y[] = {
  0x37BF51F5, 0xCBB64068, 0x6B315ECE, 0x2BCE3357,
  0x7C0F9E16, 0x8EE7EB4A, 0xFE1A7F9B, 0x4FE342E2
};

ecc_curve_t ecc_curve_p_256 = {
  .name                        = "NIST P-256",
  .words                       = 8,
  .bytes                       = 32,
  .p                           = nist_p_256_p,
  .p_plus_one                  = nist_p_256_p_plus_one,
  .binary_length_of_p_plus_one = 256,
  .n                           = nist_p_256_n,
  .binary_length_of_n          = 256,
  .a                           = nist_p_256_a,
  .b                           = nist_p_256_b,
  .x                           = nist_p_256_x,
  .y                           = nist_p_256_y
};

/* [NIST P-192, X9.62 prime192v1] */
static const uint32_t nist_p_192_p[] = {
  0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};
static const uint32_t nist_p_192_p_plus_one[] = {
  0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};
static const uint32_t nist_p_192_a[] = {
  0xFFFFFFFC, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};
static const uint32_t nist_p_192_b[] = {
  0xC146B9B1, 0xFEB8DEEC, 0x72243049, 0x0FA7E9AB, 0xE59C80E7, 0x64210519
};
static const uint32_t nist_p_192_x[] = {
  0x82FF1012, 0xF4FF0AFD, 0x43A18800, 0x7CBF20EB, 0xB03090F6, 0x188DA80E
};
static const uint32_t nist_p_192_y[] = {
  0x1E794811, 0x73F977A1, 0x6B24CDD5, 0x631011ED, 0xFFC8DA78, 0x07192B95
};
static const uint32_t nist_p_192_n[] = {
  0xB4D22831, 0x146BC9B1, 0x99DEF836, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF
};

ecc_curve_t ecc_curve_p_192 = {
  .name                        = "NIST P-192",
  .words                       = 6,
  .bytes                       = 24,
  .p                           = nist_p_192_p,
  .p_plus_one                  = nist_p_192_p_plus_one,
  .binary_length_of_p_plus_one = 192,
  .n                           = nist_p_192_n,
  .binary_length_of_n          = 192,
  .a                           = nist_p_192_a,
  .b                           = nist_p_192_b,
  .x                           = nist_p_192_x,
  .y                           = nist_p_192_y
};

/**
 * @}
 */
