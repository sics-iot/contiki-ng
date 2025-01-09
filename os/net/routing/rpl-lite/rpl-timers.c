/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
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
 */

/**
 * \addtogroup rpl-lite
 * @{
 *
 * \file
 *         RPL timer management.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>,
 * Simon Duquennoy <simon.duquennoy@inria.fr>
 */

#include "contiki.h"
#include "net/routing/rpl-lite/rpl.h"
#include "net/ipv6/uip-sr.h"
#include "net/link-stats.h"
#include "lib/random.h"
#include "sys/ctimer.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "RPL"
#define LOG_LEVEL LOG_LEVEL_RPL

/* A configurable function called after update of the RPL DIO interval */
#ifdef RPL_CALLBACK_NEW_DIO_INTERVAL
void RPL_CALLBACK_NEW_DIO_INTERVAL(clock_time_t dio_interval);
#endif /* RPL_CALLBACK_NEW_DIO_INTERVAL */

#ifdef RPL_PROBING_SELECT_FUNC
rpl_nbr_t *RPL_PROBING_SELECT_FUNC(void);
#endif /* RPL_PROBING_SELECT_FUNC */

#ifdef RPL_PROBING_DELAY_FUNC
clock_time_t RPL_PROBING_DELAY_FUNC(void);
#endif /* RPL_PROBING_DELAY_FUNC */

#define PERIODIC_DELAY_SECONDS     60
#define PERIODIC_DELAY             ((PERIODIC_DELAY_SECONDS) * CLOCK_SECOND)

#define handle_dis_timer rpl_handle_dis_timer
void handle_dis_timer(void *ptr);
static void handle_dio_timer(void *ptr);
static void handle_unicast_dio_timer(void *ptr);
static void send_new_dao(void *ptr);
#if RPL_WITH_DAO_ACK
static void resend_dao(void *ptr);
static void handle_dao_ack_timer(void *ptr);
#endif /* RPL_WITH_DAO_ACK */
#if RPL_WITH_PROBING
static void handle_probing_timer(void *ptr);
#endif /* RPL_WITH_PROBING */
#define handle_periodic_timer rpl_handle_periodic_timer
void handle_periodic_timer(void *ptr);
static void handle_state_update(void *ptr);

/*---------------------------------------------------------------------------*/
static struct ctimer dis_timer; /* Not part of a DAG because when not joined */
static struct ctimer periodic_timer; /* Not part of a DAG because used for general state maintenance */

