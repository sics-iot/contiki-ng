#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LPM_CONF_MAX_PM 1

#define EDHOC_CONF_TIMEOUT 100000

/* Mandatory EDHOC definitions on Client */
/* Define one kind of the following kind of identification for the authentication key */
//#define EDHOC_AUTH_SUBJECT_NAME "Node_101"
//#define EDHOC_AUTH_KID 0x2b

/* Define a value for the Connection Identifier */
// #define EDHOC_CONF_CID -24
#define EDHOC_CONF_CID 0x37

// Large size to avoid block-wise
#define COAP_MAX_CHUNK_SIZE 300
#define DEFAULT_CREDS 1

// Use default creds (not interoping)
#if DEFAULT_CREDS == 1
#define EDHOC_AUTH_KID 0x2b
#elif INTEROP_CREDS_SIGN == 1
#define EDHOC_AUTH_KID 0x02
#elif INTEROP_CREDS_DH == 1
#define EDHOC_AUTH_KID 0x03
#elif INTEROP_CREDS_CA == 1
#define EDHOC_AUTH_KID 0x2b
#endif

/* Define the coap server to connect with */
//#define EDHOC_CONF_SERVER_EP "coap://[fe80::212:4b00:615:9fec]"
#define EDHOC_CONF_SERVER_EP "coap://[fd00::202:2:2:2]" /* Server IP for Cooja simulator */
//#define EDHOC_CONF_SERVER_EP "coap://[fd00::1]" /* IP for using with socat to reach other servers */

/* Define the party role on the EDHOC protocol as Initiator and the correlation method */
#define EDHOC_CONF_ROLE EDHOC_INITIATOR

/* To run with the test vector DH ephemeral keys used on the EDHOC interoperability session */
#define EDHOC_CONF_TEST EDHOC_TEST_VECTOR_TRACE_DH

/* Define the authentication */
#define EDHOC_CONF_AUTHENT_TYPE EDHOC_CRED_KID

/* Define the library for ECDH operations */
//#define EDHOC_CONF_ECC EDHOC_ECC_CC2538
#define EDHOC_CONF_ECC EDHOC_ECC_UECC

/* Set the supported cipher suites */
#define EDHOC_CONF_SUPPORTED_SUITE_1 EDHOC_CIPHERSUITE_2
#define EDHOC_CONF_SUPPORTED_SUITE_2 EDHOC_CIPHERSUITE_6

/* May be necessary to define one of the following macros when the UECC_ECC library is
used and the target is an embedded device */
//#define WATCHDOG_CONF_ENABLE 0x00000000
//#define SYS_CTRL_CONF_SYS_DIV SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ

#define LOG_CONF_LEVEL_EDHOC LOG_LEVEL_DBG
/* #define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO */
/* #define LOG_CONF_LEVEL_COAP LOG_LEVEL_INFO */
/* #define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_DBG */
/* #define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_DBG */

#endif /* PROJECT_CONF_H_ */
/** @} */
