/*
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
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup gecko-platforms
 * @{
 *
 * \addtogroup brd4002a
 * @{
 *
 * \file
 *         brd4002a specific configuration.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *         Yevgen Gyl <yevgen.gyl@tii.ae>
 */
/*---------------------------------------------------------------------------*/
#ifndef BRD4002A_CONF_H
#define BRD4002A_CONF_H
/*---------------------------------------------------------------------------*/
#include "em_gpio.h"
/*---------------------------------------------------------------------------*/
#define PLATFORM_HAS_BUTTON             1
#define PLATFORM_SUPPORTS_BUTTON_HAL    1
/*---------------------------------------------------------------------------*/
#define GECKO_BUTTON1_PIN     1
#define GECKO_BUTTON1_PORT    gpioPortB
#define GECKO_BUTTON2_PIN     3
#define GECKO_BUTTON2_PORT    gpioPortB
/*---------------------------------------------------------------------------*/
#define GECKO_LED1_PIN        2
#define GECKO_LED1_PORT       gpioPortB
#define GECKO_LED2_PIN        3
#define GECKO_LED2_PORT       gpioPortD
/*---------------------------------------------------------------------------*/
#define LEDS_CONF_COUNT     2
/*---------------------------------------------------------------------------*/
#endif /* BRD4002A_CONF_H */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
