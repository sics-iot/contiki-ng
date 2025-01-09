#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "packetbuf.h"
#include "sys/log.h"
#include <string.h>
#include <stdio.h>

#define LOG_MODULE "TEST-MODE"

#define SEND_INTERVAL CLOCK_SECOND

PROCESS(radio_test_mode, "CC2650 Radio Test Mode");
AUTOSTART_PROCESSES(&radio_test_mode);

PROCESS_THREAD(radio_test_mode, ev, data)
{
  static struct etimer periodic_timer;

  PROCESS_BEGIN();

  static int configuration[3];
  configuration[0] = 25; // Channel
  configuration[1] = 0;  // Power [0(5 dBm) - 12(-21 dBm)]
  configuration[2] = 0;  // Modulated carrier or not

  etimer_set(&periodic_timer, SEND_INTERVAL);
  static bool carrier_on = true;
  static int power_level = 0;

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    radio_result_t response;
    if (carrier_on) {
      configuration[1] = power_level;
      response = NETSTACK_CONF_RADIO.set_object(RADIO_POWER_MODE_CARRIER_ON, configuration, 3);
      if (response == RADIO_RESULT_OK) {
        leds_on(LEDS_GREEN);
        leds_off(LEDS_RED);
      } else if (response == RADIO_RESULT_INVALID_VALUE || response == RADIO_RESULT_ERROR) {
        leds_on(LEDS_RED);
        leds_off(LEDS_GREEN);
      }
      power_level++;
      if (power_level > 12) power_level = 0;
    } else {
      response = NETSTACK_CONF_RADIO.set_object(RADIO_POWER_MODE_CARRIER_OFF, configuration, 3);
      if (response == RADIO_RESULT_OK) {
        leds_off(LEDS_RED);
      } else if (response == RADIO_RESULT_INVALID_VALUE || response == RADIO_RESULT_ERROR) {
        leds_on(LEDS_RED);
      }
      leds_off(LEDS_GREEN);
    }
    carrier_on = !carrier_on;
    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}

