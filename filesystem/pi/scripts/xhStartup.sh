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
# script to startup the watchdog when running
# on a Raspberry Pi device.
#
#-------------------------------

# get path information (home, conf, upg)
. /opt/zilker/bin/env.sh

# make common location for env.sh script
rm -f /tmp/xh_env.sh
ln -s ${IC_HOME}/bin/env.sh /tmp/xh_env.sh

# run our initial setup script before starting anything else
#
$IC_HOME/bin/xhSetup.sh

# create marker indicating we just rebooted.
# normally this would be part of init.rc...
#
touch /tmp/.bootComplete

# start watchdog and pass along the paths to use
#
$IC_HOME/sbin/xhWatchdogService -c $IC_CONF -h $IC_HOME &

