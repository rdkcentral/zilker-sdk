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

#-----------------------------------------------------
#
# helper script to clear our $ZILKER_SDK_TOP/build dir
# then produce Makefiles via CMake for a particular
# 'build-model'.
#
#-----------------------------------------------------

# globals
top=`pwd`
header="--------------------------------------";

needThirdParty=0    # 1 if need to build 3rdParty for $BUILD_MODEL
needPlatformArg=0   # 1 if $BUILD_MODEL needs to be defined (or supplied in cli args)
needCrossOutput=0   # 1 if $CROSS_OUTPUT needs to be defined

#------------------------------------
#
# print script usage
#
#------------------------------------
showUsage()
{
    # TODO: fix usage
    echo "Usage: setup-build.sh [-b|-d|-P|-m|-p|-v|-D|-R|-r] [-B target-brand] [build-model]";
    echo "   -b : build core.  will also run 'unitTest' if [build-model] is 'linux'";
    echo "   -c : build with code coverage for supported platforms, e.g. 'linux'"
    echo "   -d : build 3rdParty dependencies";
    echo "   -m : generate Makefiles (no build)";
    echo "   -p : build in parallel mode";
    echo "   -v : build in verbose mode";
    echo "   -D : build for DEBUG (-g option to compiler)";
    echo "   -R : build for RELEASE";
    echo "   -r : run without prompt (for build agents)";
    echo "   -z : run zith tests"
    echo "   -B : define branding to use (default=$targetBrand)";
    echo "   -U : skip unit tests (for static analysis tools)"
    echo "   [build-model] must be one of:";
    echo "      linux, darwin, pi";
    echo "";
}

#------------------------------------
#
# print simple banner
#
#------------------------------------
showHeader()
{
    echo "";
    echo "$header";
    echo "$1";
    echo "$header";
    echo "";
}

#------------------------------------
#
# see if cmake and gcc are in PATH.
# if missing, will print the dependency then exit the script.
#
#------------------------------------
checkTools()
{
    # look for cmake
    c=`which cmake`;
    if [ $? -ne 0 ]; then
        echo "Missing 'cmake' from PATH.  Unable to continue";
        exit 1;
    fi

    # look for gcc
    c=`which gcc`;
    if [ $? -ne 0 ]; then
        echo "Missing 'gcc' from PATH.  Unable to continue";
        exit 1;
    fi

    # check java 8.  the output (on STDERR) would look something like:
    #  > java -version
    #  java version "1.8.0_171"
    #  Java(TM) SE Runtime Environment (build 1.8.0_171-b11)
    #  Java HotSpot(TM) 64-Bit Server VM (build 25.171-b11, mixed mode)
    #
    # trim down the response so we can get the version itself.  the
    # call below should yield:  "1.8.0_171"
    ver=`java -version 2>&1 | grep '1\.8' | awk '{ print $NF }'`;
    checkVer=`echo $ver | grep '1\.8'`;
    if [ -z "$checkVer" ]; then
        echo "Incorrect version of 'java'";
        echo "Require JDK 1.8, but seem to have version $ver";
        exit 1;
    fi
}

#------------------------------------
#
# see if 3rdParty was built for $BUILD_MODEL.
# if missing, wil set 'needThirdParty=1'
#
#------------------------------------
checkThirdParty()
{
    if [ ! -f ${CROSS_OUTPUT}/target_${BUILD_TCHAIN}${BUILD_BINTYPE} ]; then
        needThirdParty=1;
    else
        needThirdParty=0;
    fi
}

#------------------------------------
#
# see if $BUILD_MODEL is set.
# if missing, wil set 'needPlatformArg=1'
#
#------------------------------------
checkTargetPlatform()
{
    # first see if variable is empty
    if [ -z "${BUILD_MODEL}" ]; then
        needPlatformArg=1;
    else
        needPlatformArg=0;
    fi

    # check CROSS_OUTPUT
    if [ -z "${CROSS_OUTPUT}" ]; then
        needCrossOutput=1;
    else
        needCrossOutput=0;
    fi
}

