/*
 * Copyright (c) 2018-2019, RISE SICS
 * Copyright (C) 2022 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * \author
 *      Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *      Yevgen Gyl <yevgen.gyl@tii.ae>
 */
#include "contiki.h"
#include "dev/radio.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "em_core.h"
#include "em_system.h"

#include "efr32-radio-buffer.h"

#include "pa_conversions_efr32.h"
#include "sl_rail_util_dma.h"
#include "sl_rail_util_pti.h"
#include "sl_rail_util_rf_path.h"
#include "sl_rail_util_rf_path_switch.h"
#include "sl_rail_util_rssi.h"
#include "rail.h"
#include <ieee802154/rail_ieee802154.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "EFR32-2p4GHz"
#define LOG_LEVEL LOG_LEVEL_NONE

enum {
  IEEE802154_ACK_REQUEST     = 1 << 5,
};

typedef enum {
  TX_IDLE,
  TX_SENDING,
  TX_SENT,
  TX_NO_ACK,
  TX_CHANNEL_BUSY,
  TX_ERROR
} tx_status_t;

#define CHANNEL_MIN            11
#define CHANNEL_MAX            26
#define PTI_ENABLED            false

extern const RAIL_ChannelConfig_t *const RAIL_IEEE802154_Phy2p4GHz;

