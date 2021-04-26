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

#---------------------------------------------------------
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
serial=`dmcli eRT getv Device.DeviceInfo.SerialNumber | grep value | cut -f3 -d : | tr -d ' '`
echo "manuf = $manuf" > /tmp/getInfo.out
echo "hwRev = $hwRev" >> /tmp/getInfo.out
echo "model = $model" >> /tmp/getInfo.out
echo "serial = $serial" >> /tmp/getInfo.out
