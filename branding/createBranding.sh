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
#
# create tarball for each branding that can be deployed with
# the firmware.  allows for each brand to have different config
# files and default configurations.
#
# expects the arguments: <top-dir> <staging-dir> [brand]
#
#---------------------------------------------------------

# check that we got our arguments
#
if [ $# -lt 2 ]; then
	echo "Usage: createBranding.sh <top-dir> <staging-dir>";
	echo "";
	exit 1;
fi

# setup directory paths
#
BRAND_TOP=$1/branding
#THIRD_PARTY_CERTS_DIR=$1/build/3rdParty/cmake/ca-certificates
THIRD_PARTY_CERTS_DIR=$CROSS_OUTPUT/etc/security
STAGING_DIR=$2/tmp
DIST_DIR=$2/branding
BRAND_OOB="${DIST_DIR}/oob"

# clean and create our output dirs
#
rm -rf $STAGING_DIR $DIST_DIR;
mkdir -p $STAGING_DIR;
mkdir -p $DIST_DIR;
mkdir -p $BRAND_OOB;

# get list of brands to process, and save to our 'valid_brands' file
#
ls -F $BRAND_TOP | grep '.*\/$' | grep -v stock | grep -v build | awk -F/ '{ print $1 }' > $DIST_DIR/valid_brands.txt;

# see if the optional arg of 'branding' was supplied
# otherwise, create each of them
#
if [ -n "$3" ]; then
    brands=$3;
else
    brands=`cat $DIST_DIR/valid_brands.txt`;
fi

if [[ ${BUILD_MODEL} == "tca"* ]]; then
    BUILD_MODEL=tca
fi

# for each brand, create a staging area so we can copy
# the 'stock' files then overlay with the 'brand' files.
#
for x in $brands; 
do

    # create staging dir for this brand
    mkdir ${STAGING_DIR}/${x};

    # copy 'stock' files
    cp -r ${BRAND_TOP}/stock/* ${STAGING_DIR}/${x};

    # copy 'brand' files
    cp -r ${BRAND_TOP}/${x}/* ${STAGING_DIR}/${x};

    # copy any 'product' files if they exist
    if [ -d "${BRAND_TOP}/${x}/products/${BUILD_PRODUCT}" ]; then
        cp -r ${BRAND_TOP}/${x}/products/${BUILD_PRODUCT}/* ${STAGING_DIR}/${x};
    fi

    # Flatten out */models/<model>/<contents> to */<contents>, converting any links to real files, with permissions, etc.
    # These will not be archived and will instead be installed out-of-band via createBrandDefaults and CMake
    find ${STAGING_DIR}/${x} -path "*/models/${BUILD_MODEL}" -execdir rsync -arL '{}'/ ../ \;

    # Copy target brand out-of-band components to staging area. Recipes will pickup 'fsroot' and 'mirror'
    # and install contents to the root filesystem and mirror (becomes application home), respectively
    for d in ${BUILD_BRAND_OOB}; do
        SRC="${STAGING_DIR}/${x}/$d"
        if [ -d "${SRC}" ]; then
            echo "Staging $SRC -> ${BRAND_OOB}/${x}/"`basename ${SRC}`
            rsync -ar --exclude="_*_" "$SRC" \
            "${BRAND_OOB}/${x}/"
        fi
    done

    # create marker file so it is easy to distinguish the brand
    echo "${x}" > ${STAGING_DIR}/${x}/brand.txt;

    # Update our certificates from third-party (special case)
    mkdir -p ${STAGING_DIR}/${x}/security/certificates
    if [ -f $THIRD_PARTY_CERTS_DIR/ca-certs.pem ]; then
        cp $THIRD_PARTY_CERTS_DIR/ca-certs.pem ${STAGING_DIR}/${x}/security/certificates;
    fi

    # Set certificate dir permissions to read-only
    chmod 744 ${STAGING_DIR}/${x}/security/certificates;

    # handle running in the IDE or variances in Darwin
    host_os=`uname`;
    if [ "$CLION_IDE" == "TRUE" ]; then
        # don't have a BUILD_TCHAIN defined necessarily
        if [ "$host_os" == "Darwin" ]; then
            BUILD_TCHAIN="darwin";
        fi
    fi

    # special case of tar option not available on Mac
    excludeMore=" --exclude-tag-all=_IGNORE_"
    if [ "$host_os" == "Darwin" ]; then
        # Mac doesn't have that '--exclude-tag-all' as an option
        excludeMore="";
    fi

    # create archive for this brand
    (cd ${STAGING_DIR}/${x} ; tar -cf ${DIST_DIR}/${x}.tar \
    --exclude=include \
    --exclude=_README_ \
    --exclude=assets \
    --exclude=res \
    $excludeMore \
    *);

done

