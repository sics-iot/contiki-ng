/*
 * Copyright (c) 2020, Toshiba BRIL
 * Copyright (C) 2020 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup nrf
 * @{
 *
 * \addtogroup nrf-net Net drivers
 * @{
 *
 * \addtogroup nrf-ieee-radio IEEE radio driver
 * @{
 *
 * \file
 *      Implementation of the nRF IEEE NETSTACK_RADIO driver
 *
 * \author
 *      Toshiba BRIL
 *      Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "dev/radio.h"
#include "nrfx_clock.h"
#include "rtimer-arch.h"
#include "sys/energest.h"
#include "sys/int-master.h"
#include "sys/critical.h"
#include "net/netstack.h"
#include "net/packetbuf.h"
#include "net/mac/tsch/tsch.h"
#include "helpers/nrfx_gppi.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "lpm.h"

#include "nrf.h"

/* Only compile the below if NRF_RADIO exists */
#ifdef NRF_RADIO

#include "hal/nrf_radio.h"
#include "hal/nrf_timer.h"
#include "hal/nrf_clock.h"
#include "hal/nrf_temp.h"
#include "nrf_erratas.h"

/*---------------------------------------------------------------------------*/
#include "sys/log.h"

#define LOG_MODULE "nRF IEEE"
#define LOG_LEVEL LOG_LEVEL_NONE
/*---------------------------------------------------------------------------*/
#define NRF_CCA_BUSY      0
#define NRF_CCA_CLEAR     1
/*---------------------------------------------------------------------------*/
#define NRF_RECEIVING_NO  0
#define NRF_RECEIVING_YES 1
/*---------------------------------------------------------------------------*/
#define NRF_PENDING_NO    0
#define NRF_PENDING_YES   1
/*---------------------------------------------------------------------------*/
#define NRF_COMMAND_ERR   0
#define NRF_COMMAND_OK    1
/*---------------------------------------------------------------------------*/
#define NRF_CHANNEL_MIN  11
#define NRF_CHANNEL_MAX  26
/*---------------------------------------------------------------------------*/
#define ED_RSSISCALE           4
/*---------------------------------------------------------------------------*/
#define FCS_LEN                2
#define MPDU_LEN             127
/*
 * The maximum number of bytes this driver can accept from the MAC layer for
 * transmission or will deliver to the MAC layer after reception. Includes
 * the MAC header and payload, but not the FCS.
 */
#define MAX_PAYLOAD_LEN      (MPDU_LEN - FCS_LEN)

#define ACK_MPDU_MIN_LEN      5
#define ACK_PAYLOAD_MIN_LEN  (ACK_MPDU_MIN_LEN - FCS_LEN)
/*---------------------------------------------------------------------------*/
/*
 * The last frame's RSSI and LQI
 *
 * Unlike other radios that write RSSI and LQI in the FCS, the nrf
 * only writes one value. This is a "hardware-reported" value, which needs
 * converted to the .15.4 standard LQI scale using an 8-bit saturating
 * multiplication by 4 (see the Product Spec). This value is based on the
 * median of three RSSI samples taken during frame reception.
 */
static int8_t last_rssi;
static uint8_t last_lqi;
/*---------------------------------------------------------------------------*/
PROCESS(nrf_ieee_rf_process, "nRF IEEE RF driver");
/*---------------------------------------------------------------------------*/
#ifndef NRF_CCA_MODE
#define NRF_CCA_MODE RADIO_CCACTRL_CCAMODE_CarrierAndEdMode
#endif

#ifndef NRF_CCA_ED_THRESHOLD
#define NRF_CCA_ED_THRESHOLD 0x14
#endif

#ifndef NRF_CCA_CORR_THRESHOLD
#define NRF_CCA_CORR_THRESHOLD 0x14
#endif

#ifndef NRF_CCA_CORR_COUNT
#define NRF_CCA_CORR_COUNT 0x02
#endif
/*---------------------------------------------------------------------------*/
/*
 * .15.4-compliant CRC:
 *
 * Lenght 2, Initial value 0.
 *
 * Polynomial x^16 + x^12 + x^5 + 1
 * CRCPOLY: 1 00010000 00100001
 */
