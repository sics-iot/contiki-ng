
/*
 * Copyright (c) 2024, RISE Research Institutes of Sweden AB (RISE), Stockholm, Sweden
 * Copyright (c) 2020, Industrial System Institute (ISI), Patras, Greece
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
 *      EDHOC client example [RFC9528] with CoAP Block-Wise Transfer [RFC7959]
 * \author
 *      Lidia Pocero <pocero@isi.gr>, Peter A Jonsson, Rikard Höglund, Marco Tiloca
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "coap-engine.h"
#include "edhoc-client-API.h"

/******************************************************************************/
PROCESS(edhoc_example_client, "EDHOC Example Client");
AUTOSTART_PROCESSES(&edhoc_example_client);
/******************************************************************************/
PROCESS_THREAD(edhoc_example_client, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  etimer_set(&timer, CLOCK_SECOND * 10);
  while(1) {
    if(NETSTACK_ROUTING.node_is_reachable()) {
      LOG_INFO("Network reached!\n");
      break;
    }
    LOG_INFO("Waiting for network...\n");
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);
  }

  /* Set the client authentication credentials and add in the storage */
  edhoc_create_key_list();

#if DEFAULT_CREDS == 1

#define EDHOC_AUTH_KID 0x2b

  cose_key_t auth_client = {
    NULL,                                           /* next */
    { 0x2b },                                       /* kid[4] */
    1,                                              /* kid_sz */
    { "42-50-31-FF-EF-37-32-39" },                  /* identity[IDENTITY_MAX_LEN] */
    strlen("42-50-31-FF-EF-37-32-39"),              /* identity_sz */
    2,                                              /* kty */
    1,                                              /* crv */
    {                                               /* ecc_key_t ecc */
      {                                             /* ecc.priv[ECC_KEY_LEN] */
        0xfb, 0x13, 0xad, 0xeb, 0x65, 0x18, 0xce, 0xe5,
        0xf8, 0x84, 0x17, 0x66, 0x08, 0x41, 0x14, 0x2e,
        0x83, 0x0a, 0x81, 0xfe, 0x33, 0x43, 0x80, 0xa9,
        0x53, 0x40, 0x6a, 0x13, 0x05, 0xe8, 0x70, 0x6b
      },
      {                                             /* ecc_point_a pub */
        {                                           /* ecc.pub.x[ECC_KEY_LEN] */
          0xac, 0x75, 0xe9, 0xec, 0xe3, 0xe5, 0x0b, 0xfc,
          0x8e, 0xd6, 0x03, 0x99, 0x88, 0x95, 0x22, 0x40,
          0x5c, 0x47, 0xbf, 0x16, 0xdf, 0x96, 0x66, 0x0a,
          0x41, 0x29, 0x8c, 0xb4, 0x30, 0x7f, 0x7e, 0xb6
        },
        {                                           /* ecc.pub.y[ECC_KEY_LEN] */
          0x6e, 0x5d, 0xe6, 0x11, 0x38, 0x8a, 0x4b, 0x8a,
          0x82, 0x11, 0x33, 0x4a, 0xc7, 0xd3, 0x7e, 0xcb,
          0x52, 0xa3, 0x87, 0xd2, 0x57, 0xe6, 0xdb, 0x3c,
          0x2a, 0x93, 0xdf, 0x21, 0xff, 0x3a, 0xff, 0xc8
        }
      }
    }
  };

  /* Set the server authentication credentials and add in the storage */
  cose_key_t auth_server = {
    NULL,                                          /* next */
    { 0x32 },                                      /* kid[4] */
    1,                                             /* kid_sz */
    { "example.edu" },                             /* identity[IDENTITY_MAX_LEN] */
    strlen("example.edu"),                         /* identity_sz */
    2,                                             /* kty */
    1,                                             /* crv */
    {                                              /* ecc_key_t ecc */
      { 0 },                                       /* ecc.priv[ECC_KEY_LEN] */
      {                                            /* ecc_point_a pub */
        {                                          /* ecc.pub.x[ECC_KEY_LEN] */
          0xbb, 0xc3, 0x49, 0x60, 0x52, 0x6e, 0xa4, 0xd3,
          0x2e, 0x94, 0x0c, 0xad, 0x2a, 0x23, 0x41, 0x48,
          0xdd, 0xc2, 0x17, 0x91, 0xa1, 0x2a, 0xfb, 0xcb,
          0xac, 0x93, 0x62, 0x20, 0x46, 0xdd, 0x44, 0xf0
        },
        {                                          /* ecc.pub.y[ECC_KEY_LEN] */
          0x45, 0x19, 0xe2, 0x57, 0x23, 0x6b, 0x2a, 0x0c,
          0xe2, 0x02, 0x3f, 0x09, 0x31, 0xf1, 0xf3, 0x86,
          0xca, 0x7a, 0xfd, 0xa6, 0x4f, 0xcd, 0xe0, 0x10,
          0x8c, 0x22, 0x4c, 0x51, 0xea, 0xbf, 0x60, 0x72
        }
      }
    }
  };