static void rail_events_cb(RAIL_Handle_t rail_handle, RAIL_Events_t events);
static RAILSched_Config_t rail_sched_config;
static RAIL_Config_t rail_config = {
  .eventsCallback = &rail_events_cb,
  .scheduler = &rail_sched_config,
};
static union {
  RAIL_FIFO_ALIGNMENT_TYPE align[RAIL_FIFO_SIZE / RAIL_FIFO_ALIGNMENT];
  uint8_t fifo[RAIL_FIFO_SIZE];
} sRailRxFifo;
static RAIL_Handle_t sRailHandle = NULL;
static RAIL_Time_t last_rx_time = 0;
static int16_t last_rssi;
static int16_t last_lqi;
/* The process for receiving packets */
PROCESS(efr32_radio_process, "efr32 radio driver");
static uint8_t send_on_cca = 1;
static uint8_t poll_mode = 0;
static RAIL_DataConfig_t data_config = {
  .txSource = TX_PACKET_DATA,
  .rxSource = RX_PACKET_DATA,
  .txMethod = PACKET_MODE,
  .rxMethod = PACKET_MODE,
};
static RAIL_IEEE802154_Config_t rail_ieee802154_config = {
  .addresses = NULL,
  .ackConfig = {
    .enable = 1,
    .ackTimeout = 672,
    .rxTransitions = {
      .success = RAIL_RF_STATE_RX,
      .error = RAIL_RF_STATE_IDLE
    },
    .txTransitions = {
      .success = RAIL_RF_STATE_RX,
      .error = RAIL_RF_STATE_IDLE
    }
  },
  .timings = {
    .idleToRx = 100,
    .idleToTx = 100,
    .rxToTx = 192,
    .txToRx = 192 - 10,
    .rxSearchTimeout = 0,
    .txToRxSearchTimeout = 0,
  },
  .framesMask = RAIL_IEEE802154_ACCEPT_STANDARD_FRAMES
    | RAIL_IEEE802154_ACCEPT_ACK_FRAMES,
  .promiscuousMode = 0,
  .isPanCoordinator = 0,
  .defaultFramePendingInOutgoingAcks = 0,
};
static union {
  RAIL_FIFO_ALIGNMENT_TYPE align[RAIL_FIFO_SIZE / RAIL_FIFO_ALIGNMENT];
  uint8_t fifo[RAIL_FIFO_SIZE];
} sRailTxFifo;
static uint16_t panid = 0xabcd;
static int channel = IEEE802154_DEFAULT_CHANNEL;
static int cca_threshold = -85;
static RAIL_TxOptions_t txOptions = RAIL_TX_OPTIONS_DEFAULT;
volatile tx_status_t tx_status;
volatile bool is_receiving = false;
/*---------------------------------------------------------------------------*/
RAIL_Status_t
RAILCb_SetupRxFifo(RAIL_Handle_t railHandle)
{
  uint16_t rxFifoSize = RAIL_FIFO_SIZE;
  RAIL_Status_t status = RAIL_SetRxFifo(railHandle, sRailRxFifo.fifo, &rxFifoSize);
  if(rxFifoSize != RAIL_FIFO_SIZE) {
    /* We set up an incorrect FIFO size */
    return RAIL_STATUS_INVALID_PARAMETER;
  }
  if(status == RAIL_STATUS_INVALID_STATE) {
    /* Allow failures due to multiprotocol */
    return RAIL_STATUS_NO_ERROR;
  }
  return status;
}
/*---------------------------------------------------------------------------*/
static uint8_t
configure_radio_interrupts(void)
{
  RAIL_Status_t rail_status;

  rail_status = RAIL_ConfigEvents(sRailHandle, RAIL_EVENTS_ALL,
                                  RAIL_EVENT_RX_SYNC1_DETECT |
                                  RAIL_EVENT_RX_SYNC2_DETECT |
                                  RAIL_EVENT_RX_ACK_TIMEOUT |
                                  RAIL_EVENT_RX_FRAME_ERROR |
                                  RAIL_EVENT_RX_PACKET_RECEIVED |
                                  RAIL_EVENT_RX_FIFO_OVERFLOW |
                                  RAIL_EVENT_RX_ADDRESS_FILTERED |
                                  RAIL_EVENT_RX_PACKET_ABORTED |
                                  RAIL_EVENT_TX_PACKET_SENT |
                                  RAIL_EVENT_TX_CHANNEL_BUSY |
                                  RAIL_EVENT_TX_ABORTED |
                                  RAIL_EVENT_TX_BLOCKED |
                                  RAIL_EVENT_TX_UNDERFLOW |
                                  RAIL_EVENT_CAL_NEEDED);

  if(rail_status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_ConfigEvents failed, return value: %d\n", rail_status);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
handle_receive(void)
{
  RAIL_RxPacketHandle_t rx_packet_handle;
  RAIL_RxPacketDetails_t packet_details;
  RAIL_RxPacketInfo_t packet_info;
  RAIL_Status_t rail_status;
  uint16_t length = 0;
  rx_buffer_t *rx_buf;

  rx_packet_handle = RAIL_GetRxPacketInfo(sRailHandle, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE, &packet_info);

  if(rx_packet_handle == RAIL_RX_PACKET_HANDLE_INVALID) {
    return;
  }

  packet_details.isAck = false;
  packet_details.timeReceived.timePosition = RAIL_PACKET_TIME_AT_SYNC_END;
  packet_details.timeReceived.totalPacketBytes = 0;

  rail_status = RAIL_GetRxPacketDetails(sRailHandle, rx_packet_handle, &packet_details);
  if(rail_status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("Failed to get packet details\n");
    return;
  }

  length = packet_info.packetBytes - 1;
  LOG_INFO("EFR32 Radio: rcv:%d\n", length);

  /* skip length byte */
  packet_info.firstPortionData++;
  packet_info.firstPortionBytes--;
  packet_info.packetBytes--;

  rx_buf = get_empty_rx_buf();
  if(rx_buf != NULL) {
    rx_buf->len = length;

    /* read packet into buffer */
    memcpy(rx_buf->buf, packet_info.firstPortionData,
           packet_info.firstPortionBytes);
    if(packet_info.lastPortionData != NULL) {
      memcpy(rx_buf->buf + packet_info.firstPortionBytes,
             packet_info.lastPortionData,
             packet_info.packetBytes - packet_info.firstPortionBytes);
    }
    rx_buf->rssi = packet_details.rssi;
    rx_buf->lqi = packet_details.lqi;
    rx_buf->timestamp = packet_details.timeReceived.packetTime;
  } else {
    LOG_INFO("EFR32 Radio: Could not allocate rx_buf\n");
  }

  /* after the copy of the packet, the RX packet can be release for RAIL */
  rail_status = RAIL_ReleaseRxPacket(sRailHandle, rx_packet_handle);
  if(rail_status != RAIL_STATUS_NO_ERROR) {
    LOG_WARN("RAIL_ReleaseRxPacket() result:%d\n", rail_status);
  }
}
/*---------------------------------------------------------------------------*/
static void
rail_events_cb(RAIL_Handle_t rail_handle, RAIL_Events_t events)
{
  if(events & (RAIL_EVENT_TX_ABORTED | RAIL_EVENT_TX_BLOCKED | RAIL_EVENT_TX_UNDERFLOW)) {
    tx_status = TX_ERROR;
  }

  if(events & RAIL_EVENT_RX_ACK_TIMEOUT) {
    tx_status = TX_NO_ACK;
  }

  if(events & (RAIL_EVENT_RX_FIFO_OVERFLOW
               | RAIL_EVENT_RX_ADDRESS_FILTERED
               | RAIL_EVENT_RX_PACKET_ABORTED
               | RAIL_EVENT_RX_FRAME_ERROR
               | RAIL_EVENT_RX_PACKET_RECEIVED)) {
    is_receiving = false;
    if(events & RAIL_EVENT_RX_PACKET_RECEIVED) {
      RAIL_HoldRxPacket(rail_handle);
      if(!poll_mode) {
        LOG_INFO("EFR32 Radio: Receive event - poll.\n");
        process_poll(&efr32_radio_process);
      }
    }
  }

  if(events & RAIL_EVENT_TX_PACKET_SENT) {
    LOG_INFO("EFR32 Radio: packet sent\n");
    tx_status = TX_SENT;
  }

  if(events & RAIL_EVENT_TX_CHANNEL_BUSY) {
    tx_status = TX_CHANNEL_BUSY;
  }

  if(events & (RAIL_EVENT_RX_SYNC1_DETECT | RAIL_EVENT_RX_SYNC2_DETECT)) {
    is_receiving = true;
  }

  if(events & RAIL_EVENT_CAL_NEEDED) {
    (void)RAIL_Calibrate(rail_handle, NULL, RAIL_CAL_ALL_PENDING);
  }
}
/*---------------------------------------------------------------------------*/
static int
init(void)
{
  RAIL_Status_t status;
  uint8_t *ext_addr;
  uint16_t short_addr;
  uint16_t allocated_tx_fifo_size;
  uint64_t system_number;

  NVIC_SetPriority(FRC_PRI_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(FRC_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(MODEM_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(RAC_SEQ_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(RAC_RSM_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(BUFC_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(AGC_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(PROTIMER_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
  NVIC_SetPriority(SYNTH_IRQn, CORE_INTERRUPT_HIGHEST_PRIORITY);
 
  sl_rail_util_dma_init();
  sl_rail_util_pa_init();
  sl_rail_util_pti_init();
  sl_rail_util_rf_path_init();
  sl_rail_util_rf_path_switch_init();
  sl_rail_util_rssi_init();

  /* initializes the RAIL core */
  sRailHandle = RAIL_Init(&rail_config, NULL);
  if(sRailHandle == NULL) {
    LOG_ERR("RAIL_Init failed, return value: NULL\n");
    return 0;
  }

  status = RAIL_InitPowerManager();
  if(status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_InitPowerManager failed, return value: %d\n", status);
    return 0;
  }

  status = RAIL_ConfigData(sRailHandle, &data_config);

  if(status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_ConfigData failed, return value: %d\n", status);
    return 0;
  }

  /* configures the channels */
  (void)RAIL_ConfigChannels(sRailHandle,
                            RAIL_IEEE802154_Phy2p4GHz,
                            &sl_rail_util_pa_on_channel_config_change);

  status = RAIL_IEEE802154_Init(sRailHandle, &rail_ieee802154_config);
  if(status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_IEEE802154_Init failed, return value: %d\n", status);
    return 0;
  }

  status = RAIL_IEEE802154_Config2p4GHzRadio(sRailHandle);
  if(status != RAIL_STATUS_NO_ERROR) {
    (void)RAIL_IEEE802154_Deinit(sRailHandle);
    LOG_ERR("RAIL_IEEE802154_Config2p4GHzRadio failed, return value: %d\n", status);
    return 0;
  }

  status = RAIL_IEEE802154_ConfigEOptions(sRailHandle,
                                          (RAIL_IEEE802154_E_OPTION_GB868
                                           | RAIL_IEEE802154_E_OPTION_ENH_ACK
                                           | RAIL_IEEE802154_E_OPTION_IMPLICIT_BROADCAST),
                                          (RAIL_IEEE802154_E_OPTION_GB868
                                           | RAIL_IEEE802154_E_OPTION_ENH_ACK
                                           | RAIL_IEEE802154_E_OPTION_IMPLICIT_BROADCAST));

  if(status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_IEEE802154_ConfigEOptions failed, return value: %d\n", status);
    return 0;
  }

  status = RAIL_ConfigCal(sRailHandle,
                          0U
                          | (0
                             ? RAIL_CAL_TEMP : 0U)
                          | (0
                             ? RAIL_CAL_ONETIME : 0U));

  if(status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_ConfigCal failed, return value: %d\n", status);
    return 0;
  }

  if(!configure_radio_interrupts()) {
    return 0;
  }

  system_number = SYSTEM_GetUnique();

  ext_addr = (uint8_t *)&system_number;
  short_addr = ext_addr[7];
  short_addr |= ext_addr[6] << 8;

  NETSTACK_RADIO.set_value(RADIO_PARAM_PAN_ID, IEEE802154_PANID);
  NETSTACK_RADIO.set_value(RADIO_PARAM_16BIT_ADDR, short_addr);
  NETSTACK_RADIO.set_object(RADIO_PARAM_64BIT_ADDR, &system_number, sizeof(system_number));

  allocated_tx_fifo_size = RAIL_SetTxFifo(sRailHandle, sRailTxFifo.fifo,
                                          0u, RAIL_FIFO_SIZE);

  if(allocated_tx_fifo_size != RAIL_FIFO_SIZE) {
    LOG_ERR("RAIL_SetTxFifo() failed to allocate a large enough fifo"
            " (%d bytes instead of %d bytes)\n",
            allocated_tx_fifo_size, RAIL_FIFO_SIZE);
    return 0;
  }

  if(PTI_ENABLED) {
    status = RAIL_SetPtiProtocol(sRailHandle, RAIL_PTI_PROTOCOL_802154);
    if(status != RAIL_STATUS_NO_ERROR) {
      LOG_ERR("RAIL_SetPtiProtocol() status: %d failed\n", status);
      return 0;
    }
    status = RAIL_EnablePti(sRailHandle, PTI_ENABLED);
    if(status != RAIL_STATUS_NO_ERROR) {
      LOG_ERR("RAIL_EnablePti() status: %d failed\n", status);
      return 0;
    }
  }

  process_start(&efr32_radio_process, NULL);

  return 0;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  if(!value) {
    return RADIO_RESULT_INVALID_VALUE;
  }

  switch(param) {
  case RADIO_PARAM_PAN_ID:
    *value = panid;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_CHANNEL:
    *value = channel;
    return RADIO_RESULT_OK;
  case RADIO_CONST_MAX_PAYLOAD_LEN:
    *value = RAIL_FIFO_SIZE;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RX_MODE:
    *value = 0;
    if(!rail_ieee802154_config.promiscuousMode) {
      *value |= RADIO_RX_MODE_ADDRESS_FILTER;
    }
    if(rail_ieee802154_config.ackConfig.enable) {
      *value |= RADIO_RX_MODE_AUTOACK;
    }
    if(poll_mode) {
      *value |= RADIO_RX_MODE_POLL_MODE;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_TX_MODE:
    *value = 0;
    if(send_on_cca) {
      *value |= RADIO_TX_MODE_SEND_ON_CCA;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_LAST_RSSI:
    *value = last_rssi;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_LAST_LINK_QUALITY:
    *value = last_lqi;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RSSI:
  {
    int16_t rssi_value = RAIL_GetRssi(sRailHandle, true);
    if(rssi_value != RAIL_RSSI_INVALID) {
      /* RAIL_RxGetRSSI() returns value in quarter dBm (dBm * 4) */
      *value = rssi_value / 4;
      return RADIO_RESULT_OK;
    }
    return RADIO_RESULT_ERROR;
  }
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  switch(param) {
  case RADIO_PARAM_CHANNEL:
    if(value < CHANNEL_MIN ||
       value > CHANNEL_MAX) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    channel = value;
    /* start receiving on that channel */
    const RAIL_Status_t status = RAIL_StartRx(sRailHandle, channel, NULL);
    if(status != RAIL_STATUS_NO_ERROR) {
      LOG_ERR("Could not start RX on channel %d\n", channel);
      return RADIO_RESULT_ERROR;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_PAN_ID:
    panid = value & 0xffff;
    RAIL_IEEE802154_SetPanId(sRailHandle, panid, 0);
    return RADIO_RESULT_OK;
  case RADIO_PARAM_16BIT_ADDR:
    (void)RAIL_IEEE802154_SetShortAddress(sRailHandle, value & 0xFFFF, 0);
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RX_MODE:
    if(value & ~(RADIO_RX_MODE_ADDRESS_FILTER |
                 RADIO_RX_MODE_AUTOACK |
                 RADIO_RX_MODE_POLL_MODE)) {
      return RADIO_RESULT_INVALID_VALUE;
    }

    rail_ieee802154_config.ackConfig.enable = (value & RADIO_RX_MODE_AUTOACK) != 0;
    rail_ieee802154_config.promiscuousMode = (value & RADIO_RX_MODE_ADDRESS_FILTER) == 0;
    poll_mode = (value & RADIO_RX_MODE_POLL_MODE) != 0;

    if(!rail_ieee802154_config.ackConfig.enable) {
      RAIL_StateTransitions_t transitions = {
        .error = RAIL_RF_STATE_RX,
        .success = RAIL_RF_STATE_RX,
      };
      RAIL_SetTxTransitions(sRailHandle, &transitions);
      RAIL_SetRxTransitions(sRailHandle, &transitions);
    }
    (void)RAIL_ConfigAutoAck(sRailHandle, &rail_ieee802154_config.ackConfig);
    (void)RAIL_IEEE802154_SetPromiscuousMode(sRailHandle, rail_ieee802154_config.promiscuousMode);
    (void)configure_radio_interrupts();

    return RADIO_RESULT_OK;
  case RADIO_PARAM_TX_MODE:
    if(value & ~(RADIO_TX_MODE_SEND_ON_CCA)) {
      return RADIO_RESULT_INVALID_VALUE;
    }

    send_on_cca = (value & RADIO_TX_MODE_SEND_ON_CCA) != 0;
    return RADIO_RESULT_OK;
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
  if(param == RADIO_PARAM_LAST_PACKET_TIMESTAMP) {
    if(size != sizeof(rtimer_clock_t) || !dest) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    *(rtimer_clock_t *)dest = last_rx_time;
    return RADIO_RESULT_OK;
  }

  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
  if(param == RADIO_PARAM_64BIT_ADDR) {
    if(size != 8 || src == NULL) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    (void)RAIL_IEEE802154_SetLongAddress(sRailHandle, src, 0);
    return RADIO_RESULT_OK;
  }
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static int
channel_clear(void)
{
  if(RAIL_GetRssi(sRailHandle, true) / 4 > cca_threshold) {
    return 0;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
prepare(const void *payload, unsigned short payload_len)
{
  uint8_t plen = (uint8_t)payload_len + 2;
  uint8_t *data = (uint8_t *)payload;
  int written = 0;
  /* write the length byte */
  written += RAIL_WriteTxFifo(sRailHandle, &plen, 1, true);
  /* write the payload */
  written += RAIL_WriteTxFifo(sRailHandle, payload, payload_len, false);

  LOG_INFO("EFR32 Radio - wrote %d bytes to TX fifo\n", written);

  if(!(poll_mode) && (data[0] & IEEE802154_ACK_REQUEST)) {
    txOptions = RAIL_TX_OPTION_WAIT_FOR_ACK;
  } else {
    txOptions = RAIL_TX_OPTIONS_DEFAULT;
  }

  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
transmit(unsigned short transmit_len)
{
  uint8_t ret = RADIO_TX_OK;
  RAIL_CsmaConfig_t csmaConfig =
    RAIL_CSMA_CONFIG_802_15_4_2003_2p4_GHz_OQPSK_CSMA;
  RAIL_Status_t status;

  LOG_INFO("EFR32 Radio: Sending packet %d bytes\n", transmit_len);

  tx_status = TX_SENDING;

  if(send_on_cca) {
    status = RAIL_StartCcaCsmaTx(sRailHandle,
                                 channel,
                                 txOptions,
                                 &csmaConfig,
                                 NULL);
  } else {
    status = RAIL_StartTx(sRailHandle,
                          channel,
                          txOptions,
                          NULL);
  }

  if(status != RAIL_STATUS_NO_ERROR) {
    LOG_ERR("RAIL_Start***Tx status: %d failed\n", status);
    tx_status = TX_IDLE;
    return RADIO_TX_ERR;
  }

  if(poll_mode) {
    /* Wait until TX operation finishes or timeout */
    const uint16_t frame_length = transmit_len + RADIO_PHY_HEADER_LEN + RADIO_PHY_OVERHEAD;
    RTIMER_BUSYWAIT_UNTIL((tx_status != TX_SENDING),
                          US_TO_RTIMERTICKS(RADIO_BYTE_AIR_TIME * frame_length + 300));
  } else {
    /* Wait for the transmission. */
    while(tx_status == TX_SENDING);
  }

  if(tx_status == TX_SENT) {
    LOG_INFO("EFR32: OK - packet sent.\n");
  } else {
    LOG_INFO("EFR32: TX error %d\n", tx_status);
  }

  /* always IDLE after transmission */
  tx_status = TX_IDLE;

  return ret;
}
/*---------------------------------------------------------------------------*/
static int
send(const void *payload, unsigned short payload_len)
{
  int ret;
  ret = prepare(payload, payload_len);
  if(ret) {
    return ret;
  }
  return transmit(payload_len);
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet(void)
{
  return is_receiving;
}
/*---------------------------------------------------------------------------*/
static int
read(void *buf, unsigned short bufsize)
{
  rx_buffer_t *rx_buf;
  int len;

  handle_receive();

  rx_buf = get_full_rx_buf();
  if(!rx_buf) {
    return 0;
  }
  LOG_INFO("EFR32 Radio Read: %d\n", rx_buf->len);
  len = rx_buf->len;
  /* Convert from rail time domain to rtimer domain */
  last_rx_time = RTIMER_NOW() - US_TO_RTIMERTICKS(RAIL_GetTime() - rx_buf->timestamp);
  memcpy(buf, rx_buf->buf, len);
  last_rssi = rx_buf->rssi;
  last_lqi = rx_buf->lqi;
  if(!poll_mode) {
    packetbuf_set_attr(PACKETBUF_ATTR_RSSI, rx_buf->rssi);
    packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, rx_buf->lqi);
  }
  free_rx_buf(rx_buf);
  return len;
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
  handle_receive();

  return has_packet();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(efr32_radio_process, ev, data)
{
  int len;

  PROCESS_BEGIN();

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    if(pending_packet()) {
      packetbuf_clear();
      len = read(packetbuf_dataptr(), PACKETBUF_SIZE);

      if(len > 0) {
        packetbuf_set_datalen(len);
        NETSTACK_MAC.input();
        /* poll again to check if there is more to read out */
        process_poll(&efr32_radio_process);
      }
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
const struct radio_driver efr32_radio_driver_IEEE802154_2p4GHz = {
  init,
  prepare,
  transmit,
  send,
  read,
  channel_clear,
  receiving_packet,
  pending_packet,
  on,
  off,
  get_value,
  set_value,
  get_object,
  set_object
};
/*---------------------------------------------------------------------------*/
