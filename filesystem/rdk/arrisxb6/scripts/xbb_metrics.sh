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
# simple script to log battery metrics
#
#------------------------------------------------------------
BATTERY_LOG=/rdklogs/logs/BatteryLog.txt.0

# Wait for gateway to bootup
sleep 5m

while true; do
    # Query battery install status
    isBatteryInstalled=`dmcli eRT getv Device.X_CISCO_COM_MTA.Battery.Installed | awk '{if(/value:/) print $5}'`
    echo "Checking Battery status. Current time: "`date` >> $BATTERY_LOG
    echo "Device.X_CISCO_COM_MTA.Battery.Installed:"${isBatteryInstalled} >> $BATTERY_LOG

    # if battery was installed log battery metrics
    if [ ${isBatteryInstalled} = "false" ]; then
        # Battery is not installed. Exit.
        echo "Battery is not installed" >> $BATTERY_LOG
        break
    elif [ ${isBatteryInstalled} = "true" ]; then
        # Adjust poweredDeviceIdlePower1 for Arris XB6 to 4250 mW
        idlePower1=`dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.poweredDeviceIdlePower1 | awk '{if(/value:/) print $5}'`
        if [ "${idlePower1}" != "4250" ]; then
            echo -n "Adjust poweredDeviceIdlePower1 from ${idlePower1} mW to 4250 mW" >> $BATTERY_LOG
            dmcli eRT setv Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.poweredDeviceIdlePower1 uint 4250 >> $BATTERY_LOG
        fi

        # Battery is installed. Log battery metrics
        echo -n "Device.X_CISCO_COM_MTA.Battery.PowerStatus:" >> $BATTERY_LOG
        dmcli eRT getv Device.X_CISCO_COM_MTA.Battery.PowerStatus | awk '{if(/value:/) print $5}' >> $BATTERY_LOG
        dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup. | grep -o -E '(Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.*|value:(.)*)' | sed ':a;N;$!ba;s/\nvalue:\s/:/g' >> $BATTERY_LOG

        # add xbb metrics script to cron.daily
        echo "#! /bin/sh" > /etc/cron/cron.daily/xbb_daily_metrics.sh
        echo "echo 'Logging Battery Metrics. Current time: ' \`date\` >> /rdklogs/logs/BatteryLog.txt.0" >> /etc/cron/cron.daily/xbb_daily_metrics.sh
        echo "dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup. | grep -o -E '(Device.DeviceInfo.X_RDKCENTRAL-COM_batteryBackup.*|value:(.)*)' | sed ':a;N;\$!ba;s/\nvalue:\s/:/g' >> /rdklogs/logs/BatteryLog.txt.0" >> /etc/cron/cron.daily/xbb_daily_metrics.sh
        chmod 700 /etc/cron/cron.daily/xbb_daily_metrics.sh
        break
    else
        # Battery install status is currently not available. Retry after 30 minutes.
        sleep 30m
    fi
done
