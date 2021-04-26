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


read -d '' MSG <<"EOF"
{
	"_evId":	22,
	"_evCode":	303,
	"_evVal":	0,
	"_evTime":	1524057808,
	"DeviceServiceResourceUpdatedEvent":	{
		"rootDeviceId":	"000d6f00023dc83d",
		"rootDeviceClass":	"light",
		"resource":	{
			"DSResource":	{
				"id":	"isOn",
				"uri":	"/000d6f00023dc83d/ep/1/r/isOn",
				"ownerId":	"1",
				"ownerClass":	"light",
				"value":	"true",
				"type":	"com.icontrol.boolean",
				"mode":	3,
				"dateOfLastSyncMillis":	1524057808658
			}
		}
	},
	"_svcIdNum":	19600
}
EOF

read -d '' PATTERN <<"EOF"
{ "DeviceServiceResourceUpdatedEvent": { "resource": { "DSResource": { "uri": "/000d6f00023dc83d/ep/1/r/isOn", "value": "true" } } } }
EOF

patmatch -m "${MSG}" -p "${PATTERN}"



