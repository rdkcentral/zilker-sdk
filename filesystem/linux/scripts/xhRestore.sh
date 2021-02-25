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
# expects 3 arguments:
#  1 - temp-dir to restore to
#  2 - path of downloaded archive
#  3 - premiseId
#
#-------------------------------

if [ $# -ne 3 ]; then
	echo "xhRestore.sh : missing arguments";
	exit 1;
fi

# save vars
tempDir=$1;
archive=$2;
premiseId=$3;

# get path information (home, conf, upg)
if [ -f /tmp/xh_env.sh ]; then
    . /tmp/xh_env.sh;
fi

if [ -f "${archive}" ]; then

    mkdir -p ${tempDir};
    cd $tempDir;

    # untar file into 'tempDir'
    tar -zxf ${archive};
    rc=$?;

    # cleanup
    rm -f ${archive};
else
    echo "xhRestore.sh : error missing archive!";
    rc=1;
fi

exit $rc;

# todo: decrypt if USE_GPG_BINARY is defined

