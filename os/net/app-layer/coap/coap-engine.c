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
 *      CoAP implementation Engine.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

/**
 * \addtogroup coap
 * @{
 */

#include "coap-engine.h"
#include "sys/cc.h"
#include "lib/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "coap-eng"
#define LOG_LEVEL  LOG_LEVEL_COAP

static void process_callback(coap_timer_t *t);

/*
 * To be called by HTTP/COAP server as a callback function when a new service
 * request appears.  This function dispatches the corresponding CoAP service.
 */
static int invoke_coap_resource_service(coap_message_t *request,
                                        coap_message_t *response,
                                        uint8_t *buffer, uint16_t buffer_size,
                                        int32_t *offset);

/*---------------------------------------------------------------------------*/
/*- Variables ---------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
LIST(coap_handlers);
LIST(coap_resource_services);
static uint8_t is_initialized = 0;

/*---------------------------------------------------------------------------*/
/*- CoAP service handlers---------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
coap_add_handler(coap_handler_t *handler)
{
  list_add(coap_handlers, handler);
}
/*---------------------------------------------------------------------------*/
void
coap_remove_handler(coap_handler_t *handler)
{
  list_remove(coap_handlers, handler);
}
/*---------------------------------------------------------------------------*/
coap_handler_status_t
coap_call_handlers(coap_message_t *request, coap_message_t *response,
                      uint8_t *buffer, uint16_t buffer_size, int32_t *offset)
{
  coap_handler_status_t status;
  coap_handler_t *r;
  for(r = list_head(coap_handlers); r != NULL; r = r->next) {
    if(r->handler) {
      status = r->handler(request, response, buffer, buffer_size, offset);
      if(status != COAP_HANDLER_STATUS_CONTINUE) {
        /* Request handled. */

        /* Check response code before doing observe! */
        if(request->code == COAP_GET) {
          coap_observe_handler(NULL, request, response);
        }

        return status;
      }
    }
  }
  return COAP_HANDLER_STATUS_CONTINUE;
}
/*---------------------------------------------------------------------------*/
static inline coap_handler_status_t
call_service(coap_message_t *request, coap_message_t *response,
             uint8_t *buffer, uint16_t buffer_size, int32_t *offset)
{
  coap_handler_status_t status;
  status = coap_call_handlers(request, response, buffer, buffer_size, offset);
  if(status != COAP_HANDLER_STATUS_CONTINUE) {
    return status;
  }
  status = invoke_coap_resource_service(request, response, buffer, buffer_size, offset);
  if(status != COAP_HANDLER_STATUS_CONTINUE) {
    return status;
  }

  coap_set_status_code(response, NOT_FOUND_4_04);

  return COAP_HANDLER_STATUS_CONTINUE;
}

