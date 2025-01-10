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
 *         ecdh, an interface between the ECC and Secure Hash Algorithms with the EDHOC implementation.
 *         Interface the ECC key used library with the EDHOC implementation. New ECC libraries can be include it here.
 *         (UECC macro must be defined at config file) and with the CC2538 HW module
 *         Interface the Secure Hash Algorithms SH256 with the EDHOC implementation.
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */

#include "contiki.h"
#include "ecdh.h"
/*---------------------------------------------------------------------------*/
bool
ecdh_generate_ikm(uint8_t curve_id, const uint8_t *gx, const uint8_t *gy,
                  const uint8_t *private_key, uint8_t *ikm)
{
  ecc_curve_t curve;
  bool er = ecdh_get_ecc_curve(curve_id, &curve);
  if(!er) {
    return er;
  }

#if EDHOC_ECC == EDHOC_ECC_UECC
  return uecc_generate_ikm(gx, gy, private_key, ikm, curve);
#elif EDHOC_ECC == EDHOC_ECC_CC2538
  return cc2538_generate_ikm(gx, gy, private_key, ikm, curve);
#else
  return false;
#endif
}
/*---------------------------------------------------------------------------*/
bool
ecdh_get_ecc_curve(uint8_t curve_id, ecc_curve_t *curve)
{
#if EDHOC_ECC == EDHOC_ECC_UECC
  switch(curve_id) {
  case EDHOC_CURVE_P256:
    curve->curve = uECC_secp256r1();
    return true;
  default:
    LOG_ERR("Invalid curve when trying to derive IKM with uECC\n");
    return false;
  }
#elif EDHOC_ECC == EDHOC_ECC_CC2538
  switch(curve_id) {
  case EDHOC_CURVE_P256:
    curve->curve = &nist_p_256;
    return true;
  default:
    LOG_ERR("Invalid curve when trying to derive IKM with CC2538 ECC\n");
    return false;
  }
#else
  LOG_ERR("No ECC implementation defined\n");
  return false;
#endif
}
/*---------------------------------------------------------------------------*/
