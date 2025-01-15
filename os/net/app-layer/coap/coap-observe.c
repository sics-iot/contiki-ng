/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
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
 */

/**
 * \file
 *      CoAP module for observing resources (draft-ietf-core-observe-11).
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

/**
 * \addtogroup coap
 * @{
 */

#include <stdio.h>
#include <string.h>
#include "coap-observe.h"
#include "coap-engine.h"
#include "lib/memb.h"
#include "lib/list.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap"
#define LOG_LEVEL  LOG_LEVEL_COAP

/*---------------------------------------------------------------------------*/
MEMB(observers_memb, coap_observer_t, COAP_MAX_OBSERVERS);
LIST(observers_list);
/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
static coap_observer_t *
add_observer(const coap_endpoint_t *endpoint, const uint8_t *token,
             size_t token_len, const char *uri, int uri_len)
{
  /* Remove existing observe relationship, if any. */
  coap_remove_observer_by_uri(endpoint, uri);

  coap_observer_t *o = memb_alloc(&observers_memb);

  if(o) {
    int max = sizeof(o->url) - 1;
    if(max > uri_len) {
      max = uri_len;
    }
    memcpy(o->url, uri, max);
    o->url[max] = 0;
    coap_endpoint_copy(&o->endpoint, endpoint);
    o->token_len = token_len;
    memcpy(o->token, token, token_len);
    o->last_mid = 0;

    LOG_INFO("Adding observer (%u/%u) for /%s [0x%02X%02X]\n",
             list_length(observers_list) + 1, COAP_MAX_OBSERVERS,
             o->url, o->token[0], o->token[1]);
    list_add(observers_list, o);
  }

  return o;
}
/*---------------------------------------------------------------------------*/
/*- Removal -----------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_remove_observer(coap_observer_t *o)
{
  LOG_INFO("Removing observer for /%s [0x%02X%02X]\n", o->url, o->token[0],
           o->token[1]);

  memb_free(&observers_memb, o);
  list_remove(observers_list, o);
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_client(const coap_endpoint_t *endpoint)
{
  int removed = 0;
  coap_observer_t *obs = NULL;

  LOG_DBG("Remove check client ");
  LOG_DBG_COAP_EP(endpoint);
  LOG_DBG_("\n");
  for(obs = (coap_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    if(coap_endpoint_cmp(&obs->endpoint, endpoint)) {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_token(const coap_endpoint_t *endpoint,
                              uint8_t *token, size_t token_len)
{
  int removed = 0;
  coap_observer_t *obs = NULL;

  for(obs = (coap_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    LOG_DBG("Remove check Token 0x%02X%02X\n", token[0], token[1]);
    if(coap_endpoint_cmp(&obs->endpoint, endpoint)
       && obs->token_len == token_len
       && memcmp(obs->token, token, token_len) == 0) {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_uri(const coap_endpoint_t *endpoint,
                            const char *uri)
{
  int removed = 0;
  coap_observer_t *obs = NULL;

  for(obs = (coap_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    LOG_DBG("Remove check URL %p\n", uri);
    if((endpoint == NULL
        || (coap_endpoint_cmp(&obs->endpoint, endpoint)))
       && (obs->url == uri || memcmp(obs->url, uri, strlen(obs->url)) == 0)) {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
int
coap_remove_observer_by_mid(const coap_endpoint_t *endpoint, uint16_t mid)
{
  int removed = 0;
  coap_observer_t *obs = NULL;

  for(obs = (coap_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    LOG_DBG("Remove check MID %u\n", mid);
    if(coap_endpoint_cmp(&obs->endpoint, endpoint)
       && obs->last_mid == mid) {
      coap_remove_observer(obs);
      removed++;
    }
  }
  return removed;
}
/*---------------------------------------------------------------------------*/
/*- Notification ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_notify_observers(coap_resource_t *resource)
{
  coap_notify_observers_sub(resource, NULL);
}
/* Can be used either for sub - or when there is not resource - just
   a handler */
