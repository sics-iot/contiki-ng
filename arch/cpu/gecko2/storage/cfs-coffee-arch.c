/*
 * Copyright (C) 2022 Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup gecko
 * @{
 *
 * \addtogroup gecko-storage Storage drivers
 * @{
 *
 * \addtogroup gecko-storage-cfs Storage CFS coffee driver
 * @{
 *
 * \file
 *         storage driver for the gecko.
 * \author
 *         Yago Fontoura do Rosario <yago.rosario@hotmail.com.br>
 *
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "sys/cc.h"
#include "cfs/cfs-coffee.h"
#include "dev/watchdog.h"
#include "cfs-coffee-arch.h"

#include <stdint.h>
#include <string.h>

#define FLASH_WORD_SIZE 4

/*---------------------------------------------------------------------------*/
#if !COFFEE_SECTOR_SIZE || COFFEE_SECTOR_SIZE % FLASH_PAGE_SIZE
#error COFFEE_SECTOR_SIZE must be a non-zero multiple of the flash page size
#endif
#if !COFFEE_PAGE_SIZE || COFFEE_SECTOR_SIZE % COFFEE_PAGE_SIZE
#error COFFEE_PAGE_SIZE must be a divisor of COFFEE_SECTOR_SIZE
#endif
#if COFFEE_PAGE_SIZE % FLASH_WORD_SIZE
#error COFFEE_PAGE_SIZE must be a multiple of the flash word size
#endif
#if COFFEE_SIZE % FLASH_PAGE_SIZE
#error COFFEE_SIZE must be aligned with a flash page boundary
#endif
#if COFFEE_SIZE % COFFEE_SECTOR_SIZE
#error COFFEE_SIZE must be a multiple of COFFEE_SECTOR_SIZE
#endif
#if COFFEE_SIZE / COFFEE_PAGE_SIZE > INT16_MAX
#error Too many Coffee pages for coffee_page_t
#endif
/*---------------------------------------------------------------------------*/
extern char linker_nvm_begin;
__attribute__((used)) uint8_t
cfs_coffee_default_storage[COFFEE_SIZE]
__attribute__ ((section(".simee")));
#define NVM_BASE (&linker_nvm_begin)
/*---------------------------------------------------------------------------*/
void
cfs_coffee_arch_erase(uint16_t sector)
{
  uint32_t flash_addr;

  flash_addr = (uint32_t)NVM_BASE +
    +(sector * COFFEE_SECTOR_SIZE);

  MSC_Init();
  MSC_ErasePage((uint32_t *)flash_addr);
  MSC_Deinit();
}
/*---------------------------------------------------------------------------*/
void
cfs_coffee_arch_write(const void *buf, unsigned int size, cfs_offset_t offset)
{
  const uint32_t *src = buf;
  uint32_t flash_addr = (uint32_t)NVM_BASE + offset;
  unsigned int align;
  uint32_t word;
  uint32_t len;
  uint32_t page_buf[COFFEE_PAGE_SIZE / FLASH_WORD_SIZE];
  unsigned int i;
  MSC_Status_TypeDef retVal = mscReturnOk;

  if(size && (align = flash_addr & (FLASH_WORD_SIZE - 1))) {
    len = MIN(FLASH_WORD_SIZE - align, size);
    word = ~((*src & ((1 << (len << 3)) - 1)) << (align << 3));
    MSC_Init();
    retVal = MSC_WriteWord((uint32_t *)(flash_addr & ~(FLASH_WORD_SIZE - 1)),
                           &word, FLASH_WORD_SIZE);
    MSC_Deinit();
    if(retVal != mscReturnOk) {
      return;
    }
    *(const uint8_t **)&src += len;
    size -= len;
    flash_addr += len;
  }

  while(size >= FLASH_WORD_SIZE) {
    len = MIN(size & ~(FLASH_WORD_SIZE - 1), COFFEE_PAGE_SIZE);
    for(i = 0; i < len / FLASH_WORD_SIZE; i++) {
      page_buf[i] = ~*src++;
    }
    MSC_Init();
    retVal = MSC_WriteWord((uint32_t *)flash_addr, page_buf, len);
    MSC_Deinit();
    if(retVal != mscReturnOk) {
      return;
    }
    size -= len;
    flash_addr += len;
  }

  if(size) {
    word = ~(*src & ((1 << (size << 3)) - 1));
    MSC_Init();
    retVal = MSC_WriteWord((uint32_t *)flash_addr, &word, FLASH_WORD_SIZE);
    MSC_Deinit();
    if(retVal != mscReturnOk) {
      return;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
cfs_coffee_arch_read(void *buf, unsigned int size, cfs_offset_t offset)
{
  const uint8_t *src;
  uint8_t *dst;

  for(src = (const void *)(NVM_BASE + offset), dst = buf; size; size--) {
    *dst++ = ~*src++;
  }
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 * @}
 */
