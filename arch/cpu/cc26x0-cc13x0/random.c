/*
 * Copyright (c) 2016, University of Bristol - http://www.bristol.ac.uk
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
 * \defgroup cc13x0-cc26x0-prng Pseudo Random Number Generator (PRNG) for CC13x0/CC26x0.
 * @{
 *
 * This file overrides os/lib/random.c. Note that the file name must
 * match the original file for the override to work.
 *
 * \file
 *        Implementation of Pseudo Random Number Generator for CC13x0/CC26x0.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "lib/smallprng.h"
/*---------------------------------------------------------------------------*/
static smallprng_t smallprng;
/*---------------------------------------------------------------------------*/
/**
 * \brief   Generates a new random number using smallprng.
 * \return  The random number.
 */
unsigned short
random_rand(void)
{
  return (unsigned short)smallprng_rand(&smallprng);
}
/*---------------------------------------------------------------------------*/
/**
 * \brief       Initialize the PRNG.
 * \param seed  Seed for the PRNG.
 */
void
random_init(unsigned short seed)
{
  smallprng_init(&smallprng, seed);
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 */