#define CRC_IEEE802154_LEN             2
#define CRC_IEEE802154_POLY      0x11021
#define CRC_IEEE802154_INIT            0
/*---------------------------------------------------------------------------*/
#define SYMBOL_DURATION_USEC          16
#define SYMBOL_DURATION_RTIMER         1
#define BYTE_DURATION_RTIMER          (SYMBOL_DURATION_RTIMER * 2)
#define TXRU_DURATION_TIMER            3
/*---------------------------------------------------------------------------*/
#define NRF_PPI_FRAMESTART_CHANNEL     1
#define NRF_PPI_END_CHANNEL            2
/*---------------------------------------------------------------------------*/
typedef struct timestamps_s {
  rtimer_clock_t sfd;           /* Derived: 1 byte = 2 rtimer ticks before FRAMESTART */
  rtimer_clock_t framestart;    /* PPI Channel 1 */
  rtimer_clock_t end;           /* PPI Channel 2 */
  rtimer_clock_t mpdu_duration; /* Calculated: PHR * 2 rtimer ticks */
  uint8_t phr;                  /* PHR: The MPDU length in bytes */
} timestamps_t;

static timestamps_t timestamps;
/*---------------------------------------------------------------------------*/
typedef struct tx_buf_s {
  uint8_t phr;
  uint8_t mpdu[MAX_PAYLOAD_LEN];
} tx_buf_t;

static tx_buf_t tx_buf;
/*---------------------------------------------------------------------------*/
typedef struct rx_buf_s {
  uint8_t phr;
  uint8_t mpdu[MPDU_LEN];
  bool full; /* Used in interrupt / non-poll mode for additional state */
} rx_buf_t;

static rx_buf_t rx_buf;
/*---------------------------------------------------------------------------*/
typedef struct rf_cfg_s {
  bool poll_mode;
  nrf_radio_txpower_t txpower;
  uint8_t channel;
  uint8_t send_on_cca; /* Perform CCA before TX */
  uint8_t cca_mode;
  uint8_t cca_corr_threshold;
  uint8_t cca_corr_count;
  uint8_t ed_threshold;
} rf_cfg_t;

static rf_cfg_t rf_config = {
  .poll_mode = false,
  .txpower = NRF_RADIO_TXPOWER_0DBM,
  .send_on_cca = RADIO_TX_MODE_SEND_ON_CCA,
  .channel = IEEE802154_DEFAULT_CHANNEL,
  .cca_mode = NRF_CCA_MODE,
  .cca_corr_threshold = NRF_CCA_CORR_THRESHOLD,
  .cca_corr_count = NRF_CCA_CORR_COUNT,
  .ed_threshold = NRF_CCA_ED_THRESHOLD,
};

static bool radio_power = false; /* Used as a POWER register when nrf53_errata_16 is present */

/*---------------------------------------------------------------------------*/
static bool
phr_is_valid(uint8_t phr)
{
  if(phr < ACK_MPDU_MIN_LEN || phr > MPDU_LEN) {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------*/
static bool
radio_is_powered(void)
{
  if(nrf53_errata_16()) {
    return radio_power;
  } else {
    return NRF_RADIO->POWER == 0 ? false : true;
  }
}
/*---------------------------------------------------------------------------*/
static void
radio_set_power(bool power)
{
  if(nrf53_errata_16()) {
    radio_power = power;
  } else {
    nrf_radio_power_set(NRF_RADIO, power);
  }
}
/*---------------------------------------------------------------------------*/
static uint8_t
radio_rssi_sample_get(void)
{
  uint8_t rssi_sample;
  uint8_t temp;

  rssi_sample = nrf_radio_rssi_sample_get(NRF_RADIO);

  if(nrf53_errata_87()) {
    temp = nrf_temp_result_get(NRF_TEMP);
    return (uint8_t)round(
      (float)((float)(1.56f * rssi_sample) + (float)(4.9e-5 * pow(rssi_sample, 3)) -
              (float)(9.9e-3 * pow(rssi_sample, 2)) - (0.05f * ((float)(temp) * 0.25f)) - 7.2f));
  } else {
    return rssi_sample;
  }
}
/*---------------------------------------------------------------------------*/
static uint8_t
get_channel(void)
{
  return NRF_RADIO->FREQUENCY / 5 + 10;
}
/*---------------------------------------------------------------------------*/
static void
set_channel(uint8_t channel)
{
  NRF_RADIO->FREQUENCY = 5 * (channel - 10);
}
/*---------------------------------------------------------------------------*/
static void
cca_reconfigure(void)
{
  nrf_radio_cca_configure(NRF_RADIO, rf_config.cca_mode, rf_config.ed_threshold,
                          rf_config.cca_corr_threshold, rf_config.cca_corr_count);
}
/*---------------------------------------------------------------------------*/
static void
crc_init(void)
{
  /*
   * Initialise the CRC engine in .15.4 mode:
   * - Length: 2 bytes
   * - Polynomial:
   * - Initial value: 0
   */
  nrf_radio_crc_configure(NRF_RADIO, CRC_IEEE802154_LEN, NRF_RADIO_CRC_ADDR_IEEE802154,
                          CRC_IEEE802154_POLY);

  nrf_radio_crcinit_set(NRF_RADIO, CRC_IEEE802154_INIT);
}
/*---------------------------------------------------------------------------*/
static void
packet_init(void)
{
  /* Configure packet format for .15.4 */
  nrf_radio_packet_conf_t conf;

  memset(&conf, 0, sizeof(conf));

  conf.lflen = 8; /* Length field, in bits */
  conf.s1incl = false;
  conf.plen = NRF_RADIO_PREAMBLE_LENGTH_32BIT_ZERO;
  conf.crcinc = true;
  conf.big_endian = false;
  conf.whiteen = false;
  conf.maxlen = MPDU_LEN;

  nrf_radio_packet_configure(NRF_RADIO, &conf);
}
/*---------------------------------------------------------------------------*/
static void
setup_interrupts(void)
{
  int_master_status_t stat;
  nrf_radio_int_mask_t interrupts = 0;

  stat = critical_enter();

  if(!rf_config.poll_mode) {
    nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CRCOK);
    nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CRCERROR);
    interrupts |= NRF_RADIO_INT_CRCOK_MASK | NRF_RADIO_INT_CRCERROR_MASK;
  }

  /* Make sure all interrupts are disabled before we enable selectively */
  nrf_radio_int_disable(NRF_RADIO, 0xFFFFFFFF);
  NVIC_ClearPendingIRQ(RADIO_IRQn);

  if(interrupts) {
    nrf_radio_int_enable(NRF_RADIO, interrupts);
    NVIC_EnableIRQ(RADIO_IRQn);
  } else {
    /* No radio interrupts required. Make sure they are all off at the NVIC */
    NVIC_DisableIRQ(RADIO_IRQn);
  }

  critical_exit(stat);
}
/*---------------------------------------------------------------------------*/
/*
 * Set up timestamping with PPI:
 * - Programme Channel 1 for RADIO->FRAMESTART--->NRF_RTIMER_TIMER->CAPTURE[3]
 * - Programme Channel 2 for RADIO->END--->NRF_RTIMER_TIMER->CAPTURE[2]
 */
