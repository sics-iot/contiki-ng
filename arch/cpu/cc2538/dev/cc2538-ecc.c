/*
 * Copyright (c) 2021, Uppsala universitet.
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
 * \addtogroup cc2538-pka
 * @{
 *
 * \file
 * Implementation of PKA-accelerated ECDH and ECDSA.
 */

#include "lib/ecc.h"
#include "lib/csprng.h"
#include "dev/pka.h"
#include <stdbool.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "ECC"
#define LOG_LEVEL LOG_LEVEL_NONE

/* maximum sizes */
#define MAX_ELEMENT_WORDS (8)
#define MAX_ELEMENT_BYTES (MAX_ELEMENT_WORDS * sizeof(uint32_t))
#define MAX_REMAINDER_WORDS \
  PKA_REMAINDER_WORDS(MAX_ELEMENT_WORDS)
#define MAX_COORDINATE_WORDS \
  PKA_COORDINATE_WORDS(MAX_ELEMENT_WORDS)
#define MAX_POINT_WORDS \
  PKA_POINT_WORDS(MAX_ELEMENT_WORDS)
#define SCRATCHPAD_WORDS \
  MAX(PKA_MULTIPLY_SCRATCHPAD_WORDS(MAX_ELEMENT_WORDS, MAX_ELEMENT_WORDS), \
   MAX(PKA_ADD_SCRATCHPAD_WORDS(MAX_ELEMENT_WORDS, MAX_ELEMENT_WORDS), \
    MAX(PKA_SUBTRACT_SCRATCHPAD_WORDS(MAX_ELEMENT_WORDS, MAX_ELEMENT_WORDS), \
     MAX(PKA_ECC_ADD_SCRATCHPAD_WORDS(MAX_ELEMENT_WORDS), \
      MAX(PKA_ECC_MUL_SCRATCHPAD_WORDS(MAX_ELEMENT_WORDS), \
       PKA_MOD_INV_SCRATCHPAD_WORDS(MAX_ELEMENT_WORDS, MAX_ELEMENT_WORDS))))))

/* useful elements */
static const uint32_t element_null[MAX_ELEMENT_WORDS];
static const uint32_t element_one[MAX_ELEMENT_WORDS] = { 1 };

/* offsets into PKA RAM */
static const uintptr_t element_null_offset = 0;
static const uintptr_t element_one_offset =
  PKA_NEXT_OFFSET(element_null_offset, MAX_ELEMENT_WORDS);
static const uintptr_t curve_g_offset =
  PKA_NEXT_OFFSET(element_one_offset, MAX_ELEMENT_WORDS);
static const uintptr_t curve_pab_offset =
  PKA_NEXT_OFFSET(curve_g_offset, MAX_POINT_WORDS);
static const uintptr_t curve_n_offset =
  PKA_NEXT_OFFSET(curve_pab_offset, 3 * MAX_COORDINATE_WORDS);
static const uintptr_t curve_a_offset =
  PKA_NEXT_OFFSET(curve_n_offset, MAX_ELEMENT_WORDS);
static const uintptr_t curve_b_offset =
  PKA_NEXT_OFFSET(curve_a_offset, MAX_ELEMENT_WORDS);
static const uintptr_t scratchpad_offset =
  PKA_NEXT_OFFSET(curve_b_offset, MAX_ELEMENT_WORDS);
static const uintptr_t variables_offset =
  PKA_NEXT_OFFSET(scratchpad_offset, SCRATCHPAD_WORDS);
static const uintptr_t curve_prime_offset = curve_pab_offset;

static struct pt main_protothread;
static struct pt helper_protothread;
static const ecc_curve_t *curve;
static process_mutex_t mutex;

/*---------------------------------------------------------------------------*/
/**
 * \brief         Tells if a bit in a little-endian element is set.
 * \param element Little-endian element.
 * \param bit     Index of the bit; the least significant bit has index 0.
 * \return        True if the bit is set.
 */
