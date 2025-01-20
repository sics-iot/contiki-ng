/*
 * Copyright (c) 2024, Konrad-Felix Krentz.
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
 * \addtogroup process
 * @{
 */

/**
 * \file
 *         Header file for Process mutexes.
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

#ifndef PROCESS_MUTEX_H_
#define PROCESS_MUTEX_H_

#include "contiki.h"
#include <stdbool.h>

/**
 * Structure of a process mutex.
 */
typedef struct process_mutex_t {
  struct process *waiting_process;
  bool has_more_waiting_processes;
  bool is_locked;
} process_mutex_t;

/**
 * \brief       Initializes a process mutex.
 * \param mutex The process mutex.
 */
void process_mutex_init(process_mutex_t *mutex);

/**
 * \brief       Tries to acquire a process mutex.
 * \param mutex The process mutex.
 * \return      True if the process mutex was locked.
 */
bool process_mutex_try_lock(process_mutex_t *mutex);

/**
 * \brief       Unlocks a process mutex.
 * \param mutex The process mutex.
 */
void process_mutex_unlock(process_mutex_t *mutex);

#endif /* PROCESS_MUTEX_H_ */

/** @} */
