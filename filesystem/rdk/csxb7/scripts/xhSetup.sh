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
# simple script to ensure default
# system properties are defined 
# when running on an XB7
#
# called by "watchdog" at each bootup
#
#-------------------------------

# get path information (home, conf, upg)
if [ -f /tmp/xh_env.sh ]; then
    . /tmp/xh_env.sh
fi

# create $IC_CONF/etc directory (if required)
if [ ! -d $IC_CONF/etc ]; then
    mkdir -p $IC_CONF/etc
fi

# In order to support downgrade, we must remove the arris marker file.
# This will trigger a pre-zilker firmware image to reflash the zigbee chip.
if [ -e /nvram/arris_zigbee ]; then
    rm -f /nvram/arris_zigbee
fi

# create the rdklogger logs directory (if required).  This is needed because on xb6 with simple xbb1 support we start very early
if [ ! -d /rdklogs/logs ]; then
    mkdir -p /rdklogs/logs
fi

# apply our timezone (if already set in properties)
/bin/sh $IC_HOME/bin/xhSet_tz.sh

# copy whitelist.xml if we don't have one in our config dir
if [ ! -f $IC_CONF/etc/WhiteList.xml ]; then
    cp $IC_HOME/etc/WhiteList.xml $IC_CONF/etc;
fi

if [ -d $IC_CONF/zigbeeFirmware/ota ]; then
    # clear out any existing ota file symlinks
    find $IC_CONF/zigbeeFirmware/ota -type l -exec rm {} \;
else
    mkdir -p $IC_CONF/zigbeeFirmware/ota
fi

# now create symlinks for any of our static ota files
ln -s $IC_HOME/ota-files/* $IC_CONF/zigbeeFirmware/ota

