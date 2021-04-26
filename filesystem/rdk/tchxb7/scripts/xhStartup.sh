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
# simple script to startup the watchdog
# when running on an XB7 device
#
# assumes /opt/icontrol for the root
#
#------------------------------------------------------------

# if our main RFC kill switch is off, shut down our service
RFC_ENABLED=`dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.XHFW.Enable | grep value | cut -d ":" -f 3 | tr -d ' '`
if [ "$RFC_ENABLED" = "false" ]; then
  echo "XHFW is RFC disabled.  Stopping Zilker"
  systemctl stop zilker
  exit 0
fi

# if this XB7 has an iot_init service, wait for it to run first.
if [ -f /lib/systemd/system/systemd-init_iot.service ]; then
    while [ ! -n "$(systemctl status systemd-init_iot | grep exited)" ]; do
        echo "waiting for iot initialization"
        sleep 5
    done
    echo "iot initialized"
else
    echo "iot initialization check complete"
fi

# wait until the clock is set.  It could be 1970 or even 1969 due to time zone
while [ `date +%Y` -lt 2018 ] ; do
        echo "Waiting for clock to be set"
        sleep 5
done

# get path information (home, conf, upg)
. /opt/icontrol/bin/env.sh

# for tch xb6 and xb7, source in their setup script
. /etc/tchxb6_setup.sh

# make common location for env.sh script
rm -f /tmp/xh_env.sh
ln -s /opt/icontrol/bin/env.sh /tmp/xh_env.sh

# run our initial setup script before starting anything else
#
$IC_HOME/bin/xhSetup.sh

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

$IC_HOME/bin/xhGetHalInfo.sh

# create marker indicating we just rebooted.
# normally this would be part of init.rc...
#
touch /tmp/.bootComplete

# start watchdog and pass along the paths to use.
#
$IC_HOME/sbin/xhWatchdogService -c $IC_CONF -h $IC_HOME



