/*
 * Copyright (c) 2018, SICS, RISE AB
 * Copyright (c) 2023, Uppsala universitet
 * Copyright (c) 2024, Siemens AG
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
 * \addtogroup cbor
 * @{
 * \file
 *         CBOR implementation.
 */

#include "lib/cbor.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
void
cbor_init_writer(cbor_writer_state_t *state,
                 uint8_t *buffer, size_t buffer_size)
{
  state->buffer = buffer + buffer_size;
  state->buffer_size = buffer_size;
  state->nesting_depth = CBOR_MAX_NESTING;
}
/*---------------------------------------------------------------------------*/
uint8_t *
cbor_stop_writer(cbor_writer_state_t *state)
{
  return state->nesting_depth == CBOR_MAX_NESTING ? state->buffer : NULL;
}
/*---------------------------------------------------------------------------*/
static void
increment(cbor_writer_state_t *state)
{
  if(state->nesting_depth == CBOR_MAX_NESTING) {
    return;
  }
  state->objects[state->nesting_depth]++;
}
/*---------------------------------------------------------------------------*/
static void
prepend_object(cbor_writer_state_t *state,
               const void *object, size_t object_size)
{
  if(!object_size) {
    return;
  }
  if(state->buffer_size < object_size) {
    state->buffer = NULL;
    state->buffer_size = 0;
    return;
  }
  state->buffer -= object_size;
  state->buffer_size -= object_size;
  memcpy(state->buffer, object, object_size);
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_object(cbor_writer_state_t *state,
                    const void *object, size_t object_size)
{
  prepend_object(state, object, object_size);
  increment(state);
}
/*---------------------------------------------------------------------------*/
static void
prepend_unsigned(cbor_writer_state_t *state, uint64_t value)
{
  size_t length_to_copy;
  uint8_t jump_byte;

  /* determine size */
  if(value < CBOR_SIZE_1) {
    length_to_copy = 0;
    jump_byte = value;
  } else if(value <= UINT8_MAX) {
    length_to_copy = 1;
    jump_byte = CBOR_SIZE_1;
  } else if(value <= UINT16_MAX) {
    length_to_copy = 2;
    jump_byte = CBOR_SIZE_2;
  } else if(value <= UINT32_MAX) {
    length_to_copy = 4;
    jump_byte = CBOR_SIZE_4;
  } else {
    length_to_copy = 8;
    jump_byte = CBOR_SIZE_8;
  }

  /* write unsigned value */
  if(state->buffer_size <= length_to_copy) {
    state->buffer = NULL;
    state->buffer_size = 0;
    return;
  }
  state->buffer_size -= length_to_copy;
#ifdef __BIG_ENDIAN__
  state->buffer -= length_to_copy;
  memcpy(state->buffer, &value, length_to_copy);
#else /* ! __BIG_ENDIAN__ */
  while(length_to_copy--) {
    state->buffer--;
    *state->buffer = value;
    value >>= 8;
  }
#endif /* ! __BIG_ENDIAN__ */
  state->buffer--;
  state->buffer_size--;
  *state->buffer = jump_byte;
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_unsigned(cbor_writer_state_t *state, uint64_t value)
{
  prepend_unsigned(state, value);
  increment(state);
}
/*---------------------------------------------------------------------------*/
void
cbor_wrap_data(cbor_writer_state_t *state, size_t data_size)
{
  cbor_prepend_unsigned(state, data_size);
  if(!state->buffer) {
    return;
  }
  *state->buffer |= CBOR_MAJOR_TYPE_BYTE_STRING;
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_data(cbor_writer_state_t *state,
                  const uint8_t *data, size_t data_size)
{
  prepend_object(state, data, data_size);
  cbor_wrap_data(state, data_size);
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_text(cbor_writer_state_t *state,
                  const char *text, size_t text_size)
{
  prepend_object(state, text, text_size);
  cbor_prepend_unsigned(state, text_size);
  if(!state->buffer) {
    return;
  }
  *state->buffer |= CBOR_MAJOR_TYPE_TEXT_STRING;
}
/*---------------------------------------------------------------------------*/
uint8_t *
cbor_open_array(cbor_writer_state_t *state)
{
  if(!state->nesting_depth) {
    state->buffer = NULL;
    state->buffer_size = 0;
    return NULL;
  }
  state->objects[--state->nesting_depth] = 0;
  return state->buffer;
}
/*---------------------------------------------------------------------------*/
uint8_t *
cbor_wrap_array(cbor_writer_state_t *state)
{
  if(state->nesting_depth == CBOR_MAX_NESTING) {
    state->buffer = NULL;
    state->buffer_size = 0;
    return NULL;
  }
  prepend_unsigned(state, state->objects[state->nesting_depth]);
  if(!state->buffer) {
    return NULL;
  }
  *state->buffer |= CBOR_MAJOR_TYPE_ARRAY;
  if(++state->nesting_depth != CBOR_MAX_NESTING) {
    state->objects[state->nesting_depth]++;
  }
  return state->buffer;
}
/*---------------------------------------------------------------------------*/
uint8_t *
cbor_open_map(cbor_writer_state_t *state)
{
  return cbor_open_array(state);
}
/*---------------------------------------------------------------------------*/
uint8_t *
cbor_wrap_map(cbor_writer_state_t *state)
{
  if((state->nesting_depth == CBOR_MAX_NESTING)
     || (state->objects[state->nesting_depth] & 1)) {
    state->buffer = NULL;
    state->buffer_size = 0;
    return NULL;
  }
  prepend_unsigned(state, state->objects[state->nesting_depth] >> 1);
  if(!state->buffer) {
    return NULL;
  }
  *state->buffer |= CBOR_MAJOR_TYPE_MAP;
  if(++state->nesting_depth != CBOR_MAX_NESTING) {
    state->objects[state->nesting_depth]++;
  }
  return state->buffer;
}
/*---------------------------------------------------------------------------*/
static void
prepend_simple(cbor_writer_state_t *state, cbor_simple_value_t value)
{
  if(!state->buffer_size) {
    state->buffer = NULL;
    return;
  }

  state->buffer--;
  state->buffer_size--;
  *state->buffer = value;
  increment(state);
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_null(cbor_writer_state_t *state)
{
  prepend_simple(state, CBOR_SIMPLE_VALUE_NULL);
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_undefined(cbor_writer_state_t *state)
{
  prepend_simple(state, CBOR_SIMPLE_VALUE_UNDEFINED);
}
/*---------------------------------------------------------------------------*/
void
cbor_prepend_bool(cbor_writer_state_t *state, int boolean)
{
  prepend_simple(state,
                 boolean ? CBOR_SIMPLE_VALUE_TRUE : CBOR_SIMPLE_VALUE_FALSE);
}
/*---------------------------------------------------------------------------*/
void
cbor_init_reader(cbor_reader_state_t *state,
                 const uint8_t *cbor, size_t cbor_size)
{
  state->cbor = cbor;
  state->cbor_size = cbor_size;
}
/*---------------------------------------------------------------------------*/
cbor_major_type_t
cbor_read_next(cbor_reader_state_t *state)
{
  if(!state->cbor_size) {
    return CBOR_MAJOR_TYPE_NONE;
  }
  return *state->cbor & 0xE0;
}
/*---------------------------------------------------------------------------*/
const uint8_t *
cbor_stop_reader(cbor_reader_state_t *state)
{
  return state->cbor;
}
/*---------------------------------------------------------------------------*/
cbor_size_t
cbor_read_unsigned(cbor_reader_state_t *state, uint64_t *value)
{
  size_t bytes_to_read;

  if(!state->cbor_size) {
    return CBOR_SIZE_NONE;
  }
  cbor_size_t size = *state->cbor & ~0xE0;
  state->cbor++;
  state->cbor_size--;

  if(size < CBOR_SIZE_1) {
    *value = size;
    return CBOR_SIZE_1;
  }

  switch(size) {
  case CBOR_SIZE_1:
    bytes_to_read = 1;
    break;
  case CBOR_SIZE_2:
    bytes_to_read = 2;
    break;
  case CBOR_SIZE_4:
    bytes_to_read = 4;
    break;
  case CBOR_SIZE_8:
    bytes_to_read = 8;
    break;
  case CBOR_SIZE_NONE:
  default:
    return CBOR_SIZE_NONE;
  }

  if(bytes_to_read > state->cbor_size) {
    return CBOR_SIZE_NONE;
  }
  state->cbor_size -= bytes_to_read;

#ifdef __BIG_ENDIAN__
  memcpy(value, state->cbor, bytes_to_read);
  memset(value + bytes_to_read, 0, sizeof(*value) - bytes_to_read);
  state->cbor += bytes_to_read;
#else /* ! __BIG_ENDIAN__ */
  *value = 0;
  while(bytes_to_read--) {
    *value <<= 8;
    *value += *state->cbor++;
  }
#endif /* ! __BIG_ENDIAN__ */
  return size;
}
/*---------------------------------------------------------------------------*/
static const uint8_t *
read_byte_or_text_string(cbor_reader_state_t *state, size_t *size)
{
  uint64_t value;

  if((CBOR_SIZE_NONE == cbor_read_unsigned(state, &value))
     || (value > SIZE_MAX)
     || (state->cbor_size < value)) {
    return NULL;
  }
  *size = value;
  const uint8_t *beginning = state->cbor;
  state->cbor += *size;
  state->cbor_size -= *size;
  return beginning;
}
/*---------------------------------------------------------------------------*/
const uint8_t *
cbor_read_data(cbor_reader_state_t *state, size_t *data_size)
{
  if(cbor_read_next(state) != CBOR_MAJOR_TYPE_BYTE_STRING) {
    return NULL;
  }
  return read_byte_or_text_string(state, data_size);
}
/*---------------------------------------------------------------------------*/
const char *
cbor_read_text(cbor_reader_state_t *state, size_t *text_size)
{
  if(cbor_read_next(state) != CBOR_MAJOR_TYPE_TEXT_STRING) {
    return NULL;
  }
  return (const char *)read_byte_or_text_string(state, text_size);
}
/*---------------------------------------------------------------------------*/
static size_t
read_array_or_map(cbor_reader_state_t *state)
{
  uint64_t value;

  if((CBOR_SIZE_NONE == cbor_read_unsigned(state, &value))
     || (value >= SIZE_MAX)) {
    return SIZE_MAX;
  }
  return value;
}
/*---------------------------------------------------------------------------*/
size_t
cbor_read_array(cbor_reader_state_t *state)
{
  if(cbor_read_next(state) != CBOR_MAJOR_TYPE_ARRAY) {
    return SIZE_MAX;
  }
  return read_array_or_map(state);
}
/*---------------------------------------------------------------------------*/
size_t
cbor_read_map(cbor_reader_state_t *state)
{
  if(cbor_read_next(state) != CBOR_MAJOR_TYPE_MAP) {
    return SIZE_MAX;
  }
  return read_array_or_map(state);
}
/*---------------------------------------------------------------------------*/
cbor_simple_value_t
cbor_read_simple(cbor_reader_state_t *state)
{
  if(!state->cbor_size) {
    return CBOR_SIMPLE_VALUE_NONE;
  }
  state->cbor_size--;
  return *state->cbor++;
}
/*---------------------------------------------------------------------------*/

/** @} */
