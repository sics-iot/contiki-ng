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

#ifndef COAP_OBSERVE_H_
#define COAP_OBSERVE_H_

#include "coap.h"
#include "coap-transactions.h"
#include "coap-engine.h"

typedef struct coap_observer {
  struct coap_observer *next;   /* for LIST */

  char url[COAP_OBSERVER_URL_LEN];
  coap_endpoint_t endpoint;
  uint8_t token_len;
  uint8_t token[COAP_TOKEN_LEN];
  uint16_t last_mid;

  int32_t obs_counter;
} coap_observer_t;

void coap_remove_observer(coap_observer_t *o);
int coap_remove_observer_by_client(const coap_endpoint_t *ep);
int coap_remove_observer_by_token(const coap_endpoint_t *ep,
                                  uint8_t *token, size_t token_len);
int coap_remove_observer_by_uri(const coap_endpoint_t *ep,
                                const char *uri);
int coap_remove_observer_by_mid(const coap_endpoint_t *ep,
                                uint16_t mid);

void coap_notify_observers(coap_resource_t *resource);
void coap_notify_observers_sub(coap_resource_t *resource, const char *subpath);

void coap_observe_handler(const coap_resource_t *resource,
                          coap_message_t *request,
                          coap_message_t *response);

uint8_t coap_has_observers(char *path);

void coap_remove_all_observers(void);

#endif /* COAP_OBSERVE_H_ */
/** @} */