#------------------------------------
#
# build 3rdParty
#
#------------------------------------
runBuildThirdParty()
{
    # clear out 3rdParty area, but only delete for this platform (preserve other platforms)
    #
    BUILD_DEPS_DIR=${top}/build/3rdParty
    rm -rf ${BUILD_DEPS_DIR}/cmake;
    rm -rf ${BUILD_DEPS_DIR}/${BUILD_TCHAIN}${BUILD_BINTYPE};

    # create makefiles (via cmake) for 3rdParty
    showHeader "Creating Makefiles for 3rdParty ${BUILD_TCHAIN}${BUILD_BINTYPE}";
    mkdir -p ${BUILD_DEPS_DIR}/cmake;
    cd ${BUILD_DEPS_DIR}/cmake;
    echo "cmake -G \"Unix Makefiles\" -C ${top}/buildTools/cmake/products/${BUILD_MODEL}.cmake $cmakeTchainArgs $buildDebugFlag $zithTestOptions ${top}/3rdParty"
    cmake -G "Unix Makefiles" -C ${top}/buildTools/cmake/products/${BUILD_MODEL}.cmake $cmakeTchainArgs $buildDebugFlag $zithTestOptions ${top}/3rdParty
    touch "${BUILD_TCHAIN}_cmake";

    # source in toolchain environment
    if [ -f $ZILKER_SDK_TOP/build/3rdParty/${BUILD_TCHAIN}${BUILD_BINTYPE}/toolchain.sh ]; then
        source $ZILKER_SDK_TOP/build/3rdParty/${BUILD_TCHAIN}${BUILD_BINTYPE}/toolchain.sh;
    fi

    # build 3rdParty
    set -e;
    showHeader "Building 3rdParty...";
    cd ${BUILD_DEPS_DIR}/cmake;
    make all install $buildParallelFlag $buildVerboseFlag;
    set +e;

    # strip the libraries/binaries to save space
    if [ -n "$STRIP" ]; then
        showHeader "Stripping 3rdParty libraries...";
        t=`find ${CROSS_OUTPUT}/lib ${CROSS_OUTPUT}/local/lib -type f -name "*.so*"`;
        ${STRIP} ${t};
        #${STRIP} ${CROSS_OUTPUT}/bin/*;
        #${STRIP} ${CROSS_OUTPUT}/sbin/*;
    fi

    cd ${top};
}

#------------------------------------
#
# resolve 3rdParty
#
#------------------------------------
resolveThirdParty()
{
    # only called if we need 3rdParty, but don't have it
    #
    BUILD_DEPS_DIR=${top}/build/3rdParty
    rm -rf ${BUILD_DEPS_DIR}/${BUILD_TCHAIN}${BUILD_BINTYPE};
    cd ${top}/3rdParty;
    bash ./resolve.sh ${BUILD_TCHAIN};

    cd ${top};
}

#------------------------------------
#
# generate the makesfiles (no build)
#
#------------------------------------
runCreateMakefiles()
{
    # to allow for multiple targets (and prevent the IDE from clobbering)
    # create a build directory specific to this target platform
    BUILD_DIR=${top}/build/${BUILD_MODEL};

    # wipe out BUILD_DIR
    rm -rf ${BUILD_DIR};
    mkdir -p ${BUILD_DIR};

    # support ability to overload the default compiler
    compOpt="";
    if [ -n "$MY_CC" ]; then
       compOpt="-DCMAKE_C_COMPILER=$MY_CC";
    fi
    if [ -n "$MY_CPP" ]; then
       compOpt="$compOpt -DCMAKE_CXX_COMPILER=$MY_CPP";
    fi

    # create Makefiles via CMake into the 'build' dir
    showHeader "Creating Makefiles for core ${BUILD_MODEL}";
    cd ${BUILD_DIR};
    echo "cmake -G \"Unix Makefiles\" -C ${top}/buildTools/cmake/products/${BUILD_MODEL}.cmake $cmakeTchainArgs $brandArg $buildDebugFlag $zithTestOptions $compOpt ${top}";
    cmake -G "Unix Makefiles" -C ${top}/buildTools/cmake/products/${BUILD_MODEL}.cmake $cmakeTchainArgs $brandArg $buildDebugFlag $zithTestOptions $compOpt ${top};

    # source in toolchain environment
    if [ -f $ZILKER_SDK_TOP/build/3rdParty/${BUILD_TCHAIN}${BUILD_BINTYPE}/toolchain.sh ]; then
        source $ZILKER_SDK_TOP/build/3rdParty/${BUILD_TCHAIN}${BUILD_BINTYPE}/toolchain.sh;
    fi

    cd ${top};
}

