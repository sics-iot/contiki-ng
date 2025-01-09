/*
 * Copyright (c) 2016-2016, Yanzi Networks
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
/**
 * \addtogroup dev
 * @{
 *
 * \defgroup i2c-hal I2C Hardware Abstraction Layer
 *
 * The I2C HAL provides a set of common functions that can be used in a
 * platform-independent fashion.
 *
 *
 * @{
 *
 * \file
 *     Header file for the I2C HAL
 */
/*---------------------------------------------------------------------------*/
#ifndef I2C_HAL_H_
#define I2C_HAL_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include <stdint.h>
#include <stdbool.h>
/*---------------------------------------------------------------------------*/
/* Include Arch-Specific conf */
#ifdef I2C_HAL_CONF_ARCH_HDR_PATH
#include I2C_HAL_CONF_ARCH_HDR_PATH
#endif /* I2C_HAL_CONF_ARCH_HDR_PATH */
/*---------------------------------------------------------------------------*/
/**
 * \brief I2C return codes
 *
 * @{
 */
typedef enum {
  I2C_HAL_STATUS_OK,
  I2C_HAL_STATUS_TIMEOUT,
  I2C_HAL_STATUS_EINVAL,
  I2C_HAL_STATUS_ADDRESS_NACK,
  I2C_HAL_STATUS_DATA_NACK,
  I2C_HAL_STATUS_BUS_LOCKED,
  I2C_HAL_STATUS_BUS_NOT_OWNED
} i2c_hal_status_t;
/** @} */
/*---------------------------------------------------------------------------*/


#define I2C_HAL_NORMAL_BUS_SPEED  100000  /* 100KHz I2C */
#define I2C_HAL_FAST_BUS_SPEED    400000  /* 400KHz I2C */

/*---------------------------------------------------------------------------*/
typedef struct i2c_hal_device i2c_hal_device_t;

typedef struct {
  i2c_hal_device_t *lock_device;
  volatile uint8_t lock; /* for locking the bus */
#ifdef I2C_HAL_CONF_ARCH_CONFIG_TYPE
  I2C_HAL_CONF_ARCH_CONFIG_TYPE config;
#endif /* I2C_HAL_CONF_ARCH_CONFIG_TYPE */
} i2c_hal_bus_t;

/**
 * \brief I2C Device Configuration
 *
 * This is a structure to an architecture-independent I2C configuration.
 *
 * @{
 */

struct i2c_hal_device {
  i2c_hal_bus_t *bus;
  uint32_t speed;
  uint16_t timeout; /* timeout in milliseconds for this device */
  uint8_t address;  /* 8 bit I2C address */
};

/** @} */
/*---------------------------------------------------------------------------*/
/* These are architecture-independent functions to be used by SPI devices.   */
/*---------------------------------------------------------------------------*/


/**
 * \brief Aquire (locks) and then opens an I2C controller
 * \param dev An I2C device configuration which defines the controller
 * to be locked and the opening configuration.
 * \return I2C return code
 */

i2c_hal_status_t i2c_hal_acquire(i2c_hal_device_t *dev);

/**
 * \brief Closes and then unlocks an I2C controller
 * \param dev An I2C device configuration which defines the controller
 * to be closed and unlocked.
 * \return I2C return code
 *
 * Releasing an I2C controller should put it in low-power mode.
 * This should work only if the device has already aquired the I2C
 * controller.
 */
i2c_hal_status_t i2c_hal_release(i2c_hal_device_t *dev);

/**
 * \brief Checks if a device has locked an I2C controller
 * \param dev An I2C device state which defines the controller.
 * \return true if the device has the lock, false otherwise.
 */
bool i2c_hal_has_bus(const i2c_hal_device_t *dev);


i2c_hal_status_t i2c_hal_restart_timeout(i2c_hal_device_t *dev);

