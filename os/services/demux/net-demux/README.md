# Introduction

`net-demux` is meant to be used in a project for `native` platform,
which runs on a Linux host. This module demultiplexes output packets
to a SLIP interface and a TUN interface.

# How to Use

Add the following two lines into your project Makefile:

```
MAKE_MAC = MAKE_MAC_OTHER
MODULES += os/services/demux/net-demux
```

`NETSTACK_CONF_NETWORK` and `NETSTACK_CONF_MAC` are automatically set
by this module.

In addition, you may want to specify a MAC address for the SLIP
interface. This can be done by defininig `PLATFORM_CONF_MAC_ADDR ` in
your `project-conf.h`.

# Network Stack

```

   +-----------------------------------------------------+
   | NETSTACK_ROUTING (your choice)                      |
   +-----------------------------------------------------+
   | NETSTACK_NETWORK: net_demultiplexer_driver          |
   +-----------------------------------+-----------------+
   | net_downlink: SLIP                |                 |
   +-----------------------------------|                 |
   | sicslowpan_driver                 | net_uplink: TUN |
   +-----------------------------------|                 |
   + NETSTACK_MAC: mac_downlink_driver |                 |
   +-----------------------------------|                 |
   | Serial                            |                 |
   +-----------------------------------+-----------------|

```
