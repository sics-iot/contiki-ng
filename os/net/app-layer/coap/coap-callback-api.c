/*
 * Copyright (c) 2016, SICS Swedish ICT
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
 *
 */

/**
 * \file
 *      Callback API for doing CoAP requests.
 *      Adapted from the blocking API.
 * \author
 *      Joakim Eriksson, joakime@sics.se
 */

/**
 * \addtogroup coap
 * @{
 */

#include "coap-engine.h"
#include "coap-callback-api.h"
#include "coap-transactions.h"
#include "sys/cc.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap"
#define LOG_LEVEL  LOG_LEVEL_COAP

static void coap_request_callback(void *callback_data, coap_message_t *response);

/*---------------------------------------------------------------------------*/

static int
progress_request(coap_callback_request_state_t *callback_state) {
  coap_request_state_t *state = &callback_state->state;
  coap_message_t *request = state->request;
  request->mid = coap_get_mid();
  if((state->transaction =
      coap_new_transaction(request->mid,
                           request->token, request->token_len,
                           state->remote_endpoint))) {
    state->transaction->callback = coap_request_callback;
    state->transaction->callback_data = state;

    if(state->block_num > 0) {
      coap_set_header_block2(request, state->block_num, 0,
                             COAP_MAX_CHUNK_SIZE);
    }
    state->transaction->message_len =
      coap_serialize_message(request, state->transaction->message);

    coap_send_transaction(state->transaction);
    LOG_DBG("Requested #%"PRIu32" (MID %u)\n", state->block_num, request->mid);
    return 1;
  }
  return 0;
}

/*---------------------------------------------------------------------------*/

static void
coap_request_callback(void *callback_data, coap_message_t *response)
{
  coap_callback_request_state_t *callback_state = (coap_callback_request_state_t*)callback_data;
  coap_request_state_t *state = &callback_state->state;

  state->response = response;

  LOG_DBG("request callback\n");

  if(!state->response) {
    LOG_WARN("Server not responding giving up...\n");
    state->status = COAP_REQUEST_STATUS_TIMEOUT;
    callback_state->callback(callback_state);
    return;
  }

  /* Got a response */
  coap_get_header_block2(state->response, &state->res_block, &state->more, NULL, NULL);
  uint32_t res_block1 = 0;
  coap_get_header_block1(state->response, &res_block1, NULL, NULL, NULL);

  LOG_DBG("Received #%lu%s B1:%lu (%u bytes)\n",
          (unsigned long)state->res_block, (unsigned)state->more ? "+" : "",
          (unsigned long)res_block1,
          state->response->payload_len);

  if(state->res_block == state->block_num) {
    /* Call the callback function as we have more data */
    if(state->more) {
      state->status = COAP_REQUEST_STATUS_MORE;
    } else {
      state->status = COAP_REQUEST_STATUS_RESPONSE;
    }
    callback_state->callback(callback_state);
    /* this is only for counting BLOCK2 blocks.*/
    ++(state->block_num);
  } else {
    LOG_WARN("WRONG BLOCK %"PRIu32"/%"PRIu32"\n", state->res_block, state->block_num);
    ++(state->block_error);
  }

  if(state->more) {
    if((state->block_error) < COAP_MAX_ATTEMPTS) {
      progress_request(callback_state);
    } else {
      /* failure - now we give up and notify the callback */
      state->status = COAP_REQUEST_STATUS_BLOCK_ERROR;
      callback_state->callback(callback_state);
    }
  } else {
    /* No more blocks, finish and notify the callback */
    state->status = COAP_REQUEST_STATUS_FINISHED;
    state->response = NULL;
    callback_state->callback(callback_state);
  }
}

/*---------------------------------------------------------------------------*/

int
coap_send_request(coap_callback_request_state_t *callback_state, coap_endpoint_t *endpoint,
                  coap_message_t *request,
                  void (*callback)(coap_callback_request_state_t *callback_state))
{
  coap_request_state_t *state = &callback_state->state;

  state->more = 0;
  state->res_block = 0;
  state->block_error = 0;
  state->block_num = 0;
  state->response = NULL;
  state->request = request;
  state->remote_endpoint = endpoint;
  callback_state->callback = callback;

  return progress_request(callback_state);
}
/*---------------------------------------------------------------------------*/
/** @} */
