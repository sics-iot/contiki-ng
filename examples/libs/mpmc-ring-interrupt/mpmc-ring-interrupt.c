/*
 * Copyright (C) 2020, Toshiba Corporation
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
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "dev/watchdog.h"
#include "lib/mpmc-ring.h"
#include "lib/ringbufindex.h"
#include "lib/random.h"
#include "sys/timer.h"
#include "sys/rtimer.h"
#include "sys/etimer.h"
#include "sys/clock.h"
#include "sys/critical.h"
#include "sys/node-id.h"

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_MODULE "mpmc"
#include "sys/log.h"

/*---------------------------------------------------------------------------*/

#if ENABLE_LOG_IN_RTIMER
#define RLOG_DBG(...) LOG_DBG(__VA_ARGS__)
#define RLOG_INFO(...) LOG_INFO(__VA_ARGS__)
#else /* ENABLE_LOG_IN_RTIMER */
#define RLOG_DBG(...)
#define RLOG_INFO(...)
#endif /* ENABLE_LOG_IN_RTIMER */


/*---------------------------------------------------------------------------*/
/*
 * Abstraction of the queue
 */

#if USE_RINGBUFINDEX
typedef struct ringbufindex the_queue_t;
typedef uint8_t the_index_t;
#else /* USE_RINGBUFINDEX */
typedef mpmc_ring_t the_queue_t;
typedef mpmc_ring_index_t the_index_t;
#endif /* USE_RINGBUFINDEX */

static inline void the_queue_init(the_queue_t *q, int s);
static inline int the_queue_put_begin(the_queue_t *q, the_index_t *got);
static inline int the_queue_put_commit(the_queue_t *q, const the_index_t *i);
static inline int the_queue_get_begin(the_queue_t *q, the_index_t *got);
static inline int the_queue_get_commit(the_queue_t *q, const the_index_t *i);
static inline int the_queue_index_get(const the_index_t *i);
static inline int the_queue_elements(the_queue_t *q);

#if USE_RINGBUFINDEX
static inline void the_queue_init(the_queue_t *q, int s) { ringbufindex_init(q,(uint8_t)s); }
static inline int the_queue_put_begin(the_queue_t *q, the_index_t *got)
{
  int ret = ringbufindex_peek_put(q);
  if(ret < 0) {
    return 0;
  } else {
    *got = ret;
    return 1;
  }
}
static inline int the_queue_put_commit(the_queue_t *q, const the_index_t *i) { return ringbufindex_put(q); }
static inline int the_queue_get_begin(the_queue_t *q, the_index_t *got)
{
  int ret = ringbufindex_peek_get(q);
  if(ret < 0) {
    return 0;
  } else {
    *got = ret;
    return 1;
  }
}
static inline int the_queue_get_commit(the_queue_t *q, const the_index_t *i) { return (ringbufindex_get(q) >= 0); }
static inline int the_queue_index_get(const the_index_t *i) { return *i; }
static inline int the_queue_elements(the_queue_t *q) { return ringbufindex_elements(q); }
#else /* USE_RINGBUFINDEX */
static inline void the_queue_init(the_queue_t *q, int s) { mpmc_ring_init(q); }
static inline int the_queue_put_begin(the_queue_t *q, the_index_t *got) { return mpmc_ring_put_begin(q,got); }
static inline int the_queue_put_commit(the_queue_t *q, const the_index_t *i) { mpmc_ring_put_commit(q,i); return 1; }
static inline int the_queue_get_begin(the_queue_t *q, the_index_t *got) { return mpmc_ring_get_begin(q,got); }
static inline int the_queue_get_commit(the_queue_t *q, const the_index_t *i) { mpmc_ring_get_commit(q,i); return 1; }
static inline int the_queue_index_get(const the_index_t *i) { return i->i; }
static inline int the_queue_elements(the_queue_t *q) { return mpmc_ring_elements(q); }
#endif /* USE_RINGBUFINDEX */


/*---------------------------------------------------------------------------*/
typedef uint16_t msg_t;

/**
 * Deliberately take time to move a message from source to dest. It
 * copies the message one bit at a time.
 */
