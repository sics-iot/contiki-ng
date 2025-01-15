#!/bin/bash

# Contiki directory
CONTIKI=$1

# Simulation file
BASENAME=$(basename $0 .sh)

WAIT_TIME=180

bash test-standalone-native-6lbr.sh $CONTIKI $BASENAME MAKE_ROUTING_RPL_CLASSIC fd00::204:4:4:4 $WAIT_TIME
