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

#------------------------------------------------------------------------------
#
# Helper script to setup the "make" environment for the various
# platforms to build on.  Used to setup for 'cmake' as well as 'make'
# (priming a 3rdParty package via "configure").
#
# Expects the caller to pass in the interested 'build-target', which will be used
# to assign the BUILD_MODEL and BUILD_TOOLCHAIN variables.
#
# Requires some base environment variables and input parameters:
#   Input Environment Vars:
#     ZILKER_SDK_TOP    - top of our tree
#
#   Input Parms:
#     build-target  - limited to known targets we support; see usage below
#
#   Output Environment Vars:
#     CROSS_OUTPUT  - build output of 3rdParty libraries/binaries
#     BUILD_MODEL   - model number we are building for (should equal input parm)
#     BUILD_TCHAIN  - toolchain used for building this model number
#     BUILD_BINTYPE - binary type produced for this model number
#     CROSS_HOST    - used for 'configure' scripts
#     CROSS_HOST2   - used for 'configure' scripts using non-standard host definitions
#     PATH          - adjusted PATH for target-host compiler
#     CC            - which compiler to use
#     CFLAGS        - flags for compiling
#     LDFLAGS       - flags for linking
#     ORIG_PATH     - PATH before this script ran
#     ORIG_CC       - CC before this script ran
#     ORIG_CFLAGS   - CFLAGS before this script ran
#     ORIG_LDFLAGS  - LDFLAGS before this script ran
#
#
# NOTE: this script will setup environment variables, so it should be 'sourced' not executed.
#
# For example, to get setup for building on a Raspberry Pi:
#   . setup_env.sh pi
#
#------------------------------------------------------------------------------

showPlatforms()
{
    echo "     linux";
    echo "     pi";
    echo "     reset   (to clear all variables set)";
    echo "";
}

