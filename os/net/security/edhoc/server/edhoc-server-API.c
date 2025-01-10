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
 *      EDHOC server API [RFC9528] with CoAP Block-Wise Transfer [RFC7959]
 * \author
 *      Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */

#include "edhoc-server-API.h"
#include "edhoc-msg-generators.h"
#include "edhoc-msg-handlers.h"
#include "sys/pt.h"
#include "sys/rtimer.h"
#include <assert.h>

/* EDHOC Client protocol states */
#define NON_MSG 0
#define RX_MSG1 1
#define RX_MSG3 2
#define TX_MSG_ERR 3
#define EXP_READY 4
#define RESTART 5

static rtimer_clock_t time;
static rtimer_clock_t time_total;

static coap_timer_t timer;
static uint8_t msg_rx[EDHOC_MAX_PAYLOAD_LEN];
static size_t msg_rx_len;
static edhoc_server_t server;
static edhoc_server_t *serv;
static process_event_t new_ecc_event;
static ecc_data_event_t new_ecc;

static coap_message_t *request;
static coap_message_t *response;
static int er = 0;
static edhoc_msg_3_t msg3;
PROCESS(edhoc_server, "EDHOC Server");

#if EDHOC_TEST == EDHOC_TEST_VECTOR_TRACE_DH
uint8_t eph_pub_x_r[ECC_KEY_LEN] = { 0x41, 0x97, 0x01, 0xd7, 0xf0, 0x0a, 0x26, 0xc2, 0xdc, 0x58, 0x7a, 0x36, 0xdd, 0x75, 0x25, 0x49, 0xf3, 0x37, 0x63, 0xc8, 0x93, 0x42, 0x2c,
                                     0x8e, 0xa0, 0xf9, 0x55, 0xa1, 0x3a, 0x4f, 0xf5, 0xd5 };

uint8_t eph_pub_y_r[ECC_KEY_LEN] = { 0x5e, 0x4f, 0x0d, 0xd8, 0xa3, 0xda, 0x0b, 0xaa, 0x16, 0xb9, 0xd3, 0xad, 0x56, 0xa0, 0xc1, 0x86, 0x0a, 0x94, 0x0a, 0xf8, 0x59, 0x14, 0x91,
                                     0x5e, 0x25, 0x01, 0x9b, 0x40, 0x24, 0x17, 0xe9, 0x9d };

uint8_t eph_private_r[ECC_KEY_LEN] = { 0xe2, 0xf4, 0x12, 0x67, 0x77, 0x20, 0x5e, 0x85, 0x3b, 0x43, 0x7d, 0x6e, 0xac, 0xa1, 0xe1, 0xf7, 0x53, 0xcd, 0xcc, 0x3e, 0x2c, 0x69, 0xfa,
                                       0x88, 0x4b, 0x0a, 0x1a, 0x64, 0x09, 0x77, 0xe4, 0x18 };
