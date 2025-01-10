# Ephemeral Diffie-Hellman Over COSE (EDHOC) [RFC9528]

The [RFC9528] IETF Internet - RFC specifies Ephemeral Diffie-Hellman Over COSE (EDHOC), a very compact, and lightweight authenticated Diffie-Hellman key exchange with ephemeral keys that provides mutual authentication, perfect forward secrecy, and identity protection. It uses COSE [RFC8152] for cryptography, CBOR [RFC7049] for encoding, and CoAP [RFC7252] for transport and the main use case is to establish an OSCORE security context. The EDHOC exchange and the key derivation follow known protocol constructions such as [SIGMA], NISTSP-800-56A and HKDF [RFC5869].

## EDHOC in Contiki-NG
The Contik-NG EDHOC module implements asymmetric key authentication by using static Diffie-Hellman keys. The authentication is provided by a Message Authentication Code (MAC) computed from an ephemeral-static ECDH shared secret which enables significant reductions in message sizes. The implementation has passed the interoperability test successfully in the IETF-Hackathon 110.

EDHOC consists of three messages (MSG1, MSG2 and, MSG3), plus an EDHOC error message (MSG_ERR) where each of them is a CBOR sequence. The current implementation transports these messages as an exchange of Confirmable CoAP [RFC725] messages where the CoAP client is the EDHOC Initiator and the
CoAP server is the EDHOC Responder. The MSG1 and MSG3 are transferred in POST requests and MSG2 in a 2.04 (Changed) response to the Uri-Path: 
"/.well-known/edhoc". When MSGs size is bigger than 64B the Block-Wise transfer mechanism [RFC7959] for fragmentation is being used.

Notice that the authentication keys must be established at the EDHOC key storage before running the EDHOC protocol. For this reason, an edhoc-key-storage.h() API function is provided in order to set the COSE_key with the correct struct format.
At the configuration file, the credential type used for authentication must be selected. Two types have been implemented:
- `EDHOC_CRED_KID` : The EDHOC exchanging a unique identity of the public authentication key to be retrieved. Before running the EDHOC protocol each party need at least a DH-static public key and a set of identities which is allowed to communicate with.
- `EDHOC_CRED_INCLUDE` : The EDHOC exchanging messages which include directly the actual credential (DH-static public key) formatted as a CCS (CWT Claims Set). The EDHOC protocol can runs without prior knowledge of the other peer. Each peer provisionally accepts the credentials of the other party until posterior authentication and verification.

### Supported functionality

The implementation supports the following features of EDHOC:
- Cipher Suites: 2, 3
- Methods: 0, 1, 2, 3
- Credential Details:
   - Identifier: KID (single byte)
   - Inclusion: By reference
   - Type: CCS
- Message_4: No
- EAD Items: None
- Message flows: Only forward message flow
- Connection Identifiers: Only single bytes (excluding empty CBOR byte string)

### EDHOC configuration
The following macro must be defined on the configuration file:
- Define the KID of the authentication key used on this node.
```c
#define EDHOC_AUTH_KID KID
```
If the parties have agreed on an identity beside the public key, the "subject name" can be defined instead.
```c
#define EDHOC_AUTH_SUBJECT_NAME "Node_Key_Identity"
```
- Define the role taking on the EDHOC protocol. `EDHOC_INITIATOR` for the Initiator and `EDHOC_RESPONDER` for the Responder.
```c
#define EDHOC_CONF_ROLE EDHOC_INITIATOR
```
- Define the Connection Identifier(`CID`)
```c
#define EDHOC_CONF_CID 0x20
```
- Define the EDHOC method to use
```c
#define EDHOC_CONF_METHOD EDHOC_METHOD3
```
- Define which library to use for ECDH operations. The SW library of `micro-ECC` with the macro `EDHOC_UECC_ECC` or the HW driver accelerator for cc2538 modules with the macro `EDHOC_CC2538_ECC .
```c
#define EDHOC_CONF_ECC EDHOC_CC2538_ECC
```
Additionally other paramets can be defined, every definde parameter with their default values are settingon the `edhoc-config.h` files. For example the number of attempts and the timeout can be set by:
```c
#define EDHOC_CONF_ATTEMPTS 3

