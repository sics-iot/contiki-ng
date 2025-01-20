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
 *      Christos Koulamas <cklm@isi.gr>
 */

/**
 * \addtogroup edhoc
 * @{
 */

#ifndef _EDHOC_CLIENT_API_H_
#define _EDHOC_CLIENT_API_H_
#include "coap-engine.h"
#include "contiki-lib.h"
#include "edhoc-exporter.h"
#include "edhoc.h"
#include "coap-timer.h"
#include "coap-callback-api.h"
#include "coap-blocking-api.h"

/**
 * \brief The CoAP Server IP where run the EDHOC Responder
 */
#ifdef EDHOC_CONF_SERVER_EP
#define SERVER_EP EDHOC_CONF_SERVER_EP
#else
#define SERVER_EP "coap://[fd00::1]"
#endif

/**
 * \brief Time limit value to EDHOC protocol finished
 */
#ifdef EDHOC_CONF_TIMEOUT
#define CL_TIMEOUT_VAL EDHOC_CONF_TIMEOUT
#else
#define CL_TIMEOUT_VAL 10000
#endif

/**
 * \brief EDHOC client struct
 */
typedef struct edhoc_client {
  uint8_t state;
  coap_endpoint_t server_ep;
  coap_message_t request[1];
  coap_message_t response[1];
  uint16_t con_num;
  bool tx_msg1;
  bool rx_msg2;
  bool tx_msg3;
  bool rx_msg3_response;
}edhoc_client_t;

/**
 * \brief EDHOC client Application data struct
 */
typedef struct edhoc_client_ad {
  char ad_1[EDHOC_MAX_AD_SZ];
  uint16_t ad_1_sz;
  char ad_2[EDHOC_MAX_AD_SZ];
  uint16_t ad_2_sz;
  char ad_3[EDHOC_MAX_AD_SZ];
  uint16_t ad_3_sz;
} edhoc_client_ad_t;

/**
 * \brief EDHOC data event struct
 */
typedef struct edhoc_data_event {
  uint8_t val;
  edhoc_client_ad_t ad;
} edhoc_data_event_t;

/**
 * \brief Run the EDHOC Initiator role
 *
 *  This function must be called from the EDHOC Initiator program to start the EDHOC protocol
 *  as Initiator. Runs a new process that implements all the EDHOC protocol and exits
 *  when the EDHOC protocol finishes successfully or expire the EDHOC_CONF_ATTEMPTS.
 *  - When the EDHOC protocol finishes successfully a CL_FINISHED event is triggered.
 *  - When the EDHOC protocol expires the EDHOC_CONF_ATTEMPTS attempts a CL_TRIES_EXPIRE event is triggered
 */
void edhoc_client_run(void);

/**
 * \brief Check if the EDHOC client have finished
 * \param ev process event
 * \param data process data
 * \retval 1 if EDHOC Client process finished success
 * \retval -1 if EDHOC Client process expire attempts
 * \retval 0 if the event is not from EDHOC Client process or the EDHOC client process has not finished yet
 *
 *  This function checks the events trigger from the EDHOC client process looking for the
 *  CL_FINISHED or CL_TRIES_EXPIRE events.
 */
int8_t edhoc_client_callback(process_event_t ev, void *data);

/**
 * \brief Close the EDHOC context
 *
 * This function must be called after the Security Context is exported to free the
 * allocated memory.
 */
void edhoc_client_close(void);

/**
 * \brief Get the Application Data received in EDHOC message 2
 * \param buff A pointer to a buffer to copy the Application data
 * \return ad_sz The Application data length
 *
 * This function copy on the buff the Application data from the EDHOC message 2 received
 */
uint8_t edhoc_server_get_ad_2(char *buff);

/**
 * \brief Set the Application Data to be carried in EDHOC message 1
 * \param buff A pointer to a buffer that contains the Application data to be copied
 * \param buff_sz The Application data length
 *
 * This function set the Application data to be carried on EDHOC message 1
 */
void edhoc_server_set_ad_1(const void *buff, uint8_t buff_sz);

/**
 * \brief Set the Application Data to be carried in EDHOC message 3
 * \param buff A pointer to a buffer that contains the Application data to be copied
 * \param buff_sz The Application data length
 *
 * This function set the Application data to be carried on EDHOC message 3
 */
void edhoc_server_set_ad_3(const void *buff, uint8_t buff_sz);

/* static int edhoc_send_msg1(uint8_t *ad, uint8_t ad_sz, bool suite_array); */
/* static int client_block2_handler(coap_message_t *response, uint8_t *target, size_t *len, size_t max_len); */
/* static int edhoc_client_post_blocks(); */
/* static int edhoc_client_start(uint8_t *ad, uint8_t ad_sz); */
/* static void client_chunk_handler(coap_callback_request_state_t *callback_state); */
/* static void client_context_free(edhoc_client_t *ctx); */
/* static void client_response_handler(coap_callback_request_state_t *callback_state); */
/* static void client_timeout_callback(coap_timer_t *timer); */
/* static void edhoc_client_init(); */
/* static void edhoc_client_post(); */

#endif /* _EDHOC_CLIENT_API_H_ */
/** @} */
