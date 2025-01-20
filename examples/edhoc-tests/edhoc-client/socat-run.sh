#!/bin/bash

# Script to connect the native client to peer outside the Docker container

# Before running:
# Switch the target IP in project-conf.h to fd00::1
# Disable the RPL_NODE functionality

# Server IP address (internal or external)
# DEST_IP="23.97.187.154"
DEST_IP="10.200.192.35" # To host system
# DEST_IP="[fe80::42:26ff:fe66:461d]" # To host system IPv6
# DEST_IP="[2a01:4f8:190:3064::6]"

# Trap SIGINT (CTRL-C) and exit gracefully
trap "echo 'Exiting...'; exit" SIGINT

# Loop until socat can bind to the IPv6 address and forward packets
until socat -d -d UDP6-RECVFROM:5683,bind=[fd00::1],fork UDP4-SENDTO:$DEST_IP:5683; do
    echo "Waiting for tun0 interface to be available..."
    sleep 0.1
done

