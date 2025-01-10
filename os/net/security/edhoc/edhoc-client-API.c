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
 *      EDHOC client API [RFC9528] with CoAP Block-Wise Transfer [RFC7959]
 * \author
 *      Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */

#include "edhoc-client-API.h"
#include "edhoc-msg-generators.h"
#include "edhoc-msg-handlers.h"
#include "lib/memb.h"
#include "contiki-lib.h"
#include "sys/timer.h"
#include "sys/rtimer.h"
#include <assert.h>

/* EDHOC Client protocol states */
#define NON_MSG 0
#define RX_MSG2 4
#define RX_RESPONSE_MSG3 5
#define EXP_READY 6

/* EDHOC process states */
#define CL_TIMEOUT 2
#define CL_RESTART 0
#define CL_FINISHED 1
#define CL_BLOCK1 3
#define CL_POST 4
#define CL_TRIES_EXPIRE 5
#define CL_BLOCKING 7

/* For use of block-wise post and answer */
static coap_callback_request_state_t state;
static uint8_t msg_num;
static uint8_t send_sz;
static uint8_t *rx_ptr;
static size_t rx_sz;
static int pro;
static edhoc_client_t *cli;
static coap_timer_t timer;
static edhoc_data_event_t edhoc_state;
static process_event_t edhoc_event = PROCESS_EVENT_NONE;

static rtimer_clock_t time;
static rtimer_clock_t time_total;
static uint8_t attempt = 0;
static int er = 0;
static edhoc_msg_2_t msg2;

PROCESS(edhoc_client, "EDHOC Client");
PROCESS(edhoc_client_protocol, "EDHOC Client Protocol");

#if EDHOC_TEST == EDHOC_TEST_VECTOR_TRACE_DH
uint8_t eph_pub_x_i[ECC_KEY_LEN] = { 0x8a, 0xf6, 0xf4, 0x30, 0xeb, 0xe1, 0x8d, 0x34, 0x18, 0x40, 0x17, 0xa9, 0xa1, 0x1b, 0xf5, 0x11, 0xc8, 0xdf, 0xf8, 0xf8, 0x34, 0x73, 0x0b,
                                     0x96, 0xc1, 0xb7, 0xc8, 0xdb, 0xca, 0x2f, 0xc3, 0xb6 };

uint8_t eph_pub_y_i[ECC_KEY_LEN] = { 0x51, 0xe8, 0xaf, 0x6c, 0x6e, 0xdb, 0x78, 0x16, 0x01, 0xad, 0x1d, 0x9c, 0x5f, 0xa8, 0xbf, 0x7a, 0xa1, 0x57, 0x16, 0xc7, 0xc0, 0x6a, 0x5d,
                                     0x03, 0x85, 0x03, 0xc6, 0x14, 0xff, 0x80, 0xc9, 0xb3 };

uint8_t eph_private_i[ECC_KEY_LEN] = { 0x36, 0x8e, 0xc1, 0xf6, 0x9a, 0xeb, 0x65, 0x9b, 0xa3, 0x7d, 0x5a, 0x8d, 0x45, 0xb2, 0x1b, 0xdc, 0x02, 0x99, 0xdc, 0xea, 0xa8, 0xef, 0x23,
                                       0x5f, 0x3c, 0xa4, 0x2c, 0xe3, 0x53, 0x0f, 0x95, 0x25 };
#endif

int8_t
edhoc_client_callback(process_event_t ev, void *data)
{
  if(ev == edhoc_event && edhoc_state.val == CL_FINISHED) {
    LOG_DBG("client callback: CL_FINISHED\n");
    return 1;
  }
  if(ev == edhoc_event && edhoc_state.val == CL_TRIES_EXPIRE) {
    LOG_WARN("client callback: CL_TRIES_EXPIRE\n");
    return -1;
  }
  return 0;
}

void
edhoc_client_run(void)
{
  process_start(&edhoc_client, NULL);
}

void
edhoc_server_set_ad_1(const void *buff, uint8_t buff_sz)
{
  memcpy(edhoc_state.ad.ad_1, (void *)buff, buff_sz);
  edhoc_state.ad.ad_1_sz = buff_sz;
}

