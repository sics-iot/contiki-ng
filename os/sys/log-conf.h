/*
* Copyright (c) 2017, Inria.
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
 *         Default log levels for a number of modules
 * \author
 *         Simon Duquennoy <simon.duquennoy@inria.fr>
 */

/** \addtogroup sys
 * @{ */

/** \addtogroup log
* @{ */

#ifndef LOG_CONF_H_
#define LOG_CONF_H_

/* Log only the last 16 bytes of link-layer and IPv6 addresses (or, if)
 * the deployment module is enabled, the node IDs */
#ifdef LOG_CONF_WITH_COMPACT_ADDR
#define LOG_WITH_COMPACT_ADDR LOG_CONF_WITH_COMPACT_ADDR
#else /* LOG_CONF_WITH_COMPACT_ADDR */
#define LOG_WITH_COMPACT_ADDR 0
#endif /* LOG_CONF_WITH_COMPACT_ADDR */

/* Prefix all logs with file name and line-of-code */
#ifdef LOG_CONF_WITH_LOC
#define LOG_WITH_LOC LOG_CONF_WITH_LOC
#else /* LOG_CONF_WITH_LOC */
#define LOG_WITH_LOC 0
#endif /* LOG_CONF_WITH_LOC */

/* Prefix all logs with Module name and logging level */
#ifdef LOG_CONF_WITH_MODULE_PREFIX
#define LOG_WITH_MODULE_PREFIX LOG_CONF_WITH_MODULE_PREFIX
#else /* LOG_CONF_WITH_MODULE_PREFIX */
#define LOG_WITH_MODULE_PREFIX 1
#endif /* LOG_CONF_WITH_MODULE_PREFIX */

/* Cooja annotations */
#ifdef LOG_CONF_WITH_ANNOTATE
#define LOG_WITH_ANNOTATE LOG_CONF_WITH_ANNOTATE
#else /* LOG_CONF_WITH_ANNOTATE */
#define LOG_WITH_ANNOTATE 0
#endif /* LOG_CONF_WITH_ANNOTATE */

/* Custom output function -- default is printf */
#ifdef LOG_CONF_OUTPUT
#define LOG_OUTPUT(...) LOG_CONF_OUTPUT(__VA_ARGS__)
#else /* LOG_CONF_OUTPUT */
#define LOG_OUTPUT(...) printf(__VA_ARGS__)
#endif /* LOG_CONF_OUTPUT */

/* Color the prefix based on the log level. Disabled by default */
#ifdef LOG_CONF_WITH_COLOR
#define LOG_WITH_COLOR LOG_CONF_WITH_COLOR
#else /* LOG_CONF_WITH_COLOR */
#define LOG_WITH_COLOR 0
#endif /* LOG_CONF_WITH_COLOR */

/*
 * Custom output function to prefix logs with level and module.
 *
 * This will only be called when LOG_CONF_WITH_MODULE_PREFIX is enabled and
 * all implementations should be based on LOG_OUTPUT.
 *
 * \param level     The log level
 * \param levelstr  The log level as string
 * \param module    The module string descriptor
 */
#ifdef LOG_CONF_OUTPUT_PREFIX
#define LOG_OUTPUT_PREFIX(level, levelstr, module) LOG_CONF_OUTPUT_PREFIX(level, levelstr, module)
#else /* LOG_CONF_OUTPUT_PREFIX */
#define LOG_OUTPUT_PREFIX(level, levelstr, module) LOG_OUTPUT("[%-4s: %-10s] ", levelstr, module)
#endif /* LOG_CONF_OUTPUT_PREFIX */

/******************************************************************************/
/********************* A list of currently supported modules ******************/
/******************************************************************************/

#ifndef LOG_CONF_LEVEL_RPL
#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_NONE /* Only for rpl-lite */
#endif /* LOG_CONF_LEVEL_RPL */

#ifndef LOG_CONF_LEVEL_TCPIP
#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_TCPIP */

#ifndef LOG_CONF_LEVEL_IPV6
#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_IPV6 */

#ifndef LOG_CONF_LEVEL_6LOWPAN
#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_6LOWPAN */

#ifndef LOG_CONF_LEVEL_NULLNET
#define LOG_CONF_LEVEL_NULLNET                     LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_NULLNET */

#ifndef LOG_CONF_LEVEL_MAC
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_MAC */

#ifndef LOG_CONF_LEVEL_FRAMER
#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_FRAMER */

#ifndef LOG_CONF_LEVEL_6TOP
#define LOG_CONF_LEVEL_6TOP                        LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_6TOP */

#ifndef LOG_CONF_LEVEL_COAP
#define LOG_CONF_LEVEL_COAP                        LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_COAP */

#ifndef LOG_CONF_LEVEL_DTLS
#define LOG_CONF_LEVEL_DTLS                        LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_DTLS */

#ifndef LOG_CONF_LEVEL_SNMP
#define LOG_CONF_LEVEL_SNMP                        LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_SNMP */

#ifndef LOG_CONF_LEVEL_LWM2M
#define LOG_CONF_LEVEL_LWM2M                       LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_LWM2M */

#ifndef LOG_CONF_LEVEL_MAIN
#define LOG_CONF_LEVEL_MAIN                        LOG_LEVEL_INFO
#endif /* LOG_CONF_LEVEL_MAIN */

#ifndef LOG_CONF_LEVEL_SYS
#define LOG_CONF_LEVEL_SYS                         LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_SYS */

#ifndef LOG_CONF_LEVEL_EDHOC
#define LOG_CONF_LEVEL_EDHOC                       LOG_LEVEL_NONE
#endif /* LOG_CONF_LEVEL_EDHOC */

#endif /* LOG_CONF_H_ */

/** @} */
/** @} */
