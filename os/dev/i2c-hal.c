/*
 * Copyright (c) 2016, Yanzi Networks, 2018 RISE SICS
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
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COMPANY AND CONTRIBUTORS ``AS IS'' AND
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
 *
 */

#include "dev/i2c-hal.h"
#include "sys/cc.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
/*---------------------------------------------------------------------------*/
bool
i2c_hal_has_bus(const i2c_hal_device_t *dev)
{
  if(dev == NULL || dev->bus == NULL) {
    return 0;
  }
  return dev->bus->lock != 0 && dev->bus->lock_device == dev;
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_acquire(i2c_hal_device_t *dev)
{
  i2c_hal_status_t status;
  if(dev == NULL || dev->bus == NULL) {
    return I2C_HAL_STATUS_EINVAL;
  }

  /* If the bus is already ours - things are ok */
  if(dev->bus->lock_device == dev && dev->bus->lock == 1) {
    return I2C_HAL_STATUS_OK;
  }

  if(++dev->bus->lock == 1) {
    dev->bus->lock_device = dev;

    /* lock the bus */
    status = i2c_hal_arch_lock(dev);
    if(status == I2C_HAL_STATUS_OK) {
      /* Bus has been locked */
      return I2C_HAL_STATUS_OK;
    }

    dev->bus->lock_device = NULL;

    /* Continue down to unlock */
  }

  /* problem... unlock */
  dev->bus->lock--;
  return I2C_HAL_STATUS_BUS_LOCKED;
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_release(i2c_hal_device_t *dev)
{
  if(!i2c_hal_has_bus(dev)) {
    /* The device does not own the bus */
    return I2C_HAL_STATUS_EINVAL;
  }

  /* unlock the bus */
  dev->bus->lock_device = NULL;
  if(--dev->bus->lock == 0) {
    return i2c_hal_arch_unlock(dev);
  }
  return I2C_HAL_STATUS_EINVAL;
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_restart_timeout(i2c_hal_device_t *dev)
{
  if(!i2c_hal_has_bus(dev)) {
    return I2C_HAL_STATUS_EINVAL;
  }
  return i2c_hal_arch_restart_timeout(dev);
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_write_byte(i2c_hal_device_t *dev, uint8_t data)
{
  if(!i2c_hal_has_bus(dev)) {
    return I2C_HAL_STATUS_BUS_LOCKED;
  }
  return i2c_hal_arch_write(dev, &data, 1);
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_write(i2c_hal_device_t *dev, const uint8_t *data, int size)
{
  if(!i2c_hal_has_bus(dev)) {
    return I2C_HAL_STATUS_BUS_LOCKED;
  }
  return i2c_hal_arch_write(dev, data, size);
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_read_byte(i2c_hal_device_t *dev, uint8_t *data)
{
  if(!i2c_hal_has_bus(dev)) {
    return I2C_HAL_STATUS_BUS_LOCKED;
  }
  return i2c_hal_arch_read(dev, data, 1);
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_read(i2c_hal_device_t *dev, uint8_t *data, int size)
{
  if(!i2c_hal_has_bus(dev)) {
    return I2C_HAL_STATUS_BUS_LOCKED;
  }
  return i2c_hal_arch_read(dev, data, size);
}
/*---------------------------------------------------------------------------*/
/* Read a register - e.g. first write a data byte to select register to read from, then read */
i2c_hal_status_t
i2c_hal_read_register(i2c_hal_device_t *dev, uint8_t reg, uint8_t *data, int size)
{
  uint8_t status;
  /* write the register first */
  status = i2c_hal_write_byte(dev, reg);
  if(status != I2C_HAL_STATUS_OK) {
    return status;
  }
  /* then read the value */
  status = i2c_hal_read(dev, data, size);
  if(status != I2C_HAL_STATUS_OK) {
    return status;
  }
  return i2c_hal_arch_stop(dev);
}
/*---------------------------------------------------------------------------*/
/* Write a register - e.g. first write a data byte to select register to write to, then write */
i2c_hal_status_t
i2c_hal_write_register(i2c_hal_device_t *dev, uint8_t reg, uint8_t data)
{
  uint8_t buffer[2];
  i2c_hal_status_t status;

  /* write the register first */
  buffer[0] = reg;
  buffer[1] = data;

  status = i2c_hal_write(dev, buffer, 2);
  if(status != I2C_HAL_STATUS_OK) {
    return status;
  }

  return i2c_hal_arch_stop(dev);
}
/*---------------------------------------------------------------------------*/
/* Write a register - e.g. first write a data byte to select register to write to, then write */
i2c_hal_status_t
i2c_hal_write_register_buf(i2c_hal_device_t *dev, uint8_t reg, const uint8_t *data, int size)
{
  uint8_t buffer[size + 1];
  i2c_hal_status_t status;

  /* write the register first */
  buffer[0] = reg;
  memcpy(&buffer[1], data, size);

  status = i2c_hal_write(dev, buffer, size + 1);
  if(status != I2C_HAL_STATUS_OK) {
    return status;
  }

  return i2c_hal_arch_stop(dev);
}
/*---------------------------------------------------------------------------*/
i2c_hal_status_t
i2c_hal_stop(i2c_hal_device_t *dev)
{
  if(!i2c_hal_has_bus(dev)) {
    return I2C_HAL_STATUS_BUS_LOCKED;
  }
  return i2c_hal_arch_stop(dev);
}
/*---------------------------------------------------------------------------*/
