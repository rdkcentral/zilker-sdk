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

printUsage()
{
    echo "";
    echo "*********************************************";
    echo "Usage: dump_symbols.sh <dump_sym binary> <target_dir> <binary_dir1> <binary_dir2> ...";
    echo "  dump_sym binary : path to dump_sym binary";
    echo "  target_dir      : where to put the symbols";
    echo "  binary_dirN     : directories to scan for binaries to dump symbols";
    echo "*********************************************";
    echo "";
}

# check that we got at least 3 args
#
if [ $# -lt 3 ]; then
    printUsage;
    exit 1;
fi

DUMP_SYM="$1"
TARGET_DIR="$2"
shift
shift

mkdir -p "${TARGET_DIR}"

while [ -n "$1" ]; do
    echo "Scanning for binaries in $1"
    for filename in "$1"/*; do
        OUTPUT=`file "$filename" | grep ELF`
        BASE_FILENAME=`basename "${filename}"`
        if [ -n "${OUTPUT}" ]; then
            echo "Dumping symbols for ${filename}"
            "${DUMP_SYM}" "${filename}" > "${TARGET_DIR}"/"${BASE_FILENAME}".sym
            if [ $? -ne 0 ]; then
                rm "${TARGET_DIR}"/"${BASE_FILENAME}".sym
            fi
        fi
    done
    shift
done

for filename in "${TARGET_DIR}"/*.sym; do
    HASH=`head -n1 "${filename}" | cut -d ' ' -f4`
    NAME=`head -n1 "${filename}" | cut -d ' ' -f5`
    SYM_DEST_DIR="${TARGET_DIR}/${NAME}/${HASH}"
    mkdir -p "${SYM_DEST_DIR}"
    mv "${filename}" "${SYM_DEST_DIR}"
done
