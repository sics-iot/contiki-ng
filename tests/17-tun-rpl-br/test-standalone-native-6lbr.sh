#!/bin/bash
source ../utils.sh

# Contiki directory
CONTIKI=$1

# Simulation file
BASENAME=$2

# MAKE variables
MAKE_ROUTING=$3

# Destination IPv6
IPADDR=$4

# Time allocated for convergence
WAIT_TIME=$5

# Payload len. Default is ping6's default, 56.
PING_SIZE=${6:-56}

# Inter-ping delay. Default is ping6's default, 1s.
PING_DELAY=${7:-1}

# ICMP request-reply count
COUNT=5

# Setting for the 6LR
SIXLR_MAC_ADDR="00:01:00:01:00:01:00:01"
TCP_PORT=60001

# Start simulation
echo "Starting Cooja simulation $BASENAME.csc"
java -Xshare:on -jar $CONTIKI/tools/cooja/dist/cooja.jar -nogui=$BASENAME.csc -contiki=$CONTIKI > $BASENAME.coojalog &
JPID=$!
sleep 20

# Connect to the simulation
echo "Starting standalone native RPL border router"
make -C $CONTIKI/examples/standalone-native-rpl-border-router/6lbr -B MAKE_ROUTING=$MAKE_ROUTING TARGET=native > $BASENAME.6lbr.log 2>&1
sudo $CONTIKI/examples/standalone-native-rpl-border-router/6lbr/6lbr.native -p $TCP_PORT $SIXLR_MAC_ADDR >> $BASENAME.6lbr.log 2>&1 &
MPID=$!
printf "Waiting for network formation (%d seconds)\n" "$WAIT_TIME"
sleep $WAIT_TIME

# Do ping
echo "Pinging"
ping6 $IPADDR -c $COUNT -s $PING_SIZE -i $PING_DELAY | tee $BASENAME.scriptlog
# Fetch ping6 status code (not $? because this is piped)
STATUS=${PIPESTATUS[0]}
REPLIES=`grep -c 'icmp_seq=' $BASENAME.scriptlog`

echo "Closing simulation and 6lbr"
sleep 1
kill_bg $JPID
kill_bg $MPID
sleep 1
rm COOJA.testlog
rm COOJA.log

# accept less responses than requests (count)
if [ $STATUS -eq 0 ] && [ $REPLIES -gt 0 ] ; then
  printf "%-32s TEST OK\n" "$BASENAME" | tee $BASENAME.testlog;
else
  echo "==== $BASENAME.coojalog ====" ; cat $BASENAME.coojalog;
  echo "==== $BASENAME.6lbr.log ====" ; cat $BASENAME.6lbr.log;
  echo "==== $BASENAME.scriptlog ====" ; cat $BASENAME.scriptlog;

  printf "%-32s TEST FAIL\n" "$BASENAME" | tee $BASENAME.testlog;
fi

# We do not want Make to stop -> Return 0
# The Makefile will check if a log contains FAIL at the end

exit 0