static inline
void msg_move(msg_t *dest, const msg_t *source)
{
  int bit_index;
  int wait_max = MOVE_WAIT_COUNT + (MOVE_WAIT_COUNT * random_rand() / RANDOM_RAND_MAX);
  *dest = 0;
  for(bit_index = 0 ; bit_index < sizeof(msg_t) * 8 ; bit_index++) {
    msg_t mask = 1 << bit_index;
    int wait;
    *dest = (*dest & (~mask)) | (*source & mask);
    for(wait = 0 ; wait < wait_max ; wait++) {
      /* emulate long access time... */
      ;
    }
  }
}

/*---------------------------------------------------------------------------*/
/**
 * A producer of the queue. It generates messages sequentially.
 */
struct message_gen {
  msg_t next;
  uint32_t generated_num;
  uint32_t count_queue_full;
};

static void
message_gen_init(struct message_gen *gen, msg_t init_msg)
{
  gen->next = init_msg;
  gen->generated_num = 0;
  gen->count_queue_full = 0;
}

static msg_t
message_gen_next(struct message_gen *gen)
{
  msg_t n = gen->next;
  gen->next++;
  gen->generated_num++;
  return n;
}
/*---------------------------------------------------------------------------*/
/**
 * A consumer of the queue. It stores messages obtained from the
 * queue.
 */
struct message_store {
  msg_t *storage;
  uint32_t current_num;
  uint32_t count_queue_drain;
};

static void
message_store_init(struct message_store *store, msg_t *storage_impl)
{
  store->storage = storage_impl;
  store->current_num = 0;
  store->count_queue_drain = 0;
}

static void
message_store_input(struct message_store *store, const msg_t *val)
{
  msg_move(&store->storage[store->current_num], val);
  store->current_num++;
}

/*---------------------------------------------------------------------------*/
/**
 * Controller of a producer or consumer. it regulates the speed of
 * the message stream, and keeps track of the time when the stream
 * started and finished.
 */
struct stream_control {
  int finished;
  int try_counter;
  int try_interval;
  clock_time_t start_time;
  clock_time_t finish_time;
};

static void
stream_control_init(volatile struct stream_control *sc, int finished, int try_interval)
{
  sc->finished = finished;
  sc->try_counter = 0;
  sc->try_interval = try_interval;
  sc->start_time = clock_time();
  sc->finish_time = 0;
}

/**
 * \return Non-zero if a producer or consumer can process a
 * message. Zero otherwise.
 */
static int
stream_control_try(volatile struct stream_control *sc)
{
  sc->try_counter++;
  return (sc->try_counter % sc->try_interval == 0);
}

static void
stream_control_finish(volatile struct stream_control *sc)
{
  sc->finished = 1;
  sc->finish_time = clock_time();
}

static inline int
stream_control_is_finished(volatile struct stream_control *sc)
{
  return sc->finished;
}

static inline clock_time_t
stream_control_duration(volatile struct stream_control *sc)
{
  return sc->finish_time - sc->start_time;
}

/*---------------------------------------------------------------------------*/
/**
 * The memory region for messages in the queue.
 */
static msg_t queue[QUEUE_LEN];
#if USE_RINGBUFINDEX
static struct ringbufindex queue_r;
#else /* USE_RINGBUFINDEX */
MPMC_RING(queue_r, QUEUE_LEN);
#endif /* USE_RINGBUFINDEX */

#define MESSAGE_POOL_SIZE (NORMAL_GET_NUM + INTERRUPT_GET_NUM)

/**
 * The memory region to store messages obtained from the queue. It's
 * divided into two, and assigned to nornal_store and
 * interrupt_store.
 */
static msg_t message_pool[MESSAGE_POOL_SIZE];

/**
 * The consumer in normal context.
 */
static struct message_store normal_store;

/**
 * The consumer in interrupt context.
 */
static struct message_store interrupt_store;

/**
 * The producer in normal context.
 */
static struct message_gen normal_put_gen;

/**
 * The producer in interrupt context.
 */
static struct message_gen interrupt_put_gen;

/**
 * Controls the role of task_rtimer. If non-zero, task_rtimer puts
 * a message. Otherwise it gets a message.
 */
