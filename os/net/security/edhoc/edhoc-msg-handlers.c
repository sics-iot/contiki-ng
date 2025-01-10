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
 *         EDHOC message handling functions.
 * \author
 *         Lidia Pocero <pocero@isi.gr>
 *         Peter A Jonsson
 *         Rikard Höglund
 *         Marco Tiloca
 */

#include <assert.h>

#include "edhoc-msg-handlers.h"
#include "cose.h"

/*----------------------------------------------------------------------------*/
static uint16_t
decrypt_ciphertext_3(edhoc_context_t *ctx, const uint8_t *ciphertext,
                     uint16_t ciphertext_sz, uint8_t *plaintext)
{
  cose_encrypt0 *cose = cose_encrypt0_new();

  /* set external AAD in cose */
  cose_encrypt0_set_content(cose, NULL, 0, NULL, 0);
  uint8_t *th3_ptr = cose->external_aad;
  memcpy(th3_ptr, ctx->state.th, HASH_LEN);
  cose->external_aad_sz = HASH_LEN;

  cose_encrypt0_set_ciphertext(cose, ciphertext, ciphertext_sz);
  /* COSE encrypt0 set header */
  cose_encrypt0_set_header(cose, NULL, 0, NULL, 0);

  /* generate K_3 */
  cose->alg = ctx->config.aead_alg;
  cose->key_sz = cose_get_key_len(cose->alg);
  int16_t er = edhoc_kdf(ctx->state.prk_3e2m, K_3_LABEL, ctx->state.th,
                         HASH_LEN, cose->key_sz, cose->key);
  if(er < 1) {
    LOG_ERR("error in expand for decrypt ciphertext 3\n");
    return 0;
  }
  LOG_DBG("K_3 (%d bytes): ", cose->key_sz);
  print_buff_8_dbg(cose->key, cose->key_sz);

  /* generate IV_3 */
  cose->nonce_sz = cose_get_iv_len(cose->alg);
  er = edhoc_kdf(ctx->state.prk_3e2m, IV_3_LABEL, ctx->state.th, HASH_LEN,
                 cose->nonce_sz, cose->nonce);
  if(er < 1) {
    LOG_ERR("error in expand for decrypt ciphertext 3\n");
    return 0;
  }
  LOG_DBG("IV_3 (%d bytes): ", cose->nonce_sz);
  print_buff_8_dbg(cose->nonce, cose->nonce_sz);

  /* Decrypt COSE */
  if(!cose_decrypt(cose)) {
    LOG_ERR("ciphertext 3 decrypt error\n");
    return 0;
  }

  for(int i = 0; i < cose->plaintext_sz; i++) {
    plaintext[i] = cose->plaintext[i];
  }

  /* Free memory */
  cose_encrypt0_finalize(cose);
  return cose->plaintext_sz;
}
/*----------------------------------------------------------------------------*/
static int8_t
set_rx_cid(edhoc_context_t *ctx, uint8_t *cidrx, uint8_t cidrx_sz)
{
  /* set connection id from rx */
  if(cidrx_sz == 1) {
    ctx->state.cid_rx = (uint8_t)edhoc_get_byte_identifier(&cidrx);
  }

  if(ctx->state.cid_rx == ctx->state.cid) {
    LOG_ERR("error code2 (%d)\n", ERR_CID_NOT_VALID);
    return ERR_CID_NOT_VALID;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
static int8_t
check_rx_suite_i(edhoc_context_t *ctx,
                 const uint8_t *suite_rx, size_t suite_rx_sz)
{
  /* Get the selected cipher suite (last element) */
  uint8_t peer_selected_suite = suite_rx[suite_rx_sz - 1];

  /* Check if the selected suite is supported */
  for(uint8_t i = 0; i < ctx->config.suite_num; i++) {
    if(ctx->config.suite[i] == peer_selected_suite) {
      ctx->state.suite_selected = peer_selected_suite;
      LOG_DBG("Selected cipher suite: %d\n", ctx->state.suite_selected);

      /* Responder sets config to use based on selected suite */
      int8_t er = edhoc_set_config_from_suite(ctx, ctx->state.suite_selected);
      if(er != 1) {
        LOG_WARN("ERR_NEW_SUITE_PROPOSE\n");
        return ERR_NEW_SUITE_PROPOSE;
      }
      return 0;
    }
  }

  LOG_WARN("ERR_NEW_SUITE_PROPOSE\n");
  return ERR_NEW_SUITE_PROPOSE;
}
/*----------------------------------------------------------------------------*/
static void
set_rx_gx(edhoc_context_t *ctx, const uint8_t *gx)
{
  memcpy(ctx->state.gx, gx, ECC_KEY_LEN);
}
/*----------------------------------------------------------------------------*/
static int8_t
set_rx_method(edhoc_context_t *ctx, uint8_t method)
{
  if(method != EDHOC_METHOD) {
    LOG_ERR("error code (%d)\n", ERR_REJECT_METHOD);
    return ERR_REJECT_METHOD;
  }
  ctx->config.method = method;
  return 0;
}
/*----------------------------------------------------------------------------*/
static void
set_rx_msg(edhoc_context_t *ctx, const uint8_t *msg, uint8_t msg_sz)
{
  memcpy(ctx->buffers.msg_rx, msg, msg_sz);
  ctx->buffers.rx_sz = msg_sz;
}
/*----------------------------------------------------------------------------*/
static int8_t
edhoc_check_err_rx_msg(uint8_t *payload, uint8_t payload_sz)
{
  /* Check if the rx msg is an msg_err */
  uint8_t *msg_err = payload;
  edhoc_msg_error_t err;
  int8_t msg_err_sz = 0;

  msg_err_sz = edhoc_deserialize_err(&err, msg_err, payload_sz);
  if(msg_err_sz > 0) {
    LOG_ERR("RX MSG_ERR: ");
    print_char_8_err(err.err_info, err.err_info_sz);
    return RX_ERR_MSG;
  }
  if(msg_err_sz == -1) {
    LOG_ERR("RX MSG_ERROR WITH SUITE PROPOSE: ");
    print_char_8_err(err.err_info, err.err_info_sz);
    return RX_ERR_MSG;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
static int8_t
edhoc_check_err_rx_msg_2(uint8_t *payload, uint8_t payload_sz,
                         const edhoc_context_t *ctx)
{
  /* Check if the rx msg is an msg_err */
  uint8_t *msg_err = payload;
  edhoc_msg_error_t err = { 0 };

  int8_t msg_err_sz = edhoc_deserialize_err(&err, msg_err, payload_sz);
  if(msg_err_sz < 0) {
    LOG_ERR("RX MSG_ERR: ");
    print_char_8_err(err.err_info, err.err_info_sz);
    return RX_ERR_MSG;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
int
edhoc_handler_msg_1(edhoc_context_t *ctx, uint8_t *payload,
                    size_t payload_sz, uint8_t *ad)
{
  edhoc_msg_1_t msg1 = { 0 };
  int er = 0;

  /* Decode MSG1 */
  set_rx_msg(ctx, payload, payload_sz);

  /* Check if the rx msg is an msg_err */
  er = edhoc_check_err_rx_msg(payload, payload_sz);
  if(er < 0) {
    return RX_ERR_MSG;
  } else if(er == 2) {
    return ERR_NEW_SUITE_PROPOSE;
  }

  LOG_DBG("MSG1 (%d bytes): ", (int)ctx->buffers.rx_sz - 1);
  print_buff_8_dbg((ctx->buffers.msg_rx) + 1, ctx->buffers.rx_sz - 1);
  er = edhoc_deserialize_msg_1(&msg1, (ctx->buffers.msg_rx) + 1,
                               ctx->buffers.rx_sz - 1);
  if(er < 0) {
    LOG_ERR("MSG1 malformed\n");
    return er;
  }
  print_msg_1(&msg1);

  /* check rx suite and set connection identifier of the other peer */
  er = check_rx_suite_i(ctx, msg1.suites_i, msg1.suites_i_sz);
  if(er < 0) {
    LOG_ERR("Rx Suite not supported\n");
    return er;
  }

  /* Check to not have the same cid */
  er = set_rx_cid(ctx, msg1.c_i, EDHOC_CID_LEN);
  if(er < 0) {
    LOG_ERR("Not support cid rx\n");
    return er;
  }

  /* Set EDHOC method */
  er = set_rx_method(ctx, msg1.method);
  if(er < 0) {
    LOG_ERR("Rx Method not supported\n");
    return er;
  }

  /* Set GX */
  set_rx_gx(ctx, msg1.g_x);
  edhoc_print_session_info(ctx);

  LOG_DBG("MSG EAD (%d)", (int)msg1.uad.ead_value_sz);
  print_char_8_dbg((char *)msg1.uad.ead_value, msg1.uad.ead_value_sz);

  if(msg1.uad.ead_value_sz != 0) {
    memcpy(ad, msg1.uad.ead_value, msg1.uad.ead_value_sz);
  }

  return msg1.uad.ead_value_sz;
}
/*----------------------------------------------------------------------------*/
int
edhoc_handler_msg_2(edhoc_msg_2_t *msg2, edhoc_context_t *ctx,
                    uint8_t *payload, size_t payload_sz)
{
  int er = 0;
  set_rx_msg(ctx, payload, payload_sz);
  er = edhoc_check_err_rx_msg_2(payload, payload_sz, ctx);
  if(er < 0) {
    LOG_DBG("MSG2 err: %d\n", er);
    return er;
  }
  er = edhoc_deserialize_msg_2(msg2, ctx->buffers.msg_rx, ctx->buffers.rx_sz);
  if(er < 0) {
    LOG_ERR("MSG2 malformed\n");
    return er;
  }
  print_msg_2(msg2);

  set_rx_gx(ctx, msg2->gy_ciphertext_2);
  edhoc_gen_th2(ctx, msg2->gy_ciphertext_2, ctx->buffers.msg_tx,
                ctx->buffers.tx_sz);
  edhoc_gen_prk_2e(ctx);

  /* Gen KS_2e */
  assert(msg2->gy_ciphertext_2_sz > ECC_KEY_LEN);
  int ciphertext2_sz = msg2->gy_ciphertext_2_sz - ECC_KEY_LEN;
  uint8_t ks_2e[ciphertext2_sz];
  edhoc_gen_ks_2e(ctx, ciphertext2_sz, ks_2e);

  /* Prepare ciphertext for decryption */
  memcpy(ctx->buffers.plaintext, msg2->gy_ciphertext_2 + ECC_KEY_LEN,
         ciphertext2_sz);
  LOG_DBG("CIPHERTEXT_2 (%d bytes): ", ciphertext2_sz);
  print_buff_8_dbg(ctx->buffers.plaintext, ciphertext2_sz);

  /* Actually decrypt the ciphertext */
  size_t plaint_sz = edhoc_enc_dec_ciphertext_2(ctx, ks_2e,
                                                ctx->buffers.plaintext,
                                                ciphertext2_sz);
  ctx->buffers.plaintext_sz = plaint_sz;
  LOG_DBG("PLAINTEXT_2 (%zu bytes): ", plaint_sz);
  print_buff_8_dbg(ctx->buffers.plaintext, plaint_sz);

  int cr_sz = EDHOC_CID_LEN;
  er = set_rx_cid(ctx, ctx->buffers.plaintext, cr_sz);
  if(er < 0) {
    return er;
  }
  LOG_DBG("cid (%d)\n", (uint8_t)ctx->state.cid_rx);

  return 1;
}
/*----------------------------------------------------------------------------*/
int
edhoc_handler_msg_3(edhoc_msg_3_t *msg3, edhoc_context_t *ctx,
                    uint8_t *payload, size_t payload_sz)
{
  /* Decode MSG3 */
  set_rx_msg(ctx, payload, payload_sz);

  /* Check if the rx msg is an msg_err */
  if(edhoc_check_err_rx_msg(payload, payload_sz) < 0) {
    return RX_ERR_MSG;
  }

  /* FIXME: Improve skipping of C_R */
  int8_t er = edhoc_deserialize_msg_3(msg3, (ctx->buffers.msg_rx) + 1,
                                      ctx->buffers.rx_sz - 1);
  if(er < 0) {
    LOG_ERR("MSG3 malformed\n");
    return er;
  }
  print_msg_3(msg3);

  LOG_DBG("CIPHERTEXT_3 (%d bytes): ", (int)msg3->ciphertext_3_sz);
  print_buff_8_dbg(msg3->ciphertext_3, msg3->ciphertext_3_sz);

  /* generate TH_3 */
  edhoc_gen_th3(ctx, ctx->buffers.cred_x, ctx->buffers.cred_x_sz,
                ctx->buffers.plaintext, ctx->buffers.plaintext_sz);

  /* decrypt msg3 and check the TAG for verify the outer */
  uint16_t plaintext_sz = decrypt_ciphertext_3(ctx, msg3->ciphertext_3,
                                               msg3->ciphertext_3_sz,
                                               ctx->buffers.plaintext);
  ctx->buffers.plaintext_sz = plaintext_sz;
  if(plaintext_sz == 0) {
    LOG_ERR("Error in decrypt ciphertext 3\n");
    return ERR_DECRYPT;
  }
  LOG_DBG("PLAINTEXT_3 (%d): ", (int)plaintext_sz);
  print_buff_8_dbg(ctx->buffers.plaintext, plaintext_sz);

  return 1;
}
/*----------------------------------------------------------------------------*/
