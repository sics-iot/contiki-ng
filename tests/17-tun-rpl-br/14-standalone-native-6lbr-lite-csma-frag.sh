#!/bin/bash

# Contiki directory
CONTIKI=$1

# Simulation file
BASENAME=$(basename $0 .sh)

WAIT_TIME=60
# We cannot set PING_SIZE with 1200 since such a ping packet is sent
# in 13 fragments, which cannot be handled by the default setting of
# SICSLOWPAN_FRAGMENT_BUFFERS. So, we reduce PING_SIZE to 1100 for
# this test.
PING_SIZE=1100
PING_DELAY=4

bash test-standalone-native-6lbr.sh $CONTIKI $BASENAME MAKE_ROUTING_RPL_LITE fd00::204:4:4:4 $WAIT_TIME $PING_SIZE $PING_DELAY
