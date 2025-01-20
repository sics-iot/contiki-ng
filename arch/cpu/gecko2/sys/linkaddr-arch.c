/*
 * Copyright (C) 2020 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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

/**
 * \addtogroup gecko
 * @{
 *
 * \addtogroup gecko-sys System drivers
 * @{
 *
 * \addtogroup gecko-linkaddr Link Address driver
 * @{
 *
 * \file
 *         Link address implementation for the gecko.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 */
#include "contiki.h"

#include "linkaddr.h"
#include "linkaddr-arch.h"

#include "em_system.h"
#include <string.h>
/*---------------------------------------------------------------------------*/
static inline uint64_t
swap_long(uint64_t val)
{
  val = (val & 0x00000000FFFFFFFF) << 32 | (val & 0xFFFFFFFF00000000) >> 32;
  val = (val & 0x0000FFFF0000FFFF) << 16 | (val & 0xFFFF0000FFFF0000) >> 16;
  val = (val & 0x00FF00FF00FF00FF) << 8 | (val & 0xFF00FF00FF00FF00) >> 8;
  return val;
}
/*---------------------------------------------------------------------------*/
void
populate_link_address(void)
{
  uint64_t system_number;
  uint64_t system_number_net_order;

  system_number = SYSTEM_GetUnique();
  system_number_net_order = swap_long(system_number);

  memcpy(linkaddr_node_addr.u8, &system_number_net_order, LINKADDR_SIZE);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
