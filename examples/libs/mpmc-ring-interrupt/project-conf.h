#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/*---------------------------------------------------------------------------*/

/*
 * The consumers start when the number of elements in the queue
 * reaches this value.
 */
#define START_GET_NUM 0

/*
 * A producer puts one item per `PUT_INTERVAL` attempts. That is, if
 * you set it to a big value, the producer becomes slow to produce a
 * new value. This parameter, together with `GET_INTERVAL`, can be
 * useful to test the cases where the traffic volume is unbalanced
 * between producers and consumers.
 */
#define PUT_INTERVAL 1

/*
 * Same as `PUT_INTERVAL` but for consumers.
 */
#define GET_INTERVAL 1

/*
 * Length of the message queue.
 */
#define QUEUE_LEN 64

/*
 * Number of get or put trials done in a single call to the
 * interrupt handler.
 */
#define TRY_PER_INTERRUPT 5

/*---------------------------------------------------------------------------*/

/*
 * Number of messages that the normal producer puts.
 */
#define NORMAL_PUT_NUM 2500

/*
 * Number of messages that the interrupt producer puts.
 */
#define INTERRUPT_PUT_NUM 1500

/*
 * Number of messages that the normal consumer gets.
 */
#define NORMAL_GET_NUM 2500

/*
 * Number of messages that the interrupt consumer gets.
 */
#define INTERRUPT_GET_NUM 1500

#if NORMAL_PUT_NUM + INTERRUPT_PUT_NUM != NORMAL_GET_NUM + INTERRUPT_GET_NUM
#error Total of PUT_NUM must be equal to the total of GET_NUM
#endif /* NORMAL_PUT_NUM + INTERRUPT_PUT_NUM != NORMAL_GET_NUM + INTERRUPT_GET_NUM */

/*---------------------------------------------------------------------------*/

/*
 * Time interval of interrupts
 */
#define INTERRUPT_RTIMER_INTERVAL (US_TO_RTIMERTICKS(53))

/*
 * Extra delay for moving one message.
 *
 * The higher this value is, the more often that preemption between
 * queue operations happens.
 */
#define MOVE_WAIT_COUNT 100

/*
 * If non-zero, use ringbufindex instead of mpmc-ring for queue
 * implementation. This is to demonstrate danger of using
 * ringbufindex in multi-producer multi-consumer scenarios.
 */
#define USE_RINGBUFINDEX 0

/*
 * Timeout interval in seconds. If the test doesn't finish in this
 * interval, it's considered as a failure.
 */
#define TEST_TIMEOUT 120

/*
 * If non-zero, it prints log messages in the callback for rtimer,
 * that is, in an interrupt context.
 */
#define ENABLE_LOG_IN_RTIMER 0


/*
 * Increase the watchdog timeout for testing.
 */
#if CONTIKI_TARGET_SIMPLELINK && CONTIKI_BOARD_LAUNCHPAD_CC1310
#define WATCHDOG_CONF_TIMEOUT_MS 100000
#endif /* CONTIKI_TARGET_SIMPLELINK && CONTIKI_BOARD_LAUNCHPAD_CC1310 */

#endif /* PROJECT_CONF_H_ */
