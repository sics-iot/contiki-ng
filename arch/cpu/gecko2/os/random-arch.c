/*
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
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup gecko
 * @{
 *
 * \addtogroup gecko-os OS drivers
 * @{
 *
 * \addtogroup gecko-random Random driver
 * @{
 *
 * \file
 *         Random driver for the gecko.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdint.h>

#include "rail.h"
#include "em_system.h"

/*---------------------------------------------------------------------------*/
/**
 * @brief Generates a new random number
 * @return The random number.
 */
unsigned short
random_rand(void)
{
  return (unsigned short)rand();
}
/*---------------------------------------------------------------------------*/
/**
 * @brief Initialize the random number generator
 * @param seed Ignored.
 */
void
random_init(unsigned short seed)
{
  (void)seed;
  uint32_t entropy = 0;
  //(void)RAIL_GetRadioEntropy(RAIL_EFR32_HANDLE, (uint8_t *)(&entropy), sizeof(entropy));

  /* If RAIL does not provide an entropy. */
  /* Use system unique as entropy */
  if(entropy == 0) {
    entropy = SYSTEM_GetUnique();
  }

  srand(entropy);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