static int do_put_in_rtimer;

/**
 * Controls the start time of the consumers. If non-zero, the
 * consumers work.
 */
static int enable_get;

/* stream controllers for the producers and consumers */
static volatile struct stream_control sc_normal_put;
static volatile struct stream_control sc_normal_get;
static volatile struct stream_control sc_interrupt_put;
static volatile struct stream_control sc_interrupt_get;

/*---------------------------------------------------------------------------*/
/**
 * Put a message from the producer to the queue. If the queue is
 * full, it does nothing.
 *
 * \return Non-zero if it successfully puts a message. Zero
 * otherwise.
 */
static inline
int do_put(struct message_gen *gen)
{
  the_index_t i;
  msg_t next;
  if(!the_queue_put_begin(&queue_r, &i)) {
    gen->count_queue_full++;
    return 0;
  }
  next = message_gen_next(gen);
  msg_move(&queue[the_queue_index_get(&i)], &next);
  return the_queue_put_commit(&queue_r, &i);
}

/**
 * Get a message from the queue to the consumer. If the queue is
 * empty, it does nothing.
 *
 * \return Non-zero if it successfully gets a message. Zero
 * otherwise.
 */
static inline
int do_get(struct message_store *store)
{
  the_index_t i;
  if(!the_queue_get_begin(&queue_r, &i)) {
    store->count_queue_drain++;
    return 0;
  }
  message_store_input(store, &queue[the_queue_index_get(&i)]);
  return the_queue_get_commit(&queue_r, &i);
}
/*---------------------------------------------------------------------------*/

static void task_rtimer(struct rtimer *rt, void *data);

static void
init_states(void)
{
  message_store_init(&normal_store, message_pool);
  message_store_init(&interrupt_store, message_pool + NORMAL_GET_NUM);
  message_gen_init(&normal_put_gen, 0);
  message_gen_init(&interrupt_put_gen, NORMAL_PUT_NUM);
  do_put_in_rtimer = 0;
  enable_get = (START_GET_NUM == 0);
  stream_control_init(&sc_normal_put, (NORMAL_PUT_NUM == 0), PUT_INTERVAL);
  stream_control_init(&sc_normal_get, (NORMAL_GET_NUM == 0), GET_INTERVAL);
  stream_control_init(&sc_interrupt_put, (INTERRUPT_PUT_NUM == 0), PUT_INTERVAL);
  stream_control_init(&sc_interrupt_get, (INTERRUPT_GET_NUM == 0), GET_INTERVAL);
  memset(message_pool, 0xFF, sizeof(message_pool));
  memset(queue, 0, sizeof(queue));
  the_queue_init(&queue_r, QUEUE_LEN);
}

static void
schedule_rtimer(void)
{
  static struct rtimer rt;
  rtimer_set(&rt, RTIMER_NOW() + INTERRUPT_RTIMER_INTERVAL, 0, &task_rtimer, NULL);
}

static void
task_rtimer(struct rtimer *rt, void *data)
{
  /*
   * Run the producer or consumer alternately.
   */
  int try_i;
  for(try_i = 0 ; try_i < TRY_PER_INTERRUPT ; try_i++) {
    if(do_put_in_rtimer) {
      if(interrupt_put_gen.generated_num < INTERRUPT_PUT_NUM) {
        if(stream_control_try(&sc_interrupt_put) && do_put(&interrupt_put_gen)) {
          RLOG_DBG("Interrupt put\n");
        }
        if(interrupt_put_gen.generated_num == INTERRUPT_PUT_NUM) {
          stream_control_finish(&sc_interrupt_put);
          RLOG_INFO("Interrupt put done\n");
        }
      }
    } else if(enable_get) {
      if(interrupt_store.current_num < INTERRUPT_GET_NUM) {
        if(stream_control_try(&sc_interrupt_get) && do_get(&interrupt_store)) {
          RLOG_DBG("Interrupt get\n");
        }
        if(interrupt_store.current_num == INTERRUPT_GET_NUM) {
          stream_control_finish(&sc_interrupt_get);
          RLOG_INFO("Interrupt get done\n");
        }
      }
    }
    do_put_in_rtimer = !do_put_in_rtimer;
    if(stream_control_is_finished(&sc_interrupt_put)
       && stream_control_is_finished(&sc_interrupt_get)) {
      RLOG_INFO("Finished rtimer\n");
      return;
    }
  }
  schedule_rtimer();
}
/*---------------------------------------------------------------------------*/
static int
message_comparator(const void *a, const void *b)
{
  const msg_t *msg_a = (const msg_t *)a;
  const msg_t *msg_b = (const msg_t *)b;
  return
    (*msg_a < *msg_b) ? -1
    : (*msg_a > *msg_b) ? 1
    : 0;
}

