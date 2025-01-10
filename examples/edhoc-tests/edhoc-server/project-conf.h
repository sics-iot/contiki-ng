#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LPM_CONF_MAX_PM 1

#define EDHOC_CONF_TIMEOUT 100000

/* Mandatory EDHOC definitions on Server */
/* Define one kind of the following kind of identification for the authentication key */
//#define EDHOC_AUTH_SUBJECT_NAME "Serv_A"
//#define EDHOC_AUTH_KID 0x32

/* Define a value for the Connection Identifier */
// #define EDHOC_CONF_CID 0x20
// #define EDHOC_CONF_CID -8
#define EDHOC_CONF_CID 0x27

// Large size to avoid usage of block-wise
#define COAP_MAX_CHUNK_SIZE 300

// Set default creds (not interoping)
#define DEFAULT_CREDS 1

#if DEFAULT_CREDS == 1
#define EDHOC_AUTH_KID 0x32
#elif INTEROP_CREDS_SIGN == 1
#define EDHOC_AUTH_KID 0x09
#elif INTEROP_CREDS_DH == 1
#define EDHOC_AUTH_KID 0x0a
#endif

/* Define the party role on the EDHOC protocol as responder and the correlation method */
#define EDHOC_CONF_ROLE EDHOC_RESPONDER /* Server */

/* To run with the test vector DH ephemeral keys used on the interoperability session */
#define EDHOC_CONF_TEST EDHOC_TEST_VECTOR_TRACE_DH

/* Define the authentication */
#define EDHOC_CONF_AUTHENT_TYPE EDHOC_CRED_KID

/* Define the library for ECDH operations */
//#define EDHOC_CONF_ECC EDHOC_ECC_CC2538
#define EDHOC_CONF_ECC EDHOC_ECC_UECC

/* To tell EDHOC server example to start as network root */
#ifndef IS_NETWORK_ROUTING_ROOT
#define IS_NETWORK_ROUTING_ROOT 1
#endif /* IS_NETWORK_ROUTING_ROOT */

/* Set the supported cipher suites */
#define EDHOC_CONF_SUPPORTED_SUITE_1 EDHOC_CIPHERSUITE_2

/* May be necesary to define one of the following macros when the UECC_ECC library is
used and the target is an embedded device */
//#define WATCHDOG_CONF_ENABLE 0x00000000
//#define SYS_CTRL_CONF_SYS_DIV SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ

#define LOG_CONF_LEVEL_EDHOC LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_COAP LOG_LEVEL_DBG
/*#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_DBG */
//#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_DBG

#endif /* PROJECT_CONF_H_ */
