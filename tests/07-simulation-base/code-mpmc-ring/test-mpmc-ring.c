/*
 * Copyright (c) 2020, Toshio Ito
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
 *
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

#include <stdio.h>

#include "contiki.h"
#include "sys/cc.h"
#include "unit-test/unit-test.h"

#include "lib/mpmc-ring.h"

#define UTEST_RINGS(name,desc)                             \
  UNIT_TEST_REGISTER(name##_2, desc ": size 2");           \
  UNIT_TEST(name##_2) { name(utp, &ring2); }               \
  UNIT_TEST_REGISTER(name##_32, desc ": size 32");         \
  UNIT_TEST(name##_32) { name(utp, &ring32); }             \
  UNIT_TEST_REGISTER(name##_64, desc ": size 64");       \
  UNIT_TEST(name##_64) { name(utp, &ring64); }           \

#define UTEST_RING_RUN(name)                    \
  UNIT_TEST_RUN(name##_2);                      \
  UNIT_TEST_RUN(name##_32);                     \
  UNIT_TEST_RUN(name##_64);

/*****************************************************************/

PROCESS(test_process, "mpmc-ring test");
AUTOSTART_PROCESSES(&test_process);

static void
init_index(mpmc_ring_index_t *i)
{
  i->i = 0;
  i->_pos = 0;
}

void
test_print_report(const unit_test_t *utp)
{
  printf("=check-me= ");
  if(utp->result == unit_test_failure) {
    printf("FAILED   - %s: exit at L%u\n", utp->descr, utp->exit_line);
  } else {
    printf("SUCCEEDED - %s\n", utp->descr);
  }
}

static void
test_init_get(unit_test_t *utp, mpmc_ring_t *ring)
{
  mpmc_ring_index_t index;
  int is_success;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(ring);
  UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == 0);
  UNIT_TEST_ASSERT(mpmc_ring_empty(ring));
  is_success = mpmc_ring_get_begin(ring, &index);
  UNIT_TEST_ASSERT(!is_success);

  UNIT_TEST_END();
}

static void
test_put_get(unit_test_t *utp, mpmc_ring_t *ring)
{
  const int GET_COUNT = mpmc_ring_size(ring) / 2;
  const int LOOP_NUM = 300;
  mpmc_ring_index_t index;
  int vals[128];
  int put_val = 100;
  int exp_val = put_val;
  int got_val;
  int i;
  int is_success;

  UNIT_TEST_BEGIN();

  mpmc_ring_init(ring);

  /* Put to full */
  for(i = 0 ; i < mpmc_ring_size(ring) ; i++) {
    init_index(&index);
    is_success = mpmc_ring_put_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = put_val;
    mpmc_ring_put_commit(ring, &index);
    put_val++;

    // if(i > 100) {
    //   printf("Loop = %d, elems = %d, get_pos = %u, put_pos = %u\n",
    //          i, mpmc_ring_elements(ring), ring->get_pos, ring->put_pos);
    // }
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == i + 1);
  }
  init_index(&index);
  is_success = mpmc_ring_put_begin(ring, &index);
  UNIT_TEST_ASSERT(!is_success);

  /* Get half-way */
  for(i = 0 ; i < GET_COUNT ; i++) {
    init_index(&index);
    is_success = mpmc_ring_get_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    got_val = vals[index.i];
    mpmc_ring_get_commit(ring, &index);
    UNIT_TEST_ASSERT(got_val == exp_val);
    exp_val++;
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == mpmc_ring_size(ring) - i - 1);
  }

  /* Put and get for a while */
  for(i = 0 ; i < LOOP_NUM ; i++) {
    init_index(&index);
    is_success = mpmc_ring_put_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = put_val;
    mpmc_ring_put_commit(ring, &index);
    put_val++;

    // printf("Loop = %d, elems = %d, get_pos = %u, put_pos = %u\n",
    //        i, mpmc_ring_elements(ring), ring->get_pos, ring->put_pos);
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == mpmc_ring_size(ring) - GET_COUNT + 1);

    init_index(&index);
    is_success = mpmc_ring_get_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    got_val = vals[index.i];
    mpmc_ring_get_commit(ring, &index);
    UNIT_TEST_ASSERT(got_val == exp_val);
    exp_val++;
    
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == mpmc_ring_size(ring) - GET_COUNT);
  }

  /* Get to empty */
  for(i = 0 ; i < mpmc_ring_size(ring) - GET_COUNT ; i++ ) {
    init_index(&index);
    is_success = mpmc_ring_get_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    got_val = vals[index.i];
    mpmc_ring_get_commit(ring, &index);
    UNIT_TEST_ASSERT(got_val == exp_val);
    exp_val++;

    // printf("Loop = %d, elems = %d, get_pos = %u, put_pos = %u\n",
    //        i, mpmc_ring_elements(ring), ring->get_pos, ring->put_pos);
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == mpmc_ring_size(ring) - GET_COUNT - i - 1);
  }
  is_success = mpmc_ring_get_begin(ring, &index);
  UNIT_TEST_ASSERT(!is_success);
  
  UNIT_TEST_END();
}

