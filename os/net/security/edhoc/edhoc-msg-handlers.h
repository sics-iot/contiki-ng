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
 *         Declarations for the EDHOC message handlers.
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 *         Christos Koulamas <cklm@isi.gr>
 */

/**
 * \addtogroup edhoc
 * @{
 */

#ifndef EDHOC_MSG_HANDLERS_H_
#define EDHOC_MSG_HANDLERS_H_

#include "edhoc.h"

/**
 * \brief Handle the EDHOC Message 1 received
 * \param ctx EDHOC Context struct
 * \param payload A pointer to the buffer containing the EDHOC message received
 * \param payload_sz Size of the EDHOC message received
 * \param ad A pointer to a buffer to copy the Application Data received in Message 1
 * \retval negative number (EDHOC ERROR CODES) when an EDHOC ERROR is detected
 * \retval ad_sz The length of the Application Data received in Message 1, when EDHOC success
 *
 * Used by Responder EDHOC role to process the Message 1 received
 * - Decode the message 1
 * - Verify that cipher suite
 * - Pass Application data AD_1
 * - If any verification step fails to return an EDHOC ERROR code and, if all the steps success
 * - the length of the Application Data receive on the Message 1 is returned.
 */
int edhoc_handler_msg_1(edhoc_context_t *ctx, uint8_t *payload,
			size_t payload_sz, uint8_t *ad);

/**
 * \brief Handle the EDHOC Message 2 received
 * \param msg2 A pointer to the buffer containing the received EDHOC message 2
 * \param ctx EDHOC Context struct
 * \param buffer A pointer to the buffer containing the EDHOC message received
 * \param buff_sz Size of the EDHOC message received
 * \retval ERR_CODE when an EDHOC ERROR is detected return a negative number corresponding to the specific error code
 * \retval 1 when EDHOC decode and verify success
 *
 * Used by Initiator EDHOC role to process the Message 2 received
 * - Decode the message 2
 * - Verify the other peer through 5-tuple and/or connection identifier C_I
 *
 * If any verification step fails to return an EDHOC ERROR code and, if all the steps success return 1.
 */
int edhoc_handler_msg_2(edhoc_msg_2_t *msg2, edhoc_context_t *ctx,
			uint8_t *buffer, size_t buff_sz);

/**
 * \brief Handle the EDHOC Message 3 received
 * \param msg3 A pointer to the buffer containing the received EDHOC message 3
 * \param ctx EDHOC Context struct
 * \param buffer A pointer to the buffer containing the EDHOC message received
 * \param buff_sz Size of the EDHOC message received
 * \retval negative number (EDHOC ERROR CODE) when an EDHOC ERROR is detected
 * \retval 1 when EDHOC decode and verify success
 *
 * Used by Responder EDHOC role to process the Message 3 receive
 * - Decode the message 3
 * - Verify the other peer through 5-tuple and/or connection identifier C_R
 *
 * If any verification step fails to return an EDHOC ERROR code and,if all the steps success return 1.
 */
int edhoc_handler_msg_3(edhoc_msg_3_t *msg3, edhoc_context_t *ctx,
			uint8_t *buffer, size_t buff_sz);

#endif /* EDHOC_MSG_HANDLERS_H_ */
/** @} */
