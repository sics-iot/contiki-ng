/*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
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
 * \file
 *      Collection of default configuration values.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

/**
 * \addtogroup coap
 * @{
 */

#ifndef COAP_CONF_H_
#define COAP_CONF_H_

#include "contiki.h"

/*
 * The maximum buffer size that is provided for resource responses and must be
 * respected due to the limited IP buffer.  Larger data must be handled by the
 * resource and will be sent chunk-wise through a TCP stream or CoAP blocks.
 */
#ifndef COAP_MAX_CHUNK_SIZE
#ifdef REST_MAX_CHUNK_SIZE
#define COAP_MAX_CHUNK_SIZE REST_MAX_CHUNK_SIZE
#else /* REST_MAX_CHUNK_SIZE */
#define COAP_MAX_CHUNK_SIZE     64
#endif /* REST_MAX_CHUNK_SIZE */
#endif /* COAP_MAX_CHUNK_SIZE */

/* Define REST_MAX_CHUNK_SIZE for backward compatibility */
#ifndef REST_MAX_CHUNK_SIZE
#define REST_MAX_CHUNK_SIZE COAP_MAX_CHUNK_SIZE
#endif /* REST_MAX_CHUNK_SIZE */

/* Features that can be disabled to achieve smaller memory footprint */
#ifndef COAP_LINK_FORMAT_FILTERING
#define COAP_LINK_FORMAT_FILTERING     0
#endif /* COAP_LINK_FORMAT_FILTERING */

#ifndef COAP_PROXY_OPTION_PROCESSING
#define COAP_PROXY_OPTION_PROCESSING   0
#endif /* COAP_PROXY_OPTION_PROCESSING */

/* Listening port for the CoAP REST Engine */
#ifndef COAP_SERVER_PORT
#define COAP_SERVER_PORT               COAP_DEFAULT_PORT
#endif /* COAP_SERVER_PORT */

/* The number of concurrent messages that can be stored for retransmission in the transaction layer. */
#ifndef COAP_MAX_OPEN_TRANSACTIONS
#define COAP_MAX_OPEN_TRANSACTIONS     4
#endif /* COAP_MAX_OPEN_TRANSACTIONS */

/* Maximum number of failed request attempts before action */
#ifndef COAP_MAX_ATTEMPTS
#define COAP_MAX_ATTEMPTS              4
#endif /* COAP_MAX_ATTEMPTS */

/* Conservative size limit, as not all options have to be set at the same time. Check when Proxy-Uri option is used */
#ifndef COAP_MAX_HEADER_SIZE    /*     Hdr                  CoF  If-Match         Obs Blo strings   */
#define COAP_MAX_HEADER_SIZE           (4 + COAP_TOKEN_LEN + 3 + 1 + COAP_ETAG_LEN + 4 + 4 + 30)  /* 65 */
#endif /* COAP_MAX_HEADER_SIZE */

/* Number of observer slots */
#ifndef COAP_MAX_OBSERVERS
#define COAP_MAX_OBSERVERS      5 
#endif /* COAP_MAX_OBSERVERS */

/* Interval in notifies in which NON notifies are changed to CON notifies to check client. */
#ifdef COAP_CONF_OBSERVE_REFRESH_INTERVAL
#define COAP_OBSERVE_REFRESH_INTERVAL COAP_CONF_OBSERVE_REFRESH_INTERVAL
#else
#define COAP_OBSERVE_REFRESH_INTERVAL  5
#endif /* COAP_OBSERVE_REFRESH_INTERVAL */

/* Maximal length of observable URL */
#ifdef COAP_CONF_OBSERVER_URL_LEN
#define COAP_OBSERVER_URL_LEN COAP_CONF_OBSERVER_URL_LEN
#else
#define COAP_OBSERVER_URL_LEN 20
#endif

/* Enable the well-known resource (well-known/core) by default */
#ifdef COAP_CONF_WELL_KNOWN_RESOURCE_ENABLED
#define COAP_WELL_KNOWN_RESOURCE_ENABLED  COAP_CONF_WELL_KNOWN_RESOURCE_ENABLED
#else
#define COAP_WELL_KNOWN_RESOURCE_ENABLED  1
#endif

/* Add a human readable message to payload if an error occurs */
#ifdef COAP_CONF_MESSAGE_ON_ERROR
#define COAP_MESSAGE_ON_ERROR   COAP_CONF_MESSAGE_ON_ERROR
#else
#define COAP_MESSAGE_ON_ERROR   1
#endif

#endif /* COAP_CONF_H_ */
/** @} */
