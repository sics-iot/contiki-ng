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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "contiki.h"
#include "unit-test.h"
#include "sys/process-mutex.h"
#include <stdio.h>

static process_mutex_t mutex;
PROCESS(contender_1, "contender_1");
PROCESS(contender_2, "contender_2");
PROCESS(test_process, "test");
AUTOSTART_PROCESSES(&test_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(contender_1, ev, data)
{
  PROCESS_BEGIN();

  PROCESS_WAIT_UNTIL(process_mutex_try_lock(&mutex));
  process_mutex_unlock(&mutex);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(contender_2, ev, data)
{
  PROCESS_BEGIN();

  PROCESS_WAIT_UNTIL(process_mutex_try_lock(&mutex));
  process_mutex_unlock(&mutex);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(process_mutex_one_contender, "one contender");
UNIT_TEST(process_mutex_one_contender)
{
  UNIT_TEST_BEGIN();

  process_mutex_init(&mutex);
  PT_WAIT_UNTIL(&unit_test_pt, process_mutex_try_lock(&mutex));
  process_start(&contender_1, NULL);
  UNIT_TEST_ASSERT(process_is_running(&contender_1));
  process_mutex_unlock(&mutex);
  PT_YIELD_UNTIL(&unit_test_pt, !process_is_running(&contender_1));

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(process_mutex_two_contenders, "two contenders");
UNIT_TEST(process_mutex_two_contenders)
{
  UNIT_TEST_BEGIN();

  process_mutex_init(&mutex);
  PT_WAIT_UNTIL(&unit_test_pt, process_mutex_try_lock(&mutex));
  process_start(&contender_1, NULL);
  UNIT_TEST_ASSERT(process_is_running(&contender_1));
  process_start(&contender_2, NULL);
  UNIT_TEST_ASSERT(process_is_running(&contender_2));
  process_mutex_unlock(&mutex);
  PT_YIELD_UNTIL(&unit_test_pt, !process_is_running(&contender_1));
  PT_WAIT_UNTIL(&unit_test_pt, !process_is_running(&contender_2));

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(test_process, ev, data)
{
  PROCESS_BEGIN();

  printf("Run unit-test\n");
  printf("---\n");

  UNIT_TEST_RUN(process_mutex_one_contender);
  UNIT_TEST_RUN(process_mutex_two_contenders);

  if(!UNIT_TEST_PASSED(process_mutex_one_contender)
      || !UNIT_TEST_PASSED(process_mutex_two_contenders)) {
    printf("=check-me= FAILED\n");
    printf("---\n");
  }

  printf("=check-me= DONE\n");
  printf("---\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
