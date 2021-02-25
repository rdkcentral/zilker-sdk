/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


package com.comcast.xh.zith.mockzigbeecore.event;

//
// Created by tlea on 1/7/19.
//

/*
{
	"timestamp":	"1549915949507",
	"eui64":	"000d6f00015d8757",
	"eventType":	"clusterCommandReceived",
	"sourceEndpoint":	1,
	"profileId":	260,
	"direction":	0,
	"clusterId":	1280,
	"commandId":	15,
	"mfgSpecific":	true,
	"mfgCode":	4256,
	"seqNum":	5,
	"apsSeqNum":	0,
	"rssi":	-26,
	"lqi":	255,
	"encodedBuf":	"AQ=="
}
 */

import java.util.Base64;

public class LegacyPingMessageEvent extends LegacyApsAckedCommandEvent
{
    private int devNum;

    public LegacyPingMessageEvent(String eui64, int endpointId, int devNum, int rssi, int lqi)
    {
        super(eui64, endpointId, 15, rssi, lqi);

        this.devNum = devNum;
    }

    @Override
    public String getEncodedBuf()
    {
        byte[] payload = new byte[]
                {
                        (byte) devNum
                };

        return Base64.getEncoder().encodeToString(payload);
    }
}