static void
setup_ppi_timestamping(void)
{
  nrfx_gppi_channel_endpoints_setup(
    NRF_PPI_FRAMESTART_CHANNEL,
    nrf_radio_event_address_get(NRF_RADIO, NRF_RADIO_EVENT_FRAMESTART),
    nrf_timer_task_address_get(NRF_RTIMER_TIMER, NRF_TIMER_TASK_CAPTURE3));
  nrfx_gppi_channel_endpoints_setup(
    NRF_PPI_END_CHANNEL,
    nrf_radio_event_address_get(NRF_RADIO, NRF_RADIO_EVENT_END),
    nrf_timer_task_address_get(NRF_RTIMER_TIMER, NRF_TIMER_TASK_CAPTURE2));
  nrfx_gppi_channels_enable(1uL << NRF_PPI_FRAMESTART_CHANNEL
                            | 1uL << NRF_PPI_END_CHANNEL);
}
/*---------------------------------------------------------------------------*/
static void
set_poll_mode(bool enable)
{
  rf_config.poll_mode = enable;
  setup_interrupts();
}
/*---------------------------------------------------------------------------*/
static void
rx_buf_clear(void)
{
  memset(&rx_buf, 0, sizeof(rx_buf));
}
/*---------------------------------------------------------------------------*/
static void
rx_events_clear()
{
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_FRAMESTART);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_END);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_PHYEND);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CRCERROR);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CRCOK);
}
/*---------------------------------------------------------------------------*/
/*
 * Powering off the peripheral will reset all registers to default values
 * This function here must be called at every power on to set the radio in a
 * known state
 */
static void
configure(void)
{
  nrf_radio_mode_set(NRF_RADIO, NRF_RADIO_MODE_IEEE802154_250KBIT);

  if(nrf53_errata_117()) {
    *((volatile uint32_t *)0x41008588) = *((volatile uint32_t *)0x01FF0084);
  }

  set_channel(rf_config.channel);

  cca_reconfigure();

  /* Initialise the CRC engine in .15.4 mode */
  crc_init();

  /* Initialise the packet format */
  packet_init();

  /*
   * MODECNF: Fast ramp up, DTX=center
   * The Nordic driver is using DTX=0, but this is against the PS (v1.1 p351)
   */
  nrf_radio_modecnf0_set(NRF_RADIO, true, RADIO_MODECNF0_DTX_Center);
}
/*---------------------------------------------------------------------------*/
static void
power_on_and_configure(void)
{
  radio_set_power(true);
  configure();
}
/*---------------------------------------------------------------------------*/
/*
 * The caller must first make sure the radio is powered and configured.
 *
 * When we enter this function we can be in one of the following states:
 * -       STATE_RX: We were already in RX. Do nothing
 * -   STATE_RXIDLE: A reception just finished and we reverted to RXIDLE.
 *                   We just need to send the START task.
 * -   STATE_TXIDLE: A TX just finished and we reverted to TXIDLE.
 *                   We just need to send the START task.
 * - STATE_DISABLED: We just turned on. We need to request radio rampup
 */
