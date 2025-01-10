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
 *      EDHOC configuration file
 * \author
 *      Lidia Pocero <pocero@isi.gr>, Rikard Höglund, Marco Tiloca
 */

/**
 * \addtogroup edhoc
 * @{
 */

#ifndef _EDHOC_CONFIG_H_
#define _EDHOC_CONFIG_H_

/**
 * \brief The max size of the buffers
 */
#define EDHOC_MAX_BUFFER 256

/**
 * \brief Set EDHOC connection identifier
 */
#ifndef EDHOC_CID
#ifdef EDHOC_CONF_CID
#define EDHOC_CID EDHOC_CONF_CID
#else
#define EDHOC_CID 0x1
#endif
#endif /* EDHOC_CID */

/**
 * \brief The length of connection identifiers
 * TODO: Support other than 1 byte CIDs
 */
#define EDHOC_CID_LEN 1

/* EDHOC Role definitions */
#define EDHOC_RESPONDER 0   /* The Responder of the EDHOC protocol */
#define EDHOC_INITIATOR 1   /* The Initiator of the EDHOC protocol */

/**
 * \brief Set the EDHOC Protocol role
 */
#ifdef EDHOC_CONF_ROLE
#define EDHOC_ROLE EDHOC_CONF_ROLE
#else
#define EDHOC_ROLE EDHOC_INITIATOR
#endif

/* EDHOC Authentication Method Types: Initiator (I) | Responder (R) */
#define EDHOC_METHOD0 0                  /* Signature Key  | Signature Key  */
#define EDHOC_METHOD1 1                  /* Signature Key  | Static DH Key  */
#define EDHOC_METHOD2 2                  /* Static DH Key  | Signature Key  */
#define EDHOC_METHOD3 3                  /* Static DH Key  | Static DH Key  */

/**
 * \brief Set the Authentication method
 */
#ifndef EDHOC_METHOD
#ifdef EDHOC_CONF_METHOD
#define EDHOC_METHOD EDHOC_CONF_METHOD
#else
#define EDHOC_METHOD EDHOC_METHOD0
#endif
#endif /* EDHOC_METHOD */

/**
 * \brief Buffer size for mac_or_sig
 */
#if EDHOC_METHOD == EDHOC_METHOD3
#define EDHOC_MAC_OR_SIG_BUF_LEN EDHOC_MAX_MAC_LEN
#else
#define EDHOC_MAC_OR_SIG_BUF_LEN P256_SIGNATURE_LEN
#endif

/**
 * \brief Helper defines for method handling on msg. reception
 */
#define INITIATOR_METHOD2 \
  (EDHOC_METHOD == EDHOC_METHOD2 && EDHOC_ROLE == EDHOC_INITIATOR)
#define RESPONDER_METHOD1 \
  (EDHOC_METHOD == EDHOC_METHOD1 && EDHOC_ROLE == EDHOC_RESPONDER)
#define INITIATOR_METHOD1 \
  (EDHOC_METHOD == EDHOC_METHOD1 && EDHOC_ROLE == EDHOC_INITIATOR)
#define RESPONDER_METHOD2 \
  (EDHOC_METHOD == EDHOC_METHOD2 && EDHOC_ROLE == EDHOC_RESPONDER)

/* Credential type/usage */
#define EDHOC_CRED_KID 2
#define EDHOC_CRED_INCLUDE 3

/**
 * \brief Set the authentication credential type/usage
 */
#ifdef EDHOC_CONF_AUTHENT_TYPE
#define EDHOC_AUTHENT_TYPE EDHOC_CONF_AUTHENT_TYPE
#else
#define EDHOC_AUTHENT_TYPE EDHOC_CRED_KID
#endif

