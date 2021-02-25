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


############################
#
# Script to create the version file --> icontrol/etc/version
# Assumes the Android OS artifacts have been extracted into tools/build_converge/build 
#
# Since we don't have SVN to rely on for a version, this has to be
# executed with the "-v" option to increment the version.  Generally
# done by the production build user (release build)
#
# Can be executed from tools/build_converge or tools/build_converge/package
# Ex:  package/gen_version.sh <dest-dir> -v
#
############################

# check input args
#
if [ $# -lt 3 ]; then
	echo "Usage: gen_version.sh <dest-dir> <curr-version> <feature-name> [official-build-number] [branding number] [last-compatible-version]";
	exit 1;
fi

# define output
#
OUTPUT_DIR=$1
mkdir -p ${OUTPUT_DIR}
OUTFILE=${OUTPUT_DIR}/version

# break down the <curr-version> input.  It should be formatted
# something like:
#   5.1.0.0-SNAPSHOT
#
currVersion=$2
featureName=$3
FULL_RELEASE_VER=`echo $currVersion | awk -F. '{ print $1 }'`
SERVICE_RELEASE_VER=`echo $currVersion | awk -F. '{ print $2 }'`
MAINT_RELEASE_VER=`echo $currVersion | awk -F. '{ print $3 }'`
remaining=`echo $currVersion | awk -F. '{ print $4 }'`
HOT_FIX_VER=`echo $remaining | awk -F- '{ print $1 }'`
BUILD_NUM=`echo $remaining | awk -F- '{ print $2 }'`
REV_DATE=`date +"%Y-%m-%d %H:%M:%S"`;

# look at the two optional input variables
#
if [ -n "$4" ]; then

	# see if date format
	#
	if [ "$4" = "SNAPSHOT" ]; then
		# take the build number string 'as is'
		BUILD_NUM=$4;

	elif [ $4 -gt 0 ]; then
		# take the build number 'as is'
		BUILD_NUM=$4;

	else
		# assume date format (2012-11-09T14:22:38.623-06:00)
		# first remove millis + timezone
		#
		base=`echo $4 | awk -F. '{ print $1 }'`;
		HOSTENV=`uname`;
		if [ $HOSTENV = "Darwin" ]; then
			BUILD_NUM=`date -j -f "%Y-%m-%dT%H:%M:%S" "$base" +"%y%m%d%H%M%S"`;
		else
			trim=`echo $base | sed 's/T/ /'`
			BUILD_NUM=`date -d "$trim" +"%y%m%d%H%M%S"`;
		fi
	fi
else
	BUILD_NUM="SNAPSHOT";
fi

# if a branding "positive number" was provided, simply add it to BUILD_NUM
if [ -n "$5" ]; then
    if [ $5 -gt 0 ]; then
        BUILD_NUM=$(($BUILD_NUM + $5))
    fi
fi

# if a last compatible version was provided, set it. Else set it to minimum release version
LAST_COMPATIBLE_VERSION="${FULL_RELEASE_VER}_00_00_000000_0";
if [ -n "$6" ]; then
    LAST_COMPATIBLE_VERSION=$6;
fi

# obtain build revision & date from the "curr_version" file
#
REVISION=$BUILD_NUM
if [ "$REVISION" = "SNAPSHOT" ]; then
	REVISION_LONG=$REVISION;
else
	REVISION_LONG=`echo $REVISION | awk '{ printf "%07d", $1 }'`;
fi

FULL_VER_STR="${FULL_RELEASE_VER}.${SERVICE_RELEASE_VER}.${MAINT_RELEASE_VER}.${HOT_FIX_VER}"
BUILD_TIME=`date "+%Y-%m-%d %H:%M:%S"`

# Add in feature so we we can identify
FEATURE_NAME=""
if [ "$featureName" != "master" ]; then
    FEATURE_NAME="-$featureName"
fi

echo "LONG_VERSION: ${FULL_VER_STR} (Build ${REVISION}${FEATURE_NAME}) " > $OUTFILE
echo "version: ${FULL_VER_STR}" >> $OUTFILE
echo "release_ver: ${FULL_RELEASE_VER}" >> $OUTFILE
echo "service_ver: ${SERVICE_RELEASE_VER}" >> $OUTFILE
echo "maintenance_ver: ${MAINT_RELEASE_VER}" >> $OUTFILE
echo "hot_fix_ver: ${HOT_FIX_VER}" >> $OUTFILE
echo "display_version: ${FULL_RELEASE_VER}_${SERVICE_RELEASE_VER}_${MAINT_RELEASE_VER}_${HOT_FIX_VER}_${REVISION_LONG}" >> $OUTFILE
echo "server_version: ${FULL_RELEASE_VER}_${SERVICE_RELEASE_VER}_${MAINT_RELEASE_VER}_${HOT_FIX_VER}_${REVISION}" >> $OUTFILE
echo "lastCompatibleVersion: ${LAST_COMPATIBLE_VERSION}" >> $OUTFILE

echo "svn_build: ${REVISION}" >> $OUTFILE
echo "svn_date: ${REV_DATE}" >> $OUTFILE
echo "build_id: `git describe --dirty --always`" >> $OUTFILE
echo "build_date: ${BUILD_TIME}" >> $OUTFILE
echo "build_by: `whoami`" >> $OUTFILE

