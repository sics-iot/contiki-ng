/*
 * Copyright (c) 2024, RISE Research Institutes of Sweden AB (RISE), Stockholm, Sweden
 * Copyright (c) 2020, Industrial Systems Institute (ISI), Patras, Greece
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
 */

/**
 * \file
 *         Cose, heade file for log configuration
 * \author
 *         Lidia Pocero <pocero@isi.gr>
 */

#ifndef _COSE_LOG_H_
#define _COSE_LOG_H_
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sys/log.h"

#ifndef LOG_MODULE
#define LOG_MODULE "COSE"
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_EDHOC
#endif

void cose_print_buff(const uint8_t *buff, size_t len);
void cose_print_char(const uint8_t *buff, size_t len);

#define LOG_COSE_BUFF(level, data, len) do { \
          if(level <= (LOG_LEVEL)) { \
            cose_print_buff(data, len); \
          } \
} while(0)

#define LOG_COSE_STR(level, data, len) do { \
          if(level <= (LOG_LEVEL)) { \
            cose_print_char(data, len); \
          } \
} while(0)

#define LOG_ERR_COSE_BUFF(data, len)  LOG_COSE_BUFF(LOG_LEVEL_ERR, data, len)
#define LOG_WARN_COSE_BUFF(data, len) LOG_COSE_BUFF(LOG_LEVEL_WARN, data, len)
#define LOG_INFO_COSE_BUFF(data, len) LOG_COSE_BUFF(LOG_LEVEL_INFO, data, len)
#define LOG_DBG_COSE_BUFF(data, len)  LOG_COSE_BUFF(LOG_LEVEL_DBG, data, len)

#define LOG_ERR_COSE_STR(data, len)  LOG_COSE_STR(LOG_LEVEL_ERR, data, len)
#define LOG_WARN_COSE_STR(data, len) LOG_COSE_STR(LOG_LEVEL_WARN, data, len)
#define LOG_INFO_COSE_STR(data, len) LOG_COSE_STR(LOG_LEVEL_INFO, data, len)
#define LOG_DBG_COSE_STR(data, len)  LOG_COSE_STR(LOG_LEVEL_DBG, data, len)

static inline void
cose_print_buff_8_dbg(const uint8_t *buff, size_t len)
{
  LOG_DBG_COSE_BUFF(buff, len);
}

static inline void
cose_print_buff_8_info(const uint8_t *buff, size_t len)
{
  LOG_INFO_COSE_BUFF(buff, len);
}

static inline void
cose_print_buff_8_err(const uint8_t *buff, size_t len)
{
  LOG_ERR_COSE_BUFF(buff, len);
}

static inline void
cose_print_char_8_info(const uint8_t *buff, size_t len)
{
  LOG_INFO_COSE_STR(buff, len);
}

static inline void
cose_print_char_8_dbg(const uint8_t *buff, size_t len)
{
  LOG_DBG_COSE_STR(buff, len);
}

#endif /* _COSE_LOG_H_ */
