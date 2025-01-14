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
 *         edhoc-exporter This is an interface to derive application security context
 *         from the EDHOC shared secret
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 *         Christos Koulamas <cklm@isi.gr>
 */

/**
 * \addtogroup edhoc
 * @{
 */
#ifndef _EDHOC_EXPORTER_H_
#define _EDHOC_EXPORTER_H_

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "edhoc.h"

/* OSCORE KEY and SALT size */
#define OSCORE_SALT_SZ 8
#define OSCORE_KEY_SZ 16 /* Key length of the APP AEAD algorithm in bytes */

/* PSK KEY and SALT sizes */
#define PSK_KEY_SZ 16
#define PSK_KEY_ID_SZ 4

/* EDHOC Exporter Labels */
#define OSCORE_MASTER_SECRET_LABEL 0
#define OSCORE_MASTER_SALT_LABEL   1

/**
 * \brief OSCORE context struct
 */
typedef struct oscore_ctx {
  uint8_t master_secret[OSCORE_KEY_SZ];
  uint8_t master_salt[OSCORE_SALT_SZ];
  int client_ID;   /* CoAP client is the Initiator */
  int server_ID;   /* CoAP server is the Responder */
} oscore_ctx_t;

/**
 * \brief Derive an OSCORE Context from EDHOC
 * \param osc output OSCORE Context struct
 * \param ctx input EDHOC Context struct
 * \return negative number HKDF ERROR code when an ERROR happens in the key derivation
 *
 *  This function is used to derive an OSCORE Security Context [RFC8613] from the EDHOC shared secret.
 *  This can be run from both EDHOC Initiator and Responder once the EDHOC protocol has finished
 *  successfully.
 */
int8_t edhoc_exporter_oscore(oscore_ctx_t *osc, edhoc_context_t *ctx);

/**
 * \brief Print an OSCORE Security Context contents for debugging
 * \param osc input OSCORE Context struct
 *
 */
void print_oscore_ctx(oscore_ctx_t *osc);

/**
 * \brief Derive an application-specific key from EDHOC
 * \param result Output buffer where the derived key will be stored
 * \param in_key PRK_Exporter key to use
 * \param exporter_label Label used to differentiate different key derivation outputs
 * \param context Context data used to generate the info input for key derivatio
n
 * \param context_sz The size of the context data
 * \param length Length of the key to be derived
 * \return Negative number indicating an error code if key derivation fails, 0 on success
 *
 * This function derives a key for application-specific use from the EDHOC shared secret using the EDHOC KDF.
 * The key derivation is based on the provided label and length. This can be used for exporting keys
 * after the successful completion of the EDHOC protocol.
 */
int8_t edhoc_exporter(const uint8_t *in_key, uint8_t exporter_label,
                      const uint8_t *context, uint8_t context_sz,
                      uint16_t length, uint8_t *result);

#endif /* _EDHOC_EXPORTER_H_ */
/** @} */