static void
enter_rx(void)
{
  nrf_radio_state_t curr_state = nrf_radio_state_get(NRF_RADIO);

  LOG_DBG("Enter RX, state=%u", curr_state);

  /* Do nothing if we are already in RX */
  if(curr_state == NRF_RADIO_STATE_RX) {
    LOG_DBG_(". Was in RX");
    LOG_DBG_("\n");
    return;
  }

  /* Prepare the RX buffer */
  nrf_radio_packetptr_set(NRF_RADIO, &rx_buf);

  /* Initiate PPI timestamping */
  setup_ppi_timestamping();

  /* Make sure the correct interrupts are enabled */
  setup_interrupts();

  nrf_radio_shorts_enable(NRF_RADIO, NRF_RADIO_SHORT_ADDRESS_RSSISTART_MASK);
  nrf_radio_shorts_enable(NRF_RADIO, NRF_RADIO_SHORT_RXREADY_START_MASK);

  if(curr_state != NRF_RADIO_STATE_RXIDLE) {
    /* Clear EVENTS_RXREADY and trigger RXEN (which will trigger START) */
    nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_RXREADY);
    nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_RXEN);
  } else {
    /* Trigger the Start task */
    nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_START);
  }

  LOG_DBG_("--->%u\n", nrf_radio_state_get(NRF_RADIO));

  LOG_DBG("PACKETPTR=0x%08" PRIx32 " (rx_buf @ 0x%08" PRIx32 ")\n",
          (uint32_t)nrf_radio_packetptr_get(NRF_RADIO), (uint32_t)&rx_buf);
}
/*---------------------------------------------------------------------------*/
/* Retrieve an RSSI sample. The radio must be in RX mode */
static int8_t
rssi_read(void)
{
  uint8_t rssi_sample;

  nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_RSSISTART);

  while(nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_RSSIEND) == false);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_RSSIEND);

  rssi_sample = radio_rssi_sample_get();

  return -((int8_t)rssi_sample);
}
/*---------------------------------------------------------------------------*/
/*
 * Convert the hardware-reported LQI to 802.15.4 range using an 8-bit
 * saturating multiplication by 4, as per the Product Spec.
 */