#endif

#if INTEROP_CREDS_SIGN == 1

#define EDHOC_AUTH_KID 0x02

  /* Set the client authentication credentials and add in the storage */
  cose_key_t auth_client = {
    NULL,                                            /* next */
    { 0x02 },                                        /* kid[4] */
    1,                                               /* kid_sz */
    { "" },                                          /* identity[IDENTITY_MAX_LEN] */
    strlen(""),                                      /* identity_sz */
    2,                                               /* kty */
    1,                                               /* crv */
    {                                                /* ecc_key_t ecc */
      { 0x04, 0xf3, 0x47, 0xf2, 0xbe, 0xad, 0x69, 0x9a, 0xdb, 0x24, 0x73, 0x44, 0xf3, 0x47, 0xf2, 0xbd, 0xac, 0x93, 0xc7, 0xf2, 0xbe, 0xad, 0x6a, 0x9d, 0x2a, 0x9b, 0x24, 0x75, 0x4a, 0x1e, 0x2b, 0x62 },                                                /* ecc.priv[ECC_KEY_LEN] */
      {                                              /* ecc_point_a pub */
        { 0xCD, 0x41, 0x77, 0xBA, 0x62, 0x43, 0x33, 0x75, 0xED, 0xE2, 0x79, 0xB5, 0xE1, 0x8E, 0x8B, 0x91, 0xBC, 0x3E, 0xD8, 0xF1, 0xE1, 0x74, 0x47, 0x4A, 0x26, 0xFC, 0x0E, 0xDB, 0x44, 0xEA, 0x53, 0x73 },                                            /* ecc.pub.x[ECC_KEY_LEN] */
        { 0xA0, 0x39, 0x1D, 0xE2, 0x9C, 0x5C, 0x5B, 0xAD, 0xDA, 0x61, 0x0D, 0x4E, 0x30, 0x1E, 0xAA, 0xA1, 0x84, 0x22, 0x36, 0x77, 0x22, 0x28, 0x9C, 0xD1, 0x8C, 0xBE, 0x66, 0x24, 0xE8, 0x9B, 0x9C, 0xFD }                                             /* ecc.pub.y[ECC_KEY_LEN] */
      }
    }
  };

  /* Set the server authentication credentials and add in the storage */
  cose_key_t auth_server = {
    NULL,                                          /* next */
    { 0x09 },                                      /* kid[4] */
    1,                                             /* kid_sz */
    { "" },                                        /* identity[IDENTITY_MAX_LEN] */
    strlen(""),                                    /* identity_sz */
    2,                                             /* kty */
    1,                                             /* crv */
    {                                              /* ecc_key_t ecc */
      {                                            /* ecc.priv[ECC_KEY_LEN] */
        0
      },
      {                                            /* ecc_point_a pub */
        {                                          /* ecc.pub.x[ECC_KEY_LEN] */
          0x6F, 0x97, 0x02, 0xA6, 0x66, 0x02, 0xD7, 0x8F, 0x5E, 0x81, 0xBA, 0xC1, 0xE0, 0xAF, 0x01, 0xF8, 0xB5, 0x28, 0x10, 0xC5, 0x02, 0xE8, 0x7E, 0xBB, 0x7C, 0x92, 0x6C, 0x07, 0x42, 0x6F, 0xD0, 0x2F
        },
        {                                          /* ecc.pub.y[ECC_KEY_LEN] */
          0xC8, 0xD3, 0x32, 0x74, 0xC7, 0x1C, 0x9B, 0x3E, 0xE5, 0x7D, 0x84, 0x2B, 0xBF, 0x22, 0x38, 0xB8, 0x28, 0x3C, 0xB4, 0x10, 0xEC, 0xA2, 0x16, 0xFB, 0x72, 0xA7, 0x8E, 0xA7, 0xA8, 0x70, 0xF8, 0x00
        }
      }
    }
  };