#------------------------------------
#
# build core
#
#------------------------------------
runBuildCore()
{
    runCreateMakefiles
    
    # # to allow for multiple targets (and prevent the IDE from clobbering)
    # # create a build directory specific to this target platform
    # BUILD_DIR=${top}/build/${BUILD_MODEL};

    # # wipe out BUILD_DIR
    # rm -rf ${BUILD_DIR};
    # mkdir -p ${BUILD_DIR};

    # # create Makefiles via CMake into the 'build' dir
    # showHeader "Creating Makefiles for core ${BUILD_MODEL}";
    # echo "cmake -G \"Unix Makefiles\" -C ${top}/buildTools/cmake/products/${BUILD_MODEL}.cmake $brandArg $buildDebugFlag $zithTestOptions ${top}"
    # cmake -G "Unix Makefiles" -C ${top}/buildTools/cmake/products/${BUILD_MODEL}.cmake $brandArg $buildDebugFlag $zithTestOptions ${top};

    # build code (using parallel option)

    cd "${BUILD_DIR}"
    showHeader "Building core...";
    set -e;
    make all install $buildParallelFlag $buildVerboseFlag;
    if [ "${BUILD_MODEL}" = "linux" ]; then
        . ${ZILKER_SDK_TOP}/buildTools/functions.sh
        combinePropertyDefs ${BUILD_DIR}/mirror
        if [ $doCodeCoverage -eq 0 ]; then
            if [ $doUnitTests -eq 1 ]; then
                # building for Mac or Linux, so build/run unit tests
                make unitTest $buildVerboseFlag;
            fi
        else
            make unitTestCoverage;
        fi
    fi
    set +e;
    cd ${top};
}

# Helper function for Darwin
function myreadlink() {
  (
  cd $(dirname $1)         # or  cd ${1%/*}
  echo $PWD/$(basename $1) # or  echo $PWD/${1##*/}
  )
}

# define the ZILKER_SDK_TOP variable
#
# Absolute path to this script, e.g. /home/user/src/setup_build.sh
if [ "$(uname)" == "Darwin" ]; then
    SCRIPT=$(myreadlink "$0")
else
    SCRIPT=$(readlink -f "$0")
fi
export ZILKER_SDK_TOP=$(dirname "$SCRIPT")
cd $ZILKER_SDK_TOP
showHeader "Checking environment..."

# before processing the command line arguments,
# probe the system to ensure min requirements are met
#
checkTools

# see if environment is already sourced
#
checkTargetPlatform

# from the example off the web, set the OPTIND before calling 'getopts'
OPTIND=1

# Initialize our own variables:
doBuildThirdParty=0
doBuildCore=0
doMakeOnly=0
doParallel=0
doVerbose=0
doDebugBuild=0
doReleaseBuild=0
noPrompt=0
zithTestOptions=""
targetPlatformArg=""
targetBrand="devpartner"
doCodeCoverage=0
doUnitTests=1

# process CLI args
while getopts "h?bdmpvrzDRB:U" opt; do
    case "$opt" in
    h|\?)
        showUsage;
        exit 0;
        ;;
    b)  doBuildCore=1;
        doMakeOnly=0;
        ;;
    d)  doBuildThirdParty=1;
        ;;
    m)  doMakeOnly=1;
        doBuildCore=0;
        ;;
    p)  doParallel=1;
        ;;
    v)  doVerbose=1;
        ;;
    D)  doDebugBuild=1;
        ;;
    R)  doReleaseBuild=1;
        ;;
    r)  noPrompt=1;
        ;;
    z)  zithTestOptions="-DCONFIG_DEBUG_ZITH_CI_TESTS=ON";
        ;;
    B)  targetBrand=$OPTARG;
        ;;
    U)  doUnitTests=0
	;;
    esac
done
shift $((OPTIND-1))

# Everything that's left in "$@" is a non-option.  In our case, the target platform
possibleTargetPlatform="$1"
platformArg="";

