#!/usr/bin/env bash
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

#---------------------------------------
#
# This is a WORK IN PROGRESS.  The idea is
# to launch a zilker binary and run through
# automated tests using the ZITH harness
#
#---------------------------------------

usage() {
  echo "Usage: $0 [-b ZilkerBinPath] [-m mirrorPath] [-t testClass]"
  exit 1
}

CUSTOM_ZITH_PATH=""
ZITH_TEST_ARG=""
while getopts ":b:m:t:" o; do
    case "${o}" in
        b)
            CUSTOM_ZITH_PATH="${CUSTOM_ZITH_PATH} -Dzith.zilker.bin.path=${OPTARG}"
            ;;
        m)
            CUSTOM_ZITH_PATH="${CUSTOM_ZITH_PATH} -Dzith.mirror.dir.path=${OPTARG}"
            ;;
        t)
            ZITH_TEST_ARG="${ZITH_TEST_ARG} --tests ${OPTARG}"
            ;;
        *)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

# Helper function for Darwin
function myreadlink() {
  (
  cd $(dirname $1)         # or  cd ${1%/*}
  echo $PWD/$(basename $1) # or  echo $PWD/${1##*/}
  )
}

if [ "$(uname)" == "Darwin" ]; then
    SCRIPT=$(myreadlink "$0")
    # attempt to set the DYLD_LIBRARY_PATH for Mac
    if [ $# -eq 2 ]; then
        export DYLD_LIBRARY_PATH=$2/lib:$DYLD_LIBRARY_PATH;
        echo "Using DYLD_LIBRARY_PATH $DYLD_LIBRARY_PATH";
    fi
else
    SCRIPT=$(readlink -f "$0")
fi

# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")

# Goto ZILKER_SDK_TOP
export ZILKER_SDK_TOP="$SCRIPTPATH/../.."
cd "$ZILKER_SDK_TOP"

# settings.gradle uses this to determine whether to include all the java projects, or just a minimal set for Clion.
# We want all the java projects so we can use our public APIs in zith
unset BUILD_TCHAIN

# TODO filter to only run CI suite
gradle --info $CUSTOM_ZITH_PATH -PjarToApk.enabled=false :zith:cleanTest :zith:test ${ZITH_TEST_ARG}
