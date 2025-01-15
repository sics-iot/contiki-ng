/*
 * Copyright (c) 2020, Toshiba Corporation
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
/**
 * \file
 *         Multi-Producer Multi-Consumer lock-free ring buffer
 * \author
 *         Toshio Ito <toshio9.ito@toshiba.co.jp>
 *
 * mpmc-ring is analogous to ringbufindex, but it supports
 * multi-producer, multi-consumer scenarios. To do that, mpmc-ring
 * requires extra memory.
 *
 * mpmc-ring uses sys/atomic to arbitrate parallel accesses to the
 * queue. If your platform implements sys/atomic by deferring
 * interrupts, mpmc-queue also defers interrupts.
 *
 * To define and allocate a mpmc-ring object, use MPMC_RING macro. For
 * basic usage, see tests/07-simulation-base/code-mpmc-ring and
 * examples/libs/mpmc-ring-interrupt/mpmc-ring-interrupt.c, especially
 * do_get and do_put functions.
 */

#ifndef _MPMC_RING_H_
#define _MPMC_RING_H_

#include "contiki.h"
#include "sys/atomic.h"
#include "sys/cc.h"

/*----------------------------------------------------------------------------------------*/

/**
 * The multi-producer multi-consumer ring buffer.
 *
 * Do not declare and define struct mpmc_ring directly. Use
 * MPMC_RING macro instead.
 *
 * All fields are private. Users should not read or write them.
 */
typedef struct mpmc_ring {
  uint8_t put_pos;
  uint8_t get_pos;
  uint8_t * const sequences;
  const uint8_t mask;
} mpmc_ring_t;

/**
 * The array index managed by mpmc_ring.
 */
typedef struct mpmc_ring_index {
  /**
   * The index for the user-defined array that keeps the actual data
   * structures. Users can read this field.
   */
  uint8_t i;

  /**
   * This field is private. Users should not read or write it.
   */
  uint8_t _pos;
} mpmc_ring_index_t;

/*----------------------------------------------------------------------------------------*/

/**
 * Declare and define a mpmc_ring.
 *
 * \param name Variable name of the mpmc_ring_t.
 *
 * \param size Size of the array that keeps the queue elements. Must
 * be power of 2, and 0 < size <= 64.
 */
#define MPMC_RING(name, size)                                           \
  static uint8_t CC_CONCAT(name,_sequences)[size];                      \
  static mpmc_ring_t name = { 0, 0, CC_CONCAT(name,_sequences), (size) - 1 };

/**
 * Initialize the mpmc_ring.
 */
void mpmc_ring_init(mpmc_ring_t *ring);

/**
 * Start putting an element to the queue. Every call to
 * mpmc_ring_put_begin must be finished by one and only call to
 * mpmc_ring_put_commit.
 *
 * \param ring The mpmc_ring object
 *
 * \param got_index (output) The index that the caller should use to
 * put an element.
 *
 * \retval 0 Failure. The queue is full. got_index is not modified.
 * \retval 1 Success. got_index is modified.
 */
int mpmc_ring_put_begin(mpmc_ring_t *ring, mpmc_ring_index_t *got_index);

/**
 * Finish putting an element to the queue.
 *
 * \param ring The mpmc_ring object.
 *
 * \param index The index obtained by the matching call to
 * mpmc_ring_put_begin.
 *
 */
void mpmc_ring_put_commit(mpmc_ring_t *ring, const mpmc_ring_index_t *index);

/**
 * Start getting an element from the queue. Every call to
 * mpmc_ring_get_begin must be finished by one and only call to
 * mpmc_ring_get_commit.
 *
 * \param ring The mpmc_ring object
 *
 * \param got_index (output) The index that the caller should use to
 * get an element.
 *
 * \retval 0 Failure. The queue is empty. got_index is not modified.
 * \retval 1 Success. got_index is modified.
 */
int mpmc_ring_get_begin(mpmc_ring_t *ring, mpmc_ring_index_t *got_index);

/**
 * Finish getting an element to the queue.
 *
 * \param ring The mpmc_ring object.
 *
 * \param index The index obtained by the matching call to
 * mpmc_ring_get_begin.
 *
 */
void mpmc_ring_get_commit(mpmc_ring_t *ring, const mpmc_ring_index_t *index);

/**
 * \return Number of elements currently in the queue.
 */
int mpmc_ring_elements(const mpmc_ring_t *ring);

/**
 * \return Non-zero if the queue is empty. Zero otherwise.
 */
int mpmc_ring_empty(const mpmc_ring_t *ring);

/**
 * \return Number of elements that the queue can keep.
 */
uint8_t mpmc_ring_size(const mpmc_ring_t *ring);

#endif /* _MPMC_RING_H_ */
