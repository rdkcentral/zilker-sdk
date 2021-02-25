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

import java.util.Base64;

public class AttributeReportEvent extends Event
{
    private String eui64;
    private int endpointId;
    private int clusterId;
    private int attributeId;
    private int attributeType;
    private byte[] payload;
    private boolean mfgSpecific;
    private int mfgCode;

    public AttributeReportEvent(String eui64, int endpointId, int clusterId, int attributeId, int attributeType, byte[] payload)
    {
        super("attributeReport");

        this.eui64 = eui64;
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.attributeId = attributeId;
        this.attributeType = attributeType;
        this.payload = payload;
    }

    public AttributeReportEvent(String eui64, int endpointId, int clusterId, int attributeId, int attributeType, byte[] payload, int mfgCode)
    {
        this(eui64, endpointId, clusterId, attributeId, attributeType, payload);
        this.mfgCode = mfgCode;

        if (this.mfgCode != 0)
        {
            this.mfgSpecific = true;
        }
    }

    public String getEui64()
    {
        return eui64;
    }

    public int getSourceEndpoint()
    {
        return endpointId;
    }

    public int getClusterId()
    {
        return clusterId;
    }

    public String getEncodedBuf()
    {
        byte[] report = new byte[payload.length+3];

        //first 2 bytes are attribute id
        report[0] = (byte)(attributeId & 0xff);
        report[1] = (byte)((attributeId >> 8) & 0xff);

        //next is attribute type
        report[2] = (byte) attributeType;

        System.arraycopy(payload, 0, report, 3, payload.length);

        return Base64.getEncoder().encodeToString(report);
    }

    public int getRssi()
    {
        return -50;
    }

    public int getLqi()
    {
        return 255;
    }

    public int getMfgCode()
    {
        return mfgCode;
    }

    public boolean isMfgSpecific()
    {
        return mfgSpecific;
    }
}