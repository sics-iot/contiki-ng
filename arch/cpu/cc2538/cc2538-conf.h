/*
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * \addtogroup cc2538
 * @{
 */
/*---------------------------------------------------------------------------*/
#ifndef CC2538_CONF_H_
#define CC2538_CONF_H_
/*---------------------------------------------------------------------------*/
/**
 * \name CFS configuration
 *
 * @{
 */
#ifndef COFFEE_CONF_SIZE
#define COFFEE_CONF_SIZE            (4 * COFFEE_SECTOR_SIZE)
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name CC2538 System Control configuration
 *
 * @{
 */
#ifndef SYS_CTRL_CONF_OSC32K_USE_XTAL
#define SYS_CTRL_CONF_OSC32K_USE_XTAL   0 /**< Use the on-board 32.768-kHz crystal */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Watchdog Timer configuration
 *
 * @{
 */
#ifndef WATCHDOG_CONF_ENABLE
#define WATCHDOG_CONF_ENABLE        1 /**< Enable the watchdog timer */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name USB 'core' configuration
 *
 * Those values are not meant to be modified by the user, except where stated
 * otherwise
 * @{
 */
#define CTRL_EP_SIZE                8
#define USB_EP1_SIZE               32
#define USB_EP2_SIZE               64
#define USB_EP3_SIZE               64
#define USB_ARCH_WRITE_NOTIFY       0

#ifndef USB_ARCH_CONF_DMA
#define USB_ARCH_CONF_DMA           1 /**< Change to Enable/Disable USB DMA */

#endif
/** @} */
/**
 * \name uDMA Configuration and channel allocations
 *
 * @{
 */
#define USB_ARCH_CONF_RX_DMA_CHAN   0 /**< USB -> RAM DMA channel */
#define USB_ARCH_CONF_TX_DMA_CHAN   1 /**< RAM -> USB DMA channel */
#define CC2538_RF_CONF_TX_DMA_CHAN  2 /**< RF -> RAM DMA channel */
#define CC2538_RF_CONF_RX_DMA_CHAN  3 /**< RAM -> RF DMA channel */
#define UDMA_CONF_MAX_CHANNEL       CC2538_RF_CONF_RX_DMA_CHAN
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Character I/O Configuration
 *
 * @{
 */
#ifndef UART_CONF_ENABLE
#define UART_CONF_ENABLE            1 /**< Enable/Disable UART I/O */
#endif

#ifndef UART0_CONF_BAUD_RATE
#define UART0_CONF_BAUD_RATE   115200 /**< Default UART0 baud rate */
#endif

#ifndef UART1_CONF_BAUD_RATE
#define UART1_CONF_BAUD_RATE   115200 /**< Default UART1 baud rate */
#endif

#ifndef SLIP_ARCH_CONF_USB
#define SLIP_ARCH_CONF_USB          0 /**< SLIP over UART by default */
#endif

#ifndef DBG_CONF_USB
#define DBG_CONF_USB                0 /**< All debugging over UART by default */
#endif

#ifndef SERIAL_LINE_CONF_UART
#define SERIAL_LINE_CONF_UART       0 /**< UART to use with serial line */
#endif

#if !SLIP_ARCH_CONF_USB
#ifndef SLIP_ARCH_CONF_UART
#define SLIP_ARCH_CONF_UART         0 /**< UART to use with SLIP */
#endif
#endif

#if !DBG_CONF_USB
#ifndef DBG_CONF_UART
#define DBG_CONF_UART               0 /**< UART to use for debugging */
#endif
#endif

#ifndef UART1_CONF_UART
#define UART1_CONF_UART             0 /**< UART to use for examples relying on
                                           the uart1_* API */
#endif

#ifndef SLIP_ARCH_CONF_ENABLED
/*
 * Determine whether we need SLIP
 * This will keep working while UIP_FALLBACK_INTERFACE and CMD_CONF_OUTPUT
 * keep using SLIP
 */
#if defined(UIP_FALLBACK_INTERFACE) || defined(CMD_CONF_OUTPUT)
#define SLIP_ARCH_CONF_ENABLED      1
#endif
#endif

/**
 * \brief Define this as 1 to build a headless node.
 *
 * The UART will not be initialised its clock will be gated, offering some
 * energy savings. The USB will not be initialised either
 */
#ifndef CC2538_CONF_QUIET
#define CC2538_CONF_QUIET           0
#endif

/* CC2538_CONF_QUIET is hard and overrides all other related defines */
#if CC2538_CONF_QUIET
#undef USB_SERIAL_CONF_ENABLE
#define USB_SERIAL_CONF_ENABLE      0

#undef UART_CONF_ENABLE
#define UART_CONF_ENABLE            0
#endif /* CC2538_CONF_QUIET */

/**
 * \brief Enable the USB core only if we need it
 */
#ifndef USB_SERIAL_CONF_ENABLE
#define USB_SERIAL_CONF_ENABLE \
  ((SLIP_ARCH_CONF_USB && SLIP_ARCH_CONF_ENABLED) || \
   (MAC_CONF_WITH_TSCH && (SLIP_ARCH_CONF_ENABLED || BUILD_WITH_SHELL)) || \
   DBG_CONF_USB)
#endif

/*
 * If debugging and SLIP use the same peripheral, this will be 1. Don't modify
 * this
 */
#if SLIP_ARCH_CONF_ENABLED
#define DBG_CONF_SLIP_MUX (SLIP_ARCH_CONF_USB == DBG_CONF_USB && \
                           (SLIP_ARCH_CONF_USB || \
                            SLIP_ARCH_CONF_UART == DBG_CONF_UART))
#endif

/*
 * Automatic detection of whether a specific UART is in use
 */
#define UART_IN_USE_BY_SERIAL_LINE(u) (SERIAL_LINE_CONF_UART == (u))
#define UART_IN_USE_BY_SLIP(u)        (SLIP_ARCH_CONF_ENABLED && \
                                       !SLIP_ARCH_CONF_USB && \
                                       SLIP_ARCH_CONF_UART == (u))
#define UART_IN_USE_BY_DBG(u)         (!DBG_CONF_USB && DBG_CONF_UART == (u))
#define UART_IN_USE_BY_UART1(u)       (UART1_CONF_UART == (u))

#define UART_IN_USE(u) ( \
  UART_CONF_ENABLE && \
  (UART_IN_USE_BY_SERIAL_LINE(u) || \
   UART_IN_USE_BY_SLIP(u) || \
   UART_IN_USE_BY_DBG(u) || \
   UART_IN_USE_BY_UART1(u)) \
)
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name RF configuration
 *
 * @{
 */
/* RF Config */

#ifndef CC2538_RF_CONF_AUTOACK
#define CC2538_RF_CONF_AUTOACK               1 /**< RF H/W generates ACKs */
#endif /* CC2538_CONF_AUTOACK */

#ifndef CC2538_RF_CONF_TX_USE_DMA
#define CC2538_RF_CONF_TX_USE_DMA            1 /**< RF TX over DMA */
#endif

#ifndef CC2538_RF_CONF_RX_USE_DMA
#define CC2538_RF_CONF_RX_USE_DMA            1 /**< RF RX over DMA */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name LPM configuration
 * @{
 */
#ifndef LPM_CONF_ENABLE
#define LPM_CONF_ENABLE       1 /**< Set to 0 to disable LPM entirely */
#endif

/**
 * \brief Maximum PM
 *
 * The SoC will never drop to a Power Mode deeper than the one specified here.
 * 0 for PM0, 1 for PM1 and 2 for PM2
 */
#ifndef LPM_CONF_MAX_PM
#define LPM_CONF_MAX_PM       1
#endif

#ifndef LPM_CONF_STATS
#define LPM_CONF_STATS        0 /**< Set to 1 to enable LPM-related stats */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Radio Configuration
 *
 * @{
 */
#ifndef NETSTACK_CONF_RADIO
#define NETSTACK_CONF_RADIO         cc2538_rf_driver
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name IEEE address configuration
 *
 * Used to generate our link-layer & IPv6 address
 * @{
 */
/**
 * \brief Location of the IEEE address
 * 0 => Read from InfoPage,
 * 1 => Use a hardcoded address, configured by IEEE_ADDR_CONF_ADDRESS
 */
#ifndef IEEE_ADDR_CONF_HARDCODED
#define IEEE_ADDR_CONF_HARDCODED             0
#endif

/**
 * \brief The hardcoded IEEE address to be used when IEEE_ADDR_CONF_HARDCODED
 * is defined as 1
 */
#ifndef IEEE_ADDR_CONF_ADDRESS
#define IEEE_ADDR_CONF_ADDRESS { 0x00, 0x12, 0x4B, 0x00, 0x89, 0xAB, 0xCD, 0xEF }
#endif

/**
 * \brief Location of the IEEE address in the InfoPage when
 * IEEE_ADDR_CONF_HARDCODED is defined as 0
 * 0 => Use the primary address location
 * 1 => Use the secondary address location
 */
#ifndef IEEE_ADDR_CONF_USE_SECONDARY_LOCATION
#define IEEE_ADDR_CONF_USE_SECONDARY_LOCATION 0
#endif
/** @} */
/*---------------------------------------------------------------------------*/
/**
 * \name Security
 *
 * @{
 */
#ifndef CRYPTO_CONF_INIT
#define CRYPTO_CONF_INIT        1 /**< Whether to init cryptoprocessor */
#endif

#ifndef AES_128_CONF
#define AES_128_CONF            cc2538_aes_128_driver /**< AES-128 driver */
#endif

#ifndef CCM_STAR_CONF
#define CCM_STAR_CONF           cc2538_ccm_star_driver /**< AES-CCM* driver */
#endif

#ifndef SHA_256_CONF
#define SHA_256_CONF            cc2538_sha_256_driver /**< SHA-256 driver */
#endif

#ifndef ECC_CONF
#define ECC_CONF                cc2538_ecc_driver /**< ECC driver */
#endif
/** @} */
/*---------------------------------------------------------------------------*/
#endif /* CC2538_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */
