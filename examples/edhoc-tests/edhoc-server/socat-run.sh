#!/bin/bash

# Allow connections from outside and forward to native server
# Needs Docker to have port 5683 published when launched
sudo socat -d -d UDP4-RECVFROM:5683,bind=172.17.0.2,fork UDP6-SENDTO:[fd00::302:304:506:708]:5683

