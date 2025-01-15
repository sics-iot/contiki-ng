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
 *      ETSI Plugtest resource
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <string.h>
#include "coap-engine.h"
#include "coap.h"
#include "coap-transactions.h"
#include "coap-separate.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "Plugtest"
#define LOG_LEVEL LOG_LEVEL_PLUGTEST

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_resume_handler(void);

PERIODIC_RESOURCE(res_plugtest_separate,
                  "title=\"Resource which cannot be served immediately and which cannot be acknowledged in a piggy-backed way\"",
                  res_get_handler,
                  NULL,
                  NULL,
                  NULL,
                  3000,
                  res_resume_handler);

/* A structure to store the required information */
typedef struct application_separate_store {
  /* Provided by Erbium to store generic request information such as remote address and token. */
  coap_separate_t request_metadata;
  /* Add fields for addition information to be stored for finalizing, e.g.: */
  char buffer[MAX_PLUGFEST_PAYLOAD];
} application_separate_store_t;

static uint8_t separate_active = 0;
static application_separate_store_t separate_store[1];

void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  coap_message_t *const coap_req = (coap_message_t *)request;

  LOG_DBG("/separate       ");
  if(separate_active) {
    LOG_DBG_("REJECTED ");
    coap_separate_reject();
  } else {
    LOG_DBG_("STORED ");
    separate_active = 1;

    /* Take over and skip response by engine. */
    coap_separate_accept(request, &separate_store->request_metadata);
    /* Be aware to respect the Block2 option, which is also stored in the coap_separate_t. */

    snprintf(separate_store->buffer, MAX_PLUGFEST_PAYLOAD,
             "Type: %u\nCode: %u\nMID: %u", coap_req->type, coap_req->code,
             coap_req->mid);
  }

  LOG_DBG_("(%s %u)\n", coap_req->type == COAP_TYPE_CON ? "CON" : "NON", coap_req->mid);
}
static void
res_resume_handler()
{
  if(separate_active) {
    LOG_DBG("/separate       ");
    coap_transaction_t *transaction = NULL;
    if((transaction = coap_new_transaction(separate_store->request_metadata.mid,
                                           separate_store->request_metadata.token,
                                           separate_store->request_metadata.token_len,
                                           &separate_store->request_metadata.endpoint))) {
      LOG_DBG_(
        "RESPONSE (%s %u)\n", separate_store->request_metadata.type == COAP_TYPE_CON ? "CON" : "NON", separate_store->request_metadata.mid);

      coap_message_t response[1]; /* This way the message can be treated as pointer as usual. */

      /* Restore the request information for the response. */
      coap_separate_resume(response, &separate_store->request_metadata, CONTENT_2_05);

      coap_set_header_content_format(response, TEXT_PLAIN);
      coap_set_payload(response, separate_store->buffer,
                       strlen(separate_store->buffer));

      /*
       * Be aware to respect the Block2 option, which is also stored in the coap_separate_t.
       * As it is a critical option, this example resource pretends to handle it for compliance.
       */
      coap_set_header_block2(response,
                             separate_store->request_metadata.block2_num, 0,
                             separate_store->request_metadata.block2_size);

      /* Warning: No check for serialization error. */
      transaction->message_len = coap_serialize_message(response,
                                                       transaction->message);
      coap_send_transaction(transaction);
      /* The engine will clear the transaction (right after send for NON, after acked for CON). */

      separate_active = 0;
    } else {
      LOG_DBG_("ERROR (transaction)\n");
    }
  } /* if (separate_active) */
}
