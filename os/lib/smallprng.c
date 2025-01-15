/*
 * Copyright (c) 2022, Andreas Urke
 * Copyright (c) 2018, Texas Instruments Incorporated - http://www.ti.com/
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

/** \addtogroup smallprng
 * @{
 */

/**
 * \file
 *        Implementation of small non-cryptographic random number generator
 *        based on Edvard Pettersens random.c for simplelink platform,
 *        which was based on Bob Jenkins' small non-cryptographic PRNG:
 *        http://burtleburtle.net/bob/rand/smallprng.html
 *
 * \author
 *        Andreas Urke
 */

#include "lib/smallprng.h"
/*---------------------------------------------------------------------------*/
#define rot32(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
/*---------------------------------------------------------------------------*/
uint32_t
smallprng_rand(smallprng_t *smallprng)
{
  uint32_t e = smallprng->a - rot32(smallprng->b, 27);
  smallprng->a = smallprng->b ^ rot32(smallprng->c, 17);
  smallprng->b = smallprng->c + smallprng->d;
  smallprng->c = smallprng->d + e;
  smallprng->d = e + smallprng->a;
  return smallprng->d;
}
/*---------------------------------------------------------------------------*/
void
smallprng_init(smallprng_t *smallprng, uint32_t seed)
{
  smallprng->a = 0xf1ea5eed;
  smallprng->b = smallprng->c = smallprng->d = seed;
  for(uint8_t i = 0; i < 20; ++i) {
    (void)smallprng_rand(smallprng);
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
