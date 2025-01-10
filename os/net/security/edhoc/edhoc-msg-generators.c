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
 *         EDHOC, an implementation of Ephemeral Diffie-Hellman Over COSE (EDHOC) (IETF RFC9528)
 * \author
 *         Lidia Pocero <pocero@isi.gr>
 *         Peter A Jonsson
 *         Rikard Höglund
 *         Marco Tiloca
 */

#include "contiki.h"
#include "lib/sha-256.h"
#include "edhoc.h"
#include "edhoc-config.h"
#include "edhoc-msgs.h"
#include "edhoc-msg-generators.h"
#include "cose.h"
#include "cbor.h"
#include <assert.h>

/*----------------------------------------------------------------------------*/
static uint8_t
gen_mac(const edhoc_context_t *ctx, uint8_t mac_len, uint8_t *mac)
{
  uint8_t mac_num;
  if(EDHOC_ROLE == EDHOC_INITIATOR) {
    mac_num = EDHOC_MAC_3;
  } else if(EDHOC_ROLE == EDHOC_RESPONDER) {
    mac_num = EDHOC_MAC_2;
  }

  if(!edhoc_calc_mac(ctx, mac_num, mac_len, mac)) {
    LOG_ERR("Set MAC error\n");
    return 0;
  }

  return mac_len;
}
static uint16_t
gen_plaintext(edhoc_context_t *ctx, const uint8_t *ad, size_t ad_sz,
              bool msg2, const uint8_t *mac_or_sig,
              uint8_t mac_or_signature_sz, uint8_t *plaintext_buf)
{
  uint8_t *pint = (ctx->buffers.id_cred_x);
  uint8_t *pout = plaintext_buf;
  uint8_t num = edhoc_get_maps_num(&pint);
  uint8_t *buf_ptr = &(plaintext_buf[0]);

  size_t size;
  if(msg2) {
    size = edhoc_put_byte_identifier(&buf_ptr, (uint8_t *)&ctx->state.cid,
                                     EDHOC_CID_LEN);
  } else {
    size = 0;
  }

  if(num == 1) {
    num = (uint8_t)edhoc_get_unsigned(&pint);
    size_t sz = edhoc_get_bytes(&pint, &pout);
    if(sz == 0) {
      LOG_ERR("error to get bytes\n");
      return 0;
    }
    /* Check if to use compact encoding */
    if(sz == 1 && (pout[0] < 0x18 || (0x20 <= pout[0] && pout[0] <= 0x37))) {
      size += cbor_put_num(&buf_ptr, pout[0]);
    } else {
      size += cbor_put_bytes(&buf_ptr, pout, sz);
    }
  } else {
    memcpy(buf_ptr, ctx->buffers.id_cred_x, ctx->buffers.id_cred_x_sz);
    size += ctx->buffers.id_cred_x_sz;
  }

  size += cbor_put_bytes(&buf_ptr, &(mac_or_sig[0]), mac_or_signature_sz);
  if(ad_sz != 0) {
    size += cbor_put_bytes(&buf_ptr, ad, ad_sz);
  }

  return size;
}
/*----------------------------------------------------------------------------*/
static uint16_t
gen_ciphertext_3(edhoc_context_t *ctx, const uint8_t *ad, uint16_t ad_sz,
                 const uint8_t *mac_or_sig, uint16_t mac_sz,
                 uint8_t *ciphertext)
{
  cose_encrypt0 *cose = cose_encrypt0_new();

  /* set external AAD in cose */
  uint8_t *th3_ptr = cose->external_aad;
  cose->external_aad_sz = HASH_LEN;
  memcpy(th3_ptr, ctx->state.th, HASH_LEN);

  cose->plaintext_sz = gen_plaintext(ctx, ad, ad_sz, false, mac_or_sig,
                                     mac_sz, cose->plaintext);
  LOG_DBG("PLAINTEXT_3 (%d bytes): ", (int)cose->plaintext_sz);
  print_buff_8_dbg(cose->plaintext, cose->plaintext_sz);

  /* Save plaintext_3 for TH_3 */
  memcpy(ctx->buffers.plaintext, cose->plaintext, cose->plaintext_sz);
  ctx->buffers.plaintext_sz = cose->plaintext_sz;

  /* generate K_3 */
  cose->alg = ctx->config.aead_alg;
  cose->key_sz = cose_get_key_len(cose->alg);
  int16_t er = edhoc_kdf(ctx->state.prk_3e2m, K_3_LABEL, ctx->state.th,
                         HASH_LEN, cose->key_sz, cose->key);
  if(er < 1) {
    LOG_ERR("error in expand for decrypt ciphertext 3\n");
    return 0;
  }
  LOG_DBG("K_3 (%d bytes): ", (int)cose->key_sz);
  print_buff_8_dbg(cose->key, cose->key_sz);

  /* generate IV_3 */
  uint8_t iv_len = cose_get_iv_len(cose->alg);
  er = edhoc_kdf(ctx->state.prk_3e2m, IV_3_LABEL, ctx->state.th, HASH_LEN,
                 iv_len, cose->nonce);
  if(er < 1) {
    LOG_ERR("error in expand for decrypt ciphertext 3\n");
    return 0;
  }
  cose->nonce_sz = iv_len;
  LOG_DBG("IV_3 (%d bytes): ", (int)cose->nonce_sz);
  print_buff_8_dbg(cose->nonce, cose->nonce_sz);

  /* COSE encrypt0 set header */
  cose_encrypt0_set_header(cose, NULL, 0, NULL, 0);

  /* Encrypt COSE */
  cose_encrypt(cose);

  uint8_t *ptr = ciphertext;
  uint16_t ext = cbor_put_bytes(&ptr, cose->ciphertext, cose->ciphertext_sz);

  /* Free memory */
  cose_encrypt0_finalize(cose);
  return ext;
}
/*----------------------------------------------------------------------------*/
void
edhoc_gen_msg_1(edhoc_context_t *ctx, uint8_t *ad, size_t ad_sz, bool suite_array)
{
  /* Generate message 1 */
  edhoc_msg_1_t msg1 = {
    .method = ctx->config.method,
    .suites_i = ctx->config.suite,
    .suites_i_sz = ctx->config.suite_num,
    .g_x = (uint8_t *)&ctx->creds.ephemeral_key.pub.x,
    .c_i = (uint8_t *)&ctx->state.cid,
    .uad = { .ead_label = 0, .ead_value = ad, .ead_value_sz = ad_sz },
  };

  /* CBOR encode message in the buffer */
  size_t size = edhoc_serialize_msg_1(&msg1, (ctx->buffers.msg_tx) + 1, suite_array);
  ctx->buffers.tx_sz = size + 1;
  (ctx->buffers.msg_tx)[0] = 0xF5; /*FIXME: Improve pre-pending of CBOR true (do in client/server?) */

  LOG_DBG("C_I chosen by Initiator (%d bytes): 0x", EDHOC_CID_LEN);
  print_buff_8_dbg(msg1.c_i, EDHOC_CID_LEN);
  LOG_DBG("AD_1 (%d bytes): ", (int)ad_sz);
  print_char_8_dbg((char *)ad, ad_sz);
  for(int i = 0; i < msg1.suites_i_sz; ++i) {
    LOG_DBG("SUITES_I[%d]: %d\n", i, (int)msg1.suites_i[i]);
  }

  LOG_DBG("message_1 (CBOR Sequence) (%d bytes): ", (int)ctx->buffers.tx_sz);
  print_buff_8_dbg(ctx->buffers.msg_tx, ctx->buffers.tx_sz);
  LOG_INFO("MSG1 sz: %d\n", (int)ctx->buffers.tx_sz);
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_gen_msg_2(edhoc_context_t *ctx, const uint8_t *ad, size_t ad_sz)
{
  int8_t rv = edhoc_gen_th2(ctx, ctx->creds.ephemeral_key.pub.x,
                            ctx->buffers.msg_rx, ctx->buffers.rx_sz);
  if(rv < 0) {
    LOG_ERR("Failed to generate TH_2 (%d)\n", rv);
    return -1;
  }

  /* generate cred_x and id_cred_x */
  ctx->buffers.cred_x_sz = edhoc_generate_cred_x(ctx->creds.authen_key,
						 ctx->buffers.cred_x);
  LOG_DBG("CRED_R (%d bytes): ", (int)ctx->buffers.cred_x_sz);
  print_buff_8_dbg(ctx->buffers.cred_x, ctx->buffers.cred_x_sz);

  ctx->buffers.id_cred_x_sz = edhoc_generate_id_cred_x(ctx->creds.authen_key,
						       ctx->buffers.id_cred_x);
  LOG_DBG("ID_CRED_R (%d bytes): ", (int)ctx->buffers.id_cred_x_sz);
  print_buff_8_dbg(ctx->buffers.id_cred_x, ctx->buffers.id_cred_x_sz);

  edhoc_gen_prk_2e(ctx);

  uint8_t mac_or_signature_sz = -1;

  /* Generate MAC or Signature */

#if (EDHOC_METHOD == EDHOC_METHOD1) || (EDHOC_METHOD == EDHOC_METHOD3)
  /* generate prk_3e2m */
  edhoc_gen_prk_3e2m(ctx, &ctx->creds.authen_key->ecc, 1);

  uint8_t edhoc_mac_len = ctx->config.mac_len;
  uint8_t mac_or_sig[edhoc_mac_len];
  gen_mac(ctx, edhoc_mac_len, mac_or_sig);
  LOG_DBG("MAC_2 (%d bytes): ", edhoc_mac_len);
  print_buff_8_dbg(mac_or_sig, edhoc_mac_len);
  mac_or_signature_sz = edhoc_mac_len;
#endif

#if (EDHOC_METHOD == EDHOC_METHOD0) || (EDHOC_METHOD == EDHOC_METHOD2)

  /* prk_3e2m is prk_2e */
  memcpy(ctx->state.prk_3e2m, ctx->state.prk_2e, HASH_LEN);

  /* Derive MAC with HASH_LEN size (buf fits later signature) */
  uint8_t mac_or_sig[EDHOC_MAC_OR_SIG_BUF_LEN];
  gen_mac(ctx, HASH_LEN, mac_or_sig);
  LOG_DBG("MAC_2 (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(mac_or_sig, HASH_LEN);

  /* Create signature from MAC and other data using COSE_Sign1 */

  /* Protected (ID_CRED_R) */
  cose_sign1 *cose_sign1 = cose_sign1_new();
  cose_sign1_set_header(cose_sign1, ctx->buffers.id_cred_x,
                        ctx->buffers.id_cred_x_sz, NULL, 0);

  /* External AAD (TH_2, CRED_R, ? EAD_2) */
  uint8_t *aad_ptr = cose_sign1->external_aad;
  uint8_t th_sz = cbor_put_bytes(&aad_ptr, ctx->state.th, HASH_LEN);
  memcpy(aad_ptr, ctx->buffers.cred_x, ctx->buffers.cred_x_sz);
  cose_sign1->external_aad_sz = ctx->buffers.cred_x_sz + th_sz;

  /* Payload (MAC_2) */
  uint8_t er = cose_sign1_set_payload(cose_sign1, mac_or_sig, HASH_LEN);
  if(er == 0) {
    LOG_ERR("Failed to set payload in COSE_Sign1 object\n");
    return -1;
  }

  cose_sign1_set_key(cose_sign1, ctx->config.sign_alg,
                     ctx->creds.authen_key->ecc.priv, ECC_KEY_LEN);
  er = cose_sign(cose_sign1);
  if(er == 0) {
    LOG_ERR("Failed to sign for COSE_Sign1 object\n");
    return -1;
  }

  cose_sign1_finalize(cose_sign1);

  LOG_DBG("Signature from COSE_Sign1 (%d bytes): ",
          EDHOC_MAC_OR_SIG_BUF_LEN);
  print_buff_8_dbg(cose_sign1->signature, EDHOC_MAC_OR_SIG_BUF_LEN);

  mac_or_signature_sz = EDHOC_MAC_OR_SIG_BUF_LEN;
  memcpy(mac_or_sig, cose_sign1->signature, cose_sign1->signature_sz);
#endif

  /* Generate and store the plaintext in the session */
  uint16_t plaint_sz = gen_plaintext(ctx, ad, ad_sz, true, mac_or_sig,
                                     mac_or_signature_sz,
                                     ctx->buffers.plaintext);
  LOG_DBG("PLAINTEXT_2 (%d bytes): ", (int)plaint_sz);
  print_buff_8_dbg(ctx->buffers.plaintext, plaint_sz);
  ctx->buffers.plaintext_sz = plaint_sz;

  /* Derive KEYSTREAM_2 */
  uint8_t ks_2e[plaint_sz];
  edhoc_gen_ks_2e(ctx, plaint_sz, ks_2e);

  /* Encrypt the plaintext */
  uint8_t ciphertext[plaint_sz];
  memcpy(ciphertext, ctx->buffers.plaintext, plaint_sz);
  edhoc_enc_dec_ciphertext_2(ctx, ks_2e, ciphertext, plaint_sz);
  LOG_DBG("CIPHERTEXT_2 (%d bytes): ", (int)plaint_sz);
  print_buff_8_dbg(ciphertext, plaint_sz);

  /* Set x and ciphertext in msg_tx */
  uint8_t *ptr = &(ctx->buffers.msg_tx[0]);
  int sz = cbor_put_bytes(&ptr, ctx->creds.ephemeral_key.pub.x, ECC_KEY_LEN);
  memcpy(ptr, ciphertext, plaint_sz);
  ctx->buffers.tx_sz = sz + plaint_sz;
  ctx->buffers.msg_tx[1] = ctx->buffers.tx_sz - 2;
  LOG_INFO("MSG2 sz: %d\n", ctx->buffers.tx_sz);

  return 1;
}
/*----------------------------------------------------------------------------*/
void
edhoc_gen_msg_3(edhoc_context_t *ctx, const uint8_t *ad, size_t ad_sz)
{
  /* gen TH_3 */
  edhoc_gen_th3(ctx, ctx->buffers.cred_x, ctx->buffers.cred_x_sz,
                ctx->buffers.plaintext, ctx->buffers.plaintext_sz);

  cose_print_key(ctx->creds.authen_key);
  LOG_DBG("SK_I (Initiator's private authentication key) (%d bytes): ",
          ECC_KEY_LEN);
  print_buff_8_dbg(ctx->creds.authen_key->ecc.priv, ECC_KEY_LEN);

  LOG_DBG("G_I (x)(Initiator's public authentication key) (%d bytes): ",
          ECC_KEY_LEN);
  print_buff_8_dbg(ctx->creds.authen_key->ecc.pub.x, ECC_KEY_LEN);

  LOG_DBG("(y) (Initiator's public authentication key) (%d bytes): ",
          ECC_KEY_LEN);
  print_buff_8_dbg(ctx->creds.authen_key->ecc.pub.y, ECC_KEY_LEN);

  /* generate cred_x */
  ctx->buffers.cred_x_sz = edhoc_generate_cred_x(ctx->creds.authen_key,
						 ctx->buffers.cred_x);
  LOG_DBG("CRED_I (%d bytes): ", (int)ctx->buffers.cred_x_sz);
  print_buff_8_dbg(ctx->buffers.cred_x, ctx->buffers.cred_x_sz);

  /* generate id_cred_x */
  ctx->buffers.id_cred_x_sz = edhoc_generate_id_cred_x(ctx->creds.authen_key,
						       ctx->buffers.id_cred_x);
  LOG_DBG("ID_CRED_I (%d bytes): ", (int)ctx->buffers.id_cred_x_sz);
  print_buff_8_dbg(ctx->buffers.id_cred_x, ctx->buffers.id_cred_x_sz);

  uint8_t mac_or_signature_sz = -1;

#if ((EDHOC_METHOD == EDHOC_METHOD2) || (EDHOC_METHOD == EDHOC_METHOD3))
  /* Generate prk_4e3m */
  edhoc_gen_prk_4e3m(ctx, &ctx->creds.authen_key->ecc, 0);

  uint8_t edhoc_mac_len = ctx->config.mac_len;
  uint8_t mac_or_sig[edhoc_mac_len];
  gen_mac(ctx, edhoc_mac_len, mac_or_sig);
  LOG_DBG("MAC 3 (%d bytes): ", edhoc_mac_len);
  print_buff_8_dbg(mac_or_sig, edhoc_mac_len);
  mac_or_signature_sz = edhoc_mac_len;
#endif

#if (EDHOC_METHOD == EDHOC_METHOD0) || (EDHOC_METHOD == EDHOC_METHOD1)

  /* prk_4e3m is prk_3e2m */
  memcpy(ctx->state.prk_4e3m, ctx->state.prk_3e2m, HASH_LEN);

  /* Derive MAC with HASH_LEN size (buf fits later signature) */
  uint8_t mac_or_sig[EDHOC_MAC_OR_SIG_BUF_LEN];
  gen_mac(ctx, HASH_LEN, mac_or_sig);
  LOG_DBG("MAC_3 (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(mac_or_sig, HASH_LEN);

  /* Create signature from MAC and other data using COSE_Sign1 */

  /* Protected (ID_CRED_I) */
  cose_sign1 *cose_sign1 = cose_sign1_new();
  cose_sign1_set_header(cose_sign1, ctx->buffers.id_cred_x,
                        ctx->buffers.id_cred_x_sz, NULL, 0);

  /* External AAD (TH_3, CRED_I, ? EAD_3) */
  uint8_t *aad_ptr = cose_sign1->external_aad;
  uint8_t th_sz = cbor_put_bytes(&aad_ptr, ctx->state.th, HASH_LEN);
  memcpy(aad_ptr, ctx->buffers.cred_x, ctx->buffers.cred_x_sz);
  cose_sign1->external_aad_sz = ctx->buffers.cred_x_sz + th_sz;

  /* Payload (MAC_3) */
  uint8_t er = cose_sign1_set_payload(cose_sign1, mac_or_sig, HASH_LEN);
  if(er == 0) {
    LOG_ERR("Failed to set payload in COSE_Sign1 object\n");
    return;
  }

  cose_sign1_set_key(cose_sign1, ctx->config.sign_alg,
                     ctx->creds.authen_key->ecc.priv, ECC_KEY_LEN);
  er = cose_sign(cose_sign1);
  if(er == 0) {
    LOG_ERR("Failed to sign for COSE_Sign1 object\n");
    return;
  }

  cose_sign1_finalize(cose_sign1);

  LOG_DBG("Signature from COSE_Sign1 (%d bytes): ",
          EDHOC_MAC_OR_SIG_BUF_LEN);
  print_buff_8_dbg(cose_sign1->signature, EDHOC_MAC_OR_SIG_BUF_LEN);

  mac_or_signature_sz = EDHOC_MAC_OR_SIG_BUF_LEN;
  memcpy(mac_or_sig, cose_sign1->signature, cose_sign1->signature_sz);
#endif

  /* Gen ciphertext_3 */
  uint16_t ciphertext_sz = gen_ciphertext_3(ctx, ad, ad_sz, mac_or_sig,
                                            mac_or_signature_sz,
                                            (ctx->buffers.msg_tx) + 1);
  /*FIXME: Improve prepending of C_R */
  (ctx->buffers.msg_tx)[0] = (uint8_t)ctx->state.cid_rx;
  ctx->buffers.tx_sz = ciphertext_sz + 1;

  /* Compute TH_4 WIP */
  edhoc_gen_th4(ctx, ctx->buffers.cred_x, ctx->buffers.cred_x_sz,
		ctx->buffers.plaintext, ctx->buffers.plaintext_sz);
}
/*----------------------------------------------------------------------------*/
