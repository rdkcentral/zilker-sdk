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

#-------------------------------
#
# script to install ZITH + Zilker on a Raspberry Pi device
#
#------------------------------

# define target directories
zithDir=$HOME/zith
zilkerRunDir=/opt/zilker
zilkerConfigDir=/opt/etc
zigbeeConfigDir=/opt/zigbeeFirmware

# show user what we are attempting to do
echo "Zilker Installation Script for Raspberry Pi 3 B/B+"
echo "Requires the following directories, and may leverage 'sudo' to create them."
echo " zith   : $zithDir   (requires Java 8)"
echo " zilker : $zilkerRunDir"
echo "        : $zilkerConfigDir"
echo " zigbee : $zigbeeConfigDir"

# promt for approval
echo "";
read -r -p "Ready to continue? [Y/n]" response;
if [ "$response" != "y" -a "$response" != "Y" -a "$response" != "" ]; then
    echo "Canceling...";
    exit 0;
fi

# create the directories, then ensure $USER can read/write
echo "Creating directories..."
mkdir -p $zithDir;
sudo mkdir -p $zilkerRunDir $zilkerConfigDir $zigbeeConfigDir
sudo chown -R $USER $zilkerRunDir $zilkerConfigDir $zigbeeConfigDir

# unpack
echo "Extracting files..."
PRG="$0"
here=`dirname $PRG`
tar -zxf $here/zilker_pi.tgz -C /tmp
tar -zxf /tmp/zith.tgz -C $zithDir
tar -zxf /tmp/zilker.tgz -C $zilkerRunDir

echo "Installation Complete!"
rm -f /tmp/zith.tgz /tmp/zilker.tgz
