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

# Absolute path to this script, e.g. /home/user/src/buildTools/pi/recipe.sh
if [ "$(uname)" == "Darwin" ]; then
    SCRIPT=$(myreadlink "$0")
else
    SCRIPT=$(readlink -f "$0")
fi
piDir=$(dirname "$SCRIPT")
toolsDir=$(dirname "$piDir")
export ZILKER_SDK_TOP=$(dirname "$toolsDir")

# define globall paths
export BUILD_MODEL="pi"
export BUILD_DIR="${ZILKER_SDK_TOP}/build/${BUILD_MODEL}"
baseSubdir=build/${BUILD_MODEL}
buildDir=${ZILKER_SDK_TOP}/${baseSubdir}
mirrorDir=${buildDir}/mirror
includeDir=${mirrorDir}/include
installDir=${buildDir}/install

#
# show options
#
printUsage()
{
    echo "pi_recipe:";
    echo "  options:";
    echo "  -h         : show this help";
    echo "  -t         : build 3rd-party";
    echo "  -m         : build mirror and includes  (${baseSubdir}/mirror)";
    echo "  -i         : build installation package (${baseSubdir}/install)";
    echo "  -D         : build DEBUG";
    echo "  -R         : build RELEASE";
    echo "  -v         : verbose build";
    echo "";
}

# define defaults
#
set -e
doThirdParty=0
doMirror=0
doInstall=0
doDebugBuild=0
doReleaseBuild=0
doVerbose=0

if [ $# -eq 0 ]; then
    printUsage;
    exit 1;
fi

# parse options
#
OPTIND=1    # reset getops
while getopts "htmivDR" opt; do
    case "$opt" in
    h)  printUsage;
        exit 0;
        ;;

    t)  doThirdParty=1;
        ;;

    m)  doMirror=1;
        ;;

    i)  doInstall=1;
        doMirror=1;
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
    rm -rf ${includeDir};
fi

# pass along to setup_build
#
cd ${ZILKER_SDK_TOP};
./setup_build.sh -rp${verboseOpt} ${buildTypeArg} ${buildThirdArg} ${buildMirrorArg} pi;

# copy specific files that are not part of CMake
if [ $doMirror -eq 1 ]; then

    # first the includes outside of CMake
    mkdir -p ${includeDir};
    cp ${buildDir}/include/icBuildtime.h ${includeDir};

    # now the generated headers
    list=`find build/generated -name public -type d`;
    for x in $list; do
        cp -r ${x}/* ${includeDir};
    done
fi

# potentially create the installation package
if [ $doInstall -eq 1 ]; then

    # create temp area
    rm -rf ${installDir}/tmp;
    mkdir -p ${installDir}/tmp;

    # build and package ZITH
    cd ${ZILKER_SDK_TOP};
    gradle -PjarToApk.enabled=false :zith:installDist;
    (cd ${ZILKER_SDK_TOP}/tools/zith/build/install/zith ; tar -czf ${installDir}/tmp/zith.tgz *);

    # create tar of the runtime code
    (cd ${mirrorDir} ; tar -czf ${installDir}/tmp/zilker.tgz --exclude=include *);

    # create rollup of the two + our script
    cp -a ${ZILKER_SDK_TOP}/buildTools/pi/installZilker.sh ${installDir};
    (cd ${installDir}/tmp ; tar -czf ${installDir}/zilker_pi.tgz zith.tgz zilker.tgz);

    # cleanup
    rm -rf ${installDir}/tmp;
fi
