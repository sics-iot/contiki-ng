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

#ifndef CBOR_H_
#define CBOR_H_

/**
 * \addtogroup data
 * @{
 *
 * \defgroup cbor CBOR
 *
 * Functions for reading and writing CBOR.
 * @{
 *
 * \file
 *         CBOR API.
 */

#include <stddef.h>
#include <stdint.h>

/**
 * Defines how many arrays and maps can be open simultaneously while writing.
 */
#ifdef CBOR_CONF_MAX_NESTING
#define CBOR_MAX_NESTING CBOR_CONF_MAX_NESTING
#else /* CBOR_CONF_MAX_NESTING */
#define CBOR_MAX_NESTING (8)
#endif /* CBOR_CONF_MAX_NESTING */

#define CBOR_UNSIGNED_SIZE(uint) ((uint) < CBOR_SIZE_1 \
                                  ? 1 \
                                  : ((uint) <= UINT8_MAX \
                                     ? 1 + 1 \
                                     : ((uint) <= UINT16_MAX \
                                        ? 1 + 2 \
                                        : ((uint) <= UINT32_MAX \
                                           ? 1 + 4 \
                                           : 1 + 8))))
#define CBOR_BYTE_STRING_SIZE(bytes) (CBOR_UNSIGNED_SIZE(bytes) + (bytes))

/**
 * Enumeration of major types.
 */
typedef enum cbor_major_type_t {
  CBOR_MAJOR_TYPE_NONE = -1,
  CBOR_MAJOR_TYPE_UNSIGNED = 0x00,
  CBOR_MAJOR_TYPE_BYTE_STRING = 0x40,
  CBOR_MAJOR_TYPE_TEXT_STRING = 0x60,
  CBOR_MAJOR_TYPE_ARRAY = 0x80,
  CBOR_MAJOR_TYPE_MAP = 0xA0,
  CBOR_MAJOR_TYPE_SIMPLE = 0xE0,
} cbor_major_type_t;

/**
 * Enumeration of simple values.
 */
typedef enum cbor_simple_value_t {
  CBOR_SIMPLE_VALUE_NONE = -1,
  CBOR_SIMPLE_VALUE_FALSE = 0xF4,
  CBOR_SIMPLE_VALUE_TRUE = 0xF5,
  CBOR_SIMPLE_VALUE_NULL = 0xF6,
  CBOR_SIMPLE_VALUE_UNDEFINED = 0xF7,
} cbor_simple_value_t;

/**
 * Enumeration of size information in various major types.
 */
typedef enum cbor_size_t {
  CBOR_SIZE_NONE = -1, /**< error condition */
  CBOR_SIZE_1 = 0x18,  /**< 1 byte */
  CBOR_SIZE_2 = 0x19,  /**< 2 bytes */
  CBOR_SIZE_4 = 0x1A,  /**< 4 bytes */
  CBOR_SIZE_8 = 0x1B,  /**< 8 bytes */
} cbor_size_t;

/**
 * Structure of the internal state of a CBOR writer.
 */
typedef struct cbor_writer_state_t {
  uint8_t *buffer;
  size_t buffer_size;
  size_t nesting_depth;
  size_t objects[CBOR_MAX_NESTING];
} cbor_writer_state_t;

/**
 * Structure of the internal state of a CBOR reader.
 */
typedef struct cbor_reader_state_t {
  const uint8_t *cbor;
  size_t cbor_size;
} cbor_reader_state_t;

/**
 * Prepares for writing CBOR output.
 *
 * \param state       State of the CBOR writer.
 * \param buffer      Buffer for holding CBOR output.
 * \param buffer_size Size of @p buffer in bytes.
 */
void cbor_init_writer(cbor_writer_state_t *state,
                      uint8_t *buffer, size_t buffer_size);

/**
 * Finishes writing CBOR output.
 *
 * \param state State of the CBOR writer.
 *
 * \return      First byte of the CBOR output or \c NULL on error.
 */
uint8_t *cbor_stop_writer(cbor_writer_state_t *state);

/**
 * Prepends an arbitrary CBOR object to the CBOR output.
 *
 * \param state       State of the CBOR writer.
 * \param object      Location of the CBOR object.
 * \param object_size Size of @p object in bytes.
 */
void cbor_prepend_object(cbor_writer_state_t *state,
                         const void *object, size_t object_size);

/**
 * Prepends an unsigned integer to the CBOR output.
 *
 * \param state State of the CBOR writer.
 * \param value Unsigned integer.
 */
void cbor_prepend_unsigned(cbor_writer_state_t *state,
                           uint64_t value);

/**
 * Wraps previously prepended CBOR object(s) in a byte string.
 *
 * \param state     State of the CBOR writer.
 * \param data_size Size of the CBOR object(s) in bytes.
 */
void cbor_wrap_data(cbor_writer_state_t *state, size_t data_size);