/*---------------------------------------------------------------------------*/
/*------------------------------- DIS -------------------------------------- */
/*---------------------------------------------------------------------------*/
void
rpl_timers_schedule_periodic_dis(void)
{
  if(ctimer_expired(&dis_timer)) {
    clock_time_t expiration_time = RPL_DIS_INTERVAL / 2 + (random_rand() % (RPL_DIS_INTERVAL));
    ctimer_set(&dis_timer, expiration_time, handle_dis_timer, NULL);
  }
}
/*---------------------------------------------------------------------------*/
void
handle_dis_timer(void *ptr)
{
  if(!rpl_dag_root_is_root() &&
     (!curr_instance.used ||
       curr_instance.dag.preferred_parent == NULL ||
       curr_instance.dag.rank == RPL_INFINITE_RANK)) {
    /* Send DIS and schedule next */
    rpl_icmp6_dis_output(NULL);
    rpl_timers_schedule_periodic_dis();
  }
}
/*---------------------------------------------------------------------------*/
/*------------------------------- DIO -------------------------------------- */
/*---------------------------------------------------------------------------*/
static void
new_dio_interval(void)
{
  uint32_t time;
  clock_time_t ticks;

  time = 1UL << curr_instance.dag.dio_intcurrent;

  /* Convert from milliseconds to CLOCK_TICKS. */
  ticks = (time * CLOCK_SECOND) / 1000;
  curr_instance.dag.dio_next_delay = ticks;

  /* random number between I/2 and I */
  ticks = ticks / 2 + (ticks / 2 * (uint32_t)random_rand()) / RANDOM_RAND_MAX;

  /*
   * The intervals must be equally long among the nodes for Trickle to
   * operate efficiently. Therefore we need to calculate the delay between
   * the randomized time and the start time of the next interval.
   */
  curr_instance.dag.dio_next_delay -= ticks;
  curr_instance.dag.dio_send = 1;
  /* reset the redundancy counter */
  curr_instance.dag.dio_counter = 0;

  /* schedule the timer */
  ctimer_set(&curr_instance.dag.dio_timer, ticks, &handle_dio_timer, NULL);

#ifdef RPL_CALLBACK_NEW_DIO_INTERVAL
  RPL_CALLBACK_NEW_DIO_INTERVAL((CLOCK_SECOND * 1UL << curr_instance.dag.dio_intcurrent) / 1000);
#endif /* RPL_CALLBACK_NEW_DIO_INTERVAL */
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_dio_reset(const char *str)
{
  if(rpl_dag_ready_to_advertise() &&
     (curr_instance.dag.dio_intcurrent == 0 ||
      curr_instance.dag.dio_intcurrent > curr_instance.dio_intmin)) {
    /*
     * don't reset the DIO timer if the current interval is Imin; see
     * Section 4.2, RFC 6206.
     */
    LOG_INFO("reset DIO timer (%s)\n", str);
    if(!rpl_get_leaf_only()) {
        curr_instance.dag.dio_counter = 0;
        curr_instance.dag.dio_intcurrent = curr_instance.dio_intmin;
        new_dio_interval();
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
handle_dio_timer(void *ptr)
{
  if(!rpl_dag_ready_to_advertise()) {
    return; /* We will be scheduled again later */
  }

  if(curr_instance.dag.dio_send) {
    /* send DIO if counter is less than desired redundancy, or if dio_redundancy
    is set to 0, or if we are the root */
    if(rpl_dag_root_is_root() || curr_instance.dio_redundancy == 0 ||
        curr_instance.dag.dio_counter < curr_instance.dio_redundancy) {
#if RPL_TRICKLE_REFRESH_DAO_ROUTES
      if(rpl_dag_root_is_root()) {
        static int count = 0;
        if((count++ % RPL_TRICKLE_REFRESH_DAO_ROUTES) == 0) {
          /* Request new DAO to refresh route. */
          RPL_LOLLIPOP_INCREMENT(curr_instance.dtsn_out);
          LOG_INFO("trigger DAO updates with a DTSN increment (%u)\n", curr_instance.dtsn_out);
        }
      }
#endif /* RPL_TRICKLE_REFRESH_DAO_ROUTES */
      curr_instance.dag.last_advertised_rank = curr_instance.dag.rank;
      rpl_icmp6_dio_output(NULL);
    }
    curr_instance.dag.dio_send = 0;
    ctimer_set(&curr_instance.dag.dio_timer, curr_instance.dag.dio_next_delay, handle_dio_timer, NULL);
  } else {
    /* check if we need to double interval */
    if(curr_instance.dag.dio_intcurrent < curr_instance.dio_intmin + curr_instance.dio_intdoubl) {
      curr_instance.dag.dio_intcurrent++;
    }
    new_dio_interval();
  }
}
/*---------------------------------------------------------------------------*/
/*------------------------------- Unicast DIO ------------------------------ */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
rpl_timers_schedule_unicast_dio(rpl_nbr_t *target)
{
  if(curr_instance.used) {
    curr_instance.dag.unicast_dio_target = target;
    ctimer_set(&curr_instance.dag.unicast_dio_timer, 0,
                  handle_unicast_dio_timer, NULL);
  }
}
/*---------------------------------------------------------------------------*/
static void
handle_unicast_dio_timer(void *ptr)
{
  uip_ipaddr_t *target_ipaddr = rpl_neighbor_get_ipaddr(curr_instance.dag.unicast_dio_target);
  if(target_ipaddr != NULL) {
    rpl_icmp6_dio_output(target_ipaddr);
  }
}
/*---------------------------------------------------------------------------*/
/*------------------------------- DAO -------------------------------------- */
/*---------------------------------------------------------------------------*/
#if RPL_WITH_DAO_ACK
/*---------------------------------------------------------------------------*/
static void
schedule_dao_retransmission(void)
{
  clock_time_t expiration_time = RPL_DAO_RETRANSMISSION_TIMEOUT / 2 + (random_rand() % (RPL_DAO_RETRANSMISSION_TIMEOUT));
  ctimer_set(&curr_instance.dag.dao_timer, expiration_time, resend_dao, NULL);
}
#endif /* RPL_WITH_DAO_ACK */
/*---------------------------------------------------------------------------*/
static void
schedule_dao_refresh(void)
{
  if(curr_instance.used && curr_instance.default_lifetime != RPL_INFINITE_LIFETIME) {
#if RPL_WITH_DAO_ACK
    /* DAO-ACK enabled: the last DAO was ACKed, wait until expiration before refresh */
    clock_time_t target_refresh = CLOCK_SECOND * RPL_LIFETIME(curr_instance.default_lifetime);
#else /* RPL_WITH_DAO_ACK */
    /* DAO-ACK disabled: use half the expiration time to get two chances to refresh per lifetime */
    clock_time_t target_refresh = (CLOCK_SECOND * RPL_LIFETIME(curr_instance.default_lifetime) / 2);
#endif /* RPL_WITH_DAO_ACK */

    /* Send between 60 and 120 seconds before target refresh */
    clock_time_t safety_margin = (60 * CLOCK_SECOND) + (random_rand() % (60 * CLOCK_SECOND));

    if(target_refresh > safety_margin) {
      target_refresh -= safety_margin;
    }

    /* Schedule transmission */
    ctimer_set(&curr_instance.dag.dao_timer, target_refresh, send_new_dao, NULL);
  }
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_schedule_dao(void)
{
  if(curr_instance.used && curr_instance.mop != RPL_MOP_NO_DOWNWARD_ROUTES) {
    /* No need for DAO aggregation delay as per RFC 6550 section 9.5, as this
    * only serves storing mode. Use simple delay instead, with the only purpose
    * to reduce congestion. */
    clock_time_t expiration_time = RPL_DAO_DELAY / 2 + (random_rand() % (RPL_DAO_DELAY));
    ctimer_set(&curr_instance.dag.dao_timer, expiration_time, send_new_dao, NULL);
  }
}
/*---------------------------------------------------------------------------*/
static void
send_new_dao(void *ptr)
{
#if RPL_WITH_DAO_ACK
  /* We are sending a new DAO here. Prepare retransmissions */
  curr_instance.dag.dao_transmissions = 1;
  /* Schedule next retransmission */
  schedule_dao_retransmission();
#else /* RPL_WITH_DAO_ACK */
  /* No DAO-ACK: assume we are reachable as soon as we send a DAO */
  if(curr_instance.dag.state == DAG_JOINED) {
    curr_instance.dag.state = DAG_REACHABLE;
  }
  rpl_timers_dio_reset("Reachable");
  /* There is no DAO-ACK, schedule a refresh. */
  schedule_dao_refresh();
#endif /* !RPL_WITH_DAO_ACK */

  /* Increment seqno */
  RPL_LOLLIPOP_INCREMENT(curr_instance.dag.dao_last_seqno);
  /* Send a DAO with own prefix as target and default lifetime */
  rpl_icmp6_dao_output(curr_instance.default_lifetime);
}
#if RPL_WITH_DAO_ACK
/*---------------------------------------------------------------------------*/
/*------------------------------- DAO-ACK ---------------------------------- */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void
rpl_timers_schedule_dao_ack(uip_ipaddr_t *target, uint16_t sequence)
{
  if(curr_instance.used) {
    uip_ipaddr_copy(&curr_instance.dag.dao_ack_target, target);
    curr_instance.dag.dao_ack_sequence = sequence;
    ctimer_set(&curr_instance.dag.dao_ack_timer, 0, handle_dao_ack_timer, NULL);
  }
}
/*---------------------------------------------------------------------------*/
static void
handle_dao_ack_timer(void *ptr)
{
  rpl_icmp6_dao_ack_output(&curr_instance.dag.dao_ack_target,
    curr_instance.dag.dao_ack_sequence, RPL_DAO_ACK_UNCONDITIONAL_ACCEPT);
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_notify_dao_ack(void)
{
  /* The last DAO was ACKed. Schedule refresh to avoid route expiration. This
  implicitly de-schedules resend_dao, as both share curr_instance.dag.dao_timer */
  schedule_dao_refresh();
}
/*---------------------------------------------------------------------------*/
static void
resend_dao(void *ptr)
{
  /* Increment transmission counter before sending */
  curr_instance.dag.dao_transmissions++;
  /* Send a DAO with own prefix as target and default lifetime */
  rpl_icmp6_dao_output(curr_instance.default_lifetime);

  /* Schedule next retransmission, or abort */
  if(curr_instance.dag.dao_transmissions < RPL_DAO_MAX_RETRANSMISSIONS) {
    schedule_dao_retransmission();
  } else {
    /* No more retransmissions. Perform local repair. */
    rpl_local_repair("DAO max rtx");
    return;
  }
}
#endif /* RPL_WITH_DAO_ACK */
/*---------------------------------------------------------------------------*/
/*------------------------------- Probing----------------------------------- */
/*---------------------------------------------------------------------------*/
#if RPL_WITH_PROBING
clock_time_t
get_probing_delay(void)
{
  return ((RPL_PROBING_INTERVAL) / 2) + random_rand() % (RPL_PROBING_INTERVAL);
}
/*---------------------------------------------------------------------------*/
rpl_nbr_t *
get_probing_target(void)
{
  /* Returns the next probing target. The current implementation probes the urgent
   * probing target if any, or the preferred parent if its link statistics need refresh.
   * Otherwise, it picks at random between:
   * (1) selecting the best neighbor with non-fresh link statistics
   * (2) selecting the least recently updated neighbor
   */

  rpl_nbr_t *nbr;
  rpl_nbr_t *probing_target = NULL;
  rpl_rank_t probing_target_rank = RPL_INFINITE_RANK;
  clock_time_t probing_target_age = 0;
  clock_time_t clock_now = clock_time();

  if(curr_instance.used == 0) {
    return NULL;
  }

  /* There is an urgent probing target */
  if(curr_instance.dag.urgent_probing_target != NULL) {
    return curr_instance.dag.urgent_probing_target;
  }

  /* The preferred parent needs probing */
  if(curr_instance.dag.preferred_parent != NULL && !rpl_neighbor_is_fresh(curr_instance.dag.preferred_parent)) {
    return curr_instance.dag.preferred_parent;
  }

  /* Now consider probing other non-fresh neighbors. With 2/3 proabability,
  pick the best non-fresh. Otherwise, pick the lest recently updated non-fresh. */

  if(random_rand() % 3 != 0) {
    /* Look for best non-fresh */
    nbr = nbr_table_head(rpl_neighbors);
    while(nbr != NULL) {
      if(!rpl_neighbor_is_fresh(nbr)) {
        /* nbr needs probing */
        rpl_rank_t nbr_rank = rpl_neighbor_rank_via_nbr(nbr);
        if(probing_target == NULL
            || nbr_rank < probing_target_rank) {
          probing_target = nbr;
          probing_target_rank = nbr_rank;
        }
      }
      nbr = nbr_table_next(rpl_neighbors, nbr);
    }
  } else {
    /* Look for least recently updated non-fresh */
    nbr = nbr_table_head(rpl_neighbors);
    while(nbr != NULL) {
      if(!rpl_neighbor_is_fresh(nbr)) {
        /* nbr needs probing */
        const struct link_stats *stats = rpl_neighbor_get_link_stats(nbr);
        if(stats != NULL) {
          if(probing_target == NULL
              || clock_now - stats->last_tx_time > probing_target_age) {
            probing_target = nbr;
            probing_target_age = clock_now - stats->last_tx_time;
          }
        }
      }
      nbr = nbr_table_next(rpl_neighbors, nbr);
    }
  }

  return probing_target;
}
/*---------------------------------------------------------------------------*/
static void
handle_probing_timer(void *ptr)
{
  rpl_nbr_t *probing_target = RPL_PROBING_SELECT_FUNC();
  uip_ipaddr_t *target_ipaddr = rpl_neighbor_get_ipaddr(probing_target);

  /* Perform probing */
  if(target_ipaddr != NULL) {
    const struct link_stats *stats = rpl_neighbor_get_link_stats(probing_target);
    (void)stats;
    LOG_INFO("probing ");
    LOG_INFO_6ADDR(target_ipaddr);
    LOG_INFO_(" %s last tx %u min ago\n",
        curr_instance.dag.urgent_probing_target != NULL ? "(urgent)" : "",
        stats != NULL ?
        (unsigned)((clock_time() - stats->last_tx_time) / (60 * CLOCK_SECOND)) : 0
        );
    /* Send probe, e.g. unicast DIO or DIS */
    RPL_PROBING_SEND_FUNC(target_ipaddr);
    /* urgent_probing_target will be NULLed in the packet_sent callback */
  } else {
    LOG_INFO("no neighbor needs probing\n");
  }

  /* Schedule next probing */
  rpl_schedule_probing();
}
/*---------------------------------------------------------------------------*/
void
rpl_schedule_probing(void)
{
  if(curr_instance.used) {
    ctimer_set(&curr_instance.dag.probing_timer, RPL_PROBING_DELAY_FUNC(),
                  handle_probing_timer, NULL);
  }
}
/*---------------------------------------------------------------------------*/
void
rpl_schedule_probing_now(void)
{
  if(curr_instance.used) {
    ctimer_set(&curr_instance.dag.probing_timer,
      random_rand() % (CLOCK_SECOND * 4), handle_probing_timer, NULL);
  }
}
#endif /* RPL_WITH_PROBING */
/*---------------------------------------------------------------------------*/
/*------------------------------- Leaving-- -------------------------------- */
/*---------------------------------------------------------------------------*/
static void
handle_leaving_timer(void *ptr)
{
  if(curr_instance.used) {
    rpl_dag_leave();
  }
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_unschedule_leaving(void)
{
  if(curr_instance.used) {
    if(!ctimer_expired(&curr_instance.dag.leave)) {
      ctimer_stop(&curr_instance.dag.leave);
    }
  }
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_schedule_leaving(void)
{
  if(curr_instance.used) {
    if(ctimer_expired(&curr_instance.dag.leave)) {
      ctimer_set(&curr_instance.dag.leave, RPL_DELAY_BEFORE_LEAVING, handle_leaving_timer, NULL);
    }
  }
}
/*---------------------------------------------------------------------------*/
/*------------------------------- Periodic---------------------------------- */
/*---------------------------------------------------------------------------*/
void
rpl_timers_init(void)
{
  ctimer_set(&periodic_timer, PERIODIC_DELAY, handle_periodic_timer, NULL);
  rpl_timers_schedule_periodic_dis();
}
/*---------------------------------------------------------------------------*/
void
handle_periodic_timer(void *ptr)
{
  if(curr_instance.used) {
    rpl_dag_periodic(PERIODIC_DELAY_SECONDS);
    uip_sr_periodic(PERIODIC_DELAY_SECONDS);
  }

  if(!curr_instance.used ||
      curr_instance.dag.preferred_parent == NULL ||
      curr_instance.dag.rank == RPL_INFINITE_RANK) {
    rpl_timers_schedule_periodic_dis(); /* Schedule DIS if needed */
  }

  /* Useful because part of the state update is time-dependent, e.g.,
  the meaning of last_advertised_rank changes with time */
  rpl_dag_update_state();

  if(LOG_INFO_ENABLED) {
    rpl_neighbor_print_list("Periodic");
    rpl_dag_root_print_links("Periodic");
  }

  ctimer_reset(&periodic_timer);
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_stop_dag_timers(void)
{
  /* Stop all timers related to the DAG */
  ctimer_stop(&curr_instance.dag.state_update);
  ctimer_stop(&curr_instance.dag.leave);
  ctimer_stop(&curr_instance.dag.dio_timer);
  ctimer_stop(&curr_instance.dag.unicast_dio_timer);
  ctimer_stop(&curr_instance.dag.dao_timer);
#if RPL_WITH_PROBING
  ctimer_stop(&curr_instance.dag.probing_timer);
#endif /* RPL_WITH_PROBING */
#if RPL_WITH_DAO_ACK
  ctimer_stop(&curr_instance.dag.dao_ack_timer);
#endif /* RPL_WITH_DAO_ACK */
}
/*---------------------------------------------------------------------------*/
void
rpl_timers_schedule_state_update(void)
{
  if(curr_instance.used) {
    ctimer_set(&curr_instance.dag.state_update, 0, handle_state_update, NULL);
  }
}
/*---------------------------------------------------------------------------*/
static void
handle_state_update(void *ptr)
{
  rpl_dag_update_state();
}

/** @}*/