#endif
/*----------------------------------------------------------------------------*/
static void
generate_ephemeral_key(uint8_t curve_id, uint8_t *pub_x, uint8_t *pub_y, uint8_t *priv)
{
  rtimer_clock_t drv_time = RTIMER_NOW();

  ecc_curve_t curve;
  ecdh_get_ecc_curve(curve_id, &curve);

#if EDHOC_ECC == EDHOC_ECC_UECC
  LOG_DBG("Generate key with uEcc\n");
  uECC_make_key(pub_x, priv, curve.curve);
#elif EDHOC_ECC == EDHOC_ECC_CC2538
  LOG_DBG("Generate key with CC2538 HW modules\n");
  static key_gen_t key = {
    .process = &edhoc_server,
    .curve_info = curve.curve,
  };
  PT_SPAWN(&edhoc_server.pt, &key.pt, generate_key_hw(&key));
  memcpy(pub_x, key.x, ECC_KEY_LEN);
  memcpy(pub_y, key.y, ECC_KEY_LEN);
  memcpy(priv, key.private, ECC_KEY_LEN);
#endif

#if EDHOC_TEST == EDHOC_TEST_VECTOR_TRACE_DH
  memcpy(edhoc_ctx->creds.ephemeral_key.pub.x, eph_pub_x_r, ECC_KEY_LEN);
  memcpy(edhoc_ctx->creds.ephemeral_key.pub.y, eph_pub_y_r, ECC_KEY_LEN);
  memcpy(edhoc_ctx->creds.ephemeral_key.priv, eph_private_r, ECC_KEY_LEN);
#endif

  drv_time = RTIMER_NOW() - drv_time;
  LOG_INFO("Server time to generate new key: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
	   (uint32_t)((uint64_t)drv_time * 1000 / RTIMER_SECOND),
	   (uint32_t)drv_time);
  LOG_DBG("Gy (%d bytes): ", ECC_KEY_LEN);
  print_buff_8_dbg(edhoc_ctx->creds.ephemeral_key.pub.x, ECC_KEY_LEN);
  LOG_DBG("Y (%d bytes): ", ECC_KEY_LEN);
  print_buff_8_dbg(edhoc_ctx->creds.ephemeral_key.priv, ECC_KEY_LEN);
}
/*----------------------------------------------------------------------------*/
int8_t
edhoc_server_callback(process_event_t ev, void *data)
{
  if(ev == new_ecc_event && new_ecc.val == SERV_FINISHED) {
    return SERV_FINISHED;
  }

  if(ev == new_ecc_event && new_ecc.val == SERV_RESTART) {
    LOG_DBG("server callback: SERV_RESTART\n");
    return SERV_RESTART;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
void
edhoc_server_set_ad_2(const void *buff, uint8_t buff_sz)
{
  memcpy(new_ecc.ad.ad_2, (void *)buff, buff_sz);
  new_ecc.ad.ad_2_sz = buff_sz;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_server_get_ad_1(char *buff)
{
  memcpy(buff, (void *)new_ecc.ad.ad_1, new_ecc.ad.ad_1_sz);
  return new_ecc.ad.ad_1_sz;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_server_get_ad_3(char *buff)
{
  memcpy(buff, (void *)new_ecc.ad.ad_3, new_ecc.ad.ad_3_sz);
  return new_ecc.ad.ad_3_sz;
}
/*----------------------------------------------------------------------------*/
static void
server_timeout_callback(coap_timer_t *timer)
{
  LOG_ERR("Timeout\n");
  coap_timer_stop(timer);
  edhoc_server_close();
  new_ecc.val = SERV_RESTART;
  process_post(PROCESS_BROADCAST, new_ecc_event, &new_ecc);
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_server_restart(void)
{
  serv->con_num = 0;
  serv->state = 0;
  serv->rx_msg1 = false;
  serv->rx_msg3 = false;
  serv->state = NON_MSG;
  edhoc_setup_suites(edhoc_ctx);
  return edhoc_initialize_context(edhoc_ctx);
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_server_start(void)
{
  LOG_INFO("SERVER: EDHOC new\n");
  edhoc_ctx = edhoc_new();
  serv = &server;
  return edhoc_server_restart();
}
/*----------------------------------------------------------------------------*/
void
edhoc_server_init(void)
{
  LOG_INFO("SERVER: CoAP active resource\n");
  coap_activate_resource(&res_edhoc, EDHOC_WELL_KNOWN);
  new_ecc_event = process_alloc_event();
}
/*----------------------------------------------------------------------------*/
void
edhoc_server_close(void)
{
  edhoc_finalize(edhoc_ctx);
}
/*----------------------------------------------------------------------------*/
void
edhoc_server_process(coap_message_t *req, coap_message_t *res,
                     edhoc_server_t *ser, uint8_t *msg, uint8_t len)
{
  serv_data_t serv_data = { req, res, ser };
  memcpy(msg_rx, msg, len);
  msg_rx_len = len;
  process_start(&edhoc_server, (process_data_t)&serv_data);
  while(process_is_running(&edhoc_server)) {
    process_run();
  }
}
/*----------------------------------------------------------------------------*/
PROCESS_THREAD(edhoc_server, ev, data)
{
  PROCESS_BEGIN();

  request = ((serv_data_t *)data)->request;
  response = ((serv_data_t *)data)->response;
  serv = ((serv_data_t *)data)->serv;
  if(serv->state == EXP_READY) {
    LOG_DBG("process exit\n");
  }

  /* Check the 5-tuple information before retrieve the state protocol */
  if(serv->state != NON_MSG &&
     memcmp(&serv->con_ipaddr, &request->src_ep->ipaddr, sizeof(uip_ipaddr_t)) != 0) {
    LOG_ERR("rx request from an error ipaddr\n");
    coap_set_payload(response, NULL, 0);
    coap_set_status_code(response, BAD_REQUEST_4_00);
  } else {
    switch(serv->state) {
    case NON_MSG:
      coap_timer_set_callback(&timer, server_timeout_callback);
      coap_timer_set(&timer, SERV_TIMEOUT_VAL);
    /* FIXME: missing break? */
    case RX_MSG1:
      LOG_DBG("----------------------------------Handler message_1-----------------------------\n");
      LOG_DBG("RX message_1 (CBOR Sequence) (%d bytes):\n", (int)msg_rx_len);
      print_buff_8_dbg(msg_rx, msg_rx_len);

      time_total = RTIMER_NOW();
      time = RTIMER_NOW();
      er = edhoc_handler_msg_1(edhoc_ctx, msg_rx, msg_rx_len,
                               (uint8_t *)new_ecc.ad.ad_1);
      time = RTIMER_NOW() - time;
      LOG_INFO("Server time to handler MSG1: %" PRIu32 " ms (%" PRIu32 " ticks ).\n",
               (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND),
               (uint32_t)time);

      if(er == RX_ERR_MSG) {
        LOG_WARN("error code (%d)\n", er);
        serv->state = NON_MSG;
        coap_timer_stop(&timer);
        edhoc_server_close();
        new_ecc.val = SERV_RESTART;
        process_post(PROCESS_BROADCAST, new_ecc_event, &new_ecc);
      } else if(er < RX_ERR_MSG) {
        LOG_WARN("Send MSG error with code (%d)\n", er);
        edhoc_ctx->buffers.tx_sz = edhoc_gen_msg_error(edhoc_ctx->buffers.msg_tx,
                                                       edhoc_ctx, er);
        serv->state = TX_MSG_ERR;
        if(er != ERR_NEW_SUITE_PROPOSE) {
          coap_timer_stop(&timer);
          edhoc_server_close();
          new_ecc.val = SERV_RESTART;
          process_post(PROCESS_BROADCAST, new_ecc_event, &new_ecc);
        }
      } else {
        /* Set the 5-tuple to identify the connection */
        memcpy(&serv->con_ipaddr, &request->src_ep->ipaddr,
               sizeof(uip_ipaddr_t));
        new_ecc.ad.ad_1_sz = er;
        if(new_ecc.ad.ad_1_sz > 0) {
          LOG_DBG("AD_1 (%d bytes): ", new_ecc.ad.ad_1_sz);
          print_char_8_dbg((char *)new_ecc.ad.ad_1, new_ecc.ad.ad_1_sz);
        }
        serv->rx_msg1 = true;

        /* Generate MSG2 */
        time = RTIMER_NOW();
        LOG_DBG("--------------------Generate message_2--------------------\n");
        generate_ephemeral_key(edhoc_ctx->config.ecdh_curve,
                               edhoc_ctx->creds.ephemeral_key.pub.x,
                               edhoc_ctx->creds.ephemeral_key.pub.y,
                               edhoc_ctx->creds.ephemeral_key.priv);
        edhoc_gen_msg_2(edhoc_ctx, (uint8_t *)new_ecc.ad.ad_2,
                        new_ecc.ad.ad_2_sz);
        time = RTIMER_NOW() - time;
        LOG_INFO("Server time to gen MSG2: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
                 (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND),
                 (uint32_t)time);
        LOG_DBG("message_2 (CBOR Sequence) (%d bytes): ",
                edhoc_ctx->buffers.tx_sz);
        print_buff_8_dbg(edhoc_ctx->buffers.msg_tx, edhoc_ctx->buffers.tx_sz);
        serv->state = RX_MSG3;
      }
      break;
    case RX_MSG3:
      LOG_DBG("---------------------Handler message_3---------------------\n");
      LOG_DBG("RX message_3 (%d bytes): ", (int)msg_rx_len);
      print_buff_8_dbg(msg_rx, msg_rx_len);
      time = RTIMER_NOW();
      er = edhoc_handler_msg_3(&msg3, edhoc_ctx, msg_rx, msg_rx_len);
      time = RTIMER_NOW() - time;
      LOG_INFO("Server time to handler MSG3: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
               (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND),
               (uint32_t)time);
      time = RTIMER_NOW();
      if(er > 0) {
        er = edhoc_authenticate_msg(edhoc_ctx, (uint8_t *)new_ecc.ad.ad_3,
                                    false);
        time = RTIMER_NOW() - time;
        LOG_INFO("Server time to authenticate: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
                 (uint32_t)((uint64_t)time * 1000 / RTIMER_SECOND),
                 (uint32_t)time);
      }

      if(er == RX_ERR_MSG) {
        serv->state = NON_MSG;
        coap_timer_stop(&timer);
        edhoc_server_close();
        new_ecc.val = SERV_RESTART;
        process_post(PROCESS_BROADCAST, new_ecc_event, &new_ecc);
        break;
      } else if(er < RX_ERR_MSG) {
        edhoc_ctx->buffers.tx_sz = edhoc_gen_msg_error(edhoc_ctx->buffers.msg_tx,
                                                       edhoc_ctx, er);
        coap_timer_stop(&timer);
        edhoc_server_close();
        new_ecc.val = SERV_RESTART;
        process_post(PROCESS_BROADCAST, new_ecc_event, &new_ecc);
        serv->state = TX_MSG_ERR;
        break;
      } else {
        /*TODO: Include a way to pass application msgs. */
        new_ecc.ad.ad_3_sz = er;
        if(new_ecc.ad.ad_3_sz > 0) {
          LOG_DBG("AD_3 (%d bytes): ", new_ecc.ad.ad_3_sz);
          print_char_8_dbg((char *)new_ecc.ad.ad_3, new_ecc.ad.ad_3_sz);
        }

        serv->state = EXP_READY;
        serv->rx_msg3 = true;
      }
    /* FIXME: missing break? */
    case EXP_READY:
      if(serv->rx_msg1 && serv->rx_msg3) {
        LOG_DBG("--------------EXPORTER------------------------\n");
        edhoc_ctx->buffers.tx_sz = 0;
        new_ecc.val = SERV_FINISHED;
        time_total = RTIMER_NOW() - time_total;
        LOG_INFO("Server time to finish: %" PRIu32 " ms (%" PRIu32 " ticks).\n",
                 (uint32_t)(time_total * 1000 / RTIMER_SECOND),
                 (uint32_t)time_total);
        time_total = RTIMER_NOW();
        coap_timer_stop(&timer);
        process_post(PROCESS_BROADCAST, new_ecc_event, &new_ecc);
        coap_timer_stop(&timer);
        break;
      } else {
        LOG_ERR("Protocol step missed\n");
        serv->state = NON_MSG;
      }
    }
  }

  if(serv->state == NON_MSG) {
    LOG_ERR("RX MSG ERROR response\n");
    coap_set_payload(response, NULL, 0);
    coap_set_status_code(response, DELETED_2_02);
  } else {
    response->payload = (uint8_t *)edhoc_ctx->buffers.msg_tx;
    response->payload_len = edhoc_ctx->buffers.tx_sz;
    coap_set_status_code(response, CHANGED_2_04);
    assert(&(response->options) != NULL);
    memset(&(response->options), 0, sizeof(response->options));
    if(response->payload_len == 0) {
      assert(&(request->options) != NULL);
      memset(&(request->options), 0, sizeof(request->options));
    } else {
      coap_set_header_block2(response, 0,
                             edhoc_ctx->buffers.tx_sz > COAP_MAX_CHUNK_SIZE ?
                               1 : 0, COAP_MAX_CHUNK_SIZE);
    }
    if(serv->state == TX_MSG_ERR) {
      serv->state = NON_MSG;
    }
    coap_set_status_code(response, CHANGED_2_04);
    LOG_DBG("Blockwise: block 1 response: Num: %" PRIu32
            ", More: %u, Size: %u, Offset: %" PRIu32 "\n",
            response->block1_num,
            response->block1_more,
            response->block1_size,
            response->block1_offset);
    LOG_DBG("Blockwise: block 2 response: Num: %" PRIu32
            ", More: %u, Size: %u, Offset: %" PRIu32 "\n",
            response->block2_num,
            response->block2_more,
            response->block2_size,
            response->block2_offset);
  }

  PROCESS_END();
}