/**
 * Prepends a byte string to the CBOR output.
 *
 * \param state     State of the CBOR writer.
 * \param data      The byte string.
 * \param data_size Size of @p data in bytes.
 */
void cbor_prepend_data(cbor_writer_state_t *state,
                       const uint8_t *data, size_t data_size);

/**
 * Prepends a text string to the CBOR output.
 *
 * \param state     State of the CBOR writer.
 * \param text      The text string.
 * \param text_size Number of characters in @p text.
 */
void cbor_prepend_text(cbor_writer_state_t *state,
                       const char *text, size_t text_size);

/**
 * Adds subsequently prepended CBOR objects to an array.
 *
 * \param state State of the CBOR writer.
 *
 * \return      First byte after the array or \c NULL on error.
 */
uint8_t *cbor_open_array(cbor_writer_state_t *state);

/**
 * Stops adding subsequently prepended CBOR objects to the innermost
 * not-yet-wrapped array.
 *
 * \param state State of the CBOR writer.
 *
 * \return      First byte of the wrapped array or \c NULL on error.
 */
uint8_t *cbor_wrap_array(cbor_writer_state_t *state);

/**
 * Adds subsequently prepended entries to a map.
 *
 * \param state State of the CBOR writer.
 *
 * \return      First byte after the map or \c NULL on error.
 */
uint8_t *cbor_open_map(cbor_writer_state_t *state);

/**
 * Stops adding subsequently prepended entries to the innermost not-yet-wrapped
 * map.
 *
 * \param state State of the CBOR writer.
 *
 * \return      First byte of the wrapped map or \c NULL on error.
 */
uint8_t *cbor_wrap_map(cbor_writer_state_t *state);

/**
 * Prepends the simple value \c null.
 *
 * \param state State of the CBOR writer.
 */
void cbor_prepend_null(cbor_writer_state_t *state);

/**
 * Prepends the simple value \c undefined.
 *
 * \param state State of the CBOR writer.
 */
void cbor_prepend_undefined(cbor_writer_state_t *state);

/**
 * Prepends a boolean simple value.
 *
 * \param state   State of the CBOR writer.
 * \param boolean Value \c 0 means false.
 */
void cbor_prepend_bool(cbor_writer_state_t *state, int boolean);

/**
 * Prepares for reading CBOR input.
 *
 * \param state     State of the CBOR reader.
 * \param cbor      Buffer that holds the CBOR input.
 * \param cbor_size Size of @p cbor in bytes.
 */
void cbor_init_reader(cbor_reader_state_t *state,
                      const uint8_t *cbor, size_t cbor_size);

/**
 * Inspects the next major type.
 *
 * \param state State of the CBOR reader.
 *
 * \return      The next major type (could be \c CBOR_MAJOR_TYPE_NONE).
 */
cbor_major_type_t cbor_read_next(cbor_reader_state_t *state);

/**
 * Stops reading CBOR input.
 *
 * \param state State of the CBOR reader.
 *
 * \return      The byte after the last read CBOR object.
 */
const uint8_t *cbor_stop_reader(cbor_reader_state_t *state);

/**
 * Reads an unsigned integer.
 *
 * \param state State of the CBOR reader.
 * \param value Buffer to store the unsigned integer.
 *
 * \return      Size of the unsigned integer or \c CBOR_SIZE_NONE on error.
 */
cbor_size_t cbor_read_unsigned(cbor_reader_state_t *state, uint64_t *value);

/**
 * Reads a byte string.
 *
 * \param state     State of the CBOR reader.
 * \param data_size Size of the byte string in bytes.
 *
 * \return          First byte of the byte string or \c NULL on error.
 */
const uint8_t *cbor_read_data(cbor_reader_state_t *state, size_t *data_size);

/**
 * Reads a text string.
 *
 * \param state     State of the CBOR reader.
 * \param text_size Number of characters in the text string.
 *
 * \return          First character of the text string or \c NULL on error.
 */
const char *cbor_read_text(cbor_reader_state_t *state, size_t *text_size);

/**
 * Reads the number of elements of an array.
 *
 * \param state State of the CBOR reader.
 *
 * \return      Number of elements or \c SIZE_MAX on error.
 */
size_t cbor_read_array(cbor_reader_state_t *state);

/**
 * Reads the number of entries of a map.
 *
 * \param state State of the CBOR reader.
 *
 * \return      Number of entries or \c SIZE_MAX on error.
 */
size_t cbor_read_map(cbor_reader_state_t *state);

/**
 * Reads a simple value.
 *
 * \param state State of the CBOR reader.
 *
 * \return      The simple value or \c CBOR_SIMPLE_VALUE_NONE on error.
 */
cbor_simple_value_t cbor_read_simple(cbor_reader_state_t *state);

/** @} */
/** @} */

#endif /* CBOR_H_ */