# now that we have the "requested" pieces, check against the "missing"
# pieces to see what we have to do.
# first, check BUILD_MODEL and CROSS_OUTPUT
#
if [ -n "$possibleTargetPlatform" ]; then
    # use user-supplied value
    platformArg=$possibleTargetPlatform;

elif [ -n "$BUILD_MODEL" ]; then
    # use existing environment value
    platformArg=$BUILD_MODEL;

else
    # no supplied platform and BUILD_MODEL not defined
    echo "Missing BUILD_MODEL.  Unable to continue";
    exit 1;
fi

# setup the environment for this platform, which will set CROSS_OUTPUT properly
#
cd $ZILKER_SDK_TOP
export BUILD_TCHAIN=${platformArg}
export BUILD_MODEL=${platformArg}
export CROSS_OUTPUT=${ZILKER_SDK_TOP}/build/3rdParty/${BUILD_TCHAIN};

# see if there is a toolchain cmake file for this platform
if [ -f buildTools/cmake/toolchains/${platformArg}.cmake ]; then
    cmakeTchainArgs=" -D CMAKE_TOOLCHAIN_FILE=$ZILKER_SDK_TOP/buildTools/cmake/toolchains/${platformArg}.cmake"
fi

# check 3rdParty
#
if [ $doBuildThirdParty -eq 0 ]; then

    # user didn't ask to do 3rdParty, so see if we need it
    checkThirdParty;
    if [ $needThirdParty -eq 1 ]; then

        # attempt resolve first
        resolveThirdParty;
        # check if 3rdParty is set
        checkThirdParty;
        if [ $needThirdParty -eq 1 ]; then
            # 3rdParty not built and failed to extract, so set the flag
            doBuildThirdParty=1;
        fi
    fi
fi

# make sure we're doing "something"
#
if [ $doBuildCore -eq 0 -a $doBuildThirdParty -eq 0 -a $doMakeOnly -eq 0 ]; then
    echo "Nothing to do!";
    exit 1;
fi

# show all of the things we're going to do
#
showHeader "Performing the following steps:"
if [ $doMakeOnly -eq 1 ]; then
    echo " -> generate Makefiles on \"${BUILD_MODEL}\"";
fi
if [ $doBuildCore -eq 1 ]; then
    echo " -> build core for branding \"$targetBrand\" on \"${BUILD_MODEL}\"";
fi
if [ $doBuildThirdParty -eq 1 ]; then
    echo " -> build 3rdParty dependencies for \"${BUILD_TCHAIN}\"";
fi

# potentially wait for confirmation
#
if [ $noPrompt -eq 0 ]; then
    echo "";
    read -r -p "Ready to continue? [Y/n]" response;
    if [ "$response" != "y" -a "$response" != "Y" -a "$response" != "" ]; then
        echo "Canceling...";
        exit 0;
    fi
fi

# setup common build flags
if [ $doVerbose -eq 1 ]; then
    buildVerboseFlag=" VERBOSE=1";
fi
if [ $doParallel -eq 1 ]; then
    nproc=$(nproc)

    if [ ${nproc} -gt 1 ]; then
        nproc=$((${nproc}-1))
    fi

    buildParallelFlag=" -j ${nproc}";
fi

# set DEBUG or RELEASE build type if either is defined
# TODO: right now our code won't build using "Release"
#       we have too many "unused variables"
buildDebugFlag=" -DCMAKE_BUILD_TYPE=Debug";
#if [ $doDebugBuild -eq 1 ]; then
#    buildDebugFlag=" -DCMAKE_BUILD_TYPE=Debug";
#elif [ $doReleaseBuild -eq 1 ]; then
#    buildDebugFlag=" -DCMAKE_BUILD_TYPE=Release";
#fi

# set the requested branding
if [ -n "$targetBrand" ]; then
    brandArg=" -DCONFIG_BRAND=${targetBrand}"
fi

#-----------------------------------------------------
#
# run through our tasks...
#
#-----------------------------------------------------

set -e;

if [ $doBuildThirdParty -eq 1 ]; then
    runBuildThirdParty;
fi

if [ $doMakeOnly -eq 1 ]; then
    runCreateMakefiles;

elif [ $doBuildCore -eq 1 ]; then
    runBuildCore;
fi


