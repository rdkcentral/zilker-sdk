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
// Created by tlea on 7/18/19.
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
	"commandId":	64,
	"mfgSpecific":	true,
	"mfgCode":	4256,
	"seqNum":	5,
	"apsSeqNum":	0,
	"rssi":	-1,
	"lqi":	255,
	"encodedBuf":	"<TODO>"
}
 */

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Base64;

public class LegacyGodparentInfoMessageEvent extends LegacyApsAckedCommandEvent
{
    private byte godparentDevNum;
    private byte targetDevNum;

    public LegacyGodparentInfoMessageEvent(String eui64, int endpointId, byte godparentDevNum, byte targetDevNum)
    {
        super(eui64, endpointId, 0x40, -1, 255);

        this.godparentDevNum = godparentDevNum;
        this.targetDevNum = targetDevNum;
    }

    @Override
    public byte[] getCommandPayload()
    {
        ByteBuffer payload = ByteBuffer.allocate(12);
        payload.order(ByteOrder.BIG_ENDIAN);

        payload.put(godparentDevNum)
               .put(targetDevNum)
               .put(new byte[] {0,0,0,0,0,0,0,0}) //target EUI64 not currently used
               .put((byte)getRssi())
               .put((byte)getLqi());

        return payload.array();
    }
}