#define EDHOC_CONF_TIMEOUT 10000
```
## EDHOC dependencies
The EDHOC module implementation depends on the following libraries:

### ECDH Operation
The EDHOC module need to generate the ephemeral Diffie-Hellman pairs key and the EDHOC shared secrets. The `EDHOC_CONF_ECC` macro can be defined at the config file to choose between the following ECDH libraries:
#### Micro - ECC(UECC)
A small and fast ECDH and ECDSA SW implementation for 8-bit, 32-bit, and 64-bit processors. Implements five standard NIST curves, with `secp256r1` among them. The library is implemented in C and can be optionally optimised for either speed or code size at compilation time.
The specific external repository is added as a submodule in `os/net/security/uecc` folder.
- Author: K. Mackay
- Link:[MicroECC] (https://github.com/kmackay/micro-ecc/tree/601bd11062c551b108adbb43ba99f199b840777c) 
### cc2538-ecc-algo cc2538 ECC
Algorithms(CC2538)
This is a implementation of ECDH, ECDSA sign and ECDSA verify. It uses `ecc-driver`, the river for the cc2538 ECC mode of the PKC engine, to communicate with the PKA. It uses continuations to free the main CPU/thread while the PKA is calculating. It address the hardware acceleration of the ECDH cryptography operations.

### CBOR(Concise Binary Object Representation)
The EDHOC module uses CBOR to encode the EDHOC exchanging messages. The `cbor` Contiki-NG implementation has been added from `group-oscore` in `lib/cbor` folder.
- Author:Martin Gunnarsson
- Link:[group-oscore] (https://github.com/Gunzter/contiki-ng/tree/group_oscore/os/net/app-layer/coap/oscore-support)

A `cbor_put_bytes_identifier()` function has been added in order to cover the EDHOC protocol requirements.

### COSE module
The EDHOC module uses COSE_Encrypt0 and COSE_Sign1 from [RFC8152] for cryptography and signing as well as the COSE_key format. The required COSE functionality has been implemented in a lightweight module under `os/net/security/cose `.

## EDHOC Client API
The EDHOC - client - API.h file contains the EDHOC interface to be used by the EDHOC Initiator role as CoAP client.

- `edhoc_client_run()` : Runs the EDHOC Initiator role. This function must be called from the EDHOC Initiator program to start the EDHOC protocol as Initiator. Runs a new process that implements all the EDHOC protocol and exits when the EDHOC protocol finishes successfully or the EDHOC_CONF_ATTEMPTS are exceeded.
- `edhoc_client_callback()` : This function checks the events trigger from the EDHOC client process and returns '1' when the EDHOC protocol successfuly ends, -1 when the EDHOC protocol max retries are exceeded, and 0 when the EDHOC client process is steel running.
- `edhoc_client_close()` : This function must be called after the Security Context is exported to free the allocated memory.
- Optionally some functionalities for getting and setting the Aditional Application Data of the EDHOC messages are provided.

Once the EDHOC Client process is successfully finished, the security context can be exported by using the ehdoc - exporter.h API. For example:
- `edhoc_exporter_oscore.h()` : This function is used to derive an OSCORE Security Context[RFC8613] from the EDHOC shared secret.

### EDHOC Client Example
An EDHOC Client Example is provided at `examples/edhoc-tests/edhoc-test-client.c `.
For the specific example the EDHOC Server IP must be selected on the project-conf file, its own Node Key Identity and, the EDHOC role as Initiator:

- Define the Server IP address working as Responder
```c
#define EDHOC_CONF_SERVER_EP "coap://[fd00::201:1:1:1]" /* Server IP for works in Cooja simulator */
```
- Define the ROLEY as Initiator
```c
#define EDHOC_CONF_ROLE EDHOC_INITIATOR
```

## EDHOC Server API
The EDHOC - server - API.h file contains the EDHOC interface to be used by the EDHOC Responder as CoAP Server

- `edhoc_server_process()` : This function must be called from a CoAP response POST handler to run the EDHOC protocol Responder party process.
- `edhoc_server_callback()` :  This function checks the events trigger from the EDHOC server protocol looking for the `SERV_FINSHED` event.
- `edhoc_server_init()` : This function activates the EDHOC well-known CoAP Resource (that runs edhoc_server_resource() functionality) at the Uri-Path defined on the EDHOC_WELL_KNOWN macro (`./well-known.edhoc` by default).
- `edhoc_server_start()` : This function gets the DH-static authentication pair keys of the Server by using the edhoc-key-storage API, creates a new EDHOC context and generates the DH-ephemeral key for the specific session. A new EDHOC protocol session must be created for each new EDHOC client transaction.
- `edhoc_server_close()` : This function must be called after the Security Context is exported to free the allocated memory.
- `edhoc_server_restart()` : This function Rest the EDHOC context to initiate a new EDHOC protocol session with a new client.

- Optionally some functionalities for getting and setting the Additional Application Data of the EDHOC messages are provided.

From every client that the EDHOC server side is successfully finished, the security context to be used with the specific client can be exported by using the EDHOC - exporter.h API. For example:
-`edhoc_exporter_oscore.h()` : This function is used to derive an OSCORE Security Context [RFC8613] from the EDHOC shared secret.

### EDHOC Server Example

An EDHOC Server Example is provided at `examples/edhoc-tests/edhoc-test-server.c ` together with the corresponding EDHOC plug test resource at
`examples/edhoc-tests/res-edhoc.c`. The specific example runs the EDHOC Responder protocol role on the CoAP server.

The Server Identity must be selected at:
```c
#define EDHOC_AUTH_SUBJECT_NAME "Server_key_identity"
```

## EDHOC Tests
The EDHOC implementation has been tested running both `example\edhoc-test\edhoc-test-client.c ` and `example\edhoc-test\edhoc-test-server.c ` on Cooja and Zolertia RE-Mote platforms. In the latter case, it is mandatory to either set the 32Mhz Clock or else disable the watchdog when the[MicroECC] library is using. Instead when the CC2538 ECC module is used there is not need of increase the CPU clock frequency or disable the watchdog.

```c
#define SYS_CTRL_CONF_SYS_DIV SYS_CTRL_CLOCK_CTRL_SYS_DIV_32MHZ

#define WATCHDOG_CONF_ENABLE 0x00000000
```
