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
 * \file
 *         Header file of ECC
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

#ifndef ECC_H_
#define ECC_H_

#include "contiki.h"
#include "lib/ecc-curve.h"
#include "sys/process-mutex.h"

#ifdef ECC_CONF
#define ECC ECC_CONF
#endif /* ECC_CONF */

/**
 * \brief Structure of ECC drivers.
 * Input and output byte arrays
 * \li are in big-endian byte order
 * \li may overlap
 * \li must be word aligned if using uECC's little-endian mode (off by default)
 * \li may reside on the stack
 */
struct ecc_driver {

  /**
   * \brief Initializes ECC.
   */
  void (* init)(void);

  /**
   * \brief  Provides a mutex to be locked before proceeding with ECC.enable().
   * \return The mutex.
   */
  process_mutex_t *(* get_mutex)(void);

  /**
   * \brief       Sets up the ECC driver.
   * \param curve The curve to use in subsequent calls.
   * \return      0 on success and else a driver-specific error code.
   *              On error, the mutex is unlocked automatically and
   *              there is no need to call ECC.disable() either.
   */
  int (* enable)(const ecc_curve_t *curve);

  /**
   * \brief  Provides the protothread that runs long-running ECC operations.
   * \return The protothread that runs long-running ECC operations.
   */
  struct pt *(* get_protothread)(void);

  /**
   * \brief            Validates a public key.
   * \param public_key The 2|CURVE|-byte public key.
   * \param result     0 on success and else a driver-specific error code.
   */
  PT_THREAD((* validate_public_key)(const uint8_t *public_key, int *result));

  /**
   * \brief                         Compresses a public key as per SECG SEC 1.
   * \param uncompressed_public_key The uncompressed 2|CURVE|-byte public key.
   * \param compressed_public_key   The compressed (1+|CURVE|)-byte public key.
   */
  void (* compress_public_key)(const uint8_t *uncompressed_public_key,
                               uint8_t *compressed_public_key);

  /**
   * \brief                         Decompresses a public key.
   * \param compressed_public_key   The compressed (1+|CURVE|)-byte public key.
   * \param uncompressed_public_key The uncompressed 2|CURVE|-byte public key.
   * \param result                  0 on success and else a driver-specific
   *                                error code.
   */
  PT_THREAD((* decompress_public_key)(const uint8_t *compressed_public_key,
                                      uint8_t *uncompressed_public_key,
                                      int *result));

  /**
   * \brief              Generates an ECDSA signature for a message.
   * \param message_hash The |CURVE|-byte hash over the message.
   * \param private_key  The |CURVE|-byte private key.
   * \param signature    The 2|CURVE|-byte signature.
   * \param result       0 on success and else a driver-specific error code.
   */
  PT_THREAD((* sign)(const uint8_t *message_hash,
                     const uint8_t *private_key,
                     uint8_t *signature,
                     int *result));

  /**
   * \brief              Verifies an ECDSA signature of a message.
   * \param signature    The 2|CURVE|-byte signature.
   * \param message_hash The |CURVE|-byte hash over the message.
   * \param public_key   The 2|CURVE|-byte public key.
   * \param result       0 on success and else a driver-specific error code.
   */
  PT_THREAD((* verify)(const uint8_t *signature,
                       const uint8_t *message_hash,
                       const uint8_t *public_key,
                       int *result));

  /**
   * \brief              Generates a public/private key pair.
   * \param public_key   The 2|CURVE|-byte public key.
   * \param private_key  The |CURVE|-byte private key.
   * \param result       0 on success and else a driver-specific error code.
   */
  PT_THREAD((* generate_key_pair)(uint8_t *public_key,
                                  uint8_t *private_key,
                                  int *result));

  /**
   * \brief               Generates a shared secret as per ECDH.
   *
   * NOTE: Callers should derive symmetric keys from the shared secret via a
   * key derivation function.
   *
   * \param public_key    The peer's 2|CURVE|-byte public key.
   * \param private_key   Our |CURVE|-byte private key.
   * \param shared_secret The resultant |CURVE|-byte shared secret.
   * \param result        0 on success and else a driver-specific error code.
   */
  PT_THREAD((* generate_shared_secret)(const uint8_t *public_key,
                                       const uint8_t *private_key,
                                       uint8_t *shared_secret,
                                       int *result));

  /**
   * \brief Shuts down the ECC driver and unlocks the mutex.
   */
  void (* disable)(void);
};

#ifdef ECC
extern const struct ecc_driver ECC;
#endif /* ECC */

#endif /* ECC_H_ */

/** @} */
