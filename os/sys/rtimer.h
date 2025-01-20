/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
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
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Header file for the real-time timer module.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

/** \addtogroup threads
 * @{ */

/**
 * \defgroup rt Real-time task scheduling
 *
 * The real-time module handles the scheduling and execution of
 * real-time tasks (with predictable execution times).
 *
 * @{
 */

#ifndef RTIMER_H_
#define RTIMER_H_

#include "contiki.h"
#include "dev/watchdog.h"
#include <stdbool.h>

/*---------------------------------------------------------------------------*/

/** \brief The rtimer size (in bytes) */
#ifdef RTIMER_CONF_CLOCK_SIZE
#define RTIMER_CLOCK_SIZE RTIMER_CONF_CLOCK_SIZE
#else /* RTIMER_CONF_CLOCK_SIZE */
/* Default: 32bit rtimer*/
#define RTIMER_CLOCK_SIZE 4
#endif /* RTIMER_CONF_CLOCK_SIZE */

#if RTIMER_CLOCK_SIZE == 2
/* 16-bit rtimer */
typedef uint16_t rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a,b)     ((int16_t)((a)-(b)))

#elif RTIMER_CLOCK_SIZE == 4
/* 32-bit rtimer */
typedef uint32_t rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a, b)    ((int32_t)((a) - (b)))

#elif RTIMER_CLOCK_SIZE == 8
/* 64-bit rtimer */
typedef uint64_t rtimer_clock_t;
#define RTIMER_CLOCK_DIFF(a,b)     ((int64_t)((a)-(b)))

#else
#error Unsupported rtimer size (check RTIMER_CLOCK_SIZE)
#endif

#define RTIMER_CLOCK_MAX           ((rtimer_clock_t)-1)
#define RTIMER_CLOCK_LT(a, b)      (RTIMER_CLOCK_DIFF((a),(b)) < 0)

#include "rtimer-arch.h"


/*
 * RTIMER_GUARD_TIME is the minimum amount of rtimer ticks between
 * the current time and the future time when a rtimer is scheduled.
 * Necessary to avoid accidentally scheduling a rtimer in the past
 * on platforms with fast rtimer ticks. Should be >= 2.
 */
#ifdef RTIMER_CONF_GUARD_TIME
#define RTIMER_GUARD_TIME RTIMER_CONF_GUARD_TIME
#else /* RTIMER_CONF_GUARD_TIME */
#define RTIMER_GUARD_TIME (RTIMER_ARCH_SECOND >> 14)
#endif /* RTIMER_CONF_GUARD_TIME */

/*---------------------------------------------------------------------------*/

/**
 * Number of rtimer ticks for 1 second.
 */
#define RTIMER_SECOND RTIMER_ARCH_SECOND

/**
 * \brief      Initialize the real-time scheduler.
 *
 *             This function initializes the real-time scheduler and
 *             must be called at boot-up, before any other functions
 *             from the real-time scheduler is called.
 *
 * \hideinitializer
 */
#define rtimer_init() rtimer_arch_init()

struct rtimer;
typedef void (* rtimer_callback_t)(struct rtimer *t, void *ptr);

/**
 * \brief      Representation of a real-time task
 *
 *             This structure represents a real-time task and is used
 *             by the real-time module and the architecture specific
 *             support module for the real-time module.
 */
struct rtimer {
  rtimer_clock_t time;
  rtimer_callback_t func;
  void *ptr;
};

/**
 * TODO: we need to document meanings of these symbols.
 */
enum {
  RTIMER_OK, /**< rtimer task is scheduled successfully */
  RTIMER_ERR_FULL,
  RTIMER_ERR_TIME,
  RTIMER_ERR_ALREADY_SCHEDULED,
};

/**
 * \brief      Post a real-time task.
 * \param task A pointer to the task variable allocated somewhere.
 * \param time The time when the task is to be executed.
 * \param duration Unused argument.
 * \param func A function to be called when the task is executed.
 * \param ptr An opaque pointer that will be supplied as an argument to the callback function.
 * \return     RTIMER_OK if the task could be scheduled. Any other value indicates
 *             the task could not be scheduled.
 *
 *             This function schedules a real-time task at a specified
 *             time in the future.
 *
 */
int rtimer_set(struct rtimer *task, rtimer_clock_t time,
	       rtimer_clock_t duration, rtimer_callback_t func, void *ptr);

/**
 * \brief      Execute the next real-time task and schedule the next task, if any
 *
 *             This function is called by the architecture dependent
 *             code to execute and schedule the next real-time task.
 *
 */
void rtimer_run_next(void);

/**
 * \brief      Get the current clock time
 * \return     The current time
 *
 *             This function returns what the real-time module thinks
 *             is the current time. The current time is used to set
 *             the timeouts for real-time tasks.
 *
 * \hideinitializer
 */
#define RTIMER_NOW() rtimer_arch_now()

/**
 * \brief      Get the time that a task last was executed
 * \param task The task
 * \return     The time that a task last was executed
 *
 *             This function returns the time that the task was last
 *             executed. This typically is used to get a periodic
 *             execution of a task without clock drift.
 *
 * \hideinitializer
 */
#define RTIMER_TIME(task) ((task)->time)

/** \brief Busy-wait until a condition. Start time is t0, max wait time is max_time */
#ifndef RTIMER_BUSYWAIT_UNTIL_ABS
#define RTIMER_BUSYWAIT_UNTIL_ABS(cond, t0, max_time) \
  ({                                                                \
    bool c;                                                         \
    while(!(c = cond) && RTIMER_CLOCK_LT(RTIMER_NOW(), (t0) + (max_time))); \
    c;                                                              \
  })
#endif /* RTIMER_BUSYWAIT_UNTIL_ABS */

/** \brief Busy-wait until a condition for at most max_time */
#define RTIMER_BUSYWAIT_UNTIL(cond, max_time)       \
  ({                                                \
    rtimer_clock_t t0 = RTIMER_NOW();               \
    RTIMER_BUSYWAIT_UNTIL_ABS(cond, t0, max_time);  \
  })

/** \brief Busy-wait for a fixed duration */
#define RTIMER_BUSYWAIT(duration) RTIMER_BUSYWAIT_UNTIL(0, duration)

/*---------------------------------------------------------------------------*/

/**
 * \name Architecture-dependent symbols
 *
 * The functions declared in this section must be defined in
 * architecture-dependent implementation of rtimer. Alternatively,
 * they can be defined as macros in rtimer-arch.h.
 *
 * In addition, the architecture-dependent header (rtimer-arch.h)
 * must define the following macros.
 *
 * - RTIMER_ARCH_SECOND
 * - US_TO_RTIMERTICKS(us)
 * - RTIMERTICKS_TO_US(t)
 * - RTIMERTICKS_TO_US_64(t)
 *
 * @{
 */

/**
 * Initialized the architecture-dependent part of rtimer.
 */
void rtimer_arch_init(void);

/**
 * \brief Schedules an rtimer task to be triggered at time t
 * \param t The time when the task will need executed.
 *
 * \e t is an absolute time, in other words the task will be executed AT
 * time \e t, not IN \e t rtimer ticks.
 *
 */
void rtimer_arch_schedule(rtimer_clock_t t);

/*
 * Return the current time in rtimer ticks.
 *
 * Currently rtimer_arch_now() needs to be defined in rtimer-arch.h
 */

/** @} */

#endif /* RTIMER_H_ */

/** @} */
/** @} */
