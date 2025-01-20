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
 * \addtogroup dev
 * @{
 */
/*---------------------------------------------------------------------------*/
/**
 * \defgroup button_hal Button HAL
 *
 * Hardware abstraction layer for user buttons.
 *
 * This HAL enables an abstraction of general-purpose / user buttons or
 * similar peripherals (e.g. a reed relay can also be abstracted through this
 * HAL). The HAL handles software debounce timers internally, therefore the
 * platform-specific button driver does not need to worry about debouncing.
 *
 * The platform developer needs to define a variable of type
 * \c button_hal_button_t for each user button. Within this variable, the
 * developer needs to specify the GPIO pin where the button is attached,
 * whether the button uses negative logic, and whether the GPIO pin should
 * be configured with internal pullup/down. The developer also needs to provide
 * a unique index for each button, as well as a description.
 *
 * With those in place, the HAL will generate the following process events:
 *
 * - button_hal_press_event: Upon press of the button
 * - button_hal_release_event: Upon release of the button
 * - button_hal_periodic_event: Generated every second that the user button is
 *   kept pressed.
 *
 * With those events in place, an application can perform an action:
 *
 * - Immediately after the button gets pressed.
 * - After the button has been pressed for N seconds.
 * - Immediately upon release of the button. This action can vary depending
 *   on how long the button had been pressed for.
 *
 * A platform with user buttons can either implement this API (recommended) or
 * the older button_sensor API. Some examples will not work if this API is not
 * implemented.
 *
 * This API requires the platform to first support the GPIO HAL API.
 * @{
 *
 * \file
 * Header file for the button HAL
 */
/*---------------------------------------------------------------------------*/
#ifndef BUTTON_HAL_H_
#define BUTTON_HAL_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "dev/gpio-hal.h"
#include "sys/clock.h"
#include "sys/ctimer.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
/**
 * \brief Controls the software debounce timer duration.
 *
 * The platform can provide a more suitable value. This value will apply to
 * all buttons.
 */
#ifdef BUTTON_HAL_CONF_DEBOUNCE_DURATION
#define BUTTON_HAL_DEBOUNCE_DURATION BUTTON_HAL_CONF_DEBOUNCE_DURATION
#else
#define BUTTON_HAL_DEBOUNCE_DURATION (CLOCK_SECOND >> 6)
#endif
/**
 * \brief Controls the button_hal_periodic_event interval.
 *
 * Default value is 1 second.
 */
#ifdef BUTTON_HAL_CONF_PERIODIC_INTERVAL
#define BUTTON_HAL_PERIODIC_INTERVAL BUTTON_HAL_CONF_PERIODIC_INTERVAL
#else
#define BUTTON_HAL_PERIODIC_INTERVAL CLOCK_SECOND
#endif

#if (BUTTON_HAL_PERIODIC_INTERVAL < (BUTTON_HAL_DEBOUNCE_DURATION << 2))
#error BUTTON_HAL_PERIODIC_INTERVAL is too close to BUTTON_HAL_DEBOUNCE_DURATION!
#endif
/*---------------------------------------------------------------------------*/
/**
 * \brief Controls whether buttons will have human-readable names
 *
 * Define this to zero to save code space
 */
#if BUTTON_HAL_CONF_WITH_DESCRIPTION
#define BUTTON_HAL_WITH_DESCRIPTION BUTTON_HAL_CONF_WITH_DESCRIPTION
#else
#define BUTTON_HAL_WITH_DESCRIPTION 1
#endif
/*---------------------------------------------------------------------------*/
/**
 * \brief Number of different ports that buttons are connected to
 *
 * For example, if our PCB has 3 buttons conncted to pins P0.1, P0,6 and P2.3
 * then the platform configuration should define this to value 2 (one for each
 * of ports 0 and 2)
 */
#ifdef BUTTON_HAL_CONF_PORT_COUNT
#define BUTTON_HAL_PORT_COUNT BUTTON_HAL_CONF_PORT_COUNT
#else
#define BUTTON_HAL_PORT_COUNT 1
#endif
/*---------------------------------------------------------------------------*/
#define BUTTON_HAL_STATE_RELEASED 0
#define BUTTON_HAL_STATE_PRESSED  1
/*---------------------------------------------------------------------------*/
/**
 * Optional button IDs
 */
#define BUTTON_HAL_ID_BUTTON_ZERO  0x00
#define BUTTON_HAL_ID_BUTTON_ONE   0x01
#define BUTTON_HAL_ID_BUTTON_TWO   0x02
#define BUTTON_HAL_ID_BUTTON_THREE 0x03
#define BUTTON_HAL_ID_BUTTON_FOUR  0x04
#define BUTTON_HAL_ID_BUTTON_FIVE  0x05

#define BUTTON_HAL_ID_USER_BUTTON  BUTTON_HAL_ID_BUTTON_ZERO
/*---------------------------------------------------------------------------*/
/**
 * \brief A logical representation of a user button
 */
typedef struct button_hal_button_s button_hal_button_t;

struct button_hal_button_s {
  /** Used by the s/w debounce functionality */
  struct ctimer debounce_ctimer;

  /** A callback timer used to count duration of button presses */
  struct ctimer duration_ctimer;

#if BUTTON_HAL_WITH_DESCRIPTION
  /**
   * \brief A textual description of the button
   *
   * This field may only be accessed using the BUTTON_HAL_GET_DESCRIPTION()
   * macro.
   */
  const char *description;
#endif
  /** The pin's pull configuration */
  const gpio_hal_pin_cfg_t pull;

  /** True if the button uses negative logic (active: low) */
  const bool negative_logic;

#if GPIO_HAL_PORT_PIN_NUMBERING
  /** The gpio port connected to the button */
  gpio_hal_port_t port;
#endif

  /** The gpio pin connected to the button */
  const gpio_hal_pin_t pin;

  /** A counter of the amount of press_duration_events */
  uint8_t press_duration_events;

  /**
   * \brief A unique identifier for this button.
   *
   * The platform code is responsible of setting unique values here. This can
   * be used later to determine which button generated an event. Many examples
   * assume the existence of a button with ID == BUTTON_HAL_ID_BUTTON_ZERO,
   * so it is good idea to use this ID for one of your platform's buttons.
   */
  const uint8_t unique_id;
};
/*---------------------------------------------------------------------------*/
#if BUTTON_HAL_WITH_DESCRIPTION
#if GPIO_HAL_PORT_PIN_NUMBERING
/**
 * \brief Define a button to be used by the HAL
 * \param name The variable name for the button
 * \param descr A textual description
 * \param po The port connected to the button
 * \param pi The pin connected to the button
 * \param nl True if the button is connected using negative logic
 * \param u The button's pull configuration
 * \param id A unique numeric identifier
 */
#define BUTTON_HAL_BUTTON(name, descr, po, pi, u, id, nl) \
  static button_hal_button_t name = { \
    .description = descr, \
    .port = po, \
    .pin = pi, \
    .pull = u, \
    .unique_id = id, \
    .negative_logic = nl, \
  }
#else /* GPIO_HAL_PORT_PIN_NUMBERING */
#define BUTTON_HAL_BUTTON(name, descr, pi, u, id, nl) \
  static button_hal_button_t name = { \
    .description = descr, \
    .pin = pi, \
    .pull = u, \
    .unique_id = id, \
    .negative_logic = nl, \
  }
#endif /* GPIO_HAL_PORT_PIN_NUMBERING */

/**
 * \brief Retrieve the textual description of a button
 * \param b A pointer to the button button_hal_button_t
 *
 * This macro will return the value of the description field for b. If
 * BUTTON_HAL_WITH_DESCRIPTION is 0 then this macro will return ""
 */
#define BUTTON_HAL_GET_DESCRIPTION(b) (b)->description

#else /* BUTTON_HAL_WITH_DESCRIPTION */

#if GPIO_HAL_PORT_PIN_NUMBERING
#define BUTTON_HAL_BUTTON(name, descr, po, pi, u, id, nl) \
  static button_hal_button_t name = { \
    .port = po, \
    .pin = pi, \
    .pull = u, \
    .unique_id = id, \
    .negative_logic = nl, \
  }
#else /* GPIO_HAL_PORT_PIN_NUMBERING */
#define BUTTON_HAL_BUTTON(name, descr, pi, u, id, nl) \
  static button_hal_button_t name = { \
    .pin = pi, \
    .pull = u, \
    .unique_id = id, \
    .negative_logic = nl, \
  }
#endif /* GPIO_HAL_PORT_PIN_NUMBERING */

#define BUTTON_HAL_GET_DESCRIPTION(b) ""
#endif /* BUTTON_HAL_WITH_DESCRIPTION */
/*---------------------------------------------------------------------------*/
#define BUTTON_HAL_BUTTONS(...) \
  button_hal_button_t *button_hal_buttons[] = {__VA_ARGS__, NULL}; \
  const uint8_t button_hal_button_count = \
    (sizeof(button_hal_buttons) / sizeof(button_hal_buttons[0])) - 1;
/*---------------------------------------------------------------------------*/
/**
 * \brief The number of buttons on a device
 */
extern const uint8_t button_hal_button_count;
/*---------------------------------------------------------------------------*/
/**
 * \brief A broadcast event generated when a button gets pressed
 */
extern process_event_t button_hal_press_event;

/**
 * \brief A broadcast event generated when a button gets released
 */
extern process_event_t button_hal_release_event;

/**
 * \brief A broadcast event generated every second while a button is kept pressed
 */
extern process_event_t button_hal_periodic_event;
/*---------------------------------------------------------------------------*/
/**
 * \brief Initialise the button HAL
 */
void button_hal_init(void);

/**
 * \brief Retrieve a button by ID
 * \param unique_id The button unique ID to search for
 * \return A pointer to the button or NULL if not found
 */
button_hal_button_t *button_hal_get_by_id(uint8_t unique_id);

/**
 * \brief Retrieve a button by its index
 * \param index The button's index (0, 1, ... button_hal_button_count - 1)
 * \return A pointer to the button or NULL if not found
 */
button_hal_button_t *button_hal_get_by_index(uint8_t index);

/**
 * \brief Get the state of a button (pressed / released)
 * \param button A pointer to the button
 * \retval BUTTON_HAL_STATE_RELEASED The button is currently released
 * \retval BUTTON_HAL_STATE_PRESSED The button is currently pressed
 */
uint8_t button_hal_get_state(button_hal_button_t *button);
/*---------------------------------------------------------------------------*/
#endif /* BUTTON_HAL_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
