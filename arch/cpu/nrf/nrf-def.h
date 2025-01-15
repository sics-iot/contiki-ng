/*
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
 * \file
 *      Header with defines common to all nrf platforms
 * \author
 *      Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 */
/*---------------------------------------------------------------------------*/
#ifndef NRF_DEF_H_
#define NRF_DEF_H_
/*---------------------------------------------------------------------------*/
#include "arm-def.h"
/*---------------------------------------------------------------------------*/
#include "nrf.h"
/*---------------------------------------------------------------------------*/
/* Include CPU Specific defines */
#ifdef CPU_DEF_PATH
#include CPU_DEF_PATH
#else
#error "CPU_DEF_PATH undefined"
#endif /* BOARD_CONF_PATH */
/*---------------------------------------------------------------------------*/
/* Path to headers with implementation of mutexes, atomic and memory barriers */
#define MUTEX_CONF_ARCH_HEADER_PATH          "mutex-cortex.h"
#define ATOMIC_CONF_ARCH_HEADER_PATH         "atomic-cortex.h"
#define MEMORY_BARRIER_CONF_ARCH_HEADER_PATH "memory-barrier-cortex.h"
/*---------------------------------------------------------------------------*/
/* Do the math in 32bits to save precision.
 * Round to nearest integer rather than truncate. */
#define US_TO_RTIMERTICKS(US)  ((US) >= 0 ? \
                                (((int32_t)(US)*(RTIMER_ARCH_SECOND)+500000) / 1000000L) : \
                                ((int32_t)(US)*(RTIMER_ARCH_SECOND)-500000) / 1000000L)

#define RTIMERTICKS_TO_US(T)   ((T) >= 0 ? \
                                (((int32_t)(T) * 1000000L + ((RTIMER_ARCH_SECOND) / 2)) / (RTIMER_ARCH_SECOND)) : \
                                ((int32_t)(T) * 1000000L - ((RTIMER_ARCH_SECOND) / 2)) / (RTIMER_ARCH_SECOND))

/* A 64-bit version because the 32-bit one cannot handle T >= 4295 ticks.
   Intended only for positive values of T. */
#define RTIMERTICKS_TO_US_64(T)  ((uint32_t)(((uint64_t)(T) * 1000000 + ((RTIMER_ARCH_SECOND) / 2)) / (RTIMER_ARCH_SECOND)))
/*---------------------------------------------------------------------------*/
#define RADIO_PHY_OVERHEAD            3
#define RADIO_BYTE_AIR_TIME          32
#define RADIO_SHR_LEN                 5 /* Synch word + SFD */
#define RADIO_DELAY_BEFORE_TX \
        ((unsigned)US_TO_RTIMERTICKS(RADIO_SHR_LEN * RADIO_BYTE_AIR_TIME))
#define RADIO_DELAY_BEFORE_RX         ((unsigned)US_TO_RTIMERTICKS(250))
#define RADIO_DELAY_BEFORE_DETECT     0
/*---------------------------------------------------------------------------*/
/* RTC with no prescaler */
#define RTIMER_ARCH_SECOND 32768
/*---------------------------------------------------------------------------*/
#define GPIO_HAL_CONF_ARCH_HDR_PATH          "gpio-hal-arch.h"
#define GPIO_HAL_CONF_ARCH_SW_TOGGLE         0
/*---------------------------------------------------------------------------*/
#ifndef TSCH_CONF_HW_FRAME_FILTERING
#define TSCH_CONF_HW_FRAME_FILTERING  0
#endif /* TSCH_CONF_HW_FRAME_FILTERING */

#ifndef TSCH_CONF_RADIO_ON_DURING_TIMESLOT
#define TSCH_CONF_RADIO_ON_DURING_TIMESLOT 1
#endif /* TSCH_CONF_RADIO_ON_DURING_TIMESLOT */

#ifndef TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS
#define TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS 1
#endif /* TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS */

#ifndef TSCH_CONF_TIMESYNC_REMOVE_JITTER
#define TSCH_CONF_TIMESYNC_REMOVE_JITTER 0
#endif /* TSCH_CONF_TIMESYNC_REMOVE_JITTER */
/*---------------------------------------------------------------------------*/
#define CSMA_CONF_SEND_SOFT_ACK       1
/*---------------------------------------------------------------------------*/
#endif /* NRF_DEF_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
