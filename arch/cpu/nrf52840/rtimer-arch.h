/*
 * Copyright (c) 2015, Nordic Semiconductor
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
 * \addtogroup nrf52840
 * @{
 *
 * \file
 *         Architecture dependent rtimer implementation header file.
 * \author
 *         Wojciech Bober <wojciech.bober@nordicsemi.no>
 *
 */
/*---------------------------------------------------------------------------*/
#ifndef RTIMER_ARCH_H_
#define RTIMER_ARCH_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
/*---------------------------------------------------------------------------*/
rtimer_clock_t rtimer_arch_now(void);
rtimer_clock_t rtimer_arch_now_radio(void);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/

#define US_TO_RTIMERTICKS(us)   ( \
  (((us) >= 0) \
    ? (((int32_t)(us) * (RTIMER_ARCH_SECOND / 2 ) + 500000) / 1000000L) \
    : (((int32_t)(us) * (RTIMER_ARCH_SECOND / 2 ) - 500000) / 1000000L) \
  ) )

#define RTIMERTICKS_TO_US(rt)   ( \
  ((rt) >= 0) \
    ? (((int32_t)(rt) * 1000000L + (RTIMER_ARCH_SECOND / 2)) / RTIMER_ARCH_SECOND) \
    : (((int32_t)(rt) * 1000000L - (RTIMER_ARCH_SECOND / 2)) / RTIMER_ARCH_SECOND) \
  )

#define RTIMERTICKS_TO_US_64(rt)  ( \
  (uint32_t)( \
    ((uint64_t)(rt) * 1000000 + (RTIMER_ARCH_SECOND / 2)) / RTIMER_ARCH_SECOND \
  ))
/*---------------------------------------------------------------------------*/


#endif /* RTIMER_ARCH_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 */