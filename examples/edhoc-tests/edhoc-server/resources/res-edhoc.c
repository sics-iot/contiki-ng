/**
 * \file
 *      EDHOC plugtest resource [RFC9528] with CoAP Block-Wise Transfer [RFC7959]
 * \author
 *      Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund
 */

#include <stdio.h>
#include <string.h>
#include "coap-engine.h"
#include "coap.h"
#include "edhoc-server-API.h"

/*----------------------------------------------------------------------------*/
edhoc_server_t servidor;

static uint8_t msg_rx[EDHOC_MAX_PAYLOAD_LEN];
static size_t msg_rx_len;

static void res_edhoc_post_handler(coap_message_t *request,
                                   coap_message_t *response,
                                   uint8_t *buffer,
                                   uint16_t preferred_size,
                                   int32_t *offset);

RESOURCE(res_edhoc, "title=\"EDHOC resource\"", NULL,
         res_edhoc_post_handler, NULL, NULL);
static size_t big_msg_len = 0;

/*----------------------------------------------------------------------------*/
/* Example allows only one request on time. There are no checks for multiple access !!! */
static void
res_edhoc_post_handler(coap_message_t *request,
                       coap_message_t *response,
                       uint8_t *buffer,
                       uint16_t preferred_size,
                       int32_t *offset)
{
  int block_size = 300; /* FIXME: Make configurable */

  if(*offset == 0) {
    if(coap_block1_handler(request, response, msg_rx, &msg_rx_len,
                           EDHOC_MAX_PAYLOAD_LEN)) {
      LOG_DBG("handler (%d)\n", (int)msg_rx_len);
      print_buff_8_dbg(msg_rx, msg_rx_len);
      return;
    } else {
      LOG_DBG("RX msg (%d)\n", (int)msg_rx_len);
      print_buff_8_dbg(msg_rx, msg_rx_len);
      edhoc_server_process(request, response, &servidor, msg_rx, msg_rx_len);
    }
    response->payload = (uint8_t *)edhoc_ctx->buffers.msg_tx;
    response->payload_len = edhoc_ctx->buffers.tx_sz;
    big_msg_len = edhoc_ctx->buffers.tx_sz;
    coap_set_header_block1(response, request->block1_num, 0,
                           request->block1_size);

    if(response->payload_len > block_size) {
      coap_set_option(response, COAP_OPTION_BLOCK2);
      coap_set_header_block2(response, 0, 1, block_size);
    }
  } else {
    coap_set_status_code(response, CHANGED_2_04);
    memcpy(buffer, edhoc_ctx->buffers.msg_tx + *offset, block_size);
    if(edhoc_ctx->buffers.tx_sz - *offset < preferred_size) {
      preferred_size = edhoc_ctx->buffers.tx_sz - *offset;
      *offset = -1;
    } else {
      *offset += preferred_size;
    }
    coap_set_payload(response, buffer, preferred_size);
  }
}
/*----------------------------------------------------------------------------*/
