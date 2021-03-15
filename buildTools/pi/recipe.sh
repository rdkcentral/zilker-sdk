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

#-------------------------------------------
#
# build script to follow the recipe of creating
# secupg and images for a Raspberry Pi
#
#-------------------------------------------

# define globall paths
BUILD_DIR=${ZILKER_SDK_TOP}/build/pi

#
# show options
#
printUsage()
{
    echo "pi_recipe:";
    echo "  options:";
    echo "  -h         : show this help";
    echo "  -t         : build 3rd-party";
    echo "  -m         : build mirror";
    echo "  -D         : build DEBUG";
    echo "  -R         : build RELEASE";
    echo "  -v         : verbose build";
    echo "";
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

    # copy branding tar into both the mirror and mini-mirror
    # (mirror will unpack the tarball)
    #
    cp ${BUILD_DIR}/staging/branding/${3}.tar ${1}/etc/branding.tar;
    cp ${BUILD_DIR}/staging/branding/${3}.tar ${2}/etc/branding.tar;
}

# check that our environment is setup propery via the
# $ZILKER_SDK_TOP/build/setup_env.sh script
#
if [ -z "${ZILKER_SDK_TOP}" ]; then
    echo "ERROR: iControl Build Environment not setup.  Please source \$ZILKER_SDK_TOP/build/setup_env.sh";
    echo "";
    exit 1;
fi

# define defaults
#
set -e;
doThirdParty=0;
doMirror=0;
doDebugBuild=0;
doReleaseBuild=0;
doVerbose=0

if [ $# -eq 0 ]; then
    printUsage;
    exit 1;
fi

# parse options
#
OPTIND=1    # reset getops
while getopts "htmvDR" opt; do
    case "$opt" in
    h)  printUsage;
        exit 0;
        ;;

    t)  doThirdParty=1;
        ;;

    m)  doMirror=1;
        ;;

    D)  doDebugBuild=1;
        ;;

    R)  doReleaseBuild=1;
        ;;

    v)  doVerbose=1;
        ;;

    *)  echo "ERROR: unknown option.  use -h for usage";
        exit 1;
        ;;
    esac
done

# start
#
echo "---  build started at `date`  ---";

# build up the args to pass to 'setup_build'
#
buildTypeArg="";
if [ $doDebugBuild -eq 1 ]; then
    buildTypeArg="-D";
elif [ $doReleaseBuild -eq 1 ]; then
    buildTypeArg="-R";
fi

verboseOpt=""
if [ $doVerbose -eq 1 ]; then
    verboseOpt="v"
fi

buildThirdArg="";
if [ $doThirdParty -eq 1 ]; then
    buildThirdArg="-d";
fi

buildMirrorArg="";
if [ $doMirror -eq 1 ]; then
    buildMirrorArg="-b";
fi

# pass along to setup_build
#
cd ${ZILKER_SDK_TOP};
./setup_build.sh -rp${verboseOpt} ${buildTypeArg} ${buildThirdArg} ${buildMirrorArg} pi;

# copy specific files that are not part of CMake
export BUILD_MODEL="pi"
export BUILD_DIR="${ZILKER_SDK_TOP}/build/${BUILD_MODEL}"
buildDir=${ZILKER_SDK_TOP}/build/${BUILD_MODEL}
if [ -d ${buildDir}/mirror/include ]; then
    cp ${buildDir}/include/*.h ${buildDir}/mirror/include;
fi

