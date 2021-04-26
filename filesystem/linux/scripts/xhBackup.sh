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

#-------------------------------
#
# script to tar up our config files, 
# and upload the archive to the server.
#
# expects 6 arguments:
#  1 - config-dir to archive
#  2 - url to upload archive to
#  3 - username to use during upload
#  4 - password to use during upload
#  5 - cpeId value (for the upload form)
#  6 - configVersion value (for the upload form)
#  7 - premiseId
#
#-------------------------------

if [ $# -lt 7 ]; then
	echo "xhBackup.sh : missing arguments";
	exit 1;
fi

# save vars
confDir=$1
targetUrl=$2
user=$3
pass=$4
cpeId=$5
configVer=$6
premiseId=$7

# get path information (home, conf, upg)
if [ -f /tmp/xh_env.sh ]; then
    . /tmp/xh_env.sh
fi

# create archive
mkdir -p $UPG_CACHE
archive=$UPG_CACHE/backup_$$.tgz
cd $confDir ; tar -czf $archive .

# TODO: encrypt if USE_GPG_BINARY defined

# use curl to upload the archive 
#
curl -k -u "$user:$pass" -F "cpeId=$cpeId" -F "configVersion=$configVer" -F "cpeConfigBackup.tar.gz=@$archive" "$targetUrl"

# cleanup
#
rm -f $archive;
