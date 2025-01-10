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
 *         ecc-ccc2538 headers
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 */
#ifndef ECC_CC2538_H_
#define ECC_CC2538_H_

#ifdef CC2538_DEF_H_
#include <stdint.h>
#include <string.h>
#include "edhoc-config.h"
#include "edhoc-log.h"

#include "dev/ecc-algorithm.h"
#include "dev/ecc-curve.h"
#include "sys/pt.h"
#include "ecc-common.h"

typedef struct  {
  /* Containers for the State */
  struct pt pt;
  struct process *process;

  ecc_curve_info_t *curve_info; /** Curve defining the CyclicGroup */
  /* Output Variables */
  uint8_t x[ECC_KEY_LEN];           /** Result Code */
  uint8_t y[ECC_KEY_LEN];
  uint8_t private[ECC_KEY_LEN];
} key_gen_t;

PT_THREAD(generate_key_hw(key_gen_t * key));

typedef struct ecc_curve {
  ecc_curve_info_t *curve;
} ecc_curve_t;

/**
 * \brief Generate IKM using ECC point multiplication
 * \param gx The x-coordinate of the ECC public point
 * \param gy The y-coordinate of the ECC public point
 * \param private_key The private key used for ECC point multiplication
 * \param ikm Output buffer where the generated IKM will be stored
 * \param curve The ECC curve being used for the operation
 * \return true for success or false for failure
 *
 * This function performs ECC point multiplication using the provided public point coordinates (gx, gy)
 * and the private key. The result is used to generate the IKM, which is stored in the output buffer.
 * The function uses the CC2538 hardware for ECC operations and relies on the NIST P-256 curve for the calculations.
 */
bool cc2538_generate_ikm(const uint8_t *gx, const uint8_t *gy, const uint8_t *private_key, uint8_t *ikm, ecc_curve_t curve);

void eccBytes_to_native(uint32_t *native, const uint8_t *bytes, int num_bytes);
void eccNative_to_bytes(uint8_t *bytes, int num_bytes, const uint32_t *native);

#endif /* CC2538_DEF_H */
#endif /* ECC_CC2538_H_ */
