#!/usr/bin/env bash
#
# Copyright 2021 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#

################################################################################
# Usage Notes:
# Enables routing of zhal traffic between a zilker instance and a ZITH Shell.
#
# Prerequisites:
# 1) the two instances to be network routable to one-another
# 2) copy of this script on both instances
# 3) socat available on each instance.
#
# Steps to use:
# 1) Stop ZigbeeCore on the zilker-client instance:
#    xhServiceUtil -k -s ZigbeeCore
# 2) Start ZITH Shell:
#    $ZILKER_SDK_TOP/tools/zith/zithShell.sh
# 3) Start zhalForward.sh on zithShell side:
#    $ZILKER_SDK_TOP/tools/zith/zhalForward.sh <IP of zilker-client> y
# 4) Start zhalForward.sh on zilker-client side:
#    ./zhalForward.sh <IP of ZITH>
# 5) Start zigbee in ZITH Shell:
#    zith>zigbee-start
#
# Things to note:
# 1) At this time ZITH devices are not persisted, if you stop ZITH your
# devices will be gone the next time you run ZITH.  At that point you
# will need to remove devices from zilker and pair new devices
# 2) ZITH mock devices do not respect attribute reporting, so devices
# will be marked as being in comm fail because they fail to checkin
# regularly.
################################################################################

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
   echo "Usage: zhalForward.sh <remote-ip> [Is Zith]"
   echo "Example to forward from zilker->ZITH: zhalForward.sh 192.168.0.5"
   echo "Example to forward from ZITH->zilker: zhalForward.sh 192.168.0.4 y"
   exit 1
fi

REMOTE_IP="$1"
ZITH="${2:-n}"

# Path to socat
SOCAT=`which socat`

# Determine external IP to bind to
MY_IP=$(ip route get ${REMOTE_IP} | awk '{ print $5 }')

# zhal event forwarding
if [ "$ZITH" = "y" ] || [ "$ZITH" = "Y" ]; then
   # ZITH will be sending events, so receive from localhost and unicast to remote
   ${SOCAT} UDP4-RECVFROM:8711,bind=127.0.0.1,fork,reuseaddr UDP4-SENDTO:${REMOTE_IP}:8711 &
else
   # Receive from remote and put on localhost
   ${SOCAT} UDP4-RECVFROM:8711,bind=${MY_IP},fork,reuseaddr UDP4-SENDTO:127.0.0.1:8711 &
fi

# zhal IPC forward
if [ "$ZITH" = "y" ] || [ "$ZITH" = "Y" ]; then
   # ZITH binds to all interfaces for receiving IPC, so nothing needed
   :
else
   # Receive from localhost and send to remote
   ${SOCAT} TCP-LISTEN:18443,bind=127.0.0.1,fork,reuseaddr TCP:${REMOTE_IP}:18443 & 
fi
