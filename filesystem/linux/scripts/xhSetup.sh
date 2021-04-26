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
# when running in a developers environment
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

# fill in 'macAddress' file if missing
if [ ! -f $IC_CONF/etc/macAddress ]; then
    # this is for a dev setup, so really only
    # going to be Linux or OS X
    #
    # most systems can use:
    #   /sys/class/net/eth0/address
    #
    mkdir -p $IC_CONF/etc;
    if [ "${BUILD_MODEL}" = "darwin" ]; then
        addr=`ifconfig en0 | grep 'ether' | awk '{ print $2 }'`;
        echo $addr > $IC_CONF/etc/macAddress;
    else
        DEFAULT_IFACE=`ip route | awk '/^default/ {print $5}'`
        cp /sys/class/net/${DEFAULT_IFACE}/address $IC_CONF/etc/macAddress;
    fi
fi

# copy whitelist.xml if we don't have one in our config dir
if [ ! -f $IC_CONF/etc/WhiteList.xml ]; then
    cp $IC_HOME/etc/WhiteList.xml $IC_CONF/etc;
fi

# lower stack size for our processes
# default is 8M, which is awfully large
ulimit -s 1024

# apply our timezone (if already set in properties)
/bin/sh $IC_HOME/bin/xhSet_tz.sh

# ensure upgrade location is defined in properties
$IC_HOME/bin/xhProperties -w -s -k IC_UPG_CACHE -v $UPG_CACHE &

# set property to disable coredump (if not set)
$IC_HOME/bin/xhProperties -w -s -k coredumps.save -v false -S 1 &

# clear out any existing ota file symlinks
find $IC_CONF/zigbeeFirmware/ota -type l -exec rm {} \;

# now create symlinks for any of our static ota files
ln -s $IC_HOME/ota-files/* $IC_CONF/zigbeeFirmware/ota
