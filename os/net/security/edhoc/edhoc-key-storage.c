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
 *         edhoc-key-storage an implementation of a key storage to keep the ECC authentication keys to work with.
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 */

#include "edhoc-key-storage.h"
#include "contiki.h"
#include "contiki-lib.h"
#include <string.h>
#include "edhoc-log.h"

/*----------------------------------------------------------------------------*/
LIST(key_list);
MEMB(key_memb, cose_key_t, 2);
/*----------------------------------------------------------------------------*/
void
edhoc_create_key_list(void)
{
  list_init(key_list);
  memb_init(&key_memb);
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_check_key_list_identity(char *identity, uint8_t identity_sz,
                              cose_key_t **auth_key)
{
  int n = list_length(key_list);
  cose_key_t *key = list_head(key_list);
  while(n > 0) {
    if(memcmp(key->identity, identity, (size_t)identity_sz) == 0) {
      if(key->identity_sz == identity_sz) {
        *auth_key = key;
        return 1;
      }
    }
    n--;
    key = list_item_next(key);
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_check_key_list_kid(uint8_t *kid, uint8_t kid_sz, cose_key_t **auth_key)
{
  int n = list_length(key_list);
  cose_key_t *key = list_head(key_list);
  while(n > 0) {
    if(kid_sz == key->kid_sz) {
      if(memcmp(key->kid, kid, (size_t)kid_sz) == 0) {
        *auth_key = key;
        return 1;
      }
    }
    n--;
    key = list_item_next(key);
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
void
edhoc_add_key(cose_key_t *key)
{
  cose_key_t *k = memb_alloc(&key_memb);
  list_add(key_list, k);
  memcpy(k, key, sizeof(cose_key_t));
}
/*----------------------------------------------------------------------------*/
void
edhoc_copy_key(cose_key_t *k, cose_key_t *key)
{
  memcpy(k, key, sizeof(cose_key_t));
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_remove_key_kid(uint8_t *kid, uint8_t kid_sz)
{
  cose_key_t *key = NULL;
  if(edhoc_check_key_list_kid(kid, kid_sz, &key)) {
    list_remove(key_list, key);
    return 1;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
uint8_t
edhoc_remove_key_identity(char *identity, uint8_t identity_sz)
{
  cose_key_t *key = NULL;
  if(edhoc_check_key_list_identity(identity, identity_sz, &key)) {
    list_remove(key_list, key);
    return 1;
  }
  return 0;
}
/*----------------------------------------------------------------------------*/
void
edhoc_remove_key(cose_key_t *auth_key)
{
  list_remove(key_list, auth_key);
}
/*----------------------------------------------------------------------------*/
void
cose_print_key(cose_key_t *cose)
{
  LOG_DBG("kid: ");
  LOG_PRINT_EDHOC_BUFF(cose->kid, cose->kid_sz);
  LOG_DBG("identity: ");
  LOG_PRINT_EDHOC_BUFF((uint8_t *)cose->identity, cose->identity_sz);
  LOG_DBG("kty: %d\n", cose->kty);
  LOG_DBG("crv: %d\n", cose->crv);
  LOG_DBG("x: ");
  LOG_PRINT_EDHOC_BUFF(cose->ecc.pub.x, ECC_KEY_LEN);
  LOG_DBG("y: ");
  LOG_PRINT_EDHOC_BUFF(cose->ecc.pub.y, ECC_KEY_LEN);
}
/*----------------------------------------------------------------------------*/
