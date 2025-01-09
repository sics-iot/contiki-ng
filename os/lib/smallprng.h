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

/**
 * \file
 *          Small non-cryptographic pseudo-random number generator
 * \author
 *          Andreas Urke
 */

/** \addtogroup lib
 * @{ */

/**
 * \defgroup smallprng Small PRNG library
 *
 * Implementation of a small PRNG based on Edvard Pettersens
 * random.c for simplelink platform, which was based on Bob Jenkins'
 * small non-cryptographic PRNG:
 * http://burtleburtle.net/bob/rand/smallprng.html
 *
 * @{
 */

#ifndef SMALLPRNG_H_
#define SMALLPRNG_H_

#include <stdint.h>

/**< Small-PRNG context, access only via smallprgn API */
typedef struct {
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
} smallprng_t;

/*
 * \brief Calculate a pseudo random number from the given small-PRGN
 * \return A pseudo-random number
 */
uint32_t smallprng_rand(smallprng_t *smallprng);

/*
 * \brief Initialize the given small-PRGN
 * \param smallprng - Context for the small-PRNG to initialize
 * \param seed      - Seed for initialization
 *
 */
void smallprng_init(smallprng_t *smallprng, uint32_t seed);

#endif /* SMALLPRNG_H_ */

/** @} */
/** @} */
