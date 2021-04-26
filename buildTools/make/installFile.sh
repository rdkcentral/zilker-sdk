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
# sucks to have a script to perform the linux 'install' command,
# but CMake doesn't help since the CMake 'INSTALL' command will
# not resolve linked files.
#
#-----------------------------------------------------------

# check args....
#
if [ $# -lt 2 ]; then
    echo "Usage: installFile.sh <input-filename> <output-filename> [octal perms]";
    exit 1;
fi

if [ -f $1 ]; then
    # file exists, copy it
    if [ -n "$3" ]; then
        # install with specific mode (preserve time)
        install -p -m $3 $1 $2;
    else
        # install (preserve time)
        install -p $1 $2;
    fi
fi

