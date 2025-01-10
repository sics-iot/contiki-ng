/*
 * Copyright (c) 2018, SICS, RISE AB
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
 *      An implementation of the Concise Binary Object Representation (RFC7049).
 * \author
 *      Martin Gunnarsson  <martin.gunnarsson@ri.se>
 *
 */


#ifndef _CBOR_H
#define _CBOR_H
#include <stddef.h>
#include <inttypes.h>


int cbor_put_nil(uint8_t **buffer);

int cbor_put_text(uint8_t **buffer, char *text, uint8_t text_len);

int cbor_put_array(uint8_t **buffer, uint8_t elements);

int cbor_put_bytes(uint8_t **buffer, const uint8_t *bytes, uint8_t bytes_len);

int cbor_put_map(uint8_t **buffer, uint8_t elements);

int cbor_put_num(uint8_t **buffer, uint8_t value);

int cbor_put_unsigned(uint8_t **buffer, uint8_t value);

int cbor_put_negative(uint8_t **buffer, int64_t value);

/**
 * \brief Predict the size of a CBOR byte string wrapping an array with a specific length
 * \param len The length of the byte array to be wrapped
 * \return the size of the wrapped CBOR byte string
 *
 * Get the size of the resulting CBOR byte string which would be produced by wrapping
 * a byte array of len as a CBOR byte string.
 */
uint8_t cbor_bytestr_size(uint32_t len);

uint8_t cbor_int_size(int32_t num);

#endif /* _cbor_H */