static uint8_t
lqi_convert_to_802154_scale(uint8_t lqi_hw)
{
  return (uint8_t)lqi_hw > 63 ? 255 : lqi_hw * ED_RSSISCALE;
}
/*---------------------------------------------------------------------------*/
/* Netstack API functions */
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  LOG_DBG("On\n");

  if(radio_is_powered() == false) {
    LOG_DBG("Not powered\n");
    power_on_and_configure();
  }

  enter_rx();

  ENERGEST_ON(ENERGEST_TYPE_LISTEN);
  return NRF_COMMAND_OK;
}
/*---------------------------------------------------------------------------*/
static int
channel_clear(void)
{
  bool busy, idle;

  LOG_DBG("channel_clear\n");

  on();

  /* Clear previous CCA-related events, if any */
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CCABUSY);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CCAIDLE);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CCASTOPPED);

  LOG_DBG("channel_clear: CCACTRL=0x%08" PRIx32 "\n", NRF_RADIO->CCACTRL);

  /* We are now in RX. Send CCASTART */
  nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_CCASTART);

  while((nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CCABUSY) == false) &&
        (nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CCAIDLE) == false));

  busy = nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CCABUSY);
  idle = nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CCAIDLE);

  LOG_DBG("channel_clear: I=%u, B=%u\n", idle, busy);

  if(busy) {
    return NRF_CCA_BUSY;
  }

  return NRF_CCA_CLEAR;
}
/*---------------------------------------------------------------------------*/
static uint8_t
request(void)
{
  /*
   * prevent lpm from going into deep sleep if the radio is powered on
   */
  if(radio_is_powered()) {
    return LPM_MODE_SLEEP;
  }

  return LPM_MODE_MAX_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
LPM_MODULE(nrf_rf_lpm_module, request, NULL, NULL);
/*---------------------------------------------------------------------------*/
static int
init(void)
{
  LOG_DBG("Init\n");

  lpm_register_module(&nrf_rf_lpm_module);

  last_rssi = 0;
  last_lqi = 0;

  timestamps.sfd = 0;
  timestamps.framestart = 0;
  timestamps.end = 0;
  timestamps.mpdu_duration = 0;
  timestamps.phr = 0;

  /* Request the HF clock */
  nrfx_clock_hfclk_start();

  /* Enable Timers which will be needed in PPI */
  nrf_timer_event_clear(NRF_RTIMER_TIMER, NRF_TIMER_EVENT_COMPARE0);
  nrf_timer_frequency_set(NRF_RTIMER_TIMER, NRF_TIMER_FREQ_62500Hz);
  nrf_timer_bit_width_set(NRF_RTIMER_TIMER, NRF_TIMER_BIT_WIDTH_32);
  nrf_timer_mode_set(NRF_RTIMER_TIMER, NRF_TIMER_MODE_TIMER);
  nrf_timer_int_enable(NRF_RTIMER_TIMER, NRF_TIMER_INT_COMPARE0_MASK);
  nrf_timer_task_trigger(NRF_RTIMER_TIMER, NRF_TIMER_TASK_START);

  /* Start the RF driver process */
  process_start(&nrf_ieee_rf_process, NULL);

  /* Prepare the RX buffer */
  rx_buf_clear();

  /* Power on the radio */
  power_on_and_configure();

  /* Set up initial state of poll mode. This will configure interrupts. */
  set_poll_mode(rf_config.poll_mode);

  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
prepare(const void *payload, unsigned short payload_len)
{
  LOG_DBG("Prepare %u bytes\n", payload_len);

  if(payload_len > MAX_PAYLOAD_LEN) {
    LOG_ERR("Too long: %u bytes, max %u\n", payload_len, MAX_PAYLOAD_LEN);
    return RADIO_TX_ERR;
  }

  /* Populate the PHR. Packet length, including the FCS */
  tx_buf.phr = (uint8_t)payload_len + FCS_LEN;

  /* Copy the payload over */
  memcpy(tx_buf.mpdu, payload, payload_len);

  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
transmit(unsigned short transmit_len)
{
  int i;

  LOG_DBG("TX %u bytes + FCS, channel=%u\n", transmit_len, get_channel());

  if(transmit_len > MAX_PAYLOAD_LEN) {
    LOG_ERR("TX: too long (%u bytes)\n", transmit_len);
    return RADIO_TX_ERR;
  }

  on();

  if(rf_config.send_on_cca) {
    if(channel_clear() == NRF_CCA_BUSY) {
      LOG_DBG("TX: Busy\n");
      return RADIO_TX_COLLISION;
    }
  }

  nrf_radio_txpower_set(NRF_RADIO, rf_config.txpower);

  /* When we reach here we are in state RX. Send a STOP to drop to RXIDLE */
  nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_STOP);
  while(nrf_radio_state_get(NRF_RADIO) != NRF_RADIO_STATE_RXIDLE);

  LOG_DBG("Transmit: %u bytes=000000", tx_buf.phr);
  for(i = 0; i < tx_buf.phr - 2; i++) {
    LOG_DBG_(" %02x", tx_buf.mpdu[i]);
  }
  LOG_DBG_("\n");

  LOG_DBG("TX Start. State %u", nrf_radio_state_get(NRF_RADIO));

  /* Pointer to the TX buffer in PACKETPTR before task START */
  nrf_radio_packetptr_set(NRF_RADIO, &tx_buf);

  /* Clear TX-related events */
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_END);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_PHYEND);
  nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_TXREADY);

  /* Start the transmission */
  ENERGEST_SWITCH(ENERGEST_TYPE_LISTEN, ENERGEST_TYPE_TRANSMIT);

  /* Enable the SHORT between TXREADY and START before triggering TXRU */
  nrf_radio_shorts_enable(NRF_RADIO, NRF_RADIO_SHORT_TXREADY_START_MASK);
  nrf_radio_task_trigger(NRF_RADIO, NRF_RADIO_TASK_TXEN);

  /*
   * With fast rampup, the transition between TX and READY (TXRU duration)
   * takes 40us. This means we will be in TX mode in less than 3 rtimer ticks
   * (3x16=42 us). After this duration, we can busy wait for TX to finish.
   */
  RTIMER_BUSYWAIT(TXRU_DURATION_TIMER);

  /*
   * The workaround for errata 91 is to wait 10us more during ramp-up
   * (1x16 = 16us)
   */
  if(nrf53_errata_91()) {
    RTIMER_BUSYWAIT(1);
  }

  LOG_DBG_("--->%u\n", nrf_radio_state_get(NRF_RADIO));

  /* Wait for TX to complete */
  while(nrf_radio_state_get(NRF_RADIO) == NRF_RADIO_STATE_TX);

  LOG_DBG("TX: Done\n");

  /*
   * Enter RX.
   * TX has finished and we are in state TXIDLE. enter_rx will handle the
   * transition from any state to RX, so we don't need to do anything further
   * here.
   */
  enter_rx();

  /* We are now in RX */
  ENERGEST_SWITCH(ENERGEST_TYPE_TRANSMIT, ENERGEST_TYPE_LISTEN);

  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