/**
 * Verify the test result.
 *
 * \return Non-zero if it's ok. Zero otherwise.
 */
static int
check_result(void)
{
  int index = 0;
  int is_ok = 1;
  int expected_diff = 0;
  qsort(message_pool, MESSAGE_POOL_SIZE, sizeof(msg_t), message_comparator);
  for(index = 0 ; index < NORMAL_GET_NUM + INTERRUPT_GET_NUM ; index++) {
    int got = (int)message_pool[index];
    if(got != index) {
      int got_diff = got - index;
      is_ok = 0;
      if(got_diff != expected_diff) {
        /* If it finds one element misplaced, basically the
         * following elements are also misplaced. To suppress logs,
         * we show an error message only when the difference
         * changes.
         */
        LOG_ERR("message_pool[%d] = %d (should be %d, difference is %d)\n", index, got, index, got_diff);
        expected_diff = got_diff;
      }
    }
    watchdog_periodic();
  }
  for( ; index < MESSAGE_POOL_SIZE ; index++) {
    if(message_pool[index] != 0xFFFF) {
      is_ok = 0;
      LOG_ERR("message_pool[%d] = %u (should be %u)\n", index, message_pool[index], 0xFFFF);
    }
    watchdog_periodic();
  }
  if(the_queue_elements(&queue_r) != 0) {
    is_ok = 0;
    LOG_ERR("%d elements still in the queue (should be 0)\n", the_queue_elements(&queue_r));
  }
  LOG_INFO("Check result: %s\n", is_ok ? "OK" : "NG");
  LOG_INFO("Get num: normal:%lu interrupt:%lu\n", normal_store.current_num, interrupt_store.current_num);
  LOG_INFO("Put num: normal:%lu interrupt:%lu\n", normal_put_gen.generated_num, interrupt_put_gen.generated_num);
  LOG_INFO("Queue full:  normal:%lu interrupt:%lu\n", normal_put_gen.count_queue_full, interrupt_put_gen.count_queue_full);
  LOG_INFO("Queue drain: normal:%lu interrupt:%lu\n", normal_store.count_queue_drain, interrupt_store.count_queue_drain);
  LOG_INFO("Get duration: normal:%ld interrupt:%ld\n", (int32_t)stream_control_duration(&sc_normal_get), (int32_t)stream_control_duration(&sc_interrupt_get));
  LOG_INFO("Put duration: normal:%ld interrupt:%ld\n", (int32_t)stream_control_duration(&sc_normal_put), (int32_t)stream_control_duration(&sc_interrupt_put));
  return is_ok;
}
/*---------------------------------------------------------------------------*/
PROCESS(normal_put, "normal PUT process");
PROCESS_THREAD(normal_put, ev, data)
{
  PROCESS_BEGIN();
  while(!stream_control_is_finished(&sc_normal_put)) {
    PROCESS_PAUSE();
    if(normal_put_gen.generated_num < NORMAL_PUT_NUM) {
      if(stream_control_try(&sc_normal_put) && do_put(&normal_put_gen)) {
        LOG_DBG("Normal put\n");
        // check_intermediate_integrity();
      }
      if(normal_put_gen.generated_num == NORMAL_PUT_NUM) {
        stream_control_finish(&sc_normal_put);
        LOG_INFO("Normal put done\n");
      }
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS(normal_get, "normal GET process");
PROCESS_THREAD(normal_get, ev, data)
{
  PROCESS_BEGIN();
  while(!stream_control_is_finished(&sc_normal_get)) {
    PROCESS_PAUSE();
    if(!enable_get) {
      continue;
    }
    if(normal_store.current_num < NORMAL_GET_NUM) {
      if(stream_control_try(&sc_normal_get) && do_get(&normal_store)) {
        LOG_DBG("Normal get\n");
        // check_intermediate_integrity();
      }
      if(normal_store.current_num == NORMAL_GET_NUM) {
        stream_control_finish(&sc_normal_get);
        LOG_INFO("Normal get done\n");
      }
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS(main_process, "the main process");
AUTOSTART_PROCESSES(&main_process);
PROCESS_THREAD(main_process, ev, data)
{
  static struct etimer et_clean_up;
  static struct timer test_timeout;
  static uint16_t run_count = 0;
  static int break_with_fail;

  PROCESS_BEGIN();
  random_init(node_id);
  LOG_INFO("mpmc-ring-interrupt started.\n");

  while(1) {
    LOG_INFO("run_count = %u\n", run_count);
    LOG_INFO("START_GET_NUM = %d\n", START_GET_NUM);
    LOG_INFO("PUT_INTERVAL = %d\n", PUT_INTERVAL);
    LOG_INFO("GET_INTERVAL = %d\n", GET_INTERVAL);
    LOG_INFO("QUEUE_LEN = %d\n", QUEUE_LEN);
    LOG_INFO("TRY_PER_INTERRUPT = %d\n", TRY_PER_INTERRUPT);
    LOG_INFO("NORMAL_PUT_NUM = %d\n", NORMAL_PUT_NUM);
    LOG_INFO("INTERRUPT_PUT_NUM = %d\n", INTERRUPT_PUT_NUM);
    LOG_INFO("NORMAL_GET_NUM = %d\n", NORMAL_GET_NUM);
    LOG_INFO("INTERRUPT_GET_NUM = %d\n", INTERRUPT_GET_NUM);
    LOG_INFO("INTERRUPT_RTIMER_INTERVAL = %ld\n", INTERRUPT_RTIMER_INTERVAL);
    LOG_INFO("USE_RINGBUFINDEX = %d\n", USE_RINGBUFINDEX);
    LOG_INFO("TEST_TIMEOUT = %d\n", TEST_TIMEOUT);
    break_with_fail = 0;
    timer_set(&test_timeout, TEST_TIMEOUT * CLOCK_SECOND);
    init_states();
    schedule_rtimer();
    process_start(&normal_get, NULL);
    process_start(&normal_put, NULL);
    PROCESS_PAUSE();
  
    while(1) {
      PROCESS_PAUSE();
      if(timer_expired(&test_timeout)) {
        break_with_fail = 1;
        LOG_ERR("Test timeout.\n");
        break;
      }
      if(!enable_get && the_queue_elements(&queue_r) >= START_GET_NUM) {
        enable_get = 1;
      }
      if(stream_control_is_finished(&sc_interrupt_put) && stream_control_is_finished(&sc_normal_put)) {
        if(stream_control_is_finished(&sc_interrupt_get) && stream_control_is_finished(&sc_normal_get)) {
          /* PUT and GET all finished. This is the normal end condition. */
          break;
        }
      } else if(stream_control_is_finished(&sc_interrupt_get) && stream_control_is_finished(&sc_normal_get)) {
        break_with_fail = 1;
        LOG_ERR("Consumers finished, but producers are not finished yet.\n");
        break;
      }
    }
    if(!check_result() || break_with_fail) {
      LOG_INFO("Now resetting..\n");
      watchdog_reboot();
    }
    LOG_INFO("Stopping the test.\n");
    stream_control_finish(&sc_normal_put);
    stream_control_finish(&sc_normal_get);
    stream_control_finish(&sc_interrupt_put);
    stream_control_finish(&sc_interrupt_get);
    etimer_set(&et_clean_up, CLOCK_SECOND * 5);
    PROCESS_WAIT_UNTIL(etimer_expired(&et_clean_up));
    PROCESS_WAIT_UNTIL(!process_is_running(&normal_get));
    PROCESS_WAIT_UNTIL(!process_is_running(&normal_put));
    LOG_INFO("The test stopped.\n");
    run_count++;
  }
  PROCESS_END();
}
