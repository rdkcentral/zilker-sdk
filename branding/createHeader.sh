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

#--------------------------------------------
#
# script to generate the "brands.h" file
# args: <valid_brands.txt> <outfile>
#
#--------------------------------------------

if [ $# -lt 2 ]; then
	echo "Usage: <valid_brands.txt> <outfile>";
	exit 1;
fi

# get the brands from the supplied file
#
brands=`cat $1`
OUT=$2
count=`wc -l $1 | awk '{ print $1 }'`

# create the .h file
#
echo "/* auto generated header file */" > $OUT
echo "" >> $OUT
echo "" >> $OUT
echo "#define NUM_BRANDS ${count}" >> $OUT
echo "static char VALID_BRANDS[NUM_BRANDS][25] = {" >> $OUT

for x in $brands; do

	echo "    \"${x}\"," >> $OUT

done

echo "};" >> $OUT
echo "" >> $OUT