# check command line arguments
#
if [ $# -lt 1 ]; then
    echo "Usage: setup_env.sh <target-platform>";
    echo "   <target-platform> is one of:";
    showPlatforms;
else

    export BUILD_MODEL=$1;
    if [ -n "$2" ]; then
        savedModel=$2;
    fi

    # preserve the original variables in case scripts need to
    # compile something local to the running host (vs a cross 
    # compiler for the target)
    #
    if [ -z "$ORIG_PATH" ]; then
        export ORIG_PATH="$PATH";
        export ORIG_CC="$CC";
        export ORIG_CFLAGS="$CFLAGS";
        export ORIG_LDFLAGS="$LDFLAGS";

        # ensure ORIG_CC is set to something
        if [ -z "$ORIG_CC" ]; then
            export ORIG_CC=`which gcc`;
        fi
    fi
fi

#---------------------------
#
# reset all variables set from
# previous run of 'setup_env.sh'
#
#---------------------------
reset_vars()
{
    # unset vars adjusted via this script
    unset BUILD_MODEL;
    unset BUILD_TCHAIN;
    unset BUILD_BINTYPE;
    unset CC;
    unset CFLAGS;
    unset CROSS_CC_PATHS;
    unset CROSS_HOST;
    unset CROSS_HOST2;
    unset CROSS_LD_PATHS;
    unset LDFLAGS;
    unset SDK_TOPDIR;
    unset STAGING_DIR;
    unset COMPILER_HOST;
    unset CXXFLAGS;
    unset CPP;
    unset CXX;
    unset LD;
    unset AR;
    unset RANLIB;
    unset STRIP;
    unset SKIP_CMOCKA;

    # preserve the original variables we adjusted
    #
    if [ -n "$ORIG_CC" ]; then
        export CC=$ORIG_CC;
    fi
    if [ -n "$ORIG_CFLAGS" ]; then
        export CFLAGS=$ORIG_CFLAGS;
    fi
    if [ -n "$ORIG_LDFLAGS" ]; then
        export LDFLAGS=$ORIG_LDFLAGS;
    fi
    if [ -n "$ORIG_PATH" ]; then
        export PATH=$ORIG_PATH;
    fi
    unset ORIG_CC;
    unset ORIG_CFLAGS;
    unset ORIG_LDFLAGS;
    unset ORIG_PATH;
}

#---------------------------
#
# for building raspberry pi
# sets:
#   BUILDENV_pi     - just another linux box
#   BUILDENV_ARM    - signify ARM processor
#
# to enable this option on ubuntu:
#  sudo apt-get install build-essential
#  sudo apt-get install g++-arm-linux-gnueabihf
#  sudo apt-get install gdb-multiarch
#
#---------------------------
setup_pi()
{
    export BUILD_TCHAIN="pi";
    export CROSS_OUTPUT=${ZILKER_SDK_TOP}/build/3rdParty/${BUILD_TCHAIN};

    # look for environment variable dictating the root of the raspberry-pi tools.
    # it can be downloaded via https://github.com/raspberrypi/tools
    if [ -z "$PI_TOOLS" ]; then
        echo "Missing environment variable \$PI_TOOLS";
        echo "Please download from github https://github.com/raspberrypi/tools";
        echo "and set export PI_TOOLS to that location";
        exit 1;
    fi

    # setup the variables needed for the cross-compile
    TCHAIN=$PI_TOOLS/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf;

    export CROSS_HOST="arm-linux-gnueabihf";
    export CROSS_HOST2="linux-generic32";
    export SKIP_CMOCKA="true"

    # set compile/link flags
    #
    export CC="$TCHAIN/bin/${CROSS_HOST}-gcc";

    export CFLAGS="--sysroot=$TCHAIN/$CROSS_HOST/sysroot -O2 -fPIC -std=gnu99 -DBUILDENV_ARM -DBUILDENV_pi"
    #include rpath so 3rdParty libs have rpath in them for ubuntu 18.04 and later
    export LDFLAGS="-L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib";
    export DEBUG_COMPILE="yes";
}

#---------------------------
#
# for building against local compiler
# generally used when testing
# sets:
#   BUILDENV_linux - signify Linux OS
# 
#
#---------------------------
setup_linux()
{
    # define where output files go (--prefix when running configure)
    export BUILD_TCHAIN="linux";
    export BUILD_BINTYPE="";
    export CROSS_OUTPUT=${ZILKER_SDK_TOP}/build/3rdParty/${BUILD_TCHAIN};

    # set compile flags, using the 'dev' versions (i.e. logs)
    export CFLAGS="-fPIC -DBUILDENV_linux -std=gnu99";
    if [ -n "$JNI_INCLUDE_DIR" ]; then
        # add to compile flags since it may contain more then 1 dir
        export CFLAGS="$CFLAGS -I${JNI_INCLUDE_DIR}";
    else
        export CFLAGS="$CFLAGS -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux"
    fi
    #include rpath so 3rdParty libs have rpath in them for ubuntu 18.04 and later
    export LDFLAGS="-L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib";
    export DEBUG_COMPILE="yes";
}

#---------------------------
#
# for building against local compiler
# generally used when testing
# sets:
#   BUILDENV_mac   - signify Mac OSX
#
#---------------------------
setup_darwin()
{
    # define where output files go (--prefix when running configure)
    export BUILD_TCHAIN="darwin";
    export BUILD_BINTYPE="";
    export CROSS_OUTPUT=${ZILKER_SDK_TOP}/build/3rdParty/${BUILD_TCHAIN};

    # set compile flags, using the 'dev' versions (i.e. logs)
    export CFLAGS="-DBUILDENV_mac";
    export LDFLAGS="-L${CROSS_OUTPUT}/lib -L/usr/local/lib -L${CROSS_OUTPUT}/local/lib";
    export DEBUG_COMPILE="yes";
}

# save the 'target platform' so we can tell what
# is setup in our environment, as well as have an
# easy check in our makefiles
#
export BUILD_MODEL=$1;

# parse options
#
case "$1" in
    (-l)
        showPlatforms;
        unset BUILD_MODEL;
        ;;

    (linux)
        setup_linux;
        ;;
    
    (darwin)
        setup_darwin;
        ;;

    (pi)
        setup_pi;
        ;;

    (reset)
        reset_vars;
        ;;
    
    (*)
        echo "Unknown target-platform, use no options for usage";
        unset BUILD_MODEL;
        unset BUILD_TCHAIN;
        unset BUILD_BINTYPE;
        ;;
    
esac

