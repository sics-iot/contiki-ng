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
 * \addtogroup gecko
 * @{
 *
 * \file
 *      Header with defines common to all gecko platforms
 * \author
 *      Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 */
/*---------------------------------------------------------------------------*/
#ifndef GECKO2_DEF_H_
#define GECKO2_DEF_H_
/*---------------------------------------------------------------------------*/
#include "arm-def.h"
/*---------------------------------------------------------------------------*/
/* Path to headers with implementation of mutexes, atomic and memory barriers */
#define MUTEX_CONF_ARCH_HEADER_PATH          "mutex-cortex.h"
#define ATOMIC_CONF_ARCH_HEADER_PATH         "atomic-cortex.h"
#define MEMORY_BARRIER_CONF_ARCH_HEADER_PATH "memory-barrier-cortex.h"
/*---------------------------------------------------------------------------*/
#define RTIMER_ARCH_SECOND 32768
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
#define RADIO_PHY_HEADER_LEN        (5)
#define RADIO_PHY_OVERHEAD          (3)
#define RADIO_BIT_RATE              (250000)
#define RADIO_BYTE_AIR_TIME         (1000000 / (RADIO_BIT_RATE / 8))
#define RADIO_DELAY_BEFORE_TX       ((unsigned)US_TO_RTIMERTICKS(192))
#define RADIO_DELAY_BEFORE_RX       ((unsigned)US_TO_RTIMERTICKS(182))
#define RADIO_DELAY_BEFORE_DETECT   ((unsigned)US_TO_RTIMERTICKS(358))
/*---------------------------------------------------------------------------*/
#define GPIO_HAL_CONF_ARCH_HDR_PATH          "gpio-hal-arch.h"
#define GPIO_HAL_CONF_ARCH_SW_TOGGLE         0
/*---------------------------------------------------------------------------*/
#define TSCH_CONF_HW_FRAME_FILTERING          0
#define TSCH_CONF_RADIO_ON_DURING_TIMESLOT    1
#define TSCH_CONF_RESYNC_WITH_SFD_TIMESTAMPS  1
#define TSCH_CONF_TIMESYNC_REMOVE_JITTER      0
#define TSCH_CONF_BASE_DRIFT_PPM              -977
/*---------------------------------------------------------------------------*/
#endif /* GECKO2_DEF_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
