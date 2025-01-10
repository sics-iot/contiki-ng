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
 *      Martin Gunnarsson  <martin.gunnarsson@ri.se>, Rikard Höglund
 *
 */


#include "cbor.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


//#define DEBUG 0
//#if DEBUG
//#include <stdio.h>
//#define PRINTF(...) printf(__VA_ARGS__)
//#define PRINTF_HEX(data, len)   printf_hex(data, len)
//#else
#define PRINTF(...)
#define PRINTF_HEX(data, len)
//#endif


int cbor_put_nil(uint8_t **buffer){
	**buffer = 0xF6;
   	(*buffer)++;
	return 1;
}


int
cbor_put_text(uint8_t **buffer, char *text, uint8_t text_len)
{
  uint8_t ret = 0;

  if(text_len > 23) {
    **buffer = 0x78;
    (*buffer)++;
    **buffer = text_len;
    (*buffer)++;
    ret += 2;
  } else {
    **buffer = (0x60 | text_len);
    (*buffer)++;
    ret += 1;
  }

  memcpy(*buffer, text, text_len);
  (*buffer) += text_len;
  ret += text_len;
  return ret;
}
int
cbor_put_array(uint8_t **buffer, uint8_t elements)
{
  if(elements > 15) {
    /*	#warning "handle this case!\n" */
    PRINTF("ERROR! in put array\n");
    return 0;
  }

  **buffer = (0x80 | elements);
  (*buffer)++;
  return 1;
}


int
cbor_put_bytes(uint8_t **buffer, const uint8_t *bytes, uint8_t bytes_len)
{
  uint8_t ret = 0;
  if(bytes_len > 23) {
    **buffer = 0x58;
    (*buffer)++;
    **buffer = bytes_len;
    (*buffer)++;
    ret += 2;
  } else {

    **buffer = (0x40 | bytes_len);
    (*buffer)++;
    ret += 1;
  }
  memcpy(*buffer, bytes, bytes_len);
  (*buffer) += bytes_len;
  ret += bytes_len;
  return ret;
}
int 
cbor_put_num(uint8_t **buffer, uint8_t value)
{
  (**buffer) = (value);
  (*buffer)++;
  return 1;
}

int
cbor_put_map(uint8_t **buffer, uint8_t elements)
{
  if(elements > 15) {
    /*	#warning "handle this case!\n" */
    PRINTF("ERROR in put map\n");
    return 0;
  }
  **buffer = (0xa0 | elements);
  (*buffer)++;

  return 1;
}
int
cbor_put_unsigned(uint8_t **buffer, uint8_t value)
{
  if(value > 0x17) {
    (**buffer) = (0x18);
    (*buffer)++;
    (**buffer) = (value);
    (*buffer)++;
    return 2;
  }
  (**buffer) = (value);
  (*buffer)++;
  return 1;
}

int
cbor_put_negative(uint8_t **buffer, int64_t value)
{
  value--;
  uint8_t *pt = *buffer;
  int nb = cbor_put_unsigned(buffer, value);
  *pt = (*pt | 0x20);
  return nb;
}

uint8_t //RH: WIP
cbor_bytestr_size(uint32_t len) {
  if (len <= 23) {
    return 1 + len; // 1 byte total for encoding
  } else if (len <= 255) {
    return 2 + len; // 1 byte for 0x18 + 1 byte for the length
  } else if (len <= 65535) {
    return 3 + len; // 1 byte for 0x19 + 2 bytes for the length
  } else if (len <= 4294967295) {
    return 5 + len; // 1 byte for 0x1A + 4 bytes for the length
  } else {
    return 9 + len; // 1 byte for 0x1B + 8 bytes for the length
  }
}

uint8_t //RH: WIP
cbor_int_size(int32_t num) {
  if (num >= -24 && num <= 23) {
    return 1;
  } else if (num >= -256 && num <= 255) {
    return 2;
  } else if (num >= -32768 && num <= 65535) {
    return 3;
  } else {
    /* 32 bit signed num is always >= -2147483648 and <= 4294967295 */
    return 5;
  }
}
