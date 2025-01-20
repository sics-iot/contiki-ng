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
#ifndef CFS_COFFEE_ARCH_H_
#define CFS_COFFEE_ARCH_H_

#include "contiki.h"
#include "cfs/cfs-coffee.h"
#include "em_device.h"
#include "em_msc.h"

#include <stdint.h>

/*---------------------------------------------------------------------------*/
/** \name Coffee port constants
 * @{
 */
/* Coffee configuration parameters. */
#define COFFEE_SECTOR_SIZE      (FLASH_PAGE_SIZE)
#define COFFEE_PAGE_SIZE        (FLASH_PAGE_SIZE / 8)
#ifdef COFFEE_CONF_SIZE
#define COFFEE_SIZE             COFFEE_CONF_SIZE
#else
#define COFFEE_SIZE             0
#endif
#define COFFEE_NAME_LENGTH      (18)
#define COFFEE_MAX_OPEN_FILES   (6)
#define COFFEE_FD_SET_SIZE      (8)
#define COFFEE_LOG_TABLE_LIMIT  (256)
#define COFFEE_DYN_SIZE         (2 * 1024)
#define COFFEE_LOG_SIZE         (1024)
#define COFFEE_MICRO_LOGS       (1)
#define COFFEE_APPEND_ONLY      (0)
/** @} */
/*---------------------------------------------------------------------------*/
/** \name Coffee port macros
 * @{
 */
/** Erase */
#define COFFEE_ERASE(sector) \
  cfs_coffee_arch_erase(sector)
/** Write */
#define COFFEE_WRITE(buf, size, offset) \
  cfs_coffee_arch_write((buf), (size), (offset))
/** Read */
#define COFFEE_READ(buf, size, offset) \
  cfs_coffee_arch_read((buf), (size), (offset))
/** @} */
/*---------------------------------------------------------------------------*/
/** \name Coffee port types
 * @{
 */
typedef int16_t coffee_page_t; /**< Page */
/** @} */
/*---------------------------------------------------------------------------*/

/** \brief Erases a device sector
 * \param sector Sector to erase
 */
void cfs_coffee_arch_erase(uint16_t sector);

/** \brief Writes a buffer to the device
 * \param buf Pointer to the buffer
 * \param size Byte size of the buffer
 * \param offset Device offset to write to
 */
void cfs_coffee_arch_write(const void *buf, unsigned int size,
                           cfs_offset_t offset);

/** \brief Reads from the device to a buffer
 * \param buf Pointer to the buffer
 * \param size Byte size of the buffer
 * \param offset Device offset to read from
 */
void cfs_coffee_arch_read(void *buf, unsigned int size, cfs_offset_t offset);

#endif /* CFS_COFFEE_ARCH_H_ */

/**
 * @}
 * @}
 * @}
 */
