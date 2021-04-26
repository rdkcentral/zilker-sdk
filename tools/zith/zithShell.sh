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
# Start an interactive shell that provides
# an environment to execute the mocked zigbee
# interface, create simulated devices, have those
# devices generate events, etc.
#
#---------------------------------------

# Helper function for Darwin
function myreadlink() {
  (
  cd $(dirname $1)         # or  cd ${1%/*}
  echo $PWD/$(basename $1) # or  echo $PWD/${1##*/}
  )
}

# Absolute path to this script, e.g. /home/user/bin/foo.sh
if [ "$(uname)" == "Darwin" ]; then
    SCRIPT=$(myreadlink "$0")
else
    SCRIPT=$(readlink -f "$0")
fi

# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")

# Goto ZILKER_SDK_TOP
cd "$SCRIPTPATH/../.."

# settings.gradle uses this to determine whether to include all the java projects, or just a minimal set for Clion.
# We want all the java projects so we can use our public APIs in zith
unset BUILD_TCHAIN

gradle -PjarToApk.enabled=false :zith:installDist && ${SCRIPTPATH}/build/install/zith/bin/zith
