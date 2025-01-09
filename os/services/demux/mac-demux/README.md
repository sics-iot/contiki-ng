# Introduction

`mac-demux` is meant to be used in a project for a radio board having
a serial connection to a Linux host. This module demultiplexes output
packets to its radio interface (IEEE 802.15.4) and a SLIP interface.

# How to Use

Add the following two lines into your project Makefile:

```
MODULES += os/services/demux/mac-demux
```

`NETSTACK_CONF_NETWORK` and `NETSTACK_CONF_MAC` are automatically set
by this module.

In addition, you need to define `MAC_UPLINK_PEER_MAC_ADDR` in your
project-conf.h with a MAC address of the peer on the SLIP interface,
which is the MAC address (of the SLIP interface) of a Contiki-NG
executable running on a Linux host.

```
#define MAC_UPLINK_PEER_MAC_ADDR  { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }
```

The MAC protocol to be used for the downlink can be specified by using
`MAKE_MAC` variable for `make`: `MAKE_MAC_TSCH` or `MAKE_MAC_CSMA`.

# Network Stack

```

   +---------------------------------------------------+
   | NETSTACK_ROUTING (your choice)                    |
   +---------------------------------------------------+
   | NETSTACK_NETWORK: sicslowpan_driver               |
   +---------------------------------------------------+
   | NETSTACK_MAC:     mac_demultiplexer_driver        |
   +----------------------------+----------------------+
   | mac_downlink: CSMA or TSCH | mac_uplink: SLIP     |
   +----------------------------|----------------------+
   | NETSTACK_RADIO             | UART                 |
   +----------------------------+----------------------+

```
