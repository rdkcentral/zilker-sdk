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
	"timestamp":	"1546960013604",
	"eui64":	"000d6f00015d8757",
	"eventType":	"clusterCommandReceived",
	"sourceEndpoint":	1,
	"profileId":	260,
	"direction":	0,
	"clusterId":	1280,
	"commandId":	3,
	"mfgSpecific":	true,
	"mfgCode":	4256,
	"seqNum":	1,
	"apsSeqNum":	249,
	"rssi":	-13,
	"lqi":	255,
	"encodedBuf":	"BAAEAREAABMAHWAJABI8AAEAAAAAAAQ="
}
 */

import com.comcast.xh.zith.device.cluster.MockLegacySecurityCluster;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class LegacySecurityDeviceStatusEvent extends LegacyApsAckedCommandEvent
{
    /**
     * Basic device status (endpoint 0)
     */
    private static final int BASIC_STATUS_LEN = 9;
    /**
     * Extra data is included when the endpoint is != 0
     */
    private static final int EXTRA_STATUS_LEN = 13;

    private MockLegacySecurityCluster cluster;

    public LegacySecurityDeviceStatusEvent(String eui64,
                                           int endpointId,
                                           MockLegacySecurityCluster cluster,
                                           boolean isCheckin)
    {
        super(eui64,
              endpointId,
              isCheckin ? 0x5 : 0x4,
              -13,
              255);

        this.cluster = cluster;
    }

    public LegacySecurityDeviceStatusEvent(String eui64,
                                           int endpointId,
                                           MockLegacySecurityCluster cluster)
    {
        this(eui64, endpointId, cluster, false);
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer payload = ByteBuffer.allocate(getSourceEndpoint() == 0 ? BASIC_STATUS_LEN : EXTRA_STATUS_LEN);
        payload.order(ByteOrder.BIG_ENDIAN);

        short temp = cluster.getTemperature();

        payload.put(cluster.getDeviceNumber())
               .put(cluster.getStatus())
               .putShort(cluster.getBatteryVoltage())
               .put(cluster.getRssi())
               .put(cluster.getLqi())
               .put((byte)(temp/100))  // Whole degrees
               .put((byte)((temp % 100)/10)); // Fractional degrees

        if (getSourceEndpoint() != 0)
        {
            //TODO: getDelayQS
            payload.putShort((byte) 0);
            //TODO:  getRetryCount
            payload.put((byte) 0);
            //TODO: getRejoinCount
            payload.put((byte) 0);
        }

        return payload.array();
    }
}