void
edhoc_server_set_ad_3(const void *buff, uint8_t buff_sz)
{
  memcpy(edhoc_state.ad.ad_3, (void *)buff, buff_sz);
  edhoc_state.ad.ad_3_sz = buff_sz;
}

uint8_t
edhoc_server_get_ad_2(char *buff)
{
  memcpy(buff, (void *)edhoc_state.ad.ad_2, edhoc_state.ad.ad_2_sz);
  return edhoc_state.ad.ad_2_sz;
}

static void
client_timeout_callback(coap_timer_t *timer)
{
  LOG_ERR("Timeout\n");
  coap_timer_stop(timer);
  edhoc_state.val = CL_TIMEOUT;
  pro = process_post(&edhoc_client, edhoc_event, &edhoc_state);
}

MEMB(edhoc_client_storage, edhoc_client_t, 1);

static inline edhoc_client_t *
client_context_new(void)
{
  return (edhoc_client_t *)memb_alloc(&edhoc_client_storage);
}

static inline void
client_context_free(edhoc_client_t *ctx)
{
  memb_free(&edhoc_client_storage, ctx);
}

static int
client_block2_handler(coap_message_t *response, uint8_t *target,
                      size_t *len, size_t max_len)
{
  const uint8_t *payload = 0;
  int pay_len = coap_get_payload(response, &payload);

  if(response->block2_offset + pay_len > max_len) {
    LOG_ERR("message too big\n");
    coap_status_code = REQUEST_ENTITY_TOO_LARGE_4_13;
    coap_error_message = "Message too big";
    return -1;
  }

  if(target && len) {
    memcpy(target + response->block2_offset, payload, pay_len);
    *len = response->block2_offset + pay_len;
    assert(*len <= EDHOC_MAX_BUFFER);
    print_buff_8_dbg((uint8_t *)payload, (unsigned long)pay_len);
    target = target + pay_len; /* FIXME: Pointless assignment. Are things wrong here? */
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
static void
client_response_handler(coap_callback_request_state_t *callback_state)
{
  if(callback_state->state.response == NULL) {
    LOG_WARN("Request timed out response\n");
    return;
  }

  if(memcmp(callback_state->state.request->token,
            callback_state->state.response->token,
            callback_state->state.request->token_len)) {
    LOG_ERR("rx response not correlated\n");
    edhoc_state.val = CL_RESTART;
    coap_timer_stop(&timer);
    pro = process_post(&edhoc_client, edhoc_event, &edhoc_state);
    return;
  }

  /* Check that the response are coming from the correct server */
  if(memcmp(&cli->server_ep.ipaddr,
            &callback_state->state.remote_endpoint->ipaddr,
            sizeof(uip_ipaddr_t)) != 0) {
    LOG_ERR("rx response from an error server\n");
    edhoc_state.val = CL_RESTART;
    coap_timer_stop(&timer);
    pro = process_post(&edhoc_client, edhoc_event, &edhoc_state);
    return;
  }

  if(callback_state->state.response->code != CHANGED_2_04) {
    LOG_WARN("The code responds received is not CHANGED_2_04\n");
  }

  coap_set_option(callback_state->state.response, COAP_OPTION_BLOCK2);

  LOG_DBG("Blockwise: block 2 response: Num: %" PRIu32
          ", More: %u, Size: %u, Offset: %" PRIu32 "\n",
          callback_state->state.response->block2_num,
          callback_state->state.response->block2_more,
          callback_state->state.response->block2_size,
          callback_state->state.response->block2_offset);
  LOG_DBG("Blockwise: block 1 response: Num: %" PRIu32
          ", More: %u, Size: %u, Offset: %" PRIu32 "\n",
          callback_state->state.response->block1_num,
          callback_state->state.response->block1_more,
          callback_state->state.response->block1_size,
          callback_state->state.response->block1_offset);

  if(callback_state->state.more) {
    client_block2_handler(callback_state->state.response,
                          rx_ptr, &rx_sz, EDHOC_MAX_PAYLOAD_LEN);
  } else {
    client_block2_handler(callback_state->state.response,
                          rx_ptr, &rx_sz, EDHOC_MAX_PAYLOAD_LEN);
    edhoc_ctx->buffers.rx_sz = (uint8_t)rx_sz;
    edhoc_state.val = CL_BLOCKING;
    pro = process_post(PROCESS_BROADCAST, edhoc_event, &edhoc_state);
  }
}
/*----------------------------------------------------------------------------*/
static void
client_chunk_handler(coap_callback_request_state_t *callback_state)
{

  if(callback_state->state.response == NULL) {
    LOG_WARN("Request timed out chunk\n");
    return;
  }
  /* Check the 5-tuple information before retrieving the protocol state */

  LOG_DBG("Blockwise: block 2 response: Num: %" PRIu32
          ", More: %u, Size: %u, Offset: %" PRIu32 "\n",
          callback_state->state.response->block2_num,
          callback_state->state.response->block2_more,
          callback_state->state.response->block2_size,
          callback_state->state.response->block2_offset);
  LOG_DBG("Blockwise: block 1 response: Num: %" PRIu32
          ", More: %u, Size: %u, Offset: %" PRIu32 "\n",
          callback_state->state.response->block1_num,
          callback_state->state.response->block1_more,
          callback_state->state.response->block1_size,
          callback_state->state.response->block1_offset);
  edhoc_state.val = CL_BLOCK1;
  pro = process_post(&edhoc_client, edhoc_event, &edhoc_state);
}
/*----------------------------------------------------------------------------*/
static void
edhoc_client_post(void)
{
  coap_init_message(state.state.request, COAP_TYPE_CON, COAP_POST, 0);
  coap_set_header_uri_path(state.state.request, EDHOC_WELL_KNOWN);

  send_sz = 0;
  msg_num = 0;
  state.state.block_num = 0;
}
/*----------------------------------------------------------------------------*/
static int
edhoc_client_post_blocks(void)
{
  if(edhoc_ctx->buffers.tx_sz - send_sz > COAP_MAX_CHUNK_SIZE) {
    coap_set_payload(state.state.request,
                     (uint8_t *)edhoc_ctx->buffers.msg_tx + send_sz,
                     COAP_MAX_CHUNK_SIZE);
    coap_set_header_block1(state.state.request, msg_num,
                           1, COAP_MAX_CHUNK_SIZE);
    msg_num++;
    send_sz += COAP_MAX_CHUNK_SIZE;
    coap_send_request(&state, state.state.remote_endpoint,
                      state.state.request, client_chunk_handler);
    return 0;
  } else if(edhoc_ctx->buffers.tx_sz < COAP_MAX_CHUNK_SIZE) {
    coap_set_payload(state.state.request,
                     (uint8_t *)edhoc_ctx->buffers.msg_tx,
                     edhoc_ctx->buffers.tx_sz);
    rx_ptr = edhoc_ctx->buffers.msg_rx;
    rx_sz = 0;
    state.state.block_num = 0;
    coap_send_request(&state, state.state.remote_endpoint,
                      state.state.request, client_response_handler);
    return 1;
  } else {
    coap_set_payload(state.state.request,
                     (uint8_t *)edhoc_ctx->buffers.msg_tx + send_sz,
                     edhoc_ctx->buffers.tx_sz - send_sz);
    coap_set_header_block1(state.state.request, msg_num, 0,
                           COAP_MAX_CHUNK_SIZE);
    send_sz += (edhoc_ctx->buffers.tx_sz - send_sz);
    rx_ptr = edhoc_ctx->buffers.msg_rx;
    rx_sz = 0;
    coap_send_request(&state, state.state.remote_endpoint,
                      state.state.request, client_response_handler);
    return 1;
  }
}
/*----------------------------------------------------------------------------*/
static int
edhoc_send_msg1(uint8_t *ad, uint8_t ad_sz, bool suite_array)
{
  LOG_DBG("--------------Generate message_1------------------\n");
  time = RTIMER_NOW();
  edhoc_gen_msg_1(edhoc_ctx, ad, ad_sz, suite_array);
  time = RTIMER_NOW() - time;
  LOG_INFO("Client time to gen MSG1: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
           (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND), (uint32_t)time);
  time = RTIMER_NOW();
  edhoc_client_post();
  cli->state = RX_MSG2;
  return edhoc_client_post_blocks();
}
/*----------------------------------------------------------------------------*/
PROCESS_THREAD(edhoc_client_protocol, ev, data)
{
  PROCESS_BEGIN();

  switch(cli->state) {
  case RX_MSG2:
    LOG_DBG("--------------Handler message_2------------------\n");
    LOG_DBG("RX message_2 (%d bytes): ", edhoc_ctx->buffers.rx_sz);
    print_buff_8_dbg(edhoc_ctx->buffers.msg_rx, edhoc_ctx->buffers.rx_sz);

    time = RTIMER_NOW();
    er = edhoc_handler_msg_2(&msg2, edhoc_ctx, edhoc_ctx->buffers.msg_rx,
                             edhoc_ctx->buffers.rx_sz);
    time = RTIMER_NOW() - time;
    LOG_INFO("Client time to handler MSG2: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
             (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND), (uint32_t)time);
    time = RTIMER_NOW();
    if(er == ERR_RESEND_MSG_1) {
      edhoc_send_msg1((uint8_t *)edhoc_state.ad.ad_1, edhoc_state.ad.ad_1_sz,
                      true);
      break;
    }

    if(er > 0) {
      assert(msg2.gy_ciphertext_2_sz >= ECC_KEY_LEN);
      assert(msg2.gy_ciphertext_2_sz - ECC_KEY_LEN <= EDHOC_MAX_BUFFER);
      er = edhoc_authenticate_msg(edhoc_ctx, (uint8_t *)edhoc_state.ad.ad_2,
                                  true);
    }
    time = RTIMER_NOW() - time;
    LOG_DBG("Client time to authenticate MSG2: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
            (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND), (uint32_t)time);

    if(er == RX_ERR_MSG) {
      LOG_ERR("error code (%d)\n", er);
    } else if(er < RX_ERR_MSG) {
      LOG_ERR("Client: Send MSG error with code (%d)\n", er);
      edhoc_ctx->buffers.tx_sz = edhoc_gen_msg_error(edhoc_ctx->buffers.msg_tx,
                                                     edhoc_ctx, er);
      cli->state = NON_MSG;
      edhoc_client_post();
      edhoc_client_post_blocks();
    } else {
      /* TODO: Include a way to pass application msgs. */
      edhoc_state.ad.ad_2_sz = er;
      if(edhoc_state.ad.ad_2_sz > 0) {
        LOG_DBG("AD_2 (%d bytes): ", edhoc_state.ad.ad_2_sz);
        print_char_8_dbg((char *)edhoc_state.ad.ad_2, edhoc_state.ad.ad_2_sz);
      }

      LOG_DBG("--------------Generate message_3------------------\n");
      /* Generate MSG3 */
      time = RTIMER_NOW();
      edhoc_gen_msg_3(edhoc_ctx, (uint8_t *)edhoc_state.ad.ad_3,
                      edhoc_state.ad.ad_3_sz);
      time = RTIMER_NOW() - time;
      LOG_INFO("Client time to gen MSG3: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
               (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND),
               (uint32_t)time);
      LOG_DBG("message_3 (%d bytes): ", edhoc_ctx->buffers.tx_sz);
      print_buff_8_dbg(edhoc_ctx->buffers.msg_tx, edhoc_ctx->buffers.tx_sz);
      cli->rx_msg2 = true;
      cli->state = RX_RESPONSE_MSG3;
      cli->tx_msg3 = true;
      edhoc_client_post();
      edhoc_client_post_blocks();
    }
    break;
  case RX_RESPONSE_MSG3:
    if(edhoc_ctx->buffers.rx_sz > 0) {
      uint8_t *msg_err = edhoc_ctx->buffers.msg_rx;
      edhoc_msg_error_t err;
      er = edhoc_deserialize_err(&err, msg_err, edhoc_ctx->buffers.rx_sz);
      if(er > 0) {
        LOG_ERR("RX error code %d, MSG_ERR", err.err_code);
        print_char_8_err(err.err_info, err.err_info_sz);
        edhoc_state.val = CL_RESTART;
        cli->state = NON_MSG;
        coap_timer_stop(&timer);
        pro = process_post(&edhoc_client, edhoc_event, &edhoc_state);
        break;
      }
    }

    /* Check every protocol step successfully */
    cli->state = EXP_READY;
    if(cli->tx_msg1 && cli->rx_msg2) {
      cli->rx_msg3_response = true;
    } else {
      LOG_ERR("The EDHOC process escape steps\n");
      edhoc_state.val = CL_RESTART;
      coap_timer_stop(&timer);
      pro = process_post(&edhoc_client, edhoc_event, &edhoc_state);
      break;
    }
  /* FIXME: missing break? */
  case EXP_READY:
    LOG_DBG("-------------------EXPORTER-----------------------\n");
    edhoc_state.val = CL_FINISHED;
    coap_timer_stop(&timer);
    pro = process_post(PROCESS_BROADCAST, edhoc_event, &edhoc_state);
    break;
  }
  PROCESS_END();
}
/*----------------------------------------------------------------------------*/
static void
edhoc_client_init(void)
{
  if(edhoc_event == PROCESS_EVENT_NONE) {
    edhoc_event = process_alloc_event();
  }
  cli = client_context_new();
  edhoc_storage_init();
  edhoc_ctx = edhoc_new();
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &cli->server_ep);
  state.state.request = cli->request;
  state.state.response = cli->response;
  state.state.remote_endpoint = &cli->server_ep;
}
/*----------------------------------------------------------------------------*/
static int
edhoc_client_start(uint8_t *ad, uint8_t ad_sz)
{
  cli->tx_msg3 = false;
  cli->rx_msg3_response = false;
  cli->tx_msg1 = true;

  coap_timer_set_callback(&timer, client_timeout_callback);
  coap_timer_set(&timer, CL_TIMEOUT_VAL);

  return edhoc_send_msg1(ad, ad_sz, false);
}
/*----------------------------------------------------------------------------*/
static void
generate_ephemeral_key(uint8_t curve_id, uint8_t *pub_x,
                       uint8_t *pub_y, uint8_t *priv)
{
  rtimer_clock_t drv_time = RTIMER_NOW();

  ecc_curve_t curve;
  ecdh_get_ecc_curve(curve_id, &curve);

#if EDHOC_ECC == EDHOC_ECC_UECC
  LOG_DBG("generate key with uEcc\n");
  uECC_make_key(pub_x, priv, curve.curve);
#elif EDHOC_ECC == EDHOC_ECC_CC2538
  LOG_DBG("generate key with CC2538\n");
  static key_gen_t key = {
    .process = &edhoc_client,
    .curve_info = curve.curve,
  };
  PT_SPAWN(&edhoc_client.pt, &key.pt, generate_key_hw(&key));

  memcpy(pub_x, key.x, ECC_KEY_LEN);
  memcpy(pub_y, key.y, ECC_KEY_LEN);
  memcpy(priv, key.private, ECC_KEY_LEN);
#endif

#if EDHOC_TEST == EDHOC_TEST_VECTOR_TRACE_DH
  memcpy(edhoc_ctx->creds.ephemeral_key.pub.x, eph_pub_x_i, ECC_KEY_LEN);
  memcpy(edhoc_ctx->creds.ephemeral_key.pub.y, eph_pub_y_i, ECC_KEY_LEN);
  memcpy(edhoc_ctx->creds.ephemeral_key.priv, eph_private_i, ECC_KEY_LEN);
#endif

  drv_time = RTIMER_NOW() - drv_time;
  LOG_INFO("Client time to gen eph key: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
           (uint32_t)((uint64_t)drv_time * 1000 / RTIMER_SECOND),
           (uint32_t)drv_time);
  LOG_DBG("X (%d bytes): ", ECC_KEY_LEN);
  print_buff_8_dbg(edhoc_ctx->creds.ephemeral_key.priv, ECC_KEY_LEN);
  LOG_DBG("G_X x (%d bytes): ", ECC_KEY_LEN);
  print_buff_8_dbg(edhoc_ctx->creds.ephemeral_key.pub.x, ECC_KEY_LEN);
  LOG_DBG("y: ");
  print_buff_8_dbg(edhoc_ctx->creds.ephemeral_key.pub.y, ECC_KEY_LEN);
}
/*----------------------------------------------------------------------------*/
void
edhoc_client_close(void)
{
  coap_timer_stop(&timer);
  client_context_free(cli);
  edhoc_finalize(edhoc_ctx);
}
/*----------------------------------------------------------------------------*/
PROCESS_THREAD(edhoc_client, ev, data)
{
  PROCESS_BEGIN();

  static struct etimer wait_timer;
  edhoc_client_init();

  if(!edhoc_initialize_context(edhoc_ctx)) {
    PROCESS_EXIT();
  }

  /* Generate ephemeral key */
  generate_ephemeral_key(edhoc_ctx->config.ecdh_curve,
                         edhoc_ctx->creds.ephemeral_key.pub.x,
                         edhoc_ctx->creds.ephemeral_key.pub.y,
                         edhoc_ctx->creds.ephemeral_key.priv);

  time_total = RTIMER_NOW();
  edhoc_client_start((uint8_t *)edhoc_state.ad.ad_1, edhoc_state.ad.ad_1_sz);

  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == edhoc_event && data == &edhoc_state) {
      if(edhoc_state.val == CL_RESTART) {
        LOG_ERR("Error\n");
        /* When timeout will restart */
        if(attempt < EDHOC_CONF_ATTEMPTS) {
          LOG_INFO("Attempt %d\n", attempt);
          etimer_set(&wait_timer, CLOCK_SECOND * (CL_TIMEOUT_VAL / 1000));
          PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&wait_timer));
          etimer_stop(&wait_timer);
          time = RTIMER_NOW();
          edhoc_client_start((uint8_t *)edhoc_state.ad.ad_1,
                             edhoc_state.ad.ad_1_sz);
          attempt++;
        } else {
          LOG_ERR("Expire EDHOC client attempts\n");
          edhoc_state.val = CL_TRIES_EXPIRE;
          pro = process_post(PROCESS_BROADCAST, edhoc_event, &edhoc_state);
          break;
        }
      }

      if(edhoc_state.val == CL_FINISHED) {
        LOG_INFO("Compile time: %s %s\n", __DATE__, __TIME__);
        time_total = RTIMER_NOW() - time_total;
        LOG_INFO("Client time to finish: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
                 (uint32_t)((uint64_t)time_total * 1000 / RTIMER_SECOND),
                 (uint32_t)time_total);
        break;
      }

      if(edhoc_state.val == CL_TIMEOUT) {
        LOG_ERR("Expire timeout\n");
        if(attempt < EDHOC_CONF_ATTEMPTS) {
          LOG_INFO("Attempt %d\n", attempt);
          etimer_set(&wait_timer, CLOCK_SECOND * (CL_TIMEOUT_VAL / 1000));
          PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&wait_timer));
          etimer_stop(&wait_timer);
          edhoc_client_start((uint8_t *)edhoc_state.ad.ad_1,
                             edhoc_state.ad.ad_1_sz);
          attempt++;
        } else {
          LOG_ERR("Expire EDHOC client attempts\n");
          edhoc_state.val = CL_TRIES_EXPIRE;
          pro = process_post(PROCESS_BROADCAST, edhoc_event, &edhoc_state);
          break;
        }
      }

      if(edhoc_state.val == CL_BLOCK1) {
        edhoc_client_post_blocks();
      }

      if(edhoc_state.val == CL_POST) {
        edhoc_client_post();
        edhoc_client_post_blocks();
      }

      if(edhoc_state.val == CL_BLOCKING) {
        process_start(&edhoc_client_protocol, NULL);
        time = RTIMER_NOW() - time;
        LOG_INFO("Client time to rx MSG: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
                 (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND),
                 (uint32_t)time);
        time = RTIMER_NOW();
        while(process_is_running(&edhoc_client_protocol)) {
          process_run();
        }
      }
    }
  }
  PROCESS_END();
}
