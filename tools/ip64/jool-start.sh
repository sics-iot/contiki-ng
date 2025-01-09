#!/bin/sh -e

sudo sysctl -w net.ipv4.conf.all.forwarding=1
sudo sysctl -w net.ipv6.conf.all.forwarding=1

sudo /sbin/modprobe jool
sudo jool instance add --netfilter --pool6 64:ff9b::/96
