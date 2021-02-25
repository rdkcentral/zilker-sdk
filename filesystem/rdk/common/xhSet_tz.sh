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
# set the timezone for all Zilker processes
# and scripts to utilize.
#
# can be ran with 0 args to retrieve the
# timezone from our properties file, and apply
# as the TZ environment variable.
#
# or can be ran with 1 arg, to use as the
# timezone applied into the TZ environment variable
#
#-------------------------------

# get path information (home, conf, upg)
#
if [ -f /tmp/xh_env.sh ]; then
    . /tmp/xh_env.sh
fi

if [ -z "$1" ]; then
    # ask for the CPE_TZ property.  since we're possibly called
    # during startup (before services are running), use hidden 
    # option to read the config file directly vs asking the propsService
    #
    tzKey=`xhProperties -g -k CPE_TZ -f $IC_CONF/etc/genericProps.xml`;
else
    # use CLI arg as the 'key' to apply
    #
    tzKey=$1;
fi

# now find the 'posix' value for our /tmp/TZ file
#
if [ -n "$tzKey" ]; then
    tzPosix=`grep "$tzKey" $IC_HOME/etc/tz_posix.properties | awk -F= '{ print $2 }'`;
fi

# save as TZ environment variable and place
# into /tmp/TZ file for other scripts to use
#
if [ -n "$tzPosix" ]; then
    export TZ=$tzPosix;
    echo $TZ > /tmp/TZ;
else
    export TZ="";
    rm -f /tmp/TZ;
fi


