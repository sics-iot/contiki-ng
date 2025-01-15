# Standalone Native RPL Border Router

This example contains two projects: `6lbr`, `6lr-with-slip`, and
`6lr`.

The following simulations demonstrate how it works.

* `examples/standalone-native-rpl-border-router/lite-tsch-orchestra.csc`
* `tests/17-tun-rpl-br/10-standalone-native-6lbr-lite-tsch.csc`
* `tests/17-tun-rpl-br/11-standalone-native-6lbr-lite-csma.csc`
* `tests/17-tun-rpl-br/12-standalone-native-6lbr-classic-tsch.csc`
* `tests/17-tun-rpl-br/13-standalone-native-6lbr-classic-csma.csc`
* `tests/17-tun-rpl-br/14-standalone-native-6lbr-lite-csma-frag.csc`

# How to Use
1. Flush a `6lr-with-slip` firmware to a radio board having a serial connection with a Linux host
1. Flush a `6lr` firmware to each of other radio boards
1. Run a `6lbr` executable on the Linux host

You will need the MAC address of the radio board running
`6lr-with-slip` to start `6lbr`. Assume the MAC address is
`00:01:00:01:00:01:00:01`, `6lbr` can be run via `make`:

```
$ make -C 6lbr connect-router MAC_ADDR_OF_DOWNLINK_PEER=00:01:00:01:00:01:00:01
```
or
```
$ make -C 6lbr connect-router-cooja MAC_ADDR_OF_DOWNLINK_PEER=00:01:00:01:00:01:00:01
```

You can run `6lbr/6lbr.native` directly, of course. `6lbr/6lbr.native
-h` gives command line options.

## Node Types
### 6LBR (`6lbr`)

`6lbr` provides a **standalone** RPL border router, called 6LBR
(6LoWPAN Border Router) here, for the `native`
platform. **Standalone** means that a resulting executable
(`6lbr.native`) works as a RPL border router without any external
dependency such as `slip-radio`.

6LBR has two interfaces: tun and slip. Both connect to an IPv6 network
having the same prefix of `fd00::0/64`. While normal
(uncompressed/bare) IPv6 packets are exchanges on the tun interface,
IPv6 packets are compressed and could have extention headers for RPL
over the slip interface. 6LBR is responsible for the
compression/decompression and RPL-related extention header operations.

### First Hop 6LR (`6lr-with-slip`)

`6lr-with-slip` is meant to be run on a physical device, referred as a
"first-hop 6LR (6LoWPAN Router)", which has a serial connection with a
6LBR. This acts as a RPL router on an IEEE 802.15.4 coordinator.

The first-hop 6LR receives DIOs from 6LBR via the slip interface to
join the DODAG.

### 6LR (`6lr`)

`6lr` is for a RPL router using only an IEEE 802.15.4 radio for its
network access. That is, `6lr` is an ordinary IEEE 802.15.4 node
acting as a RPL router.

## Architecture
### Network Interface Level

```
      <IEEE 802.15.4 PHY>  +-------+  <SLIP>  +------+
 PAN <---------------------+ First +----------+ 6LBR |       +----> Backhaul Network
                           | Hop   |          +--+---+       |
                           | 6LR   |             | <TUN>     | <Ethernet, WiFi, or ...>
                           +-------+          +--+-----------+--+
                                              | Host OS (Linux) |
                                              +-----------------+
```
### Layer 1 / Layer 2

```
                           +-------+
                           | First |
    +-----+                | Hop   |     +------+   +-----------------+
    | 6LR |                | 6LR   |     | 6LBR |   | Host OS (Linux) |
    +--+--+                ++----+-+     ++----++   ++------+---------+
       |                     |   |        |    |     |      |
  +----+---------------------++ ++--------++  ++-----++   +-+------------------...
  |     IEEE 802.15.4 MAC     | | (SLIP)   |  | (TUN) |   | Ethernet, WiFi, or ...
  +---------------------------+ +----------+  +-------+   .
  |     IEEE 802.15.4 PHY     | | Serial   |              .
  +---------------------------+ +----------+              .
```
### Layer 3

```
                           +-------+
                           | First |
    +-----+                | Hop   |      +------+   +-----------------+
    | 6LR |                | 6LR   |      | 6LBR |   | Host OS (Linux) |
    +--+--+                ++----+-+      ++----++   ++------+---------+
       |                     |   |        |     |     |      |
  +----+---------------------+---+--------+-----+-----++   +-+------------------...
  | IPv6 (fd00::/64)         |   |        |            |   | Ethernet, WiFi, or ...
  | +--+---------------------+---+--------++           +   .
  | |              RPL/6LoWPAN             |           |   .
  | +--------------------------------------+           |   .
  +----------------------------------------------------+
```
