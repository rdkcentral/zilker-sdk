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
# secupg and images for a Linux workstation.
#
#-------------------------------------------

# Absolute path to this script, e.g. /home/user/src/buildTools/linux/recipe.sh
if [ "$(uname)" == "Darwin" ]; then
    SCRIPT=$(myreadlink "$0")
else
    SCRIPT=$(readlink -f "$0")
fi
piDir=$(dirname "$SCRIPT")
toolsDir=$(dirname "$piDir")
export ZILKER_SDK_TOP=$(dirname "$toolsDir")

# define globall paths
export BUILD_MODEL="linux"
export BUILD_DIR="${ZILKER_SDK_TOP}/build/${BUILD_MODEL}"
DOCS_SUBDIR=build/docs
buildDir=${ZILKER_SDK_TOP}/build/${BUILD_MODEL}
mirrorDir=${buildDir}/mirror
includeDir=${mirrorDir}/include

#
# show options
#
printUsage()
{
    echo "linux_recipe:";
    echo "  options:";
    echo "  -h         : show this help";
    echo "  -t         : build 3rd-party";
    echo "  -m         : build mirror and includes";
    echo "  -d         : produce doxygen docs ($DOCS_SUBDIR)";
    echo "  -D         : build DEBUG";
    echo "  -R         : build RELEASE";
    echo "  -v         : verbose build";
    echo "  -z         : build with zith tests (work in progress)";
    echo "";
}

# define defaults
#
set -e
doThirdParty=0
doMirror=0
doDebugBuild=0
doReleaseBuild=0
doVerbose=0
doZith=0
doDocs=0

if [ $# -eq 0 ]; then
    printUsage;
    exit 1;
fi

# parse options
#
OPTIND=1    # reset getops
while getopts "htmvzdDR" opt; do
    case "$opt" in
    h)  printUsage;
        exit 0;
        ;;

    t)  doThirdParty=1;
        ;;

    m)  doMirror=1;
        ;;

    d)  doDocs=1;
        ;;

    D)  doDebugBuild=1;
        ;;

    R)  doReleaseBuild=1;
        ;;

    v)  doVerbose=1;
        ;;

    z)  doZith=1;
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

# for ZITH test execution
buildZith=""
if [ $doZith -eq 1 ]; then
    buildZith="-z"
fi

# pass along to setup_build
#
cd ${ZILKER_SDK_TOP};
./setup_build.sh -rp${verboseOpt} ${buildTypeArg} ${buildThirdArg} ${buildMirrorArg} ${buildZith} linux;

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

if [ $doDocs -eq 1 ]; then
    # finally, produce doxygen documentation
    set +e
    which doxygen > /dev/null
    if [ $? -eq 0 ]; then
        # have the tool, so generate the docs
        cd ${ZILKER_SDK_TOP};
        rm -rf ${DOCS_SUBDIR}
        doxygen buildTools/doxygen.conf;
    fi
fi