#endif

#if INTEROP_CREDS_DH == 1

#define EDHOC_AUTH_KID 0x03

  /* Set the client authentication credentials and add in the storage */
  cose_key_t auth_client = {
    NULL,                                            /* next */
    { 0x03 },                                        /* kid[4] */
    1,                                               /* kid_sz */
    { "" },                                          /* identity[IDENTITY_MAX_LEN] */
    strlen(""),                                      /* identity_sz */
    2,                                               /* kty */
    1,                                               /* crv */
    {                                                /* ecc_key_t ecc */
      { 0xfb, 0x13, 0xad, 0xeb, 0x65, 0x18, 0xce, 0xe5, 0xf8, 0x84, 0x17, 0x66, 0x08, 0x41, 0x14, 0x2e, 0x83, 0x0a, 0x81, 0xfe, 0x33, 0x43, 0x80, 0xa9, 0x53, 0x40, 0x6a, 0x13, 0x05, 0xe8, 0x70, 0x6b },                                         /* ecc.priv[ECC_KEY_LEN] */
      {                                              /* ecc_point_a pub */
        { 0xac, 0x75, 0xe9, 0xec, 0xe3, 0xe5, 0x0b, 0xfc, 0x8e, 0xd6, 0x03, 0x99, 0x88, 0x95, 0x22, 0x40, 0x5c, 0x47, 0xbf, 0x16, 0xdf, 0x96, 0x66, 0x0a, 0x41, 0x29, 0x8c, 0xb4, 0x30, 0x7f, 0x7e, 0xb6 },                                            /* ecc.pub.x[ECC_KEY_LEN] */
        { 0x6e, 0x5d, 0xe6, 0x11, 0x38, 0x8a, 0x4b, 0x8a, 0x82, 0x11, 0x33, 0x4a, 0xc7, 0xd3, 0x7e, 0xcb, 0x52, 0xa3, 0x87, 0xd2, 0x57, 0xe6, 0xdb, 0x3c, 0x2a, 0x93, 0xdf, 0x21, 0xff, 0x3a, 0xff, 0xc8 }                                             /* ecc.pub.y[ECC_KEY_LEN] */
      }
    }
  };

  /* Set the server authentication credentials and add in the storage */
  cose_key_t auth_server = {
    NULL,                                          /* next */
    { 0x0a },                                      /* kid[4] */
    1,                                             /* kid_sz */
    { "" },                                        /* identity[IDENTITY_MAX_LEN] */
    strlen(""),                                    /* identity_sz */
    2,                                             /* kty */
    1,                                             /* crv */
    {                                              /* ecc_key_t ecc */
      {                                            /* ecc.priv[ECC_KEY_LEN] */
        0
      },
      {                                            /* ecc_point_a pub */
        {                                          /* ecc.pub.x[ECC_KEY_LEN] */
          0xbb, 0xc3, 0x49, 0x60, 0x52, 0x6e, 0xa4, 0xd3, 0x2e, 0x94, 0x0c, 0xad, 0x2a, 0x23, 0x41, 0x48, 0xdd, 0xc2, 0x17, 0x91, 0xa1, 0x2a, 0xfb, 0xcb, 0xac, 0x93, 0x62, 0x20, 0x46, 0xdd, 0x44, 0xf0
        },
        {                                          /* ecc.pub.y[ECC_KEY_LEN] */
          0x45, 0x19, 0xe2, 0x57, 0x23, 0x6b, 0x2a, 0x0c, 0xe2, 0x02, 0x3f, 0x09, 0x31, 0xf1, 0xf3, 0x86, 0xca, 0x7a, 0xfd, 0xa6, 0x4f, 0xcd, 0xe0, 0x10, 0x8c, 0x22, 0x4c, 0x51, 0xea, 0xbf, 0x60, 0x72
        }
      }
    }
  };
#endif

#if INTEROP_CREDS_CA == 1

