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
 *         ecdh-msg header
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */
#ifndef _EDHOC_MSGS_H__
#define _EDHOC_MSGS_H__

#include <stdint.h>
#include <stddef.h>
#include "edhoc-log.h"
#include "edhoc-key-storage.h"
#include "edhoc-config.h"

/*Error definitions*/
#define ERR_SUITE_NON_SUPPORT -2
#define ERR_MSG_MALFORMED -3
#define ERR_REJECT_METHOD -4
#define ERR_CID_NOT_VALID -5
#define ERR_WRONG_CID_RX -6
#define ERR_ID_CRED_X_MALFORMED -7
#define ERR_AUTHENTICATION -8
#define ERR_DECRYPT -9
#define ERR_CODE -10
#define ERR_NOT_ALLOWED_IDENTITY -11
#define RX_ERR_MSG -1
#define ERR_TIMEOUT -12
#define ERR_CORRELATION -13
#define ERR_NEW_SUITE_PROPOSE -14
#define ERR_RESEND_MSG_1 -15

typedef struct edhoc_ead_data {
  uint8_t ead_label;
  uint8_t *ead_value;
  size_t ead_value_sz;
} edhoc_ead_data_t;

typedef struct edhoc_msg_1 {
  uint8_t method;
  uint8_t *suites_i;
  size_t suites_i_sz;
  uint8_t *g_x;
  uint8_t *c_i;
  edhoc_ead_data_t uad;
} edhoc_msg_1_t;

typedef struct edhoc_msg_2 {
  uint8_t *gy_ciphertext_2;
  size_t gy_ciphertext_2_sz;
} edhoc_msg_2_t;

typedef struct edhoc_msg_3 {
  uint8_t *ciphertext_3;
  size_t ciphertext_3_sz;
} edhoc_msg_3_t;

typedef struct edhoc_msg_error {
  uint8_t err_code;
  char *err_info;
  size_t err_info_sz;
} edhoc_msg_error_t;

void print_msg_1(edhoc_msg_1_t *msg);
void print_msg_2(edhoc_msg_2_t *msg);
void print_msg_3(edhoc_msg_3_t *msg);

size_t edhoc_serialize_msg_1(edhoc_msg_1_t *msg, unsigned char *buffer, bool suite_array);
size_t edhoc_serialize_err(edhoc_msg_error_t *msg, unsigned char *buffer);

int8_t edhoc_deserialize_msg_1(edhoc_msg_1_t *msg, unsigned char *buffer, size_t buff_sz);
int8_t edhoc_deserialize_msg_2(edhoc_msg_2_t *msg, unsigned char *buffer, size_t buff_sz);
int8_t edhoc_deserialize_msg_3(edhoc_msg_3_t *msg, unsigned char *buffer, size_t buff_sz);
int8_t edhoc_deserialize_err(edhoc_msg_error_t *msg, unsigned char *buffer, uint8_t buff_sz);
int8_t edhoc_get_key_id_cred_x(uint8_t **p, uint8_t *id_cred_x, cose_key_t *key);
uint8_t edhoc_get_auth_key_from_kid(uint8_t *kid, uint8_t kid_sz, cose_key_t **key);
uint8_t edhoc_get_sign(uint8_t **p, uint8_t **sign);
uint8_t edhoc_get_ad(uint8_t **p, uint8_t *ad);

uint8_t edhoc_get_byte_identifier(uint8_t **in);
uint8_t edhoc_get_maps_num(uint8_t **in);
size_t edhoc_get_bytes(uint8_t **in, uint8_t **out);
int16_t edhoc_get_unsigned(uint8_t **in);
uint8_t edhoc_get_array_num(uint8_t **in);

int edhoc_put_byte_identifier(uint8_t **buffer, uint8_t *bytes, uint8_t len);

#endif