/**
 * \brief Writes a single byte to an I2C device
 * \param dev An I2C device state.
 * \param data A byte of data
 * \return I2C return code
 *
 * It will work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_write_byte(i2c_hal_device_t *dev, uint8_t data);

/**
 * \brief Reads a single byte from an I2C device
 * \param dev An I2C device state.
 * \param data A pointer to a byte of data
 * \return I2C return code
 *
 * It should work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_read_byte(i2c_hal_device_t *dev, uint8_t *data);

/**
 * \brief Writes a buffer to an I2C device
 * \param dev An I2C device state.
 * \param data A pointer to the data
 * \param size Size of the data to write
 * \return I2C return code
 *
 * It should work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_write(i2c_hal_device_t *dev, const uint8_t *data,
                               int size);

/**
 * \brief Reads a buffer from an I2C device
 * \param dev An I2C device configuration.
 * \param data A pointer to the data
 * \param size Size of the data to read
 * \return I2C return code
 *
 * It should work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_read(i2c_hal_device_t *dev, uint8_t *data, int size);

/**
 * \brief Sends an explicit stop signal to an I2C device
 * \param dev An I2C device.
 * \return I2C return code
 *
 * It should work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_stop(i2c_hal_device_t *dev);

/**
 * \brief Writes a byte to a register.
 * \param dev An I2C device.
 * \param reg The register to write into (byte)
 * \param value The value to write (byte)
 * \return I2C return code
 *
 * It should work only if the device has already locked the SPI controller.
 */
i2c_hal_status_t i2c_hal_write_register(i2c_hal_device_t *dev, uint8_t reg,
                                        uint8_t value);

/**
 * \brief Writes a buffer to a register over I2C
 * \param dev An I2C device
 * \param reg The register to write into (byte)
 * \param data A pointer to the data
 * \param size Size of the data to write
 * \return I2C return code
 *
 * It should work only if the device has already locked the SPI controller.
 */
i2c_hal_status_t i2c_hal_write_register_buf(i2c_hal_device_t *dev, uint8_t reg,
                                            const uint8_t *data, int size);
/**
 * \brief Reads a buffer of bytes from a register of an I2C device
 * \param dev An I2C device configuration.
 * \param reg Register
 * \param data A pointer to the data
 * \param size Size of the data to read
 * \return I2C return code
 *
 * It should work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_read_register(i2c_hal_device_t *dev, uint8_t reg,
                                       uint8_t *data, int size);


/*---------------------------------------------------------------------------*/
/* Arch functions needed per implementation                                  */
/*---------------------------------------------------------------------------*/
/**
 * \brief Locks and I2C controller for use with the specified device.
 * \param dev An I2C device specification.
 * \return I2C return code
 *
 */
i2c_hal_status_t i2c_hal_arch_lock(i2c_hal_device_t *dev);

/**
 * \brief Unlocks I2C controller (if dev has the lock)
 * \param dev An I2C device.
 * \return I2C return code
 *
 */
i2c_hal_status_t i2c_hal_arch_unlock(i2c_hal_device_t *dev);

/**
 * \brief restart the timeout for a long-lasting I2C transaction.
 * \param dev An I2C device.
 * \return I2C return code
 *
 */
i2c_hal_status_t i2c_hal_arch_restart_timeout(i2c_hal_device_t *dev);

/**
 * \brief Read data from a specified I2C device.
 * \param dev An I2C device.
 * \param data The data buffer to read into.
 * \param len The number of bytes to read.
 * \return I2C return code
 */
i2c_hal_status_t i2c_hal_arch_read(i2c_hal_device_t *dev, uint8_t *data,
                                   int len);

/**
 * \brief Write data to a specified I2C device.
 * \param dev An I2C device.
 * \param data The data buffer to write from.
 * \param len The number of bytes to write.
 * \return I2C return code
 *
 */
i2c_hal_status_t i2c_hal_arch_write(i2c_hal_device_t *dev, const uint8_t *data,
                                    int len);

/**
 * \brief Sends an explicit stop signal to an I2C device.
 * \param dev An I2C device.
 * \return I2C return code
 *
 * It should work only if the device has already locked the I2C controller.
 */
i2c_hal_status_t i2c_hal_arch_stop(i2c_hal_device_t *dev);

#endif /* I2C_HAL_H_ */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
