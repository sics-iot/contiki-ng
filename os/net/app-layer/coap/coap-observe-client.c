/*
 * Copyright (c) 2014, Daniele Alessandrelli.
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
 * This file is part of the Contiki operating system.
 *
 */

/*
 * \file
 *        Extension to Erbium for enabling CoAP observe clients
 * \author
 *        Daniele Alessandrelli <daniele.alessandrelli@gmail.com>
 */

/**
 * \addtogroup coap
 * @{
 */

#include "coap.h"
#include "coap-observe-client.h"
#include "sys/cc.h"
#include "lib/memb.h"
#include "lib/list.h"
#include <stdio.h>
#include <string.h>

/* Compile this code only if client-side support for CoAP Observe is required */
#if COAP_OBSERVE_CLIENT

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap"
#define LOG_LEVEL  LOG_LEVEL_COAP

MEMB(obs_subjects_memb, coap_observee_t, COAP_MAX_OBSERVEES);
LIST(obs_subjects_list);

/*----------------------------------------------------------------------------*/
static size_t
get_token(coap_message_t *coap_pkt, const uint8_t **token)
{
  *token = coap_pkt->token;

  return coap_pkt->token_len;
}
/*----------------------------------------------------------------------------*/
static int
set_token(coap_message_t *coap_pkt, const uint8_t *token, size_t token_len)
{
  coap_pkt->token_len = MIN(COAP_TOKEN_LEN, token_len);
  memcpy(coap_pkt->token, token, coap_pkt->token_len);

  return coap_pkt->token_len;
}
/*----------------------------------------------------------------------------*/
coap_observee_t *
coap_obs_add_observee(const coap_endpoint_t *endpoint,
                      const uint8_t *token, size_t token_len, const char *url,
                      notification_callback_t notification_callback,
                      void *data)
{
  coap_observee_t *o;

  /* Remove existing observe relationship, if any. */
  coap_obs_remove_observee_by_url(endpoint, url);
  o = memb_alloc(&obs_subjects_memb);
  if(o) {
    o->url = url;
    coap_endpoint_copy(&o->endpoint, endpoint);
    o->token_len = token_len;
    memcpy(o->token, token, token_len);
    /* o->last_mid = 0; */
    o->notification_callback = notification_callback;
    o->data = data;
    LOG_DBG("Adding obs_subject for /%s [0x%02X%02X]\n", o->url, o->token[0],
            o->token[1]);
    list_add(obs_subjects_list, o);
  }

  return o;
}
/*----------------------------------------------------------------------------*/
void
coap_obs_remove_observee(coap_observee_t *o)
{
  LOG_DBG("Removing obs_subject for /%s [0x%02X%02X]\n", o->url, o->token[0],
          o->token[1]);
  memb_free(&obs_subjects_memb, o);
  list_remove(obs_subjects_list, o);
}
/*----------------------------------------------------------------------------*/
coap_observee_t *
coap_get_obs_subject_by_token(const uint8_t *token, size_t token_len)
{
  coap_observee_t *obs = NULL;

  for(obs = (coap_observee_t *)list_head(obs_subjects_list); obs;
      obs = obs->next) {
    LOG_DBG("Looking for token 0x%02X%02X\n", token[0], token[1]);
    if(obs->token_len == token_len
       && memcmp(obs->token, token, token_len) == 0) {
      return obs;
    }
  }

  return NULL;
}
/*----------------------------------------------------------------------------*/
int
coap_obs_remove_observee_by_token(const coap_endpoint_t *endpoint,
                                  uint8_t *token, size_t token_len)
{
  int removed = 0;
  coap_observee_t *obs = NULL;

  for(obs = (coap_observee_t *)list_head(obs_subjects_list); obs;
      obs = obs->next) {
    LOG_DBG("Remove check Token 0x%02X%02X\n", token[0], token[1]);
    if(coap_endpoint_cmp(&obs->endpoint, endpoint)
       && obs->token_len == token_len
       && memcmp(obs->token, token, token_len) == 0) {
      coap_obs_remove_observee(obs);
      removed++;
    }
  }
  return removed;
}
/*----------------------------------------------------------------------------*/
int
coap_obs_remove_observee_by_url(const coap_endpoint_t *endpoint,
                                const char *url)
{
  int removed = 0;
  coap_observee_t *obs = NULL;

  for(obs = (coap_observee_t *)list_head(obs_subjects_list); obs;
      obs = obs->next) {
    LOG_DBG("Remove check URL %s\n", url);
    if(coap_endpoint_cmp(&obs->endpoint, endpoint)
       && (obs->url == url || memcmp(obs->url, url, strlen(obs->url)) == 0)) {
      coap_obs_remove_observee(obs);
      removed++;
    }
  }
  return removed;
}
/*----------------------------------------------------------------------------*/
static void
simple_reply(coap_message_type_t type, const coap_endpoint_t *endpoint,
             coap_message_t *notification)
{
  static coap_message_t response[1];
  size_t len;

  coap_init_message(response, type, NO_ERROR, notification->mid);
  len = coap_serialize_message(response, coap_databuf());
  coap_sendto(endpoint, coap_databuf(), len);
}
/*----------------------------------------------------------------------------*/
static coap_notification_flag_t
classify_notification(coap_message_t *response, int first)
{
  if(!response) {
    LOG_DBG("no response\n");
    return NO_REPLY_FROM_SERVER;
  }
  LOG_DBG("server replied\n");
  if(!IS_RESPONSE_CODE_2_XX(response)) {
    LOG_DBG("error response code\n");
    return ERROR_RESPONSE_CODE;
  }
  if(!coap_is_option(response, COAP_OPTION_OBSERVE)) {
    LOG_DBG("server does not support observe\n");
    return OBSERVE_NOT_SUPPORTED;
  }
  if(first) {
    return OBSERVE_OK;
  }
  return NOTIFICATION_OK;
}
/*----------------------------------------------------------------------------*/
void
coap_handle_notification(const coap_endpoint_t *endpoint,
                         coap_message_t *notification)
{
  const uint8_t *token;
  int token_len;
  coap_observee_t *obs;
  coap_notification_flag_t flag;
  uint32_t observe;

  LOG_DBG("coap_handle_notification()\n");
  token_len = get_token(notification, &token);
  LOG_DBG("Getting token\n");
  if(0 == token_len) {
    LOG_DBG("Error while handling coap observe notification: "
            "no token in message\n");
    return;
  }
  LOG_DBG("Getting observee info\n");
  obs = coap_get_obs_subject_by_token(token, token_len);
  if(NULL == obs) {
    LOG_DBG("Error while handling coap observe notification: "
            "no matching token found\n");
    simple_reply(COAP_TYPE_RST, endpoint, notification);
    return;
  }
  if(notification->type == COAP_TYPE_CON) {
    simple_reply(COAP_TYPE_ACK, endpoint, notification);
  }
  if(obs->notification_callback != NULL) {
    flag = classify_notification(notification, 0);
    /* TODO: the following mechanism for discarding duplicates is too trivial */
    /* refer to Observe RFC for a better solution */
    if(flag == NOTIFICATION_OK) {
      coap_get_header_observe(notification, &observe);
      if(observe == obs->last_observe) {
        LOG_DBG("Discarding duplicate\n");
        return;
      }
      obs->last_observe = observe;
    }
    obs->notification_callback(obs, notification, flag);
  }
}
/*----------------------------------------------------------------------------*/
static void
handle_obs_registration_response(void *data, coap_message_t *response)
{
  coap_observee_t *obs;
  notification_callback_t notification_callback;
  coap_notification_flag_t flag;

  LOG_DBG("handle_obs_registration_response()\n");
  obs = (coap_observee_t *)data;
  notification_callback = obs->notification_callback;
  flag = classify_notification(response, 1);
  if(notification_callback) {
    notification_callback(obs, response, flag);
  }
  if(flag != OBSERVE_OK) {
    coap_obs_remove_observee(obs);
  }
}
/*----------------------------------------------------------------------------*/
uint8_t
coap_generate_token(uint8_t **token_ptr)
{
  static uint8_t token = 0;

  token++;
  /* FIXME: we should check that this token is not already used */
  *token_ptr = (uint8_t *)&token;
  return sizeof(token);
}
/*----------------------------------------------------------------------------*/
coap_observee_t *
coap_obs_request_registration(const coap_endpoint_t *endpoint, char *uri,
                              notification_callback_t notification_callback,
                              void *data)
{
  coap_message_t request[1];
  coap_transaction_t *t;
  uint8_t *token;
  uint8_t token_len;
  coap_observee_t *obs;

  obs = NULL;
  coap_init_message(request, COAP_TYPE_CON, COAP_GET, coap_get_mid());
  coap_set_header_uri_path(request, uri);
  coap_set_header_observe(request, 0);
  token_len = coap_generate_token(&token);
  set_token(request, token, token_len);
  t = coap_new_transaction(request->mid, request->token, request->token_len, endpoint);
  if(t) {
    obs = coap_obs_add_observee(endpoint, (uint8_t *)token, token_len, uri,
                                notification_callback, data);
    if(obs) {
      t->callback = handle_obs_registration_response;
      t->callback_data = obs;
      t->message_len = coap_serialize_message(request, t->message);
      coap_send_transaction(t);
    } else {
      LOG_DBG("Could not allocate obs_subject resource buffer\n");
      coap_clear_transaction(t);
    }
  } else {
    LOG_DBG("Could not allocate transaction buffer\n");
  }
  return obs;
}
#endif /* COAP_OBSERVE_CLIENT */
/** @} */
