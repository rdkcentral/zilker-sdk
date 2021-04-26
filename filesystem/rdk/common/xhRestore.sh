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
# script to download our config archive from
# the server, and unpack somewhere local.
#
# like all 'xhRestore.sh' implementations, this
# takes an optional command line argument of
# where to place the config tarball
#
# usage: xhRestore.sh [options]
#
# expects 6 arguments:
#  1 - temp-dir to restore to
#  2 - url to download archive from
#  3 - username to use during upload
#  4 - password to use during upload
#  5 - premiseId
#
#-------------------------------

if [ $# -lt 5 ]; then
	echo "xhRestore.sh : missing arguments";
	exit 1;
fi

# save vars
tempDir=$1
targetUrl=$2
user=$3
pass=$4
premiseId=$5

# get path information (home, conf, upg)
if [ -f /tmp/xh_env.sh ]; then
    . /tmp/xh_env.sh
fi

# download and unpack, notify backupService
#
mkdir -p ${tempDir}
cd ${tempDir}
curl -k -u "$user:$pass" "$targetUrl" | tar -zx

# todo: decrypt if USE_GPG_BINARY is defined

