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
 *         edhoc-export an implementation to export keys from the EDHOC shared secret
 *
 * \author
 *         Lidia Pocero <pocero@isi.gr>
 *         Rikard Höglund
 *         Marco Tiloca
 */

#include "edhoc-exporter.h"
#include "contiki-lib.h"

/*----------------------------------------------------------------------------*/
void
print_oscore_ctx(oscore_ctx_t *osc)
{
  LOG_PRINT("Initiator client CID: 0x%02x\n", osc->client_ID);
  LOG_PRINT("Responder server CID: 0x%02x\n", osc->server_ID);
  LOG_PRINT("OSCORE Master Secret (%d bytes): ", OSCORE_KEY_SZ);
  print_buff_8_print(osc->master_secret, OSCORE_KEY_SZ);
  LOG_PRINT("OSCORE Master Salt (%d bytes): ", OSCORE_SALT_SZ);
  print_buff_8_print(osc->master_salt, OSCORE_SALT_SZ);
}
/*----------------------------------------------------------------------------*/
int8_t
edhoc_exporter(const uint8_t *in_key, uint8_t exporter_label,
               const uint8_t *context, uint8_t context_sz,
               uint16_t length, uint8_t *result)
{
  return edhoc_kdf(in_key, exporter_label, context, context_sz,
                        length, result);
}
/*----------------------------------------------------------------------------*/
/* TODO: May be better to actually store PRK_out & PRK_exporter and
   then use them in edhoc_exporter above */
int8_t
edhoc_exporter_oscore(oscore_ctx_t *osc, edhoc_context_t *ctx)
{
  /* Derive prk_out */
  int prk_out_sz = HASH_LEN;
  uint8_t prk_out[prk_out_sz];
  int8_t er = edhoc_kdf(ctx->state.prk_4e3m, PRK_OUT_LABEL, ctx->state.th,
                        HASH_LEN, prk_out_sz, prk_out);
  if(er < 0) {
    return er;
  }
  LOG_DBG("PRK_out (%d bytes): ", prk_out_sz);
  print_buff_8_dbg(prk_out, prk_out_sz);

  /* Derive prk_exporter */
  int prk_exporter_sz = HASH_LEN;
  uint8_t prk_exporter[prk_exporter_sz];
  er = edhoc_kdf(prk_out, PRK_EXPORTER_LABEL, NULL, 0,
                 prk_exporter_sz, prk_exporter);
  if(er < 0) {
    return er;
  }
  LOG_DBG("PRK_exporter (%d bytes): ", prk_exporter_sz);
  print_buff_8_dbg(prk_exporter, prk_exporter_sz);

  /* The OSCORE client is the initiator */
  if(EDHOC_ROLE == EDHOC_INITIATOR) {
    osc->client_ID = ctx->state.cid;
    osc->server_ID = ctx->state.cid_rx;
  }
  if(EDHOC_ROLE == EDHOC_RESPONDER) {
    osc->client_ID = ctx->state.cid_rx;
    osc->server_ID = ctx->state.cid;
  }

  /* Derive OSCORE Master Secret */
  er = edhoc_exporter(prk_exporter, OSCORE_MASTER_SECRET_LABEL, NULL,
                      0, OSCORE_KEY_SZ, osc->master_secret);
  if(er < 0) {
    return er;
  }

  er = edhoc_exporter(prk_exporter, OSCORE_MASTER_SALT_LABEL, NULL,
                      0, OSCORE_SALT_SZ, osc->master_salt);
  return er;
}
/*----------------------------------------------------------------------------*/