#define EDHOC_AUTH_KID 0x2b

  cose_key_t auth_client = {
    NULL,                                           /* next */
    { 0x2b },                                       /* kid[4] */
    1,                                              /* kid_sz */
    { "42-50-31-FF-EF-37-32-39" },                  /* identity[IDENTITY_MAX_LEN] */
    strlen("42-50-31-FF-EF-37-32-39"),              /* identity_sz */
    2,                                              /* kty */
    1,                                              /* crv */
    {                                               /* ecc_key_t ecc */
      {                                             /* ecc.priv[ECC_KEY_LEN] */
        0xfb, 0x13, 0xad, 0xeb, 0x65, 0x18, 0xce, 0xe5,
        0xf8, 0x84, 0x17, 0x66, 0x08, 0x41, 0x14, 0x2e,
        0x83, 0x0a, 0x81, 0xfe, 0x33, 0x43, 0x80, 0xa9,
        0x53, 0x40, 0x6a, 0x13, 0x05, 0xe8, 0x70, 0x6b
      },
      {                                             /* ecc_point_a pub */
        {                                           /* ecc.pub.x[ECC_KEY_LEN] */
          0xac, 0x75, 0xe9, 0xec, 0xe3, 0xe5, 0x0b, 0xfc,
          0x8e, 0xd6, 0x03, 0x99, 0x88, 0x95, 0x22, 0x40,
          0x5c, 0x47, 0xbf, 0x16, 0xdf, 0x96, 0x66, 0x0a,
          0x41, 0x29, 0x8c, 0xb4, 0x30, 0x7f, 0x7e, 0xb6
        },
        {                                           /* ecc.pub.y[ECC_KEY_LEN] */
          0x6e, 0x5d, 0xe6, 0x11, 0x38, 0x8a, 0x4b, 0x8a,
          0x82, 0x11, 0x33, 0x4a, 0xc7, 0xd3, 0x7e, 0xcb,
          0x52, 0xa3, 0x87, 0xd2, 0x57, 0xe6, 0xdb, 0x3c,
          0x2a, 0x93, 0xdf, 0x21, 0xff, 0x3a, 0xff, 0xc8
        }
      }
    }
  };

  /* Set the server authentication credentials and add in the storage */
  cose_key_t auth_server = {
    NULL,                                          /* next */
    { 0x00 },                                      /* kid[4] */
    1,                                             /* kid_sz */
    { "demo.coap.amsuess.com" },                   /* identity[IDENTITY_MAX_LEN] */
    strlen("demo.coap.amsuess.com"),               /* identity_sz */
    2,                                             /* kty */
    1,                                             /* crv */
    {                                              /* ecc_key_t ecc */
      { 0 },                                       /* ecc.priv[ECC_KEY_LEN] */
      {                                            /* ecc_point_a pub */
        {                                          /* ecc.pub.x[ECC_KEY_LEN] */
          0xb9, 0xcc, 0x74, 0x6d, 0xf6, 0x64, 0x1d, 0x55, 0x04, 0x44, 0x78, 0xb2, 0x9d, 0xf0, 0x19, 0xef, 0x22, 0xb4, 0xd2, 0xe9, 0x6f, 0xfc, 0xf8, 0xde, 0x85, 0x43, 0x4e, 0x5d, 0x0f, 0x27, 0xc3, 0x3c
        },
        {                                          /* ecc.pub.y[ECC_KEY_LEN] */
          0xe1, 0x4e, 0x87, 0x33, 0x0d, 0x09, 0x3b, 0x46, 0x9b, 0x12, 0x1c, 0x3d, 0x0e, 0x4d, 0x94, 0x52, 0xcb, 0x90, 0x03, 0x6a, 0x6e, 0x20, 0x9f, 0x21, 0xf3, 0x7d, 0x35, 0xd2, 0xa0, 0x5c, 0x42, 0x6c
        }
      }
    }
  };
#endif

  edhoc_add_key(&auth_client);
  edhoc_add_key(&auth_server);

  edhoc_client_run();

  while(1) {
    PROCESS_WAIT_EVENT();
    int8_t re = edhoc_client_callback(ev, data);
    if(re > 0) {
      LOG_INFO("EDHOC protocol finished success, export your security context here\n");
      oscore_ctx_t osc;
      if(edhoc_exporter_oscore(&osc, edhoc_ctx) < 0) {
        LOG_ERR("ERROR IN EXPORT CTX\n");
      } else {
        LOG_INFO("Export OSCORE CTX success\n");
        print_oscore_ctx(&osc);
      }
      break;
    }
    if(re < 0) {
      LOG_ERR("EDHOC protocol ERROR\n");
      break;
    }
  }
  edhoc_client_close();
  LOG_INFO("Client finished\n");
  PROCESS_END();
}
/******************************************************************************/
