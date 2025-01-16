/*
 * Copyright (c) 2025, Siemens AG.
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
 *
 */

/**
 * \file
 *         Testing CCM*-MICs
 * \author
 *         Konrad Krentz <konrad.krentz@gmail.com>
 */

#include "contiki.h"
#include "lib/cbor.h"
#include "unit-test.h"
#include <stdio.h>
#include <string.h>

PROCESS(test_process, "test");
AUTOSTART_PROCESSES(&test_process);

/*---------------------------------------------------------------------------*/
UNIT_TEST_REGISTER(test_write_read, "Basic write read example");
UNIT_TEST(test_write_read)
{
  static const uint8_t foo[] = { 0xA, 0xB, 0xC };
  uint8_t buffer[128];
  uint8_t *cbor_start;

  UNIT_TEST_BEGIN();

  /* write a CBOR array that contains a byte array and an unsigned value */
  {
    cbor_writer_state_t writer;
    cbor_init_writer(&writer, buffer, sizeof(buffer));
    cbor_open_array(&writer);
    cbor_prepend_unsigned(&writer, 123);
    cbor_prepend_data(&writer, foo, sizeof(foo));
    cbor_wrap_array(&writer);
    cbor_start = cbor_stop_writer(&writer);
  }

  UNIT_TEST_ASSERT(cbor_start);

  /* read the CBOR array and compare with our inputs */
  {
    cbor_reader_state_t reader;
    cbor_init_reader(&reader,
                     cbor_start,
                     sizeof(buffer) - (cbor_start - buffer));
    UNIT_TEST_ASSERT(2 == cbor_read_array(&reader));
    size_t data_size;
    const uint8_t *data = cbor_read_data(&reader, &data_size);
    UNIT_TEST_ASSERT(data);
    UNIT_TEST_ASSERT(data_size == sizeof(foo));
    UNIT_TEST_ASSERT(!memcmp(foo, data, data_size));
    uint64_t value;
    UNIT_TEST_ASSERT(CBOR_SIZE_NONE != cbor_read_unsigned(&reader, &value));
    UNIT_TEST_ASSERT(123 == value);
  }

  UNIT_TEST_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(test_process, ev, data)
{
  PROCESS_BEGIN();

  printf("Run unit-test\n");
  printf("---\n");

  UNIT_TEST_RUN(test_write_read);

  if(!UNIT_TEST_PASSED(test_write_read)) {
    printf("=check-me= FAILED\n");
    printf("---\n");
  }

  printf("=check-me= DONE\n");
  printf("---\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
