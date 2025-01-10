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
 *         edhoc-msg serialize and deserialize the EDHOC messages using the CBOR library
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */
#include "contiki-lib.h"
#include "edhoc-msgs.h"
#include "cbor.h"
#include <assert.h>

/*---------------------------------------------------------------------------*/
void
print_msg_1(edhoc_msg_1_t *msg)
{
  LOG_DBG("Type: %d\n", msg->method);
  LOG_DBG("Suite I: ");
  print_buff_8_dbg(msg->suites_i, msg->suites_i_sz);
  LOG_DBG("Gx: ");
  print_buff_8_dbg(msg->g_x, ECC_KEY_LEN);
  LOG_DBG("Ci: ");
  print_buff_8_dbg(msg->c_i, EDHOC_CID_LEN);
  LOG_DBG("EAD (label: %d): ", msg->uad.ead_label);
  print_buff_8_dbg(msg->uad.ead_value, msg->uad.ead_value_sz);
}
/*---------------------------------------------------------------------------*/
void
print_msg_2(edhoc_msg_2_t *msg)
{
  LOG_DBG("gy_ciphertext_2: ");
  print_buff_8_dbg(msg->gy_ciphertext_2, msg->gy_ciphertext_2_sz);
}
/*---------------------------------------------------------------------------*/
void
print_msg_3(edhoc_msg_3_t *msg)
{
  LOG_DBG("CIPHERTEXT_3: ");
  print_buff_8_dbg(msg->ciphertext_3, msg->ciphertext_3_sz);
}
/*---------------------------------------------------------------------------*/
static uint8_t
get_byte(uint8_t **in)
{
  uint8_t out = **in;
  (*in)++;
  return out;
}
/*---------------------------------------------------------------------------*/
int16_t
edhoc_get_unsigned(uint8_t **in)
{
  uint8_t byte = get_byte(in);

  if(byte < 0x18) {

    return byte;
  } else if(byte == 0x18) {
    return get_byte(in);
  } else {
    (*in)--;
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
static int64_t
get_negative(uint8_t **in)
{
  uint8_t byte = get_byte(in);

  int64_t num = 0;
  if(byte < 0x38) {

    num = byte ^ 0x20;
  } else if(byte == 0x38) {
    byte = get_byte(in);
    num = byte ^ 0x20;
  } else {
    LOG_ERR("get not negative\n ");
    return 0;
  }
  num++;
  return num;
}
/*---------------------------------------------------------------------------*/
static uint8_t *
point_byte(uint8_t **in)
{
  uint8_t *out = *in;
  (*in)++;
  return out;
}
/*---------------------------------------------------------------------------*/
size_t
edhoc_get_bytes(uint8_t **in, uint8_t **out)
{
  uint8_t byte = get_byte(in);
  size_t size;
  if(byte == 0x58) {
    size = get_byte(in);
    *out = *in;
    *in = (*in + size);
    return size;
  } else if((0x40 <= byte) && (byte < 0x58)) {
    size = byte ^ 0x40;
    *out = *in;
    *in = (*in + size);
    return size;
  } else {
    (*in)--;
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
edhoc_get_maps_num(uint8_t **in)
{
  uint8_t byte = get_byte(in);
  if((byte >= 0xa0) && (byte <= 0xaf)) { /* max of 15 maps */
    uint8_t num = byte ^ 0xa0;
    return num;
  } else {
    (*in)--;
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
edhoc_get_array_num(uint8_t **in)
{
  uint8_t byte = get_byte(in);
  if((byte >= 0x80) && (byte <= 0x8f)) { /* max of 15 maps */
    uint8_t num = byte ^ 0x80;
    return num;
  } else {
    (*in)--;
    return 0;
  }
}
/*---------------------------------------------------------------------------*/
static int16_t
get_text(uint8_t **in, char **out)
{
  uint8_t byte = get_byte(in);
  size_t size;
  if(byte == 0x78) {
    size = get_byte(in);
    *out = (char *)*in;
    *in = (*in + size);
    return size;
  } else if((0x60 <= byte) && (byte < 0x78)) {
    size = byte ^ 0x60;
    *out = (char *)*in;
    *in = (*in + size);
    return size;
  } else {
    (*in)--;
    return -1;
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
edhoc_get_byte_identifier(uint8_t **in)
{
  uint8_t input_byte = **in;
  (*in)++;

  /* Check if the byte is in the range 0x00 to 0x17 (positive integers 0 to 23) */
  /* or in the range 0x20 to 0x37 (negative integers -1 to -24) */
  if((input_byte <= 0x17) || (input_byte >= 0x20 && input_byte <= 0x37)) {
    return input_byte;
  } else {
    LOG_ERR("Unsupported connection identifier received from peer\n");
  }

  /* Else: TODO: handle CBOR byte string CIDs */
  /* int out_sz = cbor_get_bytes(in, out); */
  return 0;
}
/*---------------------------------------------------------------------------*/
static size_t
edhoc_serialize_suites(unsigned char **buffer, const uint8_t *suites, size_t suites_sz)
{
  if(suites_sz == 1) {
    return cbor_put_unsigned(buffer, suites[0]);
  }
  size_t size = cbor_put_array(buffer, suites_sz);
  for(uint8_t i = 0; i < suites_sz; ++i) {
    size += cbor_put_unsigned(buffer, suites[i]);
  }
  return size;
}
/*---------------------------------------------------------------------------*/
static void
edhoc_deserialize_suites(unsigned char **buffer, uint8_t **suites_buf, size_t *suites_sz)
{
  *suites_buf = (uint8_t *)*buffer;
  int8_t unint = (int8_t)edhoc_get_unsigned(buffer);

  if(unint < 0) {
    unint = edhoc_get_array_num(buffer);
    *suites_buf = (uint8_t *)*buffer;
    *suites_sz = 0;

    while(*suites_sz < unint) {
      edhoc_get_unsigned(buffer);
      (*suites_sz)++;
    }
  } else {
    *suites_sz = 1;
  }
}
/*---------------------------------------------------------------------------*/
size_t
edhoc_serialize_msg_1(edhoc_msg_1_t *msg, unsigned char *buffer, bool suite_array)
{
  size_t size = cbor_put_unsigned(&buffer, msg->method);
  size += edhoc_serialize_suites(&buffer, msg->suites_i, msg->suites_i_sz);
  size += cbor_put_bytes(&buffer, msg->g_x, ECC_KEY_LEN);
  size += edhoc_put_byte_identifier(&buffer, msg->c_i, EDHOC_CID_LEN);
  if(msg->uad.ead_value_sz > 0) {
    size += cbor_put_bytes(&buffer, msg->uad.ead_value, msg->uad.ead_value_sz);
  }
  return size;
}
/*---------------------------------------------------------------------------*/
size_t
edhoc_serialize_err(edhoc_msg_error_t *msg, unsigned char *buffer)
{
  int size = cbor_put_unsigned(&buffer, msg->err_code);
  switch(msg->err_code) {
  default:
    LOG_ERR("edhoc_serialize_err: unknown error code: %d\n", msg->err_code);
    break;
  case 1:
    size += cbor_put_text(&buffer, msg->err_info, msg->err_info_sz);
    break;
  case 2:
    /* FIXME: strict aliasing violation */
    size += edhoc_serialize_suites(&buffer, (uint8_t *)msg->err_info, msg->err_info_sz);
    break;
  case 3:
    size += cbor_put_num(&buffer, 0xf5);
    break;
  }
  return size;
}
/*---------------------------------------------------------------------------*/
int8_t
edhoc_deserialize_err(edhoc_msg_error_t *msg, unsigned char *buffer, uint8_t buff_sz)
{
  uint8_t *buff_end = buffer + buff_sz;
  if(buffer < buff_end) {
    int16_t rv = edhoc_get_unsigned(&buffer);
    if(rv < 0) {
      LOG_ERR("edhoc_deserialize_err got invalid error code or not error message\n");
      return 0;
    }
    msg->err_code = (uint8_t)rv;
  }
  if(buffer < buff_end) {
    if(msg->err_code == 2) {
      /* FIXME: strict aliasing violation */
      edhoc_deserialize_suites(&buffer, (uint8_t **)&msg->err_info, &msg->err_info_sz);
      return ERR_NEW_SUITE_PROPOSE;
    }
    int16_t len = get_text(&buffer, &msg->err_info);
    if(len > 0) {
      msg->err_info_sz = len;
      LOG_ERR("Is an error msgs\n");
      return RX_ERR_MSG;
    }
    if(len == -1) {
      return 0;
    }
    msg->err_info_sz = (size_t)len;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
int8_t
edhoc_deserialize_msg_1(edhoc_msg_1_t *msg, unsigned char *buffer, size_t buff_sz)
{
  /* Get the METHOD */
  uint8_t *p_out = NULL;
  size_t out_sz;
  uint8_t *buff_end = buffer + buff_sz;

  if(buffer < buff_end) {
    int8_t unint = (int8_t)edhoc_get_unsigned(&buffer);
    msg->method = unint;
  }
  /* Get the suite */
  if(buffer < buff_end) {
    edhoc_deserialize_suites(&buffer, &msg->suites_i, &msg->suites_i_sz);
  }
  /* Get Gx */
  if(buffer < buff_end) {
    out_sz = edhoc_get_bytes(&buffer, &p_out);
    if(out_sz == 0) {
      LOG_ERR("error code (%d)\n ", ERR_MSG_MALFORMED);
      return ERR_MSG_MALFORMED;
    }
    msg->g_x = p_out;
  }
  /* Get the session_id (Ci) */
  if(buffer < buff_end) {
    edhoc_get_bytes(&buffer, &msg->c_i);
    msg->c_i = point_byte(&buffer);
  }
  /* Get the decrypted msg */
  if(buffer < buff_end) {
    out_sz = edhoc_get_bytes(&buffer, &p_out);
    if(out_sz == 0) {
      LOG_ERR("error code (%d)\n ", ERR_MSG_MALFORMED);
      return ERR_MSG_MALFORMED;
    }

    msg->uad.ead_value = p_out;
    msg->uad.ead_value_sz = out_sz;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
int8_t
edhoc_deserialize_msg_2(edhoc_msg_2_t *msg, unsigned char *buffer, size_t buff_sz)
{
  msg->gy_ciphertext_2_sz = edhoc_get_bytes(&buffer, &msg->gy_ciphertext_2);
  if(msg->gy_ciphertext_2_sz == 0) {
    LOG_ERR("error code (%d)\n ", ERR_MSG_MALFORMED);
    return ERR_MSG_MALFORMED;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
int8_t
edhoc_deserialize_msg_3(edhoc_msg_3_t *msg, unsigned char *buffer, size_t buff_sz)
{
  msg->ciphertext_3_sz = edhoc_get_bytes(&buffer, &msg->ciphertext_3);
  if(msg->ciphertext_3_sz == 0) {
    LOG_ERR("error code (%d)\n ", ERR_MSG_MALFORMED);
    return ERR_MSG_MALFORMED;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
uint8_t
edhoc_get_auth_key_from_kid(uint8_t *kid, uint8_t kid_sz, cose_key_t **key)
{
  cose_key_t *auth_key;
  if(edhoc_check_key_list_kid(kid, kid_sz, &auth_key) == 0) {
    LOG_ERR("The authentication key ID is not in the list\n");
    return ERR_NOT_ALLOWED_IDENTITY;
  }
  *key = auth_key;
  return ECC_KEY_LEN;
}
/*---------------------------------------------------------------------------*/
int8_t
edhoc_get_key_id_cred_x(uint8_t **p, uint8_t *out_id_cred_x, cose_key_t *key)
{
  uint8_t *start = *p;
  uint8_t num = edhoc_get_maps_num(p);
  uint8_t label = 0;
  int8_t key_sz = 0;
  uint8_t key_id_sz = 0;
  uint8_t *ptr = NULL;
  char *ch = NULL;
  cose_key_t *hkey = NULL;

  if(num > 0) {
    label = (uint8_t)edhoc_get_unsigned(p);
  } else {
    /* Compact encoding case */
    key->kid[0] = **p;
    (*p)++;
    key->kid_sz = 1;
    ptr = key->kid;

    if(key->kid[0] == 0) {
      /* Read variable-length KID */
      key->kid_sz = edhoc_get_bytes(p, &ptr);
      memcpy(key->kid, ptr, key->kid_sz);
    }
    label = 0;
  }

  switch(label) {
  case 0:
    /* ID_CRED_R = KID (compact encoding) */
    key_sz = edhoc_get_auth_key_from_kid(key->kid, key->kid_sz, &hkey);
    memcpy(key, hkey, sizeof(cose_key_t));
    if(key_sz <= 0) {
      return key_sz;
    }
    break;

  case 4:
    /* ID_CRED_R = map(4:KID) */
    key_id_sz = edhoc_get_bytes(p, &ptr);
    key_sz = edhoc_get_auth_key_from_kid(ptr, key_id_sz, &hkey);
    memcpy(key, hkey, sizeof(cose_key_t));
    if(key_sz <= 0) {
      return key_sz;
    }
    break;

  case 1:
    /* ID_CRED_R = CRED_R (inclusion of credentials) */
    /* FIXME: Does note seem to correctly rebuild the CCS and/or the CRED_X */
    LOG_DBG("**** ID_CRED_R = CRED_R");
    key->kty = edhoc_get_unsigned(p);

    if(get_negative(p) != 1) {
      break;
    }
    key->crv = (uint8_t)edhoc_get_unsigned(p);

    if(get_negative(p) != 2) {
      break;
    }
    key_sz = edhoc_get_bytes(p, &ptr);
    memcpy(key->ecc.pub.x, ptr, ECC_KEY_LEN);

    if(get_negative(p) != 3) {
      break;
    }
    key_sz = edhoc_get_bytes(p, &ptr);
    memcpy(key->ecc.pub.y, ptr, ECC_KEY_LEN);

    key->identity_sz = get_text(p, &ch);
    memcpy(key->identity, ch, key->identity_sz);
    ch = NULL;

    if(key_sz <= 0) {
      return key_sz;
    }
    break;

  default:
    LOG_ERR("Unknown label %d\n", label);
    return -1;
  }

  if(key_sz != ECC_KEY_LEN) {
    LOG_ERR("Incorrect key size\n");
    return -1;
  }

  uint16_t id_cred_x_sz = *p - start;
  if(out_id_cred_x != NULL) {
    assert(*p - start >= 0);
    assert(id_cred_x_sz <= EDHOC_MAX_BUFFER);
    memcpy(out_id_cred_x, start, id_cred_x_sz);
  }

  /* Rebuild from compact encoding if needed */
  if(out_id_cred_x != NULL && id_cred_x_sz == 1) {
    id_cred_x_sz = 0;
    id_cred_x_sz += cbor_put_map(&out_id_cred_x, 1);
    id_cred_x_sz += cbor_put_unsigned(&out_id_cred_x, 4);
    id_cred_x_sz += cbor_put_bytes(&out_id_cred_x, key->kid, 1);
  }

  return id_cred_x_sz;
}
/*---------------------------------------------------------------------------*/
uint8_t
edhoc_get_sign(uint8_t **p, uint8_t **sign)
{
  uint8_t sign_sz = edhoc_get_bytes(p, sign);
  return sign_sz;
}
/*---------------------------------------------------------------------------*/
uint8_t
edhoc_get_ad(uint8_t **p, uint8_t *ad)
{
  uint8_t *ptr;
  uint8_t ad_sz = edhoc_get_bytes(p, &ptr);
  memcpy(ad, ptr, ad_sz);
  return ad_sz;
}
/*---------------------------------------------------------------------------*/
int
edhoc_put_byte_identifier(uint8_t **buffer, uint8_t *bytes, uint8_t len)
{
  /* For single byte values check whether they are a valid CBOR integer */
  if(len == 1) {
    uint8_t byte = bytes[0];

    /* Check if the byte is in the range 0x00 to 0x17 (positive integers 0 to 23) */
    /* or in the range 0x20 to 0x37 (negative integers -1 to -24) */
    if((byte <= 0x17) || (byte >= 0x20 && byte <= 0x37)) {
      **buffer = byte;
      (*buffer)++;
      return 1;
    }
  }

  /* Else encode as a CBOR byte string */
  return cbor_put_bytes(buffer, bytes, len);
}
/*---------------------------------------------------------------------------*/
