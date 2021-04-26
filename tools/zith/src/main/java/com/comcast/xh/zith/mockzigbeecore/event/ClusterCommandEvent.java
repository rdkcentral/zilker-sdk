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
// Created by tlea on 1/9/19.
//

/*
{
	"timestamp":	"1546960002082",
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

import java.util.Base64;

import static com.comcast.xh.zith.device.MockZigbeeDevice.HA_PROFILE;

public class ClusterCommandEvent extends Event
{
    private String eui64;
    private int profileId;
    private int endpointId;
    private int clusterId;
    private boolean fromServer;
    private boolean mfgSpecific;
    private int mfgId;
    private int commandId;
    private int seqNum;
    private int apsSeqNum;
    private int rssi;
    private int lqi;

    private static int seqNumCounter = 1;

    public static final int defaultRssi = -50;
    public static final int defaultLqi = 255;

    public ClusterCommandEvent(String eui64, int profileId, int endpointId, int clusterId, boolean fromServer, boolean mfgSpecific, int mfgId, int commandId, int rssi, int lqi)
    {
        super("clusterCommandReceived");

        this.eui64 = eui64;
        this.profileId = profileId;
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.fromServer = fromServer;
        this.mfgSpecific = mfgSpecific;
        this.mfgId = mfgId;
        this.commandId = commandId;
        this.rssi = rssi;
        this.lqi = lqi;

        this.seqNum = getNextSeqNum();
        this.apsSeqNum = this.seqNum;
    }

    public ClusterCommandEvent(String eui64, int profileId, int endpointId, int clusterId, boolean fromServer, boolean mfgSpecific, int mfgId, int commandId)
    {
        this(eui64, profileId, endpointId, clusterId, fromServer, mfgSpecific, mfgId, commandId, defaultRssi, defaultLqi);
    }

    public ClusterCommandEvent(String eui64, int endpointId, int clusterId, boolean fromServer, int commandId)
    {
        this(eui64, HA_PROFILE, endpointId, clusterId, fromServer, false, 0, commandId);
    }

    public String getEui64()
    {
        return eui64;
    }

    public int getProfileId()
    {
        return profileId;
    }

    public int getSourceEndpoint()
    {
        return endpointId;
    }

    public int getClusterId()
    {
        return clusterId;
    }

    public int getDirection()
    {
        return fromServer ?  1 : 0;
    }

    public boolean getMfgSpecific()
    {
        return mfgSpecific;
    }

    public int getMfgCode()
    {
        return mfgId;
    }

    public int getCommandId()
    {
        return commandId;
    }

    public int getRssi()
    {
        return rssi;
    }

    public int getLqi()
    {
        return lqi;
    }

    public int getSeqNum()
    {
        return seqNum;
    }

    public int getApsSeqNum()
    {
        return apsSeqNum;
    }

    public String getEncodedBuf()
    {
        String result = "";

        byte[] payload = getCommandPayload();

        if(payload != null)
        {
            result = Base64.getEncoder().encodeToString(payload);
        }

        return result;
    }

    //this is typically the only thing needing to be overridden
    protected byte[] getCommandPayload()
    {
        return null;
    }

    private static int getNextSeqNum()
    {
        //legacy sensors seqNum goes from 1 to 242
        if(++seqNumCounter >= 243)
        {
            seqNumCounter = 1;
        }

        return seqNumCounter;
    }
}
