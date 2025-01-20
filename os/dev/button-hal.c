/*
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * \addtogroup button_hal
 * @{
 *
 * \file
 * Platform-independent button driver.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "sys/process.h"
#include "sys/ctimer.h"
#include "sys/critical.h"
#include "dev/gpio-hal.h"
#include "dev/button-hal.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
PROCESS(button_hal_process, "Button HAL process");
/*---------------------------------------------------------------------------*/
process_event_t button_hal_press_event;
process_event_t button_hal_release_event;
process_event_t button_hal_periodic_event;
/*---------------------------------------------------------------------------*/
/* A mask of all ports that have changed state since the last process poll */
#if GPIO_HAL_PORT_PIN_NUMBERING
static volatile uint8_t port_mask;
#endif

/* A mask of all pins that have changed state since the last process poll */
static volatile gpio_hal_pin_mask_t pmask;
/*---------------------------------------------------------------------------*/
#define PORT_UNUSED 0xFF
extern button_hal_button_t *button_hal_buttons[];
/*---------------------------------------------------------------------------*/
/* Button event handlers, one per port. Registered with the GPIO HAL */
static gpio_hal_event_handler_t button_event_handlers[BUTTON_HAL_PORT_COUNT];
/*---------------------------------------------------------------------------*/
#if GPIO_HAL_PORT_PIN_NUMBERING
#define BTN_PORT(b) (b)->port
#else
#define BTN_PORT(b) GPIO_HAL_NULL_PORT
#endif
/*---------------------------------------------------------------------------*/
static void
duration_exceeded_callback(void *btn)
{
  button_hal_button_t *button = (button_hal_button_t *)btn;

  button->press_duration_events++;
  ctimer_set(&button->duration_ctimer, BUTTON_HAL_PERIODIC_INTERVAL,
             duration_exceeded_callback, button);
  process_post(PROCESS_BROADCAST, button_hal_periodic_event, button);
}
/*---------------------------------------------------------------------------*/
static void
debounce_handler(void *btn)
{
  button_hal_button_t *button;
  int expired;
  uint8_t button_state;

  button = (button_hal_button_t *)btn;

  /*
   * A new debounce may have been triggered after expiration of the previous
   * one but before we got called.
   */
  if(!ctimer_expired(&button->debounce_ctimer)) {
    return;
  }

  expired = ctimer_expired(&button->duration_ctimer);

  button_state = button_hal_get_state(button);

  /*
   * A debounce timer expired. Inspect the button's state. If the button's
   * state is the same as it was before, then we ignore this as noise.
   */
  if(button_state == BUTTON_HAL_STATE_PRESSED && expired) {
    /*
     * Button is pressed and no tick counter running. Treat as new press.
     * Include the debounce duration in the first periodic, so that the
     * callback will happen 1 second after the button press, not 1 second
     * after the end of the debounce. Notify process about the press event.
     */
    button->press_duration_events = 0;
    ctimer_set(&button->duration_ctimer,
               BUTTON_HAL_PERIODIC_INTERVAL - BUTTON_HAL_DEBOUNCE_DURATION,
               duration_exceeded_callback, button);
    process_post(PROCESS_BROADCAST, button_hal_press_event, button);
  } else if(button_state == BUTTON_HAL_STATE_RELEASED && expired == 0) {
    /*
     * Button is released and there is a duration_ctimer running. Treat this
     * as a new release and notify processes.
     */
    ctimer_stop(&button->duration_ctimer);
    process_post(PROCESS_BROADCAST, button_hal_release_event, button);
  }
}
/*---------------------------------------------------------------------------*/
static void
press_release_handler(
#if GPIO_HAL_PORT_PIN_NUMBERING
                      gpio_hal_port_t port,
#endif
                      gpio_hal_pin_mask_t pin_mask)
{
  pmask |= pin_mask;
#if GPIO_HAL_PORT_PIN_NUMBERING
  port_mask |= (1 << port);
#endif
  process_poll(&button_hal_process);
}
/*---------------------------------------------------------------------------*/
static gpio_hal_event_handler_t *
get_handler_by_port(uint8_t port)
{
#if GPIO_HAL_PORT_PIN_NUMBERING
  uint8_t i;

  /* First, search in case a handler has already been dedicated to this port */
  for(i = 0; i < BUTTON_HAL_PORT_COUNT; i++) {
    if(button_event_handlers[i].port == port) {
      return &button_event_handlers[i];
    }
  }

  /* If not, allocate. The caller will set the port */
  for(i = 0; i < BUTTON_HAL_PORT_COUNT; i++) {
    if(button_event_handlers[i].port == PORT_UNUSED) {
      return &button_event_handlers[i];
    }
  }

  return NULL;
#else
  return &button_event_handlers[0];
#endif
}
/*---------------------------------------------------------------------------*/
button_hal_button_t *
button_hal_get_by_id(uint8_t unique_id)
{
  button_hal_button_t **button;

  for(button = button_hal_buttons; *button != NULL; button++) {
    if((*button)->unique_id == unique_id) {
      return *button;
    }
  }

  return NULL;
}
/*---------------------------------------------------------------------------*/
button_hal_button_t *
button_hal_get_by_index(uint8_t index)
{
  if(index >= button_hal_button_count) {
    return NULL;
  }

  return button_hal_buttons[index];
}
/*---------------------------------------------------------------------------*/
uint8_t
button_hal_get_state(button_hal_button_t *button)
{
  uint8_t pin_state = gpio_hal_arch_read_pin(BTN_PORT(button), button->pin);

  if((pin_state == 0 && button->negative_logic == true) ||
     (pin_state == 1 && button->negative_logic == false)) {
    return BUTTON_HAL_STATE_PRESSED;
  }

  return BUTTON_HAL_STATE_RELEASED;
}
/*---------------------------------------------------------------------------*/
void
button_hal_init()
{
  button_hal_button_t **button;
  gpio_hal_pin_cfg_t cfg;
  gpio_hal_event_handler_t *handler;
  int i;

  button_hal_press_event = process_alloc_event();
  button_hal_release_event = process_alloc_event();
  button_hal_periodic_event = process_alloc_event();

  for(i = 0; i < BUTTON_HAL_PORT_COUNT; i++) {
#if GPIO_HAL_PORT_PIN_NUMBERING
    button_event_handlers[i].port = PORT_UNUSED;
#endif
    button_event_handlers[i].pin_mask = 0;
    button_event_handlers[i].handler = press_release_handler;
  }

  for(button = button_hal_buttons; *button != NULL; button++) {
    cfg = GPIO_HAL_PIN_CFG_EDGE_BOTH | GPIO_HAL_PIN_CFG_INT_ENABLE |
      (*button)->pull;
    gpio_hal_arch_pin_set_input(BTN_PORT(*button), (*button)->pin);
    gpio_hal_arch_pin_cfg_set(BTN_PORT(*button), (*button)->pin, cfg);
    gpio_hal_arch_interrupt_enable(BTN_PORT(*button), (*button)->pin);

    handler = get_handler_by_port(BTN_PORT(*button));

    if(handler) {
#if GPIO_HAL_PORT_PIN_NUMBERING
      handler->port = BTN_PORT(*button);
#endif
      handler->pin_mask |= gpio_hal_pin_to_mask((*button)->pin);
    }
  }

  process_start(&button_hal_process, NULL);

  for(i = 0; i < BUTTON_HAL_PORT_COUNT; i++) {
    gpio_hal_register_handler(&button_event_handlers[i]);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(button_hal_process, ev, data)
{
  int_master_status_t status;
  gpio_hal_pin_mask_t pins;
  button_hal_button_t **button;
#if GPIO_HAL_PORT_PIN_NUMBERING
  uint8_t ports;
#endif

  PROCESS_BEGIN();

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);

    status = critical_enter();
    pins = pmask;
    pmask = 0;
#if GPIO_HAL_PORT_PIN_NUMBERING
    ports = port_mask;
    port_mask = 0;
#endif
    critical_exit(status);

    for(button = button_hal_buttons; *button != NULL; button++) {
      if((gpio_hal_pin_to_mask((*button)->pin) & pins)
#if GPIO_HAL_PORT_PIN_NUMBERING
         && ((1 << (*button)->port) & ports)
#endif
         ) {
        /* Ignore all button presses/releases during its debounce */
        if(ctimer_expired(&(*button)->debounce_ctimer)) {
          /*
           * Here we merely set a debounce timer. At the end of the debounce we
           * will inspect the button's state and we will take action only if it
           * has changed.
           *
           * This is to prevent erroneous edge detections due to interference.
           */
          ctimer_set(&(*button)->debounce_ctimer, BUTTON_HAL_DEBOUNCE_DURATION,
                     debounce_handler, *button);
        }
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/** @} */