static void
test_drain255(unit_test_t *utp, mpmc_ring_t *ring)
{
  mpmc_ring_index_t index;
  int i;
  int is_success;
  int vals[128];
  int got;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(ring);

  for(i = 0 ; i < 255 ; i++) {
    is_success = mpmc_ring_put_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = 77 + i;
    mpmc_ring_put_commit(ring, &index);

    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == 1);

    is_success = mpmc_ring_get_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    got = vals[index.i];
    mpmc_ring_get_commit(ring, &index);
    UNIT_TEST_ASSERT(got == 77 + i);
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == 0);
  }

  // this should return failure immediately (without blocking)
  is_success = mpmc_ring_get_begin(ring, &index);
  UNIT_TEST_ASSERT(!is_success);
  
  UNIT_TEST_END();
}

static void
test_full_at_wrapped0(unit_test_t *utp, mpmc_ring_t *ring)
{
  mpmc_ring_index_t index;
  int i;
  int is_success;
  int vals[128];
  int got;
  
  UNIT_TEST_BEGIN();

  mpmc_ring_init(ring);

  for(i = 0 ; i < 256 - (int)(mpmc_ring_size(ring)) ; i++) {
    is_success = mpmc_ring_put_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = 77 + i;
    mpmc_ring_put_commit(ring, &index);

    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == 1);

    is_success = mpmc_ring_get_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    got = vals[index.i];
    mpmc_ring_get_commit(ring, &index);
    UNIT_TEST_ASSERT(got == 77 + i);
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == 0);
  }

  for(i = 0 ; i < (int)(mpmc_ring_size(ring)) ; i++) {
    is_success = mpmc_ring_put_begin(ring, &index);
    UNIT_TEST_ASSERT(is_success);
    vals[index.i] = 888;
    mpmc_ring_put_commit(ring, &index);
    UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == i + 1);
  }

  // this should return failure immediately (without blocking)
  is_success = mpmc_ring_put_begin(ring, &index);
  UNIT_TEST_ASSERT(!is_success);

  UNIT_TEST_ASSERT(mpmc_ring_elements(ring) == mpmc_ring_size(ring));
  
  UNIT_TEST_END();
}

/*****************************************************************/

MPMC_RING(ring2, 2);
MPMC_RING(ring32, 32);
MPMC_RING(ring64, 64);

UTEST_RINGS(test_init_get, "Init and get");
UTEST_RINGS(test_put_get, "Put and get");
UTEST_RINGS(test_drain255, "Drain at pos 255");
UTEST_RINGS(test_full_at_wrapped0, "Full at wrapped pos 0");

PROCESS_THREAD(test_process, ev, data)
{
  PROCESS_BEGIN();
  printf("Run unit-test\n");
  printf("---\n");

  UTEST_RING_RUN(test_init_get);
  UTEST_RING_RUN(test_put_get);
  UTEST_RING_RUN(test_drain255);
  UTEST_RING_RUN(test_full_at_wrapped0);

  printf("=check-me= DONE\n");
  PROCESS_END();
}