/*---------------------------------------------------------------------------*/
/*- Server Part -------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

#if COAP_WELL_KNOWN_RESOURCE_ENABLED
/* The discover resource is included by default for CoAP */
extern coap_resource_t res_well_known_core;
#endif

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
int
coap_receive(const coap_endpoint_t *src,
             uint8_t *payload, uint16_t payload_length)
{
  /* static declaration reduces stack peaks and program code size */
  static coap_message_t message[1]; /* this way the message can be treated as pointer as usual */
  static coap_message_t response[1];
  coap_transaction_t *transaction = NULL;
  coap_handler_status_t status;

  coap_status_code = coap_parse_message(message, payload, payload_length);
  coap_set_src_endpoint(message, src);

  if(coap_status_code == NO_ERROR) {

    /*TODO duplicates suppression, if required by application */

    LOG_DBG("  Parsed: v %u, t %u, tkl %u, c %u, mid %u\n", message->version,
            message->type, message->token_len, message->code, message->mid);
    LOG_DBG("  URL:");
    LOG_DBG_COAP_STRING(message->uri_path, message->uri_path_len);
    LOG_DBG_("\n");
    LOG_DBG("  Payload: ");
    LOG_DBG_COAP_STRING((const char *)message->payload, message->payload_len);
    LOG_DBG_("\n");

#ifdef WITH_DTLS
    /* when using DTLS the payload can be overwritten by the respose */
    static uint8_t payload_buffer[COAP_MAX_CHUNK_SIZE];
    if(message->payload != NULL) {
      memcpy(payload_buffer, message->payload, message->payload_len);
      message->payload = payload_buffer;
    }
#endif /* WITH_DTLS */

    /* handle requests */
    if(message->code >= COAP_GET && message->code <= COAP_DELETE) {

      /* use transaction buffer for response to confirmable request */
      if((transaction = coap_new_transaction(message->mid, message->token, message->token_len, src))) {
        uint32_t block_num = 0;
        uint16_t block_size = COAP_MAX_BLOCK_SIZE;
        uint32_t block_offset = 0;
        int32_t new_offset = 0;

        /* prepare response */
        if(message->type == COAP_TYPE_CON) {
          /* reliable CON requests are answered with an ACK */
          coap_init_message(response, COAP_TYPE_ACK, CONTENT_2_05,
                            message->mid);
        } else {
          /* unreliable NON requests are answered with a NON as well */
          coap_init_message(response, COAP_TYPE_NON, CONTENT_2_05,
                            coap_get_mid());
          /* mirror token */
        }
        if(message->token_len) {
          coap_set_token(response, message->token, message->token_len);
          /* get offset for blockwise transfers */
        }
        if(coap_get_header_block2
           (message, &block_num, NULL, &block_size, &block_offset)) {
          LOG_DBG("Blockwise: block request %"PRIu32" (%u/%u) @ %"PRIu32" bytes\n",
                  block_num, block_size, COAP_MAX_BLOCK_SIZE, block_offset);
          block_size = MIN(block_size, COAP_MAX_BLOCK_SIZE);
          new_offset = block_offset;
        }

        if(new_offset < 0) {
          LOG_DBG("Blockwise: block request offset overflow\n");
          coap_status_code = BAD_OPTION_4_02;
#if COAP_MESSAGE_ON_ERROR
          coap_error_message = "BlockOutOfScope";
#endif
          status = COAP_HANDLER_STATUS_CONTINUE;
        } else {
          /* call CoAP framework and check if found and allowed */
          status = call_service(message, response,
                                transaction->message + COAP_MAX_HEADER_SIZE,
                                block_size, &new_offset);
        }

        if(status != COAP_HANDLER_STATUS_CONTINUE) {

            if(coap_status_code == NO_ERROR) {

              /* TODO coap_handle_blockwise(request, response, start_offset, end_offset); */

              /* resource is unaware of Block1 */
              if(coap_is_option(message, COAP_OPTION_BLOCK1)
                 && response->code < BAD_REQUEST_4_00
                 && !coap_is_option(response, COAP_OPTION_BLOCK1)) {
                LOG_DBG("Block1 NOT IMPLEMENTED\n");

                coap_status_code = NOT_IMPLEMENTED_5_01;
#if COAP_MESSAGE_ON_ERROR
                coap_error_message = "NoBlock1Support";
#endif

                /* client requested Block2 transfer */
              } else if(coap_is_option(message, COAP_OPTION_BLOCK2)) {

                /* unchanged new_offset indicates that resource is unaware of blockwise transfer */
                if(new_offset == block_offset) {
                  LOG_DBG("Blockwise: unaware resource with payload length %u/%u\n",
                          response->payload_len, block_size);
                  if(block_offset >= response->payload_len) {
                    LOG_DBG("handle_incoming_data(): block_offset >= response->payload_len\n");

                    response->code = BAD_OPTION_4_02;
                    coap_set_payload(response, "BlockOutOfScope", 15); /* a const char str[] and sizeof(str) produces larger code size */
                  } else {
                    coap_set_header_block2(response, block_num,
                                           response->payload_len -
                                           block_offset > block_size,
                                           block_size);
                    coap_set_payload(response,
                                     response->payload + block_offset,
                                     MIN(response->payload_len -
                                         block_offset, block_size));
                  } /* if(valid offset) */

                  /* resource provides chunk-wise data */
                } else {
                  LOG_DBG("Blockwise: blockwise resource, new offset %"PRId32"\n",
                          new_offset);
                  coap_set_header_block2(response, block_num,
                                         new_offset != -1
                                         || response->payload_len >
                                         block_size, block_size);

                  if(response->payload_len > block_size) {
                    coap_set_payload(response, response->payload,
                                     block_size);
                  }
                } /* if(resource aware of blockwise) */

                /* Resource requested Block2 transfer */
              } else if(new_offset != 0) {
                LOG_DBG("Blockwise: no block option for blockwise resource, using block size %u\n",
                        COAP_MAX_BLOCK_SIZE);

                coap_set_header_block2(response, 0, new_offset != -1,
                                       COAP_MAX_BLOCK_SIZE);
                coap_set_payload(response, response->payload,
                                 MIN(response->payload_len,
                                     COAP_MAX_BLOCK_SIZE));
              } /* blockwise transfer handling */
            } /* no errors/hooks */
            /* successful service callback */
            /* serialize response */
        }
          if(coap_status_code == NO_ERROR) {
            if((transaction->message_len = coap_serialize_message(response,
                                                                 transaction->
                                                                 message)) ==
               0) {
              coap_status_code = PACKET_SERIALIZATION_ERROR;
            }
          }
      } else {
        coap_status_code = SERVICE_UNAVAILABLE_5_03;
#if COAP_MESSAGE_ON_ERROR
        coap_error_message = "NoFreeTraBuffer";
#endif
      } /* if(transaction buffer) */

      /* handle responses */
    } else {

      if(message->type == COAP_TYPE_CON && message->code == 0) {
        LOG_INFO("Received Ping\n");
        coap_status_code = PING_RESPONSE;
      } else if(message->type == COAP_TYPE_ACK) {
        /* transactions are closed through lookup below */
        LOG_DBG("Received ACK\n");
      } else if(message->type == COAP_TYPE_RST) {
        LOG_INFO("Received RST\n");
        /* cancel possible subscriptions */
        coap_remove_observer_by_mid(src, message->mid);
      }

      const coap_endpoint_t *ep;
      if(message->type == COAP_TYPE_ACK && message->code == 0) {
        /* Handles regular (empty message) ACKs. */
        /* ACKs carrying a piggyback response is handled below in the same way as any normal response */
        LOG_DBG("Got empty ACK. Waiting for separate response\n");
        if((transaction = coap_get_transaction_by_mid(message->mid))) {
          transaction->acked = true;
        }
      } else if((ep = coap_get_src_endpoint(message))) {
        if((transaction = coap_get_transaction_by_token_and_endpoint(message->token, message->token_len, ep))) {
          /* free transaction memory before callback, as it may create a new transaction */
          coap_resource_response_handler_t callback = transaction->callback;
          void *callback_data = transaction->callback_data;

          coap_clear_transaction(transaction);

          if(message->type == COAP_TYPE_CON) {
            LOG_DBG("Got CON response. Need to ACK\n");

            coap_message_t ack[1];
            /* ACK with empty code (0) */
            coap_init_message(ack, COAP_TYPE_ACK, 0, message->mid);
            /* serializing into IPBUF: Overwrites parts that are already parsed into the response struct */
            coap_sendto(ep, coap_databuf(), coap_serialize_message(ack, coap_databuf()));
          }

          /* check if someone registered for the response */
          if(callback) {
            callback(callback_data, message);
          }
        }
      } else {
        LOG_ERR("ERROR: no endpoint in response\n");
      }
      /* if(ACKed transaction) */
      transaction = NULL;

#if COAP_OBSERVE_CLIENT
      /* if observe notification */
      if((message->type == COAP_TYPE_CON || message->type == COAP_TYPE_NON)
         && coap_is_option(message, COAP_OPTION_OBSERVE)) {
        LOG_DBG("Observe [%"PRId32"]\n", message->observe);
        coap_handle_notification(src, message);
      }
#endif /* COAP_OBSERVE_CLIENT */
    } /* request or response */
  } /* parsed correctly */

    /* if(parsed correctly) */
  if(coap_status_code == NO_ERROR) {
    if(transaction) {
      coap_send_transaction(transaction);
    }
  } else if(coap_status_code == MANUAL_RESPONSE) {
    LOG_DBG("Clearing transaction for manual response");
    coap_clear_transaction(transaction);
  } else {
    coap_message_type_t reply_type = COAP_TYPE_ACK;

#if COAP_MESSAGE_ON_ERROR
    LOG_WARN("ERROR %u: %s\n", coap_status_code, coap_error_message);
#else
    LOG_WARN("ERROR %u\n", coap_status_code);
#endif
    coap_clear_transaction(transaction);

    if(coap_status_code == PING_RESPONSE) {
      coap_status_code = 0;
      reply_type = COAP_TYPE_RST;
    } else if(coap_status_code >= 192) {
      /* set to sendable error code */
      coap_status_code = INTERNAL_SERVER_ERROR_5_00;
      /* reuse input buffer for error message */
    }
    coap_init_message(message, reply_type, coap_status_code,
                      message->mid);
#if COAP_MESSAGE_ON_ERROR
    coap_set_payload(message, coap_error_message,
                     strlen(coap_error_message));
#endif
    coap_sendto(src, payload, coap_serialize_message(message, payload));
  }

  /* if(new data) */
  return coap_status_code;
}
/*---------------------------------------------------------------------------*/
void
coap_engine_init(void)
{
  /* avoid initializing twice */
  if(is_initialized) {
    return;
  }
  is_initialized = 1;

  LOG_INFO("Starting CoAP engine...\n");

  list_init(coap_handlers);
  list_init(coap_resource_services);

#if COAP_WELL_KNOWN_RESOURCE_ENABLED
  coap_activate_resource(&res_well_known_core, ".well-known/core");
#endif

  coap_transport_init();
  coap_init_connection();
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Makes a resource available under the given URI path
 *
 * The resource implementation must be imported first using the
 * extern keyword. The build system takes care of compiling every
 * *.c file in the ./resources/ sub-directory (see example Makefile).
 */
void
coap_activate_resource(coap_resource_t *resource, const char *path)
{
  coap_periodic_resource_t *periodic;
  resource->url = path;
  list_add(coap_resource_services, resource);

  LOG_INFO("Activating: %s\n", resource->url);

  /* Only add periodic resources with a periodic_handler and a period > 0. */
  if(resource->flags & IS_PERIODIC && resource->periodic
     && resource->periodic->periodic_handler
     && resource->periodic->period) {
    LOG_DBG("Periodic resource: %p (%s)\n", resource->periodic, path);
    periodic = resource->periodic;
    coap_timer_set_callback(&periodic->periodic_timer, process_callback);
    coap_timer_set_user_data(&periodic->periodic_timer, resource);
    coap_timer_set(&periodic->periodic_timer, periodic->period);
  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/*- Internal API ------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
coap_resource_t *
coap_get_first_resource(void)
{
  return list_head(coap_resource_services);
}
/*---------------------------------------------------------------------------*/
coap_resource_t *
coap_get_next_resource(coap_resource_t *resource)
{
  return list_item_next(resource);
}
/*---------------------------------------------------------------------------*/
static int
invoke_coap_resource_service(coap_message_t *request, coap_message_t *response,
                             uint8_t *buffer, uint16_t buffer_size,
                             int32_t *offset)
{
  uint8_t found = 0;
  uint8_t allowed = 1;

  coap_resource_t *resource = NULL;
  const char *url = NULL;
  int url_len, res_url_len;

  url_len = coap_get_header_uri_path(request, &url);
  for(resource = list_head(coap_resource_services);
      resource; resource = resource->next) {

    /* if the web service handles that kind of requests and urls matches */
    res_url_len = strlen(resource->url);
    if((url_len == res_url_len
        || (url_len > res_url_len
            && (resource->flags & HAS_SUB_RESOURCES)
            && url[res_url_len] == '/'))
       && strncmp(resource->url, url, res_url_len) == 0) {
      coap_resource_flags_t method = coap_get_method_type(request);
      found = 1;

      LOG_INFO("/%s, method %u, resource->flags %u\n", resource->url,
               (uint16_t)method, resource->flags);

      if((method & METHOD_GET) && resource->get_handler != NULL) {
        /* call handler function */
        resource->get_handler(request, response, buffer, buffer_size, offset);
      } else if((method & METHOD_POST) && resource->post_handler != NULL) {
        /* call handler function */
        resource->post_handler(request, response, buffer, buffer_size,
                               offset);
      } else if((method & METHOD_PUT) && resource->put_handler != NULL) {
        /* call handler function */
        resource->put_handler(request, response, buffer, buffer_size, offset);
      } else if((method & METHOD_DELETE) && resource->delete_handler != NULL) {
        /* call handler function */
        resource->delete_handler(request, response, buffer, buffer_size,
                                 offset);
      } else {
        allowed = 0;
        coap_set_status_code(response, METHOD_NOT_ALLOWED_4_05);
      }
      break;
    }
  }
  if(!found) {
    coap_set_status_code(response, NOT_FOUND_4_04);
  } else if(allowed) {
    /* final handler for special flags */
    if(resource->flags & IS_OBSERVABLE) {
      coap_observe_handler(resource, request, response);
    }
  }
  return found & allowed;
}
/*---------------------------------------------------------------------------*/
/* This callback occurs when t is expired */
static void
process_callback(coap_timer_t *t)
{
  coap_resource_t *resource;
  resource = coap_timer_get_user_data(t);
  if(resource != NULL && (resource->flags & IS_PERIODIC)
     && resource->periodic != NULL && resource->periodic->period) {
    LOG_DBG("Periodic: timer expired for /%s (period: %"PRIu32")\n",
            resource->url, resource->periodic->period);

    if(!is_initialized) {
      /* CoAP has not yet been initialized. */
    } else if(resource->periodic->periodic_handler) {
      /* Call the periodic_handler function. */
      resource->periodic->periodic_handler();
    }

    coap_timer_set(t, resource->periodic->period);
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
