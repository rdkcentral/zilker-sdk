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

#---------------------------------------------------------
# Utility functions to support build scripts, recipes, etc.
#---------------------------------------------------------

# Check that a list of environment variables are not empty, and exit when one is missing
# Example: checkEnv "VAR_1" "VAR_2" …
checkEnv()
{
    MISSING=0
    for e in $@; do
        if [ -z "${!e}" ]; then
            echo "$0: ERROR: Environment variable "$e" is empty"
            MISSING=1
        fi
    done

    if [ "$MISSING" -ne 0 ]; then
        exit 1
    fi
}
checkEnv "BUILD_DIR"

BRAND_OOB_DIR="${BUILD_DIR}/staging/branding/oob"

#
# combine all of the property definition json files into a single
# json document.  Argument is the mirror destination directory
#
combinePropertyDefs()
{
    echo "creating property type definitions...";
    mkdir -p ${1}/etc;
    OUTFILE=${1}/etc/propertyTypeDefs.json
    rm -f ${OUTFILE}
    touch ${OUTFILE}
    echo "[" > ${OUTFILE}
    fileCount=0;
    for dir in `find ${ZILKER_SDK_TOP}/source -type d -name propDefs -print`; do
        for file in `echo ${dir}/*`; do
            if [ $fileCount -ne 0 ]; then
                echo "," >> ${OUTFILE}
            fi
            cat ${file} >> ${OUTFILE}
            fileCount=`expr $fileCount + 1`;
        done
    done;
    echo "]" >> ${OUTFILE}
}

#
# create miniturized version of a mirror to have some.
# files available within the .secupg archive.
# needs arguments of 'source dir' and 'dest dir'
#
createMiniMirror()
{
    echo "  setting up mini-mirror...";

    # first create the subdirs we want
    #
    mkdir -p ${2}/bin;
    mkdir -p ${2}/etc;
    mkdir -p ${2}/stock/actions;

    # copy min amount of data needed for createPackage.sh
    #
    cp ${1}/bin/install* ${2}/bin/;
    cp ${1}/bin/env.sh ${2}/bin/;
    cp ${1}/etc/version ${2}/etc/;
    cp ${1}/stock/actions/masterActionList.xml ${2}/stock/actions/;

    # copy branding tar into both the mirror and mini-mirror and extract to mirror
    #
    cp ${BUILD_DIR}/staging/branding/${3}.tar ${1}/etc/branding.tar;
    cp ${BUILD_DIR}/staging/branding/${3}.tar ${2}/etc/branding.tar;
    . ${ZILKER_SDK_TOP}/filesystem/createBrandDefaults.sh ${3} ${BUILD_DIR}/staging/branding ${1}/etc;

    # Install 'mirror' OOB contents
    SRC=${BRAND_OOB_DIR}/${BRAND}/mirror;
    find ${BRAND_OOB_DIR} -maxdepth 3 -path '*/mirror/*' -type d -exec sh -c 'rm -rf -- ${0}/`basename "{}"`' ${1} \;
    if [ -d "${SRC}" ]; then
        echo "Installing ${SRC} -> ${1}";
        cp -a "${SRC}"/* ${1};
    fi
}
