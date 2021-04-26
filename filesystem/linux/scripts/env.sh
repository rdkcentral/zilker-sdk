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

#-------------------------------
#
# define the paths for the development runtime 
# environment (OS X & Linux).
#
#-------------------------------

# base paths for running on a Raspberry Pi
IC_HOME=/opt/zilker
IC_CONF=/opt
UPG_CACHE=/tmp/upg
export IC_HOME IC_CONF UPG_CACHE
export PATH=$PATH:$IC_HOME/bin
export ZB_OTA_DIR=$IC_CONF/zigbeeFirmware/ota

# libraries
export LD_LIBRARY_PATH=$IC_HOME/lib:$LD_LIBRARY_PATH

# if possible, load the TZ variable
# for all processes/scripts to inherit
#
if [ -f /tmp/TZ ]; then
    # set variable to the contents of that file
    #
    export TZ=`cat /tmp/TZ`;
    export TZFILE="/tmp/TZ"
fi
