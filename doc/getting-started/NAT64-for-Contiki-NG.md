# NAT64 for Contiki‐NG

## NAT64
NAT64 is used to allow IPv6 only devices to communicate with IPv4 devices. NAT64 combines a NAT mechanism with address translation so that a specially formatted IPv6 address is converted into a regular IPv4 address. Using this any 6LoWPAN device can connect to a IPv4 host on the Internet.

## Running NAT64 / Jool
If you want to setup NAT64 on a linux machine there is a complete implementation called Jool. If you set this
up on the same machine that is running the border-router for Contiki-NG you can get all your Contiki-NG devices to access IPv4 Internet.

### Installing Jool
Jool consists of two parts - Kernel modules, and User-space tools - both need to be installed to make use of the NAT64 Jool.

Follow the guide at https://www.jool.mx/en/install.html to install both the kernel modules, and the user tools.

### Starting up Jool
There is a script in Contiki-NG at `tools/ip64/jool-start.sh`. This will setup jool for doing NAT64 on the prefix 64:ff9b::/64 and sets up the IPv4 address of the default IPv4 interface as the IPv4 NAT address. You can use any of the online tools to convert an IPv4 address to an 64:ff9b::<IPv4> NAT64 address. Sometimes they convert to the format 0:0:0:0:0:ffff:<IPv4> - then you can just add 64:ff9b:: to the IPv4 part.

## DNS64
If you like to resolve hostnames into NAT64 addresses you can use the DNS server bind9 for the DNS64 conversion. It can either convert just the hosts that do only support IPv4 to IPv6 (NAT64 address) or convert even the hosts that already support IPv6 - if your 6LoWPAN network do have an IPv4 only connectivity.

Read more on bind installation and configuration here:
https://www.jool.mx/en/dns64.html

For converting all IPv4 addresses even the ones with support for IPv6 (AAAA entries) use the following configuration (in named.conf.options):

    # Add prefix for Jool's `pool6`
    dns64 64:ff9b::/96 {
        exclude { any; };
    } ;

## Tutorial

The LWM2M tutorial includes the setup of a NAT64 translator: [tutorial:lwm2m]

[tutorial:lwm2m]:/doc/tutorials/LWM2M-and-IPSO-Objects
