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
 *         Declarations for EDHOC message generators.
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 *         Christos Koulamas <cklm@isi.gr>
 */

/**
 * \addtogroup edhoc
 * @{
 */

#ifndef EDHOC_MSG_GENERATORS_H_
#define EDHOC_MSG_GENERATORS_H_

#include "edhoc.h"

/**
 * \brief Generate the EDHOC Message 1 and set it in the EDHOC context
 * \param ctx EDHOC Context struct
 * \param ad Application data to include in MSG1
 * \param ad_sz Application data length
 * \param suite_array If true the msg1 include an array of suites if have more than one suite if 0 msg1 includes an
 * unique unsigned suite independently of the number of suites supported by the initiator
 *
 * Generate an ephemeral ECDH key pair, determinate the cipher suite to use and the
 * connection identifier. Compose the EDHOC Message 1 as described in the (RFC9528) reference
 * for EHDOC Authentication with Asymmetric Keys and encoded as a CBOR sequence in the MSG1 element of the
 * ctx struct.
 * It is used by Initiator EDHOC role.
 *
 * - ctx->MSG1 = (METHOD_CORR:unsigned, SUITES_I:unsigned, G_X, C_I_identifier)
 *
 */
void edhoc_gen_msg_1(edhoc_context_t *ctx, uint8_t *ad, size_t ad_sz, bool suite_array);

/**
 * \brief Generate the EDHOC Message 2 and set it in the EDHOC ctx
 * \param ctx EDHOC Context struct
 * \param ad Application data to include in MSG2
 * \param ad_sz Application data length
 * \retval -1 when message generation fails
 * \retval 1 when message generation succeeds
 *
 * It is used by EDHOC Responder role to processing the message 2
 * Generate an ephemeral ECDH key pair
 * Choose a connection identifier,
 * Compute the transcript hash 2 TH_2 = H(ctx->MSG1, data_2)
 * Compute MAC_2 (Message Authentication Code)
 * Compute CIPHERTEXT_2
 * Compose the EDHOC Message 2 as described in the (RFC9528) reference
 * for EHDOC Authentication with Asymmetric Keys and encoded as a CBOR sequence in the MSG2 element of the
 * ctx struct.
 * - ctx->MSG2 = (data_2, CIPHERTEXT_2)
 * - where: data_2 = (?C_I_identifier, G_Y)
 */
uint8_t edhoc_gen_msg_2(edhoc_context_t *ctx, const uint8_t *ad, size_t ad_sz);

/**
 * \brief Generate the EDHOC Message 3 and set it in the EDHOC ctx
 * \param ctx EDHOC Context struct
 * \param ad Application data to include in MSG3
 * \param ad_sz Application data length
 *
 * It is used by EDHOC Initiator role to processing the message 3.
 * Compute the transcript hash 3 TH_3 = H(TH_2, PLAINTEXT_2, data_3)
 * Compute MAC_3 (Message Authentication Code)
 * Compute CIPHERTEXT_3
 * Compose the EDHOC Message 3 as described in the (RFC9528) reference
 * for EHDOC Authentication with Asymmetric Keys and encoded as a CBOR sequence in the MSG3 element of the
 * ctx struct.
 *
 * - ctx->MSG3 = (data_3, CIPHERTEXT_3)
 * - where: data_3 = (?C_R_identifier)
 */
void edhoc_gen_msg_3(edhoc_context_t *ctx, const uint8_t *ad, size_t ad_sz);

#endif /* EDHOC_MSG_GENERATORS_H_ */
/** @} */
