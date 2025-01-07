/*
 * Copyright (c) 2018, RISE SICS
 * Copyright (C) 2022 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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

#include "contiki.h"

#include "efr32-radio-buffer.h"
/* Allocate 4 rx buffers for bursty receives */
#define RX_BUF_COUNT 4

rx_buffer_t rx_buf[RX_BUF_COUNT];
volatile int next_read = 0;
volatile int next_write = 0;

int
has_packet(void)
{
  return rx_buf[next_read].len > 0;
}
rx_buffer_t *
get_full_rx_buf(void)
{
  int nr;
  if(rx_buf[nr = next_read].len > 0) {
    /* return buffert and intrease last_read as it was full. */
    /* Write will have to check if it can allocate new buffers when
       increating the write pos */
    next_read = (next_read + 1) % RX_BUF_COUNT;
    return &rx_buf[nr];
  }
  /* nothing to read */
  return NULL;
}
rx_buffer_t *
get_empty_rx_buf(void)
{
  int nw;
  /* if the next write buf is empty is should be ok to write to it */
  if(rx_buf[nw = next_write].len == 0) {
    next_write = (next_write + 1) % RX_BUF_COUNT;
    return &rx_buf[nw];
  }
  /* Full - nothing to write... */
  return NULL;
}
void
free_rx_buf(rx_buffer_t *rx_buf)
{
  /* set len to zero */
  rx_buf->len = 0;
}
