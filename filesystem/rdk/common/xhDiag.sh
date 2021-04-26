#!/bin/bash
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
# collect diagnostic information and store
# in the supplied filename.
# NOTE: this is specific to XB3/XB6
#
#-------------------------------

# check args
if [ $# -lt 1 ]; then
    echo "missing argument <target-filename>";
    exit 1;
fi

# NOTE: since this is RDK, not looking at "encrypt" or "FULL vs QUICK" arguments

# get path information (home, conf, upg)
if [ -f /tmp/xh_env.sh ]; then
    . /tmp/xh_env.sh
fi

# make a temp dir to store files for the diag
OUT_DIR=/tmp/diag;
rm -rf $OUT_DIR;
mkdir -p $OUT_DIR;

# grab version information
cp $IC_HOME/etc/version $OUT_DIR;

# grab basic network info
ifconfig > $OUT_DIR/ifcfg

# copy our config files
mkdir -p $OUT_DIR/config;
cp -r $IC_CONF/etc $OUT_DIR/config;

# copy our RDK log files
if [ -f /rdklogs/logs/TouchstoneLog.txt ]; then
    cp /rdklogs/logs/TouchstoneLog.txt* $OUT_DIR;
fi

# list info about service processes
xhServiceUtil -l > $OUT_DIR/services.txt;
ps > $OUT_DIR/processes.txt;

# make sure parent dir exists
parent=`dirname $1`
if [ ! -d $parent ]; then
    mkdir -p $parent;
fi

# create output tarfile, then cleanup temp dir
cd $OUT_DIR ; tar -czf $1 *;
cd /tmp;
rm -rf $OUT_DIR;

exit 0;