static bool
test_bit(const uint32_t *element, size_t bit)
{
  return element[bit >> 5] & ((uint32_t)1 << (bit & 0x1F));
}
/*---------------------------------------------------------------------------*/
static void
element_to_pka_ram(const uint8_t *bytes, uintptr_t offset)
{
  pka_big_endian_to_pka_ram(bytes, curve->bytes, offset);
}
/*---------------------------------------------------------------------------*/
static void
element_from_pka_ram(uint8_t *bytes, uintptr_t offset)
{
  pka_big_endian_from_pka_ram(bytes, curve->words, offset);
}
/*---------------------------------------------------------------------------*/
static void
point_to_pka_ram(const uint8_t *bytes, uintptr_t offset)
{
  element_to_pka_ram(bytes, offset);
  element_to_pka_ram(bytes + curve->bytes,
                     offset + PKA_COORDINATE_WORDS(curve->words));
}
/*---------------------------------------------------------------------------*/
static void
point_from_pka_ram(uint8_t *bytes, uintptr_t offset)
{
  element_from_pka_ram(bytes, offset);
  element_from_pka_ram(bytes + curve->bytes,
                       offset + PKA_COORDINATE_WORDS(curve->words));
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(compare_a_and_b(uintptr_t a_offset,
                                 uintptr_t b_offset,
                                 int *const result))
{
  PT_BEGIN(&helper_protothread);

  REG(PKA_APTR) = a_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_BPTR) = b_offset;
  pka_run_function(PKA_FUNCTION_COMPARE);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());
  if(REG(PKA_COMPARE) == PKA_COMPARE_A_GREATER_THAN_B) {
    *result = PKA_STATUS_A_GR_B;
  } else if(REG(PKA_COMPARE) == PKA_COMPARE_A_LESS_THAN_B) {
    *result = PKA_STATUS_A_LT_B;
  } else {
    *result = PKA_STATUS_A_EQ_B;
  }

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(check_bounds(uintptr_t x_offset,
                              uintptr_t a_offset,
                              uintptr_t b_offset,
                              int *const result))
{
  PT_BEGIN(&helper_protothread);

  /* check whether x > a */
  REG(PKA_APTR) = x_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_BPTR) = a_offset;
  pka_run_function(PKA_FUNCTION_COMPARE);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());
  if(REG(PKA_COMPARE) != PKA_COMPARE_A_GREATER_THAN_B) {
    *result = PKA_STATUS_FAILURE;
    PT_EXIT(&helper_protothread);
  }

  /* check whether x < b */
  REG(PKA_BPTR) = b_offset;
  pka_run_function(PKA_FUNCTION_COMPARE);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());
  if(REG(PKA_COMPARE) != PKA_COMPARE_A_LESS_THAN_B) {
    *result = PKA_STATUS_FAILURE;
    PT_EXIT(&helper_protothread);
  }

  *result = PKA_STATUS_SUCCESS;

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(invert_modulo(uintptr_t number_offset,
                               uintptr_t modulus_offset,
                               uintptr_t result_offset,
                               int *const result))
{
  PT_BEGIN(&helper_protothread);

  /* invert number */
  REG(PKA_APTR) = number_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_BPTR) = modulus_offset;
  REG(PKA_BLENGTH) = curve->words;
  REG(PKA_DPTR) = scratchpad_offset;
  pka_run_function(PKA_FUNCTION_INVMOD);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  /* check result */
  if(REG(PKA_MSW) & PKA_MSW_RESULT_IS_ZERO) {
    *result = PKA_STATUS_RESULT_0;
    PT_EXIT(&helper_protothread);
  }

  /* copy result */
  REG(PKA_APTR) = scratchpad_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_CPTR) = result_offset;
  pka_run_function(PKA_FUNCTION_COPY);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  *result = PKA_STATUS_SUCCESS;

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(add_or_multiply_modulo(uint32_t function,
                                        uintptr_t a_offset,
                                        uintptr_t b_offset,
                                        uintptr_t modulus_offset,
                                        uintptr_t result_offset,
                                        int *const result))
{
  PT_BEGIN(&helper_protothread);

  /* add or multiply */
  REG(PKA_APTR) = a_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_BPTR) = b_offset;
  REG(PKA_BLENGTH) = curve->words;
  REG(PKA_CPTR) = scratchpad_offset;
  pka_run_function(function);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  /* check result */
  if(REG(PKA_MSW) & PKA_MSW_RESULT_IS_ZERO) {
    *result = PKA_STATUS_RESULT_0;
    PT_EXIT(&helper_protothread);
  }

  /* compute modulus */
  REG(PKA_APTR) = scratchpad_offset;
  REG(PKA_ALENGTH) = MAX(curve->words,
                         (REG(PKA_MSW) & PKA_MSW_MSW_ADDRESS_M)
                         - scratchpad_offset + 1);
  REG(PKA_BPTR) = modulus_offset;
  REG(PKA_CPTR) = result_offset;
  pka_run_function(PKA_FUNCTION_MODULO);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  *result = PKA_STATUS_SUCCESS;

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(subtract(uintptr_t a_offset,
                          uintptr_t b_offset,
                          uintptr_t result_offset))
{
  PT_BEGIN(&helper_protothread);

  /* subtract */
  REG(PKA_APTR) = a_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_BPTR) = b_offset;
  REG(PKA_BLENGTH) = curve->words;
  REG(PKA_CPTR) = scratchpad_offset;
  pka_run_function(PKA_FUNCTION_SUBTRACT);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  /* copy result */
  REG(PKA_APTR) = scratchpad_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_CPTR) = result_offset;
  pka_run_function(PKA_FUNCTION_COPY);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(reduce_to_element(uintptr_t hash_offset,
                                   int *const result))
{
  PT_BEGIN(&helper_protothread);

  if((curve->bytes * 8) < curve->binary_length_of_n) {
    *result = PKA_STATUS_SUCCESS;
    PT_EXIT(&helper_protothread);
  }

  /* right shift to binary length of n */
  REG(PKA_APTR) = hash_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_CPTR) = scratchpad_offset;
  REG(PKA_SHIFT) = (curve->bytes * 8) - curve->binary_length_of_n;
  pka_run_function(PKA_FUNCTION_RSHIFT);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  /* check whether hash < n */
  REG(PKA_APTR) = scratchpad_offset;
  REG(PKA_BPTR) = curve_n_offset;
  pka_run_function(PKA_FUNCTION_COMPARE);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  if(REG(PKA_COMPARE) != PKA_COMPARE_A_LESS_THAN_B) {
    /* subtract n */
    REG(PKA_APTR) = scratchpad_offset;
    REG(PKA_BPTR) = curve_n_offset;
    REG(PKA_BLENGTH) = curve->words;
    REG(PKA_CPTR) = hash_offset;
    pka_run_function(PKA_FUNCTION_SUBTRACT);
  } else {
    /* copy result */
    REG(PKA_APTR) = scratchpad_offset;
    REG(PKA_CPTR) = hash_offset;
    pka_run_function(PKA_FUNCTION_COPY);
  }
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  *result = PKA_STATUS_SUCCESS;

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(add_or_multiply_point(uint32_t function,
                                       uintptr_t a_offset,
                                       uintptr_t c_offset,
                                       uintptr_t result_offset,
                                       int *const result))
{
  PT_BEGIN(&helper_protothread);

  /* add or multiply point */
  REG(PKA_APTR) = a_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_BPTR) = curve_pab_offset;
  REG(PKA_BLENGTH) = curve->words;
  REG(PKA_CPTR) = c_offset;
  REG(PKA_DPTR) = scratchpad_offset;
  pka_run_function(function);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  /* check result */
  if(REG(PKA_SHIFT) == PKA_SHIFT_POINT_AT_INFINITY) {
    *result = PKA_STATUS_POINT_AT_INFINITY;
    PT_EXIT(&helper_protothread);
  }
  if(REG(PKA_SHIFT) != PKA_SHIFT_SUCCESS) {
    *result = PKA_STATUS_FAILURE;
    PT_EXIT(&helper_protothread);
  }

  /* copy result */
  REG(PKA_APTR) = scratchpad_offset;
  REG(PKA_ALENGTH) = PKA_POINT_WORDS(curve->words);
  REG(PKA_CPTR) = result_offset;
  pka_run_function(PKA_FUNCTION_COPY);
  PT_YIELD_UNTIL(&helper_protothread, pka_check_status());

  *result = PKA_STATUS_SUCCESS;

  PT_END(&helper_protothread);
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
  process_mutex_init(&mutex);
}
/*---------------------------------------------------------------------------*/
static process_mutex_t *
get_mutex(void)
{
  return &mutex;
}
/*---------------------------------------------------------------------------*/
static int
enable(const ecc_curve_t *c)
{
  curve = c;
  pka_init();
  pka_little_endian_to_pka_ram(element_null,
                               c->words,
                               element_null_offset);
  pka_little_endian_to_pka_ram(element_one,
                               c->words,
                               element_one_offset);
  pka_little_endian_to_pka_ram(c->x,
                               c->words,
                               curve_g_offset);
  pka_little_endian_to_pka_ram(c->y,
                               c->words,
                               curve_g_offset
                               + PKA_COORDINATE_WORDS(c->words));
  pka_little_endian_to_pka_ram(c->p,
                               c->words,
                               curve_pab_offset);
  pka_little_endian_to_pka_ram(c->a,
                               c->words,
                               curve_pab_offset
                               + PKA_COORDINATE_WORDS(c->words));
  pka_little_endian_to_pka_ram(c->b,
                               c->words,
                               curve_pab_offset
                               + (PKA_COORDINATE_WORDS(c->words) * 2));
  pka_little_endian_to_pka_ram(c->n,
                               c->words,
                               curve_n_offset);
  pka_little_endian_to_pka_ram(c->a,
                               c->words,
                               curve_a_offset);
  pka_little_endian_to_pka_ram(c->b,
                               c->words,
                               curve_b_offset);
  return 0;
}
/*---------------------------------------------------------------------------*/
static struct pt *
get_protothread(void)
{
  return &main_protothread;
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(validate_public_key(const uint8_t *public_key,
                                     int *const result))
{
  static const uintptr_t public_key_x_offset =
    variables_offset;
  static const uintptr_t public_key_y_offset =
    PKA_NEXT_OFFSET(public_key_x_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t tmp1_offset =
    PKA_NEXT_OFFSET(public_key_y_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t tmp2_offset =
    PKA_NEXT_OFFSET(tmp1_offset, MAX_REMAINDER_WORDS);
  /* PKA_NEXT_OFFSET(tmp2_offset, MAX_REMAINDER_WORDS); */

  PT_BEGIN(&main_protothread);

  /* copy inputs to PKA RAM */
  element_to_pka_ram(public_key, public_key_x_offset);
  element_to_pka_ram(public_key + curve->bytes, public_key_y_offset);

  /* ensure that 0 < x < prime */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           check_bounds(public_key_x_offset,
                        element_null_offset,
                        curve_prime_offset,
                        result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* ensure that 0 < y < prime */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           check_bounds(public_key_y_offset,
                        element_null_offset,
                        curve_prime_offset,
                        result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* ensure that y^2 = x^3 + ax + b */
  /* tmp1 = y^2 */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  public_key_y_offset,
                                  public_key_y_offset,
                                  curve_prime_offset,
                                  tmp1_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* tmp2 = x^2 */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  public_key_x_offset,
                                  public_key_x_offset,
                                  curve_prime_offset,
                                  tmp2_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* tmp2 = x^2 + a */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_ADD,
                                  tmp2_offset,
                                  curve_a_offset,
                                  curve_prime_offset,
                                  tmp2_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* tmp2 = x^3 + ax */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  tmp2_offset,
                                  public_key_x_offset,
                                  curve_prime_offset,
                                  tmp2_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* tmp2 = x^3 + ax + b */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_ADD,
                                  tmp2_offset,
                                  curve_b_offset,
                                  curve_prime_offset,
                                  tmp2_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  PT_SPAWN(&main_protothread,
           &helper_protothread,
           compare_a_and_b(tmp1_offset, tmp2_offset, result));
  if(*result != PKA_STATUS_A_EQ_B) {
    PT_EXIT(&main_protothread);
  }

  *result = PKA_STATUS_SUCCESS;

  PT_END(&main_protothread);
}
/*---------------------------------------------------------------------------*/
static void
compress_public_key(const uint8_t *public_key,
                    uint8_t *compressed_public_key)
{
  memcpy(compressed_public_key + 1, public_key, curve->bytes);
  compressed_public_key[0] = 2 + (public_key[curve->bytes * 2 - 1] & 0x01);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(decompress_public_key(const uint8_t *compressed_public_key,
                                       uint8_t *uncompressed_public_key,
                                       int *const result))
{
  static const uintptr_t public_key_x_offset =
    variables_offset;
  static const uintptr_t public_key_y_offset =
    PKA_NEXT_OFFSET(public_key_x_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t mod_sqrt_offset =
    PKA_NEXT_OFFSET(public_key_y_offset,
                    MAX(MAX_ELEMENT_WORDS, MAX_REMAINDER_WORDS));
  /* PKA_NEXT_OFFSET(mod_sqrt_offset,
         MAX(MAX_ELEMENT_WORDS, MAX_REMAINDER_WORDS)); */
  static uint8_t compression_info;
  static size_t i;

  PT_BEGIN(&main_protothread);

  /* save inputs */
  compression_info = compressed_public_key[0];
  element_to_pka_ram(compressed_public_key + 1, public_key_x_offset);

  /* y = x^2 */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  public_key_x_offset,
                                  public_key_x_offset,
                                  curve_prime_offset,
                                  public_key_y_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* y = x^2 + a */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_ADD,
                                  public_key_y_offset,
                                  curve_a_offset,
                                  curve_prime_offset,
                                  public_key_y_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* y = x^3 + ax */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  public_key_y_offset,
                                  public_key_x_offset,
                                  curve_prime_offset,
                                  public_key_y_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* y = x^3 + ax + b */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_ADD,
                                  public_key_y_offset,
                                  curve_b_offset,
                                  curve_prime_offset,
                                  public_key_y_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    PT_EXIT(&main_protothread);
  }

  /* compute mod sqrt of y */

  /* copy element 1 */
  REG(PKA_APTR) = element_one_offset;
  REG(PKA_ALENGTH) = curve->words;
  REG(PKA_CPTR) = mod_sqrt_offset;
  pka_run_function(PKA_FUNCTION_COPY);
  PT_YIELD_UNTIL(&main_protothread, pka_check_status());

  for(i = curve->binary_length_of_p_plus_one - 1; i > 1; i--) {
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                    mod_sqrt_offset,
                                    mod_sqrt_offset,
                                    curve_prime_offset,
                                    mod_sqrt_offset,
                                    result));
    if(*result != PKA_STATUS_SUCCESS) {
      PT_EXIT(&main_protothread);
    }
    if(test_bit(curve->p_plus_one, i)) {
      PT_SPAWN(&main_protothread,
               &helper_protothread,
               add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                      mod_sqrt_offset,
                                      public_key_y_offset,
                                      curve_prime_offset,
                                      mod_sqrt_offset,
                                      result));
      if(*result != PKA_STATUS_SUCCESS) {
        PT_EXIT(&main_protothread);
      }
    }
  }

  if((pka_word_from_pka_ram(mod_sqrt_offset) & 0x01)
      != (compression_info & 0x01)) {
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             subtract(curve_prime_offset, mod_sqrt_offset, mod_sqrt_offset));
  }

  element_from_pka_ram(uncompressed_public_key, public_key_x_offset);
  element_from_pka_ram(uncompressed_public_key + curve->bytes,
                       mod_sqrt_offset);
  *result = PKA_STATUS_SUCCESS;

  PT_END(&main_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(sign(const uint8_t *message_hash,
                      const uint8_t *private_key,
                      uint8_t *signature,
                      int *const result))
{
  static const uintptr_t e_offset =
    variables_offset;
  static const uintptr_t d_offset =
    PKA_NEXT_OFFSET(e_offset, MAX_REMAINDER_WORDS);
  static const uintptr_t k_offset =
    PKA_NEXT_OFFSET(d_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t r_offset =
    PKA_NEXT_OFFSET(k_offset, MAX_COORDINATE_WORDS);
  static const uintptr_t s_offset =
    PKA_NEXT_OFFSET(r_offset, MAX_POINT_WORDS);
  /* PKA_NEXT_OFFSET(s_offset, MAX_REMAINDER_WORDS); */
  uint8_t k[MAX_ELEMENT_BYTES];

  PT_BEGIN(&main_protothread);

  /* copy inputs to PKA RAM */
  element_to_pka_ram(private_key, d_offset);
  element_to_pka_ram(message_hash, e_offset);

  /* reduce hash to an element */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           reduce_to_element(e_offset, result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  while(1) {
    /* generate k */
    if(!csprng_rand(k, sizeof(k))) {
      LOG_ERR("CSPRNG error\n");
      *result = PKA_STATUS_FAILURE;
      PT_EXIT(&main_protothread);
    }
    element_to_pka_ram(k, k_offset);
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             check_bounds(k_offset,
                          element_null_offset,
                          curve_n_offset,
                          result));
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_WARN("k was not in [1, n-1]\n");
      continue;
    }

    /* calculate k x G = (r, ignore)*/
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             add_or_multiply_point(PKA_FUNCTION_ECCMUL,
                                   k_offset,
                                   curve_g_offset,
                                   r_offset,
                                   result));
    if(*result == PKA_STATUS_POINT_AT_INFINITY) {
      LOG_WARN("k x G is at infinity\n");
      continue;
    }
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
      PT_EXIT(&main_protothread);
    }

    /* ensure that r != 0 */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             compare_a_and_b(element_null_offset, r_offset, result));
    if(*result == PKA_STATUS_A_EQ_B) {
      LOG_WARN("r is zero\n");
      continue;
    }

    /* s := rd mod n */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                    d_offset,
                                    r_offset,
                                    curve_n_offset,
                                    s_offset,
                                    result));
    if(*result == PKA_STATUS_RESULT_0) {
      LOG_WARN("rd mod n was zero\n");
      continue;
    }
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
      PT_EXIT(&main_protothread);
    }

    /* ensure that rd and e don't coincide */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             compare_a_and_b(e_offset, s_offset, result));
    if(*result == PKA_STATUS_A_EQ_B) {
      LOG_WARN("rd and e coincide\n");
      continue;
    }

    /* s := e + rd mod n */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             add_or_multiply_modulo(PKA_FUNCTION_ADD,
                                    e_offset,
                                    s_offset,
                                    curve_n_offset,
                                    s_offset,
                                    result));
    if(*result == PKA_STATUS_RESULT_0) {
      LOG_WARN("e + rd mod n was zero\n");
      continue;
    }
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
      PT_EXIT(&main_protothread);
    }

    /* k := 1 / k */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             invert_modulo(k_offset, curve_n_offset, k_offset, result));
    if(*result == PKA_STATUS_RESULT_0) {
      LOG_WARN("inverse of k was zero\n");
      continue;
    }
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
      PT_EXIT(&main_protothread);
    }

    /* s := (e + r*d) / k mod n */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                    k_offset,
                                    s_offset,
                                    curve_n_offset,
                                    s_offset,
                                    result));
    if(*result == PKA_STATUS_RESULT_0) {
      LOG_WARN("s is zero\n");
      continue;
    }
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
      PT_EXIT(&main_protothread);
    }
    break;
  }

  element_from_pka_ram(signature, r_offset);
  element_from_pka_ram(signature + curve->bytes, s_offset);
  *result = PKA_STATUS_SUCCESS;

  PT_END(&main_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(verify(const uint8_t *signature,
                        const uint8_t *message_hash,
                        const uint8_t *public_key,
                        int *const result))
{
  static const uintptr_t e_offset =
    variables_offset;
  static const uintptr_t r_offset =
    PKA_NEXT_OFFSET(e_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t s_offset =
    PKA_NEXT_OFFSET(r_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t q_offset =
    PKA_NEXT_OFFSET(s_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t u1_offset =
    PKA_NEXT_OFFSET(q_offset, MAX_POINT_WORDS);
  static const uintptr_t u2_offset =
    PKA_NEXT_OFFSET(u1_offset,
                    MAX(MAX_REMAINDER_WORDS, MAX_COORDINATE_WORDS));
  static const uintptr_t p1_offset =
    PKA_NEXT_OFFSET(u2_offset,
                    MAX(MAX_REMAINDER_WORDS, MAX_COORDINATE_WORDS));
  static const uintptr_t p2_offset =
    PKA_NEXT_OFFSET(p1_offset, MAX_POINT_WORDS);
  /* PKA_NEXT_OFFSET(p2_offset, MAX_POINT_WORDS); */

  PT_BEGIN(&main_protothread);

  /* copy inputs to PKA RAM */
  point_to_pka_ram(public_key, q_offset);
  element_to_pka_ram(signature, r_offset);
  element_to_pka_ram(signature + curve->bytes, s_offset);
  element_to_pka_ram(message_hash, e_offset);

  /* reduce hash to an element */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           reduce_to_element(e_offset, result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* ensure that 0 < r < n */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           check_bounds(r_offset,
                        element_null_offset,
                        curve_n_offset,
                        result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* ensure that 0 < s < n */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           check_bounds(s_offset,
                        element_null_offset,
                        curve_n_offset,
                        result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* s := 1 / s */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           invert_modulo(s_offset, curve_n_offset, s_offset, result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* u1 := e / s mod n */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  s_offset,
                                  e_offset,
                                  curve_n_offset,
                                  u1_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* p1 := u1 x G */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_point(PKA_FUNCTION_ECCMUL,
                                 u1_offset,
                                 curve_g_offset,
                                 p1_offset,
                                 result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* u2 := r / s mod n */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_modulo(PKA_FUNCTION_MULTIPLY,
                                  s_offset,
                                  r_offset,
                                  curve_n_offset,
                                  u2_offset,
                                  result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* p2 := u2 x Q */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_point(PKA_FUNCTION_ECCMUL,
                                 u2_offset,
                                 q_offset,
                                 p2_offset,
                                 result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* p := p1 + p2 */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_point(PKA_FUNCTION_ECCADD,
                                 p1_offset,
                                 p2_offset,
                                 p1_offset,
                                 result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }

  /* verify */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           compare_a_and_b(p1_offset, r_offset, result));
  if(*result != PKA_STATUS_A_EQ_B) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }
  *result = PKA_STATUS_SUCCESS;

  PT_END(&main_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(generate_key_pair(uint8_t *public_key,
                                   uint8_t *private_key,
                                   int *const result))
{
  static const uintptr_t private_key_offset =
    variables_offset;
  static const uintptr_t public_key_offset =
    PKA_NEXT_OFFSET(private_key_offset, MAX_ELEMENT_WORDS);
  /* PKA_NEXT_OFFSET(public_key_offset, MAX_POINT_WORDS); */

  PT_BEGIN(&main_protothread);

  while(1) {
    /* generate private key */
    if(!csprng_rand(private_key, curve->bytes)) {
      LOG_ERR("CSPRNG error\n");
      *result = PKA_STATUS_FAILURE;
      PT_EXIT(&main_protothread);
    }
    element_to_pka_ram(private_key, private_key_offset);
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             check_bounds(private_key_offset,
                          element_null_offset,
                          curve_n_offset,
                          result));
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_WARN("private key was not in [1, n-1]\n");
      continue;
    }

    /* generate public key */
    PT_SPAWN(&main_protothread,
             &helper_protothread,
             add_or_multiply_point(PKA_FUNCTION_ECCMUL,
                                   private_key_offset,
                                   curve_g_offset,
                                   public_key_offset,
                                   result));
    if(*result == PKA_SHIFT_POINT_AT_INFINITY) {
      LOG_WARN("public key at infinity\n");
      continue;
    }
    if(*result != PKA_STATUS_SUCCESS) {
      LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
      PT_EXIT(&main_protothread);
    }
    break;
  }

  element_from_pka_ram(private_key, private_key_offset);
  point_from_pka_ram(public_key, public_key_offset);

  PT_END(&main_protothread);
}
/*---------------------------------------------------------------------------*/
static PT_THREAD(generate_shared_secret(const uint8_t *public_key,
                                        const uint8_t *private_key,
                                        uint8_t *shared_secret,
                                        int *const result))
{
  static const uintptr_t private_key_offset =
    variables_offset;
  static const uintptr_t public_key_offset =
    PKA_NEXT_OFFSET(private_key_offset, MAX_ELEMENT_WORDS);
  static const uintptr_t product_offset =
    PKA_NEXT_OFFSET(public_key_offset, MAX_POINT_WORDS);
  /* PKA_NEXT_OFFSET(product_offset, MAX_POINT_WORDS); */

  PT_BEGIN(&main_protothread);

  /* copy inputs to PKA RAM */
  element_to_pka_ram(private_key, private_key_offset);
  point_to_pka_ram(public_key, public_key_offset);

  /* do ECDH */
  PT_SPAWN(&main_protothread,
           &helper_protothread,
           add_or_multiply_point(PKA_FUNCTION_ECCMUL,
                                 private_key_offset,
                                 public_key_offset,
                                 product_offset,
                                 result));
  if(*result != PKA_STATUS_SUCCESS) {
    LOG_ERR("Line: %u Error: %u\n", __LINE__, *result);
    PT_EXIT(&main_protothread);
  }
  element_from_pka_ram(shared_secret, product_offset);
  *result = PKA_STATUS_SUCCESS;

  PT_END(&main_protothread);
}
/*---------------------------------------------------------------------------*/
static void
disable(void)
{
  pka_disable();
  process_mutex_unlock(&mutex);
}
/*---------------------------------------------------------------------------*/
const struct ecc_driver cc2538_ecc_driver = {
  init,
  get_mutex,
  enable,
  get_protothread,
  validate_public_key,
  compress_public_key,
  decompress_public_key,
  sign,
  verify,
  generate_key_pair,
  generate_shared_secret,
  disable
};
/*---------------------------------------------------------------------------*/

/** @} */
