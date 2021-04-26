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
# simple script to extract a specific branding tarball into
# a 'defaults' directory
#
#-----------------------------------------------------------
set -e
BRAND=$1
BRAND_STG=$2
OUTPUT=$3

# check args....
#
if [ $# -lt 3 ]; then
    echo "Usage: createBandDefaults.sh <brand> <branding-tar-dir> <output-dir>";
    exit 1;
fi

# unpack the branding tar into $OUTPUT/defaults, cleaning up anything from a previous brand
rm -rf -- ${OUTPUT}/defaults
mkdir -p ${OUTPUT}/defaults
tar -C ${OUTPUT}/defaults -xf ${BRAND_STG}/${BRAND}.tar