void
coap_notify_observers_sub(coap_resource_t *resource, const char *subpath)
{
  /* build notification */
  coap_message_t notification[1]; /* this way the message can be treated as pointer as usual */
  coap_message_t request[1]; /* this way the message can be treated as pointer as usual */
  coap_observer_t *obs = NULL;
  int url_len, obs_url_len;
  char url[COAP_OBSERVER_URL_LEN];
  uint8_t sub_ok = 0;

  if(resource != NULL) {
    url_len = strlen(resource->url);
    strncpy(url, resource->url, COAP_OBSERVER_URL_LEN - 1);
    if(url_len < COAP_OBSERVER_URL_LEN - 1 && subpath != NULL) {
      strncpy(&url[url_len], subpath, COAP_OBSERVER_URL_LEN - url_len - 1);
    }
  } else if(subpath != NULL) {
    strncpy(url, subpath, COAP_OBSERVER_URL_LEN - 1);
  } else {
    /* No resource, no subpath */
    return;
  }

  /* Ensure url is null terminated because strncpy does not guarantee this */
  url[COAP_OBSERVER_URL_LEN - 1] = '\0';
  /* url now contains the notify URL that needs to match the observer */
  LOG_INFO("Notification from %s\n", url);

  coap_init_message(notification, COAP_TYPE_NON, CONTENT_2_05, 0);
  /* create a "fake" request for the URI */
  coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
  coap_set_header_uri_path(request, url);

  /* iterate over observers */
  url_len = strlen(url);
  /* Assumes lazy evaluation... */
  sub_ok = (resource == NULL) || (resource->flags & HAS_SUB_RESOURCES);
  for(obs = (coap_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    obs_url_len = strlen(obs->url);

    /* Do a match based on the parent/sub-resource match so that it is
       possible to do parent-node observe */

    /***** TODO fix here so that we handle the notofication correctly ******/
    /* All the new-style ... is assuming that the URL might be within */
    if((obs_url_len == url_len
        || (obs_url_len > url_len
            && sub_ok
            && obs->url[url_len] == '/'))
       && strncmp(url, obs->url, url_len) == 0) {
      coap_transaction_t *transaction = NULL;

      /*TODO implement special transaction for CON, sharing the same buffer to allow for more observers */

      if((transaction = coap_new_transaction(coap_get_mid(), obs->token, obs->token_len, &obs->endpoint))) {
        /* if COAP_OBSERVE_REFRESH_INTERVAL is zero, never send observations as confirmable messages */
        if(COAP_OBSERVE_REFRESH_INTERVAL != 0
            && (obs->obs_counter % COAP_OBSERVE_REFRESH_INTERVAL == 0)) {
          LOG_DBG("           Force Confirmable for\n");
          notification->type = COAP_TYPE_CON;
        }

        LOG_DBG("           Observer ");
        LOG_DBG_COAP_EP(&obs->endpoint);
        LOG_DBG_("\n");

        /* update last MID for RST matching */
        obs->last_mid = transaction->mid;

        /* prepare response */
        notification->mid = transaction->mid;

	int32_t new_offset = 0;

        /* Either old style get_handler or the full handler */
        if(coap_call_handlers(request, notification, transaction->message +
                              COAP_MAX_HEADER_SIZE, COAP_MAX_CHUNK_SIZE,
                              &new_offset) > 0) {
          LOG_DBG("Notification on new handlers\n");
        } else {
          if(resource != NULL) {
            resource->get_handler(request, notification,
                                  transaction->message + COAP_MAX_HEADER_SIZE,
                                  COAP_MAX_CHUNK_SIZE, &new_offset);
          } else {
            /* What to do here? */
            notification->code = BAD_REQUEST_4_00;
          }
        }

        if(notification->code < BAD_REQUEST_4_00) {
          coap_set_header_observe(notification, (obs->obs_counter)++);
          /* mask out to keep the CoAP observe option length <= 3 bytes */
          obs->obs_counter &= 0xffffff;
        }
        coap_set_token(notification, obs->token, obs->token_len);

	if(new_offset != 0) {
	  coap_set_header_block2(notification,
				 0,
				 new_offset != -1,
				 COAP_MAX_BLOCK_SIZE);
	  coap_set_payload(notification,
			   notification->payload,
			   MIN(notification->payload_len,
			       COAP_MAX_BLOCK_SIZE));
	}

        transaction->message_len =
          coap_serialize_message(notification, transaction->message);

        coap_send_transaction(transaction);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void
coap_observe_handler(const coap_resource_t *resource, coap_message_t *coap_req,
                     coap_message_t *coap_res)
{
  const coap_endpoint_t *src_ep;
  coap_observer_t *obs;

  LOG_DBG("CoAP observer handler rsc: %d\n", resource != NULL);

  if(coap_req->code == COAP_GET && coap_res->code < 128) { /* GET request and response without error code */
    if(coap_is_option(coap_req, COAP_OPTION_OBSERVE)) {
      src_ep = coap_get_src_endpoint(coap_req);
      if(src_ep == NULL) {
        /* No source endpoint, can not add */
      } else if(coap_req->observe == 0) {
        obs = add_observer(src_ep,
                           coap_req->token, coap_req->token_len,
                           coap_req->uri_path, coap_req->uri_path_len);
        if(obs) {
          coap_set_header_observe(coap_res, (obs->obs_counter)++);
          /* mask out to keep the CoAP observe option length <= 3 bytes */
          obs->obs_counter &= 0xffffff;
          /*
           * Following payload is for demonstration purposes only.
           * A subscription should return the same representation as a normal GET.
           * Uncomment if you want an information about the avaiable observers.
           */
#if 0
          static char content[16];
          coap_set_payload(coap_res,
                           content,
                           snprintf(content, sizeof(content), "Added %u/%u",
                                    list_length(observers_list),
                                    COAP_MAX_OBSERVERS));
#endif
        } else {
          coap_res->code = SERVICE_UNAVAILABLE_5_03;
          coap_set_payload(coap_res, "TooManyObservers", 16);
        }
      } else if(coap_req->observe == 1) {

        /* remove client if it is currently observe */
        coap_remove_observer_by_token(src_ep,
                                      coap_req->token, coap_req->token_len);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
coap_has_observers(char *path)
{
  coap_observer_t *obs = NULL;

  for(obs = (coap_observer_t *)list_head(observers_list); obs;
      obs = obs->next) {
    if((strncmp(obs->url, path, strlen(path))) == 0) {
      return 1;
    }
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/** @} */
