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
 *         ECDH header
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */
#ifndef _ECDH_H_
#define _ECDH_H_

#include <stdint.h>
#include <stdbool.h>
#include "edhoc-key-storage.h"

/* Choose the ECC library to use */
#define EDHOC_ECC_CC2538 1
#define EDHOC_ECC_UECC 2

#ifdef EDHOC_CONF_ECC
#define EDHOC_ECC EDHOC_CONF_ECC
#else
#define EDHOC_ECC EDHOC_ECC_UECC
#endif

#if EDHOC_ECC == EDHOC_ECC_UECC
#include "ecc-uecc.h"
#elif EDHOC_ECC == EDHOC_ECC_CC2538
#include "ecc-cc2538.h"
#else
#error Please specify EDHOC_ECC
#endif

bool ecdh_generate_ikm(uint8_t curve_id, const uint8_t *gx, const uint8_t *gy, const uint8_t *private_key, uint8_t *ikm);

bool ecdh_get_ecc_curve(uint8_t curve_id, ecc_curve_t *curve);

#endif /* _ECDH_H_ */