send(const void *payload, unsigned short payload_len)
{
  prepare(payload, payload_len);
  return transmit(payload_len);
}
/*---------------------------------------------------------------------------*/
static int
read_frame(void *buf, unsigned short bufsize)
{
  int payload_len;

  /* Clear all events */
  rx_events_clear();

  payload_len = rx_buf.phr - FCS_LEN;

  if(phr_is_valid(rx_buf.phr) == false || payload_len > bufsize) {
    LOG_DBG("Incorrect length: %d (bufsize: %hu)\n", payload_len, bufsize);
    rx_buf_clear();
    enter_rx();
    return 0;
  }

  memcpy(buf, rx_buf.mpdu, payload_len);
  last_lqi = lqi_convert_to_802154_scale(rx_buf.mpdu[payload_len]);
  last_rssi = -(radio_rssi_sample_get());

  packetbuf_set_attr(PACKETBUF_ATTR_RSSI, last_rssi);
  packetbuf_set_attr(PACKETBUF_ATTR_LINK_QUALITY, last_lqi);

  /* Latch timestamp values for this most recently received frame */
  timestamps.phr = rx_buf.phr;
  timestamps.framestart = nrf_timer_cc_get(NRF_RTIMER_TIMER, NRF_TIMER_CC_CHANNEL3);
  timestamps.end = nrf_timer_cc_get(NRF_RTIMER_TIMER, NRF_TIMER_CC_CHANNEL2);
  timestamps.mpdu_duration = rx_buf.phr * BYTE_DURATION_RTIMER;


  /* Use NRF_RTIMER_TIMER CC0 for conversion to rtimer(based on RTC) */
  /* timestamps.sfd will be used in TSCH Module for slot calculation */
  NRF_RTIMER_TIMER->TASKS_CAPTURE[0] = 1;
  uint64_t now = RTIMER_NOW();
  timestamps.framestart = now - ((NRF_RTIMER_TIMER->CC[0] - timestamps.framestart) * RTIMER_SECOND / 62500 );
  timestamps.end = now - ((NRF_RTIMER_TIMER->CC[0] - timestamps.end) * RTIMER_SECOND / 62500);

  /*
   * Timestamp in rtimer ticks of the reception of the SFD. The SFD was
   * received 1 byte before the PHR, therefore all we need to do is subtract
   * 2 symbols (2 rtimer ticks) from the PPI FRAMESTART timestamp.
   */
  timestamps.sfd = timestamps.framestart - BYTE_DURATION_RTIMER;

  LOG_DBG("Read frame: len=%d, RSSI=%d, LQI=0x%02x\n", payload_len, last_rssi,
          last_lqi);

  rx_buf_clear();
  enter_rx();

  return payload_len;
}
/*---------------------------------------------------------------------------*/
static int
receiving_packet(void)
{
  /* If we are powered off, we are not receiving */
  if(radio_is_powered() == false) {
    return NRF_RECEIVING_NO;
  }

  /* If our state is not RX, we are not receiving */
  if(nrf_radio_state_get(NRF_RADIO) != NRF_RADIO_STATE_RX) {
    return NRF_RECEIVING_NO;
  }

  if(rf_config.poll_mode) {
    /* In poll mode, if the PHR is invalid we can return early */
    if(phr_is_valid(rx_buf.phr) == false) {
      return NRF_RECEIVING_NO;
    }

    /*
     * If the PHR is valid and we are actually on, inspect EVENTS_CRCOK and
     * _CRCERROR. If both of them are clear then reception is ongoing
     */
    if((nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CRCOK) == false) &&
       (nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CRCERROR) == false)) {
      return NRF_RECEIVING_YES;
    }

    return NRF_RECEIVING_NO;
  }

  /*
   * In non-poll mode, we are receiving if the PHR is valid but the buffer
   * does not contain a full packet.
   */
  if(phr_is_valid(rx_buf.phr) == true && rx_buf.full == false) {
    return NRF_RECEIVING_YES;
  }
  return NRF_RECEIVING_NO;
}
/*---------------------------------------------------------------------------*/
static int
pending_packet(void)
{
  /*
   * First check if we have received a PHR. When we enter RX the value of the
   * PHR in our RX buffer is zero so we can return early.
   */
  if(phr_is_valid(rx_buf.phr) == false) {
    return NRF_PENDING_NO;
  }

  /*
   * We have received a valid PHR. Either we are in the process of receiving
   * a frame, or we have fully received one. If we have received a frame then
   * EVENTS_CRCOK should be asserted. In poll mode that's enough. In non-poll
   * mode the interrupt handler will clear the event (else the interrupt would
   * fire again), but we save the state in rx_buf.full.
   */
  if((nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CRCOK) == true) ||
     (rx_buf.full == true)) {
    return NRF_PENDING_YES;
  }

  return NRF_PENDING_NO;
}
/*---------------------------------------------------------------------------*/
static int
off(void)
{
  radio_set_power(false);

  ENERGEST_OFF(ENERGEST_TYPE_LISTEN);

  return NRF_COMMAND_OK;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_value(radio_param_t param, radio_value_t *value)
{
  if(!value) {
    return RADIO_RESULT_INVALID_VALUE;
  }

  switch(param) {
  case RADIO_PARAM_POWER_MODE:
    return RADIO_RESULT_OK;
  case RADIO_PARAM_CHANNEL:
    *value = (radio_value_t)get_channel();
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RX_MODE:
    *value = 0;
    if(rf_config.poll_mode) {
      *value |= RADIO_RX_MODE_POLL_MODE;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_TX_MODE:
    *value = 0;
    if(rf_config.send_on_cca) {
      *value |= RADIO_TX_MODE_SEND_ON_CCA;
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_TXPOWER:
    *value = (radio_value_t)rf_config.txpower;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_CCA_THRESHOLD:
    *value = (radio_value_t)rf_config.cca_corr_threshold;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RSSI:
    *value = (radio_value_t)rssi_read();
    return RADIO_RESULT_OK;
  case RADIO_PARAM_LAST_RSSI:
    *value = (radio_value_t)last_rssi;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_LAST_LINK_QUALITY:
    *value = (radio_value_t)last_lqi;
    return RADIO_RESULT_OK;
  case RADIO_CONST_CHANNEL_MIN:
    *value = 11;
    return RADIO_RESULT_OK;
  case RADIO_CONST_CHANNEL_MAX:
    *value = 26;
    return RADIO_RESULT_OK;
  case RADIO_CONST_TXPOWER_MIN:
    *value = (radio_value_t)RADIO_TXPOWER_TXPOWER_Neg40dBm;
    return RADIO_RESULT_OK;
  case RADIO_CONST_TXPOWER_MAX:
#if defined(RADIO_TXPOWER_TXPOWER_Pos8dBm)
    *value = (radio_value_t)RADIO_TXPOWER_TXPOWER_Pos8dBm;
#else
    *value = (radio_value_t)RADIO_TXPOWER_TXPOWER_0dBm;
#endif
    return RADIO_RESULT_OK;
  case RADIO_CONST_PHY_OVERHEAD:
    *value = (radio_value_t)RADIO_PHY_OVERHEAD;
    return RADIO_RESULT_OK;
  case RADIO_CONST_BYTE_AIR_TIME:
    *value = (radio_value_t)RADIO_BYTE_AIR_TIME;
    return RADIO_RESULT_OK;
  case RADIO_CONST_DELAY_BEFORE_TX:
    *value = (radio_value_t)RADIO_DELAY_BEFORE_TX;
    return RADIO_RESULT_OK;
  case RADIO_CONST_DELAY_BEFORE_RX:
    *value = (radio_value_t)RADIO_DELAY_BEFORE_RX;
    return RADIO_RESULT_OK;
  case RADIO_CONST_DELAY_BEFORE_DETECT:
    *value = (radio_value_t)RADIO_DELAY_BEFORE_DETECT;
    return RADIO_RESULT_OK;
  case RADIO_CONST_MAX_PAYLOAD_LEN:
    *value = (radio_value_t)MAX_PAYLOAD_LEN;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_PAN_ID:
  case RADIO_PARAM_16BIT_ADDR:
  default:
    return RADIO_RESULT_NOT_SUPPORTED;
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_value(radio_param_t param, radio_value_t value)
{
  switch(param) {
  case RADIO_PARAM_POWER_MODE:
    if(value == RADIO_POWER_MODE_ON) {
      on();
      return RADIO_RESULT_OK;
    }
    if(value == RADIO_POWER_MODE_OFF) {
      off();
      return RADIO_RESULT_OK;
    }
    return RADIO_RESULT_INVALID_VALUE;
  case RADIO_PARAM_CHANNEL:
    if(value < NRF_CHANNEL_MIN ||
       value > NRF_CHANNEL_MAX) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    rf_config.channel = value;

    /* If we are powered on, apply immediately. */
    if(radio_is_powered()) {
      off();
      set_channel(value);
      on();
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_RX_MODE:
    if(value & ~(RADIO_RX_MODE_ADDRESS_FILTER |
                 RADIO_RX_MODE_AUTOACK |
                 RADIO_RX_MODE_POLL_MODE)) {
      return RADIO_RESULT_INVALID_VALUE;
    }

    set_poll_mode((value & RADIO_RX_MODE_POLL_MODE) != 0);

    return RADIO_RESULT_OK;
  case RADIO_PARAM_TX_MODE:
    if(value & ~(RADIO_TX_MODE_SEND_ON_CCA)) {
      return RADIO_RESULT_INVALID_VALUE;
    }

    rf_config.send_on_cca = (value & RADIO_TX_MODE_SEND_ON_CCA) != 0;
    return RADIO_RESULT_OK;
  case RADIO_PARAM_TXPOWER:
    rf_config.txpower = value;
    /* If we are powered on, apply immediately. */
    if(radio_is_powered()) {
      nrf_radio_txpower_set(NRF_RADIO, value);
    }
    return RADIO_RESULT_OK;
  case RADIO_PARAM_CCA_THRESHOLD:
    rf_config.cca_corr_threshold = value;
    /* If we are powered on, apply immediately. */
    if(radio_is_powered()) {
      cca_reconfigure();
    }
    return RADIO_RESULT_OK;

  case RADIO_PARAM_SHR_SEARCH:
  case RADIO_PARAM_PAN_ID:
  case RADIO_PARAM_16BIT_ADDR:
  default:
    return RADIO_RESULT_NOT_SUPPORTED;
  }
}
/*---------------------------------------------------------------------------*/
static radio_result_t
get_object(radio_param_t param, void *dest, size_t size)
{
  if(param == RADIO_PARAM_LAST_PACKET_TIMESTAMP) {
    if(size != sizeof(rtimer_clock_t) || !dest) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    *(rtimer_clock_t *)dest = timestamps.sfd;
    return RADIO_RESULT_OK;
  }

#if MAC_CONF_WITH_TSCH
  if(param == RADIO_CONST_TSCH_TIMING) {
    if(size != sizeof(uint16_t *) || !dest) {
      return RADIO_RESULT_INVALID_VALUE;
    }
    /* Assigned value: a pointer to the TSCH timing in usec */
    *(const uint16_t **)dest = tsch_timeslot_timing_us_10000;
    return RADIO_RESULT_OK;
  }
#endif /* MAC_CONF_WITH_TSCH */

  /* The radio does not support h/w frame filtering based on addresses */
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
static radio_result_t
set_object(radio_param_t param, const void *src, size_t size)
{
  /* The radio does not support h/w frame filtering based on addresses */
  return RADIO_RESULT_NOT_SUPPORTED;
}
/*---------------------------------------------------------------------------*/
const struct radio_driver nrf_ieee_driver = {
  init,
  prepare,
  transmit,
  send,
  read_frame,
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
PROCESS_THREAD(nrf_ieee_rf_process, ev, data)
{
  int len;
  PROCESS_BEGIN();

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    LOG_DBG("Polled\n");

    if(pending_packet()) {
      watchdog_periodic();
      packetbuf_clear();
      len = read_frame(packetbuf_dataptr(), PACKETBUF_SIZE);
      if(len > 0) {
        packetbuf_set_datalen(len);
        NETSTACK_MAC.input();
        LOG_DBG("last frame (%u bytes) timestamps:\n", timestamps.phr);
        LOG_DBG("      SFD=%" PRIu32 " (Derived)\n", timestamps.sfd);
        LOG_DBG("      PHY=%" PRIu32 " (PPI)\n", timestamps.framestart);
        LOG_DBG("     MPDU=%" PRIu32 " (Duration)\n", timestamps.mpdu_duration);
        LOG_DBG("      END=%" PRIu32 " (PPI)\n", timestamps.end);
        LOG_DBG(" Expected=%" PRIu32 " + %u + %" PRIu32 " = %" PRIu32 "\n",
                timestamps.sfd, BYTE_DURATION_RTIMER, timestamps.mpdu_duration,
                timestamps.sfd + BYTE_DURATION_RTIMER + timestamps.mpdu_duration);
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void
RADIO_IRQHandler(void)
{
  if(!rf_config.poll_mode) {
    if(nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CRCOK)) {
      nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CRCOK);
      rx_buf.full = true;
      process_poll(&nrf_ieee_rf_process);
    } else if(nrf_radio_event_check(NRF_RADIO, NRF_RADIO_EVENT_CRCERROR)) {
      nrf_radio_event_clear(NRF_RADIO, NRF_RADIO_EVENT_CRCERROR);
      rx_buf_clear();
      enter_rx();
    }
  }
}
/*---------------------------------------------------------------------------*/

#endif /* NRF_RADIO */

/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
