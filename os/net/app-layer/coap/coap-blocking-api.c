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
 *      CoAP implementation for the REST Engine.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

/**
 * \addtogroup coap
 * @{
 */

#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "sys/cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap"
#define LOG_LEVEL  LOG_LEVEL_COAP

/*---------------------------------------------------------------------------*/
/*- Client Part -------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_blocking_request_callback(void *callback_data, coap_message_t *response)
{
  coap_blocking_request_state_t *blocking_state = (coap_blocking_request_state_t *)callback_data;

  blocking_state->state.response = response;
  process_poll(blocking_state->process);
}
/*---------------------------------------------------------------------------*/
PT_THREAD(coap_blocking_request
          (coap_blocking_request_state_t *blocking_state, process_event_t ev,
           coap_endpoint_t *remote_ep,
           coap_message_t *request,
           coap_blocking_response_handler_t request_callback))
{
  /* Before PT_BEGIN in order to not be a local variable in the PT_Thread and maintain it */
  coap_request_state_t *state = &blocking_state->state;

  PT_BEGIN(&blocking_state->pt);

  state->block_num = 0;
  state->response = NULL;
  blocking_state->process = PROCESS_CURRENT();

  state->more = 0;
  state->res_block = 0;
  state->block_error = 0;

  do {
    request->mid = coap_get_mid();
    if((state->transaction = coap_new_transaction(request->mid,
                                                  request->token, request->token_len,
                                                  remote_ep))) {
      state->transaction->callback = coap_blocking_request_callback;
      state->transaction->callback_data = blocking_state;

      if(state->block_num > 0) {
        coap_set_header_block2(request, state->block_num, 0,
                               COAP_MAX_CHUNK_SIZE);
      }
      state->transaction->message_len = coap_serialize_message(request,
                                                              state->
                                                              transaction->
                                                              message);

      coap_send_transaction(state->transaction);
      LOG_DBG("Requested #%"PRIu32" (MID %u)\n", state->block_num, request->mid);

      PT_YIELD_UNTIL(&blocking_state->pt, ev == PROCESS_EVENT_POLL);

      if(!state->response) {
        LOG_WARN("Server not responding\n");
        state->status = COAP_REQUEST_STATUS_TIMEOUT;
        request_callback(NULL); /* Call the callback with NULL to signal timeout */
        PT_EXIT(&blocking_state->pt);
      }

      coap_get_header_block2(state->response, &state->res_block, &state->more, NULL, NULL);

      LOG_DBG("Received #%"PRIu32"%s (%u bytes)\n", state->res_block, state->more ? "+" : "",
              state->response->payload_len);
      if(state->more) {
        state->status = COAP_REQUEST_STATUS_MORE;
      } else {
        state->status = COAP_REQUEST_STATUS_RESPONSE;
      }

      if(state->res_block == state->block_num) {
        request_callback(state->response);
        ++(state->block_num);
      } else {
        LOG_WARN("WRONG BLOCK %"PRIu32"/%"PRIu32"\n",
                 state->res_block, state->block_num);
        ++(state->block_error);
      }
    } else {
      LOG_WARN("Could not allocate transaction buffer");
      PT_EXIT(&blocking_state->pt);
    }
  } while(state->more && (state->block_error) < COAP_MAX_ATTEMPTS);

  if((state->block_error) >= COAP_MAX_ATTEMPTS) {
     /* failure - now we give up */
    state->status = COAP_REQUEST_STATUS_BLOCK_ERROR;
  } else {
    /* No more blocks, request finished */
    state->status = COAP_REQUEST_STATUS_FINISHED;
  }
  PT_END(&blocking_state->pt);
}
/*---------------------------------------------------------------------------*/
/** @} */
