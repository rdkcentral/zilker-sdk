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

#-----------------------------------------------------------
#
# simple script to create a 'basic' version file.
# expected to be ran from CMake, but overloaded later when
# performing proper packaging
#
#-----------------------------------------------------------

# check args....
#
if [ $# -lt 2 ]; then
    echo "Usage: createBasicVersion.sh <top-dir> <output-dir> [official-build-number]";
    exit 1;
fi

buildNum="SNAPSHOT"

# get the general product version from our top-level version.properties file
if [ -n "$3" ]; then
    # have a build number, therefore use the 'releaseVersion'
    # first see if our 'build number' is -d, meaning grab current date (to support XB3/XB6 scenarios)
    if [ "$3" = "-d" ]; then
        buildNum=`date +"%y%m%d"`;
    else
        buildNum=$3;
    fi
    prodVersion=`cat ${1}/version.properties | grep '^releaseVersion' | awk -F= '{ print $2 }'`
else
    # no build number, therefore use the 'productVersion'
    prodVersion=`cat ${1}/version.properties | grep '^productVersion' | awk -F= '{ print $2 }'`
fi

# make a runtime file
${1}/tools/package/gen_version.sh ${2} ${prodVersion} "zilker" ${buildNum}

