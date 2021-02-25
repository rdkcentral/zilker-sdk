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

#---------------------------------------------------------
#
# cope each file from 'src-dir' into 'staging-dir' and resolving
# the soft links in place.  needed as a workaround because CMake 'install'
# does not resolve linked files
#
# expects the arguments: <src-dir> <staging-dir>
#
#---------------------------------------------------------

function cp_dir() {
    local src=${1%%/}
    local dest=${2%%/}/$(basename ${src})
    local file_list=$(find ${src}/ -maxdepth 1 ! -path ${src}/ | grep -v README.txt)

    mkdir -p ${dest}

    for arg in ${file_list}; do
        if [[ -d ${arg} ]]; then
            cp_dir ${arg} ${dest}
        else
            cp -H ${arg} ${dest}/
        fi
    done
}

if [[ $# -lt 2 ]]; then
    echo "Usage: createCopies.sh <src-dir> <staging-dir>"
    exit 1
fi

if [[ ! -d $1 ]]; then
    echo "Source argument must be a directory."
    exit 1
fi

cp_dir $1 $2
