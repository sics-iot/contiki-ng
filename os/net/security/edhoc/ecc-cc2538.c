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
 *         ecc-cc2538 driver extension of ecc-algorithm. Interface the necessary functionalities
 *         of cc2538 HW module
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 */

#include "contiki.h"

#ifdef CC2538_DEF_H_
#include "dev/pka.h"
#include "dev/watchdog.h"
#include "lib/random.h"
#include "ecc-cc2538.h"

uint32_t expn[8];
/*----------------------------------------------------------------------------*/
void
eccNative_to_bytes(uint8_t *bytes, int num_bytes, const uint32_t *native)
{
  int8_t i;
  for(i = 0; i < num_bytes; ++i) {
    unsigned b = num_bytes - 1 - i;
    bytes[i] = native[b / 4] >> (8 * (b % 4));
  }
}
/*----------------------------------------------------------------------------*/
void
eccBytes_to_native(uint32_t *native, const uint8_t *bytes, int num_bytes)
{
  int8_t i;
  memset(native, 0, sizeof(uint32_t) * 8);
  for(i = 0; i < num_bytes; ++i) {
    unsigned b = num_bytes - 1 - i;
    native[b / 4] |=
      (uint32_t)bytes[i] << (8 * (b % 4));
  }
}
/*----------------------------------------------------------------------------*/
static void
ecc_set_random_key(uint32_t *secret)
{
  int i;
  for(i = 0; i < 8; ++i) {
    secret[i] = (uint32_t)random_rand() | (uint32_t)random_rand() << 16;
  }
}
/*----------------------------------------------------------------------------*/
PT_THREAD(generate_key_hw(key_gen_t * key)) {
  static ecc_compare_state_t state = {
    .size = 8,
  };
  state.process = key->process;
  pka_init();
  PT_BEGIN(&key->pt);
  memcpy(state.b, key->curve_info->n, sizeof(uint32_t) * 8);
  static uint32_t secret_a[8];
  do {
    ecc_set_random_key(secret_a);
    memcpy(state.a, secret_a, sizeof(uint32_t) * 8);
    PT_SPAWN(&key->pt, &(state.pt), ecc_compare(&state));
  } while(state.result != PKA_STATUS_A_LT_B);

  static ecc_multiply_state_t side_a;
  side_a.process = key->process;
  side_a.curve_info = key->curve_info;

  memcpy(side_a.point_in.x, side_a.curve_info->x, sizeof(uint32_t) * 8);
  memcpy(side_a.point_in.y, side_a.curve_info->y, sizeof(uint32_t) * 8);
  memcpy(side_a.secret, secret_a, sizeof(secret_a));

  ecc_mul_start(side_a.secret, &side_a.point_in, side_a.curve_info, &side_a.rv, side_a.process);
  while(!pka_check_status()) {
  }
  ecc_mul_get_result(&side_a.point_out, side_a.rv);

  uint8_t public[ECC_KEY_LEN * 2];
  eccNative_to_bytes(public, ECC_KEY_LEN, side_a.point_out.x);
  eccNative_to_bytes(public + ECC_KEY_LEN, ECC_KEY_LEN, side_a.point_out.y);
  eccNative_to_bytes(key->y, ECC_KEY_LEN, side_a.point_out.y);
  eccNative_to_bytes(key->private, ECC_KEY_LEN, secret_a);
  eccNative_to_bytes(key->x, ECC_KEY_LEN, side_a.point_out.x);
  pka_disable();
  PT_END(&key->pt);
}
/*----------------------------------------------------------------------------*/
bool
cc2538_generate_ikm(const uint8_t *gx, const uint8_t *gy,
                    const uint8_t *private_key, uint8_t *ikm, ecc_curve_t curve)
{
  static ecc_multiply_state_t shared;
  shared.curve_info = curve.curve;
  uint8_t gy_local[ECC_KEY_LEN]; /* Create a local copy of gy */

  pka_init();

  /* Copy gy into gy_local */
  memcpy(gy_local, gy, ECC_KEY_LEN);

  /* If just one coordinate is have it we put first byte non zero */
  if(gy_local[0] == 0) {
    gy_local[0] = 0x01;
  }

  eccBytes_to_native(shared.point_in.x, gx, ECC_KEY_LEN);
  eccBytes_to_native(shared.point_in.y, gy_local, ECC_KEY_LEN); /* Use gy_local */
  eccBytes_to_native(shared.secret, private_key, ECC_KEY_LEN);
  watchdog_periodic();
  ecc_mul_start(shared.secret, &shared.point_in, shared.curve_info, &shared.rv, shared.process);
  watchdog_periodic();

  while(!pka_check_status()) {
  }

  watchdog_periodic();
  ecc_mul_get_result(&shared.point_out, shared.rv);
  watchdog_periodic();

  eccNative_to_bytes(ikm, ECC_KEY_LEN, shared.point_out.x);

  pka_disable();

  return true;
}
#endif /* CC2538_DEF_H_ */
/*----------------------------------------------------------------------------*/