/* cipher suites */
#define EDHOC_CIPHERSUITE_0 0   /* AES-CCM-16-64-128,  (HMAC 256/256) SHA-256,  MAC LEN 8,  X25519, EdDSA, Ed25519, AES-CCM-16-64-128, SHA-256 */
#define EDHOC_CIPHERSUITE_1 1   /* AES-CCM-16-128-128, (HMAC 256/256) SHA-256,  MAC LEN 16, X25519, EdDSA, Ed25519, AES-CCM-16-64-128, SHA-256 */
#define EDHOC_CIPHERSUITE_2 2 /* AES-CCM-16-64-128,  (HMAC 256/256) SHA-256,  MAC LEN 8,  P-256,  ES256, P-256,   AES-CCM-16-64-128, SHA-256 */   /* Supported */
#define EDHOC_CIPHERSUITE_3 3 /* AES-CCM-16-128-128, (HMAC 256/256) SHA-256,  MAC LEN 16, P-256,  ES256, P-256,   AES-CCM-16-64-128, SHA-256 */   /* Supported */
#define EDHOC_CIPHERSUITE_4 4   /* ChaCha20/Poly1305,  (HMAC 256/256) SHA-256,  MAC LEN 16, X25519, EdDSA, Ed25519, ChaCha20/Poly1305, SHA-256 */
#define EDHOC_CIPHERSUITE_5 5   /* ChaCha20/Poly1305,  (HMAC 256/256) SHA-256,  MAC LEN 16, P-256,  ES256, P-256,   ChaCha20/Poly1305, SHA-256 */
#define EDHOC_CIPHERSUITE_6 6   /* A128GCM,            (HMAC 256/256) SHA-256,  MAC LEN 16, X25519, ES256, P-256,   A128GCM,           SHA-256 */
#define EDHOC_CIPHERSUITE_24 24 /* A256GCM,            (HMAC 384/384) SHA-384,  MAC LEN 16, P-384,  ES384, P-384,   A256GCM,           SHA-384 */
#define EDHOC_CIPHERSUITE_25 25 /* ChaCha20/Poly1305,  (HMAC 256/256) SHAKE256, MAC LEN 16, X448,   EdDSA, Ed448,   ChaCha20/Poly1305, SHAKE256 */

/* EDHOC MAC lengths */
#define EDHOC_MAC_LEN_16  16
#define EDHOC_MAC_LEN_8   8
#define EDHOC_MAX_MAC_LEN 16

/* Curves for EDHOC key exchange algorithm (ECDH curve) */
#define EDHOC_CURVE_P256 1

/* Common settings for supported cipher suites */
#define ECC_KEY_LEN 32
#define HASH_LEN 32

/**
 * \brief Length of signatures
 */
#define P256_SIGNATURE_LEN 64
#define ED25519_SIGNATURE_LEN 64
#define ED448_SIGNATURE_LEN 114
#define P384_SIGNATURE_LEN 96

/**
 * \brief Set EDHOC cipher suite config
 */
#ifdef EDHOC_CONF_SUPPORTED_SUITE_1
#define EDHOC_SUPPORTED_SUITE_1 EDHOC_CONF_SUPPORTED_SUITE_1
#else
#define EDHOC_SUPPORTED_SUITE_1 -1
#endif

#ifdef EDHOC_CONF_SUPPORTED_SUITE_2
#define EDHOC_SUPPORTED_SUITE_2 EDHOC_CONF_SUPPORTED_SUITE_2
#else
#define EDHOC_SUPPORTED_SUITE_2 -1
#endif

#ifdef EDHOC_CONF_SUPPORTED_SUITE_3
#define EDHOC_SUPPORTED_SUITE_3 EDHOC_CONF_SUPPORTED_SUITE_3
#else
#define EDHOC_SUPPORTED_SUITE_3 -1
#endif

#ifdef EDHOC_CONF_SUPPORTED_SUITE_4
#define EDHOC_SUPPORTED_SUITE_4 EDHOC_CONF_SUPPORTED_SUITE_4
#else
#define EDHOC_SUPPORTED_SUITE_4 -1
#endif

/* Handle settings for test vectors */
#define EDHOC_NO_TEST 0
#define EDHOC_TEST_VECTOR_TRACE_DH 1
#define EDHOC_TEST_VECTOR_TRACE_SIG 2

#ifdef EDHOC_CONF_TEST
#define EDHOC_TEST EDHOC_CONF_TEST
#else
#define EDHOC_TEST EDHOC_NO_TEST
#endif

/**
 * \brief The number of attempts to try to connect with the EDHOC server successfully
 */
#ifndef EDHOC_CONF_ATTEMPTS
#define EDHOC_CONF_ATTEMPTS 2
#endif

/**
 * \brief The max length of the EDHOC message
 */
#ifdef EDHOC_CONF_MAX_PAYLOAD_LEN
#define EDHOC_MAX_PAYLOAD_LEN EDHOC_CONF_MAX_PAYLOAD_LEN
#else
#define EDHOC_MAX_PAYLOAD_LEN 256
#endif

/**
 * \brief The max length of the Application Data
 */
#ifdef EDHOC_CONF_MAX_AD_SZ
#define EDHOC_MAX_AD_SZ EDHOC_CONF_MAX_AD_SZ
#else
#define EDHOC_MAX_AD_SZ 8
#endif

/**
 * \brief EDHOC resource Uri-Path
 */
#define EDHOC_WELL_KNOWN ".well-known/edhoc"

#endif /* _EDHOC_CONFIG_H_ */

/** @} */
