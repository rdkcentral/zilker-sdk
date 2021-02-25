#!/bin/sh
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

#------------------------------------------------------------
#
# second phase of Touchstone on XB6 to launch the
# remaining services once we are enabled (via marker)
#
# assumes /opt/icontrol for the root
#
#------------------------------------------------------------

# get path information (home, conf, upg)
. /tmp/xh_env.sh

# wait for the special marker file to be present before starting
# comcast will use SNMP to create the file when they want touchstone
# to run.
#
while [ ! -f /nvram/icontrol/enabletouchstone ]; do
    sleep 15;
done

# wait for the network stack to be up-and-running
# before we start the services.  the simple way
# is to use 'ping'
#
netOk="false";
while [ "$netOk" = "false" ]; do

    # try to hit Google.  if successful, then we have a valid
    # IP, DNS is working, and the network stack is functional
    #
    echo "testing network...";
    sh $IC_HOME/bin/xhCheckNet.sh;
    if [ $? -eq 0 ]; then
        # good to go
        #
        netOk="true";
    else
        # pause, then try again
        #
        sleep 5;
    fi
done

# fill in 'macAddress' file if missing or empty
#
if [ ! -s $IC_CONF/etc/macAddress ]; then

    # try to grab the mac address
    dmcli eRT getv Device.DeviceInfo.X_COMCAST-COM_CM_MAC |  grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}' | awk '{print tolower($0)}' > /$IC_CONF/etc/macAddress

    # if it did not work the first time, sleep and keep trying
    while [ ! -s /$IC_CONF/etc/macAddress ]; do
        echo "waiting for dmcli to be ready so we can read CM MAC"
        sleep 30
        dmcli eRT getv Device.DeviceInfo.X_COMCAST-COM_CM_MAC |  grep -o -E '([[:xdigit:]]{1,2}:){5}[[:xdigit:]]{1,2}' | awk '{print tolower($0)}' > /$IC_CONF/etc/macAddress
    done
fi

# collect hardware, manufacturer, model and store in a temp file.
# used by the sysinfoHAL since there is not a direct API to use.
# NOTE: Right after bootup dmcli is not able to get data, so we must wait until it does.
#       after it works once, we can safely grab the remaining values
#
while true; do
    manuf=`dmcli eRT getv Device.DeviceInfo.Manufacturer | grep value | cut -f3 -d : | tr -d ' '`
    if [ -z "$manuf" ]; then
        echo "unable to retrieve value from dmcli, waiting and trying again"
        sleep 30;
    else
        break;
    fi
done

hwRev=`dmcli eRT getv Device.DeviceInfo.HardwareVersion | grep value | cut -f3 -d : | tr -d ' '`
model=`dmcli eRT getv Device.DeviceInfo.ModelName | grep value | cut -f3 -d : | tr -d ' '`
echo "manuf = $manuf" > /tmp/getInfo.out
echo "hwRev = $hwRev" >> /tmp/getInfo.out
echo "model = $model" >> /tmp/getInfo.out

# now that Touchstone setup is complete, start those services via watchdog
#
echo "starting touchstone..."
$IC_HOME/bin/xhServiceUtil -w -r -g touchstone


