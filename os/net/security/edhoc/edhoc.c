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
#include "cose.h"
#include "cbor.h"
#include <assert.h>

#if HASH_LEN != SHA_256_DIGEST_LENGTH
#error Only SHA256 supported. Please update HASH_LEN.
#endif /* HASH_LEN != SHA_256_DIGEST_LENGTH */

edhoc_context_t *edhoc_ctx;

MEMB(edhoc_context_storage, edhoc_context_t, 1);
/*----------------------------------------------------------------------------*/
void
edhoc_storage_init(void)
{
  memb_init(&edhoc_context_storage);
}
/*----------------------------------------------------------------------------*/
edhoc_context_t *
edhoc_new(void)
{
  edhoc_context_t *ctx = memb_alloc(&edhoc_context_storage);
  if(ctx) {
    memset(ctx, 0, sizeof(edhoc_context_t));
  }
  return ctx;
}
/*----------------------------------------------------------------------------*/
void
edhoc_finalize(edhoc_context_t *ctx)
{
  memb_free(&edhoc_context_storage, ctx);
}
/*----------------------------------------------------------------------------*/
void
edhoc_setup_suites(edhoc_context_t *ctx)
{
  /* Reverse order for the suite values */
  ctx->config.suite_num = 0;
  if(EDHOC_SUPPORTED_SUITE_4 > -1) {
    ctx->config.suite[ctx->config.suite_num] = EDHOC_SUPPORTED_SUITE_4;
    ctx->config.suite_num++;
  }
  if(EDHOC_SUPPORTED_SUITE_3 > -1) {
    ctx->config.suite[ctx->config.suite_num] = EDHOC_SUPPORTED_SUITE_3;
    ctx->config.suite_num++;
  }
  if(EDHOC_SUPPORTED_SUITE_2 > -1) {
    ctx->config.suite[ctx->config.suite_num] = EDHOC_SUPPORTED_SUITE_2;
    ctx->config.suite_num++;
  }
  if(EDHOC_SUPPORTED_SUITE_1 > -1) {
    ctx->config.suite[ctx->config.suite_num] = EDHOC_SUPPORTED_SUITE_1;
    ctx->state.suite_selected = EDHOC_SUPPORTED_SUITE_1;
    ctx->config.suite_num++;
  }

  if(ctx->config.suite_num == 0) {
    LOG_ERR("No supported cipher suites set (%d)\n", ERR_SUITE_NON_SUPPORT);
  }
}
/*----------------------------------------------------------------------------*/
static int8_t
get_edhoc_mac_len(uint8_t ciphersuite_id)
{
  switch(ciphersuite_id) {
  case EDHOC_CIPHERSUITE_1:
  case EDHOC_CIPHERSUITE_3:
  case EDHOC_CIPHERSUITE_4:
  case EDHOC_CIPHERSUITE_5:
  case EDHOC_CIPHERSUITE_6:
  case EDHOC_CIPHERSUITE_24:
  case EDHOC_CIPHERSUITE_25:
    return EDHOC_MAC_LEN_16;
  case EDHOC_CIPHERSUITE_0:
  case EDHOC_CIPHERSUITE_2:
    return EDHOC_MAC_LEN_8;
  default:
    LOG_ERR("Invalid EDHOC cipher suite specified when retrieving EDHOC MAC length (%d)\n",
            ERR_SUITE_NON_SUPPORT);
    return 0;
  }
}
/*----------------------------------------------------------------------------*/
static int8_t
get_edhoc_aead_enc_alg(uint8_t ciphersuite_id)
{
  switch(ciphersuite_id) {
  case EDHOC_CIPHERSUITE_1:
  case EDHOC_CIPHERSUITE_3:
    return COSE_ALG_AES_CCM_16_128_128;
  case EDHOC_CIPHERSUITE_0:
  case EDHOC_CIPHERSUITE_2:
    return COSE_ALG_AES_CCM_16_64_128;
  default:
    LOG_ERR("Invalid EDHOC cipher suite specified when retrieving COSE encryption algorithm (%d)\n",
            ERR_SUITE_NON_SUPPORT);
    return 0;
  }
}
/*----------------------------------------------------------------------------*/
static int8_t
get_edhoc_curve(uint8_t ciphersuite_id)
{
  switch(ciphersuite_id) {
  case EDHOC_CIPHERSUITE_2:
  case EDHOC_CIPHERSUITE_3:
  case EDHOC_CIPHERSUITE_5:
    return EDHOC_CURVE_P256;
  default:
    LOG_ERR("Invalid EDHOC cipher suite specified when retrieving EDHOC curve (%d)\n", ERR_SUITE_NON_SUPPORT);
    return 0;
  }
}
/*----------------------------------------------------------------------------*/
static int8_t
get_edhoc_sign_alg(uint8_t ciphersuite_id)
{
  switch(ciphersuite_id) {
  case EDHOC_CIPHERSUITE_2:
  case EDHOC_CIPHERSUITE_3:
  case EDHOC_CIPHERSUITE_5:
  case EDHOC_CIPHERSUITE_6:
    return ES256;
  default:
    LOG_ERR("Invalid EDHOC cipher suite specified when retrieving EDHOC curve (%d)\n",
            ERR_SUITE_NON_SUPPORT);
    return 0;
  }
}
/*----------------------------------------------------------------------------*/
int8_t
edhoc_set_config_from_suite(edhoc_context_t *ctx, uint8_t suite)
{
  if((ctx->config.ecdh_curve = get_edhoc_curve(suite)) == 0) {
    return 0;
  }

  if((ctx->config.mac_len = get_edhoc_mac_len(ctx->state.suite_selected)) == 0) {
    return 0;
  }

  if((ctx->config.aead_alg = get_edhoc_aead_enc_alg(ctx->state.suite_selected)) == 0) {
    return 0;
  }

  if((ctx->config.sign_alg = get_edhoc_sign_alg(ctx->state.suite_selected)) == 0) {
    return 0;
  }

  return 1;
}
/*----------------------------------------------------------------------------*/
static inline void
compute_th(const uint8_t *in, size_t in_sz,
           uint8_t hash[SHA_256_DIGEST_LENGTH])
{
  sha_256_hash(in, in_sz, hash);
}
/*----------------------------------------------------------------------------*/
size_t
edhoc_generate_cred_x(cose_key_t *cose, uint8_t *cred)
{
  size_t size = 0;
  size += cbor_put_map(&cred, 2);
  size += cbor_put_unsigned(&cred, 2);
  size += cbor_put_text(&cred, cose->identity, cose->identity_sz);
  size += cbor_put_unsigned(&cred, 8);
  size += cbor_put_map(&cred, 1);
  size += cbor_put_unsigned(&cred, 1);
  if(cose->crv == 1) {
    size += cbor_put_map(&cred, 5);
  } else {
    size += cbor_put_map(&cred, 4);
  }

  size += cbor_put_unsigned(&cred, 1);
  size += cbor_put_unsigned(&cred, cose->kty);
  size += cbor_put_unsigned(&cred, 2);
  size += cbor_put_bytes(&cred, cose->kid, cose->kid_sz);
  size += cbor_put_negative(&cred, 1);
  size += cbor_put_unsigned(&cred, cose->crv);
  size += cbor_put_negative(&cred, 2);
  size += cbor_put_bytes(&cred, cose->ecc.pub.x, ECC_KEY_LEN);
  if(cose->crv == 1) {
    size += cbor_put_negative(&cred, 3);
    size += cbor_put_bytes(&cred, cose->ecc.pub.y, ECC_KEY_LEN);
  }
  return size;
}
/*----------------------------------------------------------------------------*/
size_t
edhoc_generate_id_cred_x(cose_key_t *cose, uint8_t *cred)
{
  size_t size = 0;
  LOG_DBG("kid (%i bytes): ", cose->kid_sz);
  print_buff_8_dbg(cose->kid, cose->kid_sz);

  /* Include KID */
  if(EDHOC_AUTHENT_TYPE == EDHOC_CRED_KID) {
    size += cbor_put_map(&cred, 1);
    size += cbor_put_unsigned(&cred, 4);
    size += cbor_put_bytes(&cred, cose->kid, cose->kid_sz);
  }

  /* Include directly the credential used for authentication ID_CRED_X = CRED_X */
  if(EDHOC_AUTHENT_TYPE == EDHOC_CRED_INCLUDE) {
    size = edhoc_generate_cred_x(cose, cred);
  }
  return size;
}
/*----------------------------------------------------------------------------*/
static size_t
generate_info(uint8_t info_label, const uint8_t *context, uint8_t context_sz, uint8_t length, uint8_t *info)
{
  size_t size = cbor_put_num(&info, info_label);
  size += cbor_put_bytes(&info, context, context_sz);
  size += cbor_put_unsigned(&info, length);
  return size;
}
/*----------------------------------------------------------------------------*/
int8_t
edhoc_gen_th2(edhoc_context_t *ctx, const uint8_t *eph_pub,
        uint8_t *msg, uint16_t msg_sz)
{
  /* Create the input for TH_2 = H(G_Y, H(msg)), msg1 is in msg_rx */
  int h_buf_sz = cbor_bytestr_size(HASH_LEN) + cbor_bytestr_size(ECC_KEY_LEN);
  uint8_t h[h_buf_sz];
  uint8_t *h_ptr = h;

  LOG_DBG("Input to calculate H(msg1) (%d bytes): ", (int)msg_sz);
  print_buff_8_dbg(msg, msg_sz);

  uint8_t msg_1_hash[HASH_LEN];
  compute_th(msg + 1, msg_sz - 1, msg_1_hash); /*FIXME: Improve skipping of CBOR true for TH */

  cbor_put_bytes(&h_ptr, eph_pub, ECC_KEY_LEN);
  cbor_put_bytes(&h_ptr, msg_1_hash, HASH_LEN);

  /* Compute TH */
  LOG_DBG("Input to TH_2 (%d): ", h_buf_sz);
  print_buff_8_dbg(h, h_buf_sz);
  compute_th(h, h_buf_sz, ctx->state.th);

  LOG_DBG("TH_2 (%d bytes): ", (int)HASH_LEN);
  print_buff_8_dbg(ctx->state.th, HASH_LEN);
  return 0;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_gen_th3(edhoc_context_t *ctx, const uint8_t *cred, uint16_t cred_sz,
              const uint8_t *plaintext, uint16_t plaintext_sz)
{
  /* TH_3 = H(TH_2, PLAINTEXT_2, CRED_R) */
  int h_buf_sz = cbor_bytestr_size(HASH_LEN) + plaintext_sz + cred_sz;
  uint8_t h[h_buf_sz];
  uint8_t *ptr = h;
  uint16_t h_sz = cbor_put_bytes(&ptr, ctx->state.th, HASH_LEN);
  LOG_DBG("TH_2 (%d): ", HASH_LEN);
  print_buff_8_dbg(ctx->state.th, HASH_LEN);
  memcpy(h + h_sz, plaintext, plaintext_sz);
  h_sz += plaintext_sz;
  LOG_DBG("PLAINTEXT_2 (%d): ", plaintext_sz);
  print_buff_8_dbg(plaintext, plaintext_sz);
  memcpy(h + h_sz, cred, cred_sz);
  h_sz += cred_sz;
  LOG_DBG("CRED_R (%d): ", cred_sz);
  print_buff_8_dbg(cred, cred_sz);
  LOG_DBG("input to calculate TH_3 (CBOR Sequence) (%d bytes): ", (int)h_sz);
  print_buff_8_dbg(h, h_sz);

  /* Compute TH */
  compute_th(h, h_sz, ctx->state.th);
  LOG_DBG("TH_3 (%d bytes): ", (int)HASH_LEN);
  print_buff_8_dbg(ctx->state.th, HASH_LEN);
  return 0;
}
/*----------------------------------------------------------------------------*/uint8_t
edhoc_gen_th4(edhoc_context_t *ctx, const uint8_t *cred, uint16_t cred_sz,
	      const uint8_t *plaintext, uint16_t plaintext_sz)
{
  /* TH_4 = H(TH_3, PLAINTEXT_3, CRED_I) */
  int h_buf_sz = cbor_bytestr_size(HASH_LEN) + plaintext_sz + cred_sz;
  uint8_t h[h_buf_sz];
  uint8_t *ptr = h;
  uint16_t h_sz = cbor_put_bytes(&ptr, ctx->state.th, HASH_LEN);

  LOG_DBG("TH_3 (%d): ", HASH_LEN);
  print_buff_8_dbg(ctx->state.th, HASH_LEN);

  memcpy(h + h_sz, plaintext, plaintext_sz);
  h_sz += plaintext_sz;
  LOG_DBG("PLAINTEXT_3 (%d): ", plaintext_sz);
  print_buff_8_dbg(plaintext, plaintext_sz);

  memcpy(h + h_sz, cred, cred_sz);
  h_sz += cred_sz;
  LOG_DBG("CRED_I (%d): ", cred_sz);
  print_buff_8_dbg(cred, cred_sz);
  LOG_DBG("input to calculate TH_4 (CBOR Sequence) (%d bytes): ", (int)h_sz);
  print_buff_8_dbg(h, h_sz);

  /* Compute TH */
  compute_th(h, h_sz, ctx->state.th);
  LOG_DBG("TH_4 (%d bytes): ", (int)HASH_LEN);
  print_buff_8_dbg(ctx->state.th, HASH_LEN);
  return 0;
}
/*----------------------------------------------------------------------------*/
void
edhoc_print_session_info(const edhoc_context_t *ctx)
{
  LOG_DBG("Session info print:\n");
  LOG_DBG("Using test vector: %s\n",
          EDHOC_TEST == EDHOC_TEST_VECTOR_TRACE_DH ? "true (DH)" : "false");
  LOG_DBG("Connection role: %d\n", (int)ctx->config.role);
  LOG_DBG("Connection method: %d\n", (int)ctx->config.method);
  LOG_DBG("Selected cipher suite: %d\n", ctx->state.suite_selected);
  LOG_DBG("My cID: %x\n", (uint8_t)ctx->state.cid);
  LOG_DBG("Other peer cID: %x\n", (uint8_t)ctx->state.cid_rx);
  LOG_DBG("Gx: ");
  print_buff_8_dbg(ctx->state.gx, ECC_KEY_LEN);
}
/*----------------------------------------------------------------------------*/
int16_t
edhoc_kdf(const uint8_t *prk, uint8_t info_label, const uint8_t *context,
          uint8_t context_sz, uint16_t length, uint8_t *result)
{
  size_t info_buf_sz = cbor_int_size(info_label) +
    cbor_bytestr_size(context_sz) + cbor_int_size(length);
  uint8_t info_buf[info_buf_sz];
  uint16_t info_sz = generate_info(info_label, context, context_sz,
                                   length, info_buf);
  if(info_sz == 0) {
    LOG_ERR("Error generating INFO");
    return info_sz;
  }

  return edhoc_expand(prk, info_buf, info_sz, length, result);
}
/*----------------------------------------------------------------------------*/
int16_t
edhoc_expand(const uint8_t *prk, const uint8_t *info, uint16_t info_sz,
             uint16_t length, uint8_t *result)
{
  LOG_DBG("INFO for HKDF_Expand (%d bytes): ", info_sz);
  print_buff_8_dbg(info, info_sz);
  sha_256_hkdf_expand(prk, ECC_KEY_LEN, info, info_sz, result, length);
  return length;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_calc_mac(const edhoc_context_t *ctx, uint8_t mac_num,
	       uint8_t mac_len, uint8_t *mac)
{
  if(mac_num == EDHOC_MAC_2) {
    /* Build context_2 */
    size_t context_2_buf_sz = EDHOC_CID_LEN + ctx->buffers.id_cred_x_sz +
      cbor_bytestr_size(HASH_LEN) + ctx->buffers.cred_x_sz;
    uint8_t context_2[context_2_buf_sz];
    uint8_t *context_2_ptr = context_2;

    /* Add C_R */
    if(EDHOC_ROLE == EDHOC_INITIATOR) {
      context_2_ptr[0] = (uint8_t)ctx->state.cid_rx;
    } else {
      context_2_ptr[0] = (uint8_t)ctx->state.cid;
    }
    context_2_ptr += EDHOC_CID_LEN;

    memcpy(context_2_ptr, ctx->buffers.id_cred_x, ctx->buffers.id_cred_x_sz);
    context_2_ptr += ctx->buffers.id_cred_x_sz;

    cbor_put_bytes(&context_2_ptr, ctx->state.th, HASH_LEN);

    memcpy(context_2_ptr, ctx->buffers.cred_x, ctx->buffers.cred_x_sz);
    context_2_ptr += ctx->buffers.cred_x_sz;

    LOG_DBG("CONTEXT_2 (%zu bytes): ", context_2_buf_sz);
    print_buff_8_dbg(context_2, context_2_buf_sz);

    /* Use edhoc_kdf to generate MAC_2 */
    int16_t er = edhoc_kdf(ctx->state.prk_3e2m, MAC_2_LABEL,
                           context_2, context_2_buf_sz, mac_len, mac);
    if(er < 0) {
      LOG_ERR("Failed to expand MAC_2\n");
      return 0;
    }
  } else if(mac_num == EDHOC_MAC_3) {
    /* Build context_3 */
    size_t context_3_buf_sz = ctx->buffers.id_cred_x_sz +
      cbor_bytestr_size(HASH_LEN) + ctx->buffers.cred_x_sz;
    uint8_t context_3[context_3_buf_sz];
    uint8_t *context_3_ptr = context_3;

    memcpy(context_3_ptr, ctx->buffers.id_cred_x, ctx->buffers.id_cred_x_sz);
    context_3_ptr += ctx->buffers.id_cred_x_sz;

    cbor_put_bytes(&context_3_ptr, ctx->state.th, HASH_LEN);

    memcpy(context_3_ptr, ctx->buffers.cred_x, ctx->buffers.cred_x_sz);
    context_3_ptr += ctx->buffers.cred_x_sz;

    LOG_DBG("CONTEXT_3 (%zu bytes): ", context_3_buf_sz);
    print_buff_8_dbg(context_3, context_3_buf_sz);

    /* Use edhoc_kdf to generate MAC_3 */
    int16_t er = edhoc_kdf(ctx->state.prk_4e3m, MAC_3_LABEL,
                           context_3, context_3_buf_sz, mac_len, mac);
    if(er < 0) {
      LOG_ERR("Failed to expand MAC_3\n");
      return 0;
    }
  } else {
    LOG_ERR("Wrong MAC value\n");
    return 0;
  }

  return 1;
}
/*----------------------------------------------------------------------------*/
#if (EDHOC_METHOD == EDHOC_METHOD3) || INITIATOR_METHOD1 || RESPONDER_METHOD2
static uint16_t
check_mac(const edhoc_context_t *ctx, const uint8_t *received_mac,
          uint16_t received_mac_sz)
{
  uint8_t mac_num;
  if(EDHOC_ROLE == EDHOC_INITIATOR) {
    mac_num = EDHOC_MAC_2;
  } else if(EDHOC_ROLE == EDHOC_RESPONDER) {
    mac_num = EDHOC_MAC_3;
  }

  uint8_t edhoc_mac_len = ctx->config.mac_len;
  uint8_t mac[edhoc_mac_len];
  if(!edhoc_calc_mac(ctx, mac_num, edhoc_mac_len, mac)) {
    LOG_ERR("Set MAC error\n");
    return 0;
  }

  LOG_DBG("Received MAC (%d): ", (int)received_mac_sz);
  print_buff_8_dbg(received_mac, received_mac_sz);

  LOG_DBG("Recalculated MAC (%d): ", (int)edhoc_mac_len);
  print_buff_8_dbg(mac, edhoc_mac_len);

  /* Verify the MAC value */
  uint16_t mac_sz = edhoc_mac_len;
  uint8_t diff = 0;
  for(int i = 0; i < edhoc_mac_len; i++) {
    diff |= (mac[i] ^ received_mac[i]);
  }

  if(diff != 0) {
    LOG_ERR("error code in check mac (%d)\n", ERR_AUTHENTICATION);
    return 0;
  }

  return mac_sz;
}
#endif /* (EDHOC_METHOD == EDHOC_METHOD3) || INITIATOR_METHOD1 || RESPONDER_METHOD2 */
/*----------------------------------------------------------------------------*/
static bool
gen_gxy(edhoc_context_t *ctx, uint8_t *ikm)
{
  bool success = ecdh_generate_ikm(ctx->config.ecdh_curve,
                                   ctx->state.gx,
                                   ctx->state.gy,
                                   ctx->creds.ephemeral_key.priv,
                                   ikm);
  if(!success) {
    LOG_ERR("error in generate shared secret\n");
    return false;
  }
  LOG_DBG("GXY (%d bytes): ", ECC_KEY_LEN);
  print_buff_8_dbg(ikm, ECC_KEY_LEN);
  return true;
}
/*----------------------------------------------------------------------------*/
bool
edhoc_gen_prk_2e(edhoc_context_t *ctx)
{
  uint8_t ikm[ECC_KEY_LEN];

  watchdog_periodic();
  bool success = gen_gxy(ctx, ikm);
  watchdog_periodic();
  if(!success) {
    return false;
  }
  sha_256_hkdf_extract(ctx->state.th, HASH_LEN, ikm, ECC_KEY_LEN,
                       ctx->state.prk_2e);
  LOG_DBG("PRK_2e (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(ctx->state.prk_2e, HASH_LEN);
  return true;
}
/*----------------------------------------------------------------------------*/
/* Derive KEYSTREAM_2 */
int16_t
edhoc_gen_ks_2e(edhoc_context_t *ctx, uint16_t length, uint8_t *ks_2e)
{
  int er = edhoc_kdf(ctx->state.prk_2e, KEYSTREAM_2_LABEL,
                     ctx->state.th, HASH_LEN, length, ks_2e);
  if(er < 0) {
    return er;
  }
  LOG_DBG("KEYSTREAM_2 (%d bytes): ", length);
  print_buff_8_dbg(ks_2e, length);
  return 1;
}
/*----------------------------------------------------------------------------*/
#if (EDHOC_METHOD == EDHOC_METHOD3) || INITIATOR_METHOD1 || RESPONDER_METHOD2
bool
edhoc_gen_prk_3e2m(edhoc_context_t *ctx, const ecc_key_t *auth_key, uint8_t gen)
{
  uint8_t grx[ECC_KEY_LEN];
  bool success;

  if(gen) {
    success = ecdh_generate_ikm(ctx->config.ecdh_curve,
                                ctx->state.gx,
                                ctx->state.gy,
                                auth_key->priv,
                                grx);
  } else {
    success = ecdh_generate_ikm(ctx->config.ecdh_curve,
                                auth_key->pub.x,
                                auth_key->pub.y,
                                ctx->creds.ephemeral_key.priv,
                                grx);
  }
  if(!success) {
    LOG_ERR("error in generate shared secret for prk_3e2m\n");
    return false;
  }

  /* Use edhoc_kdf to generate SALT_3e2m */
  uint8_t salt[HASH_LEN];
  int16_t er = edhoc_kdf(ctx->state.prk_2e, SALT_3E2M_LABEL, ctx->state.th,
                         HASH_LEN, HASH_LEN, salt);
  if(er < 1) {
    LOG_ERR("Error calculating SALT_3e2m (%d)\n", er);
    return false;
  }
  LOG_DBG("SALT_3e2m (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(salt, HASH_LEN);

  sha_256_hkdf_extract(salt, HASH_LEN, grx, ECC_KEY_LEN, ctx->state.prk_3e2m);
  LOG_DBG("PRK_3e2m (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(ctx->state.prk_3e2m, HASH_LEN);
  return true;
}
#endif /* (EDHOC_METHOD == EDHOC_METHOD3) || INITIATOR_METHOD1 || RESPONDER_METHOD2 */
/*----------------------------------------------------------------------------*/
#if (EDHOC_METHOD == EDHOC_METHOD2) || (EDHOC_METHOD == EDHOC_METHOD3) || INITIATOR_METHOD1 || RESPONDER_METHOD2
bool
edhoc_gen_prk_4e3m(edhoc_context_t *ctx, const ecc_key_t *auth_key, uint8_t gen)
{
  uint8_t giy[ECC_KEY_LEN];
  bool success;

  if(gen) {
    success = ecdh_generate_ikm(ctx->config.ecdh_curve,
                                auth_key->pub.x,
                                auth_key->pub.y,
                                ctx->creds.ephemeral_key.priv,
                                giy);
  } else {
    success = ecdh_generate_ikm(ctx->config.ecdh_curve,
                                ctx->state.gx,
                                ctx->state.gy,
                                auth_key->priv,
                                giy);
  }
  if(!success) {
    LOG_ERR("error in generate shared secret for prk_4e3m\n");
    return false;
  }
  LOG_DBG("G_IY (ECDH shared secret) (%d bytes): ", ECC_KEY_LEN);
  print_buff_8_dbg(giy, ECC_KEY_LEN);

  /* Use edhoc_kdf to generate SALT_4e3m */
  uint8_t salt[HASH_LEN];
  int16_t er = edhoc_kdf(ctx->state.prk_3e2m, SALT_4E3M_LABEL, ctx->state.th,
                         HASH_LEN, HASH_LEN, salt);
  if(er < 1) {
    LOG_ERR("Error calculating SALT_4e3m (%d)\n", er);
    return false;
  }
  LOG_DBG("SALT_4e3m (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(salt, HASH_LEN);

  sha_256_hkdf_extract(salt, HASH_LEN, giy, ECC_KEY_LEN, ctx->state.prk_4e3m);
  LOG_DBG("PRK_4e3m (%d bytes): ", HASH_LEN);
  print_buff_8_dbg(ctx->state.prk_4e3m, HASH_LEN);
  return true;
}
#endif /* (EDHOC_METHOD == EDHOC_METHOD2) || (EDHOC_METHOD == EDHOC_METHOD3) ||
INITIATOR_METHOD1 || RESPONDER_METHOD2 */
/*----------------------------------------------------------------------------*/
int16_t
edhoc_enc_dec_ciphertext_2(const edhoc_context_t *ctx, const uint8_t *ks_2e,
                           uint8_t *plaintext, uint16_t plaintext_sz)
{
  LOG_DBG("**** Cipher/Plaintext in enc func (%d bytes): ", plaintext_sz);
  print_buff_8_dbg(plaintext, plaintext_sz);

  LOG_DBG("**** ks_2e in enc func (%d bytes): ", plaintext_sz);
  print_buff_8_dbg(ks_2e, plaintext_sz);

  for(int i = 0; i < plaintext_sz; i++) {
    plaintext[i] = plaintext[i] ^ ks_2e[i];
  }

  LOG_DBG("**** Plain/Ciphertext in enc func (%d bytes): ", plaintext_sz);
  print_buff_8_dbg(plaintext, plaintext_sz);

  return plaintext_sz;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_initialize_context(edhoc_context_t *ctx)
{
  /* Retrieve a pointer to own auth key */
  cose_key_t *key = NULL;
  if(!edhoc_get_own_auth_key(ctx, &key)) {
    return 0;
  }

  /* Set pointer to found key */
  ctx->creds.authen_key = key;

  /* Set up the cipher suites selection logic */
  edhoc_setup_suites(ctx);

  /* Set CID */
  ctx->state.cid = EDHOC_CID;

  /* Set role and method */
  ctx->config.role = EDHOC_ROLE;
  ctx->config.method = EDHOC_METHOD;

  /* Initiator sets config to use based on selected suite */
  int8_t er = edhoc_set_config_from_suite(ctx, ctx->state.suite_selected);
  if(er != 1) {
    return 0;
  }

  return 1;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_get_own_auth_key(edhoc_context_t *ctx, cose_key_t **key)
{
#ifdef EDHOC_AUTH_SUBJECT_NAME
  if(edhoc_check_key_list_identity(EDHOC_AUTH_SUBJECT_NAME,
                                   strlen(EDHOC_AUTH_SUBJECT_NAME), key)) {
    /* Key found using identity */
    return 1;
  } else {
    LOG_ERR("Does not contain a key for the authentication key identity\n");
  }
#endif

#ifdef EDHOC_AUTH_KID
  if(*key == NULL) {
    uint8_t key_id[sizeof(int)];
    int kid = EDHOC_AUTH_KID;
    int quotient = (EDHOC_AUTH_KID) / 256;
    uint8_t key_id_sz = 1;
    while(quotient != 0) {
      key_id_sz++;
      quotient /= 256;
    }
    memcpy(key_id, (uint8_t *)&kid, key_id_sz);

    if(edhoc_check_key_list_kid(key_id, key_id_sz, key)) {
      /* Key found using KID */
      return 1;
    } else {
      LOG_ERR("Does not contain a key for the key ID\n");
    }
  }
#endif

  LOG_ERR("No matching key found in the storage\n");
  return 0;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_gen_msg_error(uint8_t *msg_er, const edhoc_context_t *ctx, int8_t err)
{
  edhoc_msg_error_t msg;
  msg.err_code = 1;
  switch(err * (-1)) {
  default:
    msg.err_info = "ERR_UNKNOWN";
    msg.err_info_sz = strlen("ERR_UNKNOWN");
    break;
  case (ERR_SUITE_NON_SUPPORT * (-1)):
    msg.err_info = "ERR_SUITE_NON_SUPPORT";
    msg.err_info_sz = strlen("ERR_SUITE_NON_SUPPORT");
    break;
  case (ERR_MSG_MALFORMED * (-1)):
    msg.err_info = "ERR_MSG_MALFORMED";
    msg.err_info_sz = strlen("ERR_MSG_MALFORMED");
    break;
  case (ERR_REJECT_METHOD * (-1)):
    msg.err_info = "ERR_REJECT_METHOD";
    msg.err_info_sz = strlen("ERR_REJECT_METHOD");
    break;
  case (ERR_CID_NOT_VALID * (-1)):
    msg.err_info = "ERR_CID_NOT_VALID";
    msg.err_info_sz = strlen("ERR_CID_NOT_VALID");
    break;
  case (ERR_WRONG_CID_RX * (-1)):
    msg.err_info = "ERR_WRONG_CID_RX";
    msg.err_info_sz = strlen("ERR_WRONG_CID_RX");
    break;
  case (ERR_ID_CRED_X_MALFORMED * (-1)):
    msg.err_info = "ERR_ID_CRED_X_MALFORMED";
    msg.err_info_sz = strlen("ERR_ID_CRED_X_MALFORMED");
    break;
  case (ERR_AUTHENTICATION * (-1)):
    msg.err_info = "ERR_AUTHENTICATION";
    msg.err_info_sz = strlen("ERR_AUTHENTICATION");
    break;
  case (ERR_DECRYPT * (-1)):
    msg.err_info = "ERR_DECRYPT";
    msg.err_info_sz = strlen("ERR_DECRYPT");
    break;
  case (ERR_CODE * (-1)):
    msg.err_info = "ERR_CODE";
    msg.err_info_sz = strlen("ERR_CODE");
    break;
  case (ERR_NOT_ALLOWED_IDENTITY * (-1)):
    msg.err_info = "ERR_NOT_ALLOWED_IDENTITY";
    msg.err_info_sz = strlen("ERR_NOT_ALLOWED_IDENTITY");
    break;
  case (RX_ERR_MSG * (-1)):
    msg.err_info = "RX_ERR_MSG";
    msg.err_info_sz = strlen("RX_ERR_MSG");
    break;
  case (ERR_TIMEOUT * (-1)):
    msg.err_info = "ERR_TIMEOUT";
    msg.err_info_sz = strlen("ERR_TIMEOUT");
    break;
  case (ERR_CORRELATION * (-1)):
    msg.err_info = "ERR_CORRELATION";
    msg.err_info_sz = strlen("ERR_CORRELATION");
    break;
  case (ERR_NEW_SUITE_PROPOSE * (-1)):
    msg.err_code = 2;
    msg.err_info = (char *)ctx->config.suite;
    msg.err_info_sz = ctx->config.suite_num * sizeof(ctx->config.suite[0]);
    break;
  case (ERR_RESEND_MSG_1 * (-1)):
    msg.err_info = "ERR_RESEND_MSG_1";
    msg.err_info_sz = strlen("ERR_RESEND_MSG_1");
    break;
  }

  LOG_ERR("ERR MSG (%d): ", msg.err_code);
  if(msg.err_code == 1) {
    print_char_8_err(msg.err_info, msg.err_info_sz);
  } else {
    printf("\n");
  }

  size_t err_sz = edhoc_serialize_err(&msg, msg_er);
  LOG_DBG("ERR MSG CBOR: ");
  print_buff_8_dbg((uint8_t *)msg_er, err_sz);
  return err_sz;
}
/*----------------------------------------------------------------------------*/
int
edhoc_authenticate_msg(edhoc_context_t *ctx, uint8_t *ad, bool msg2)
{
  /* Point to decrypted plaintext for key retrieval */
  uint8_t *plaintext_ptr = NULL;
  if(msg2) {
    plaintext_ptr = ctx->buffers.plaintext + EDHOC_CID_LEN;
  } else {
    plaintext_ptr = ctx->buffers.plaintext;
  }

  /* Retrieve the peer credential from msg info and cred storage */
  cose_key_t peer_key;
  cose_key_t *key = &peer_key;
  edhoc_get_key_id_cred_x(&plaintext_ptr, NULL, key);

  /* Get MAC from the decrypted message */
  uint8_t *received_mac = NULL;
  uint16_t received_mac_sz = edhoc_get_sign(&plaintext_ptr, &received_mac);

  /* Get the additional data from the decrypted message if present */
  uint16_t ad_sz = 0;
  if(ad_sz) {
    ad_sz = edhoc_get_ad(&plaintext_ptr, ad);
  } else {
    ad = NULL;
  }

  /* generate cred_x and id_cred_x */
  ctx->buffers.cred_x_sz = edhoc_generate_cred_x(key, ctx->buffers.cred_x);
  LOG_DBG("CRED_X auth (%zu): ", ctx->buffers.cred_x_sz);
  print_buff_8_dbg(ctx->buffers.cred_x, ctx->buffers.cred_x_sz);

  ctx->buffers.id_cred_x_sz = edhoc_generate_id_cred_x(key,
						       ctx->buffers.id_cred_x);
  LOG_DBG("ID_CRED_X auth (%zu): ", ctx->buffers.id_cred_x_sz);
  print_buff_8_dbg(ctx->buffers.id_cred_x, ctx->buffers.id_cred_x_sz);

#if (EDHOC_METHOD == EDHOC_METHOD3) || INITIATOR_METHOD1 || RESPONDER_METHOD2
  /* Generate prk_3e2m or prk_4e3m */
  if(msg2 == true) {
    edhoc_gen_prk_3e2m(ctx, &key->ecc, 0);
  } else { /* msg3 */
    edhoc_gen_prk_4e3m(ctx, &key->ecc, 1);
  }

  if(check_mac(ctx, received_mac, received_mac_sz) == 0) {
    LOG_ERR("error code in handler (%d)\n", ERR_AUTHENTICATION);
    return ERR_AUTHENTICATION;
  }
#endif

#if (EDHOC_METHOD == EDHOC_METHOD0) || INITIATOR_METHOD2 || RESPONDER_METHOD1
  if(msg2 == true) {
    /* prk_3e2m is prk_2e */
    memcpy(ctx->state.prk_3e2m, ctx->state.prk_2e, HASH_LEN);
  } else { /* msg3 */
    /* prk_4e3m is prk_3e2m */
    memcpy(ctx->state.prk_4e3m, ctx->state.prk_3e2m, HASH_LEN);
  }

  /* Create signature from MAC and other data using COSE_Sign1 */

  /* Protected (ID_CRED_X) */
  cose_sign1 *cose_sign1 = cose_sign1_new();
  cose_sign1_set_header(cose_sign1, ctx->buffers.id_cred_x,
                        ctx->buffers.id_cred_x_sz, NULL, 0);

  /* External AAD (TH_2/3, CRED_X, ? EAD_2/3) */
  uint8_t *aad_ptr = cose_sign1->external_aad;
  uint8_t th_sz = cbor_put_bytes(&aad_ptr, ctx->state.th, HASH_LEN);
  memcpy(aad_ptr, ctx->buffers.cred_x, ctx->buffers.cred_x_sz);
  cose_sign1->external_aad_sz = ctx->buffers.cred_x_sz + th_sz;

  /* Set received signature */
  cose_sign1_set_signature(cose_sign1, received_mac, received_mac_sz);

  /* Payload (MAC) */
  uint8_t mac_num = -1;
  if(msg2 == true) {
    mac_num = EDHOC_MAC_2;
  } else { /* msg3 */
    mac_num = EDHOC_MAC_3;
  }

  uint8_t mac[HASH_LEN];
  edhoc_calc_mac(ctx, mac_num, HASH_LEN, mac);
  LOG_DBG("MAC_%d (%d bytes): ", mac_num == 2 ? 2 : 3, HASH_LEN);
  print_buff_8_dbg(mac, HASH_LEN);

  int8_t er2 = cose_sign1_set_payload(cose_sign1, mac, HASH_LEN);
  if(er2 < 0) {
    LOG_ERR("Failed to set payload in COSE_Sign1 object\n");
    return ERR_AUTHENTICATION;
  }

  uint8_t other_public_key[ECC_KEY_LEN * 2];
  memcpy(other_public_key, key->ecc.pub.x, ECC_KEY_LEN);
  memcpy(other_public_key + ECC_KEY_LEN, key->ecc.pub.y, ECC_KEY_LEN);

  /* Set other peer public key and verify */
  cose_sign1_set_key(cose_sign1, ctx->config.sign_alg, other_public_key,
                     ECC_KEY_LEN * 2);
  er2 = cose_verify(cose_sign1);
  cose_sign1_finalize(cose_sign1);
  if(er2 <= 0) {
    LOG_ERR("Failed to check signature for COSE_Sign1 object\n");
    return ERR_AUTHENTICATION;
  }
#endif

  /* Compute TH_4 WIP (after verifying MAC_3) */
  if(msg2 == false) { /* msg 3 */
    /* Calculate TH_4 */
    edhoc_gen_th4(ctx, ctx->buffers.cred_x, ctx->buffers.cred_x_sz,
		  ctx->buffers.plaintext, ctx->buffers.plaintext_sz);
  }

  return ad_sz;
}
/*----------------------------------------------------------------------------*/
