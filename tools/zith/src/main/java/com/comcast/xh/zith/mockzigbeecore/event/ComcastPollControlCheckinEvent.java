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
// created by jwatt on 6/3/19
//

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.device.cluster.MockPollControlCluster;

import java.nio.ByteBuffer;

public class ComcastPollControlCheckinEvent extends ClusterCommandEvent
{
    private static final int POLL_CONTROL_CHECKIN_COMMAND_ID = 0x00;

    private static final int COMCAST_MFG_ID = 0x111D;

    private static final int PAYLOAD_SIZE = 16;

    private Integer battVoltage;
    private Integer battUsedMilliAmpHr;
    private Integer temp;
    private Integer rssi;
    private Integer lqi;
    private Integer retries;
    private Integer rejoins;

    public ComcastPollControlCheckinEvent(MockPollControlCluster cluster, Integer battVoltage, Integer battUsedMilliAmpHr,
                                          Integer temp, Integer rssi, Integer lqi, Integer retries, Integer rejoins)
    {
        super(cluster.getEui64(), MockZigbeeDevice.HA_PROFILE, cluster.getEndpointId(), cluster.getClusterId(),
                cluster.isServer(), true, COMCAST_MFG_ID, POLL_CONTROL_CHECKIN_COMMAND_ID);

        this.battVoltage = battVoltage;
        this.battUsedMilliAmpHr = battUsedMilliAmpHr;
        this.temp = temp;
        this.rssi = rssi;
        this.lqi = lqi;
        this.retries = retries;
        this.rejoins = rejoins;
    }

    @Override
    public int getRssi()
    {
        return rssi;
    }

    @Override
    public int getLqi()
    {
        return lqi;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer payload = ByteBuffer.allocate(PAYLOAD_SIZE);

        payload.put((byte)(battVoltage & 0x00FF));
        payload.put((byte)((battVoltage & 0xFF00) >> 8));

        payload.put((byte)(battUsedMilliAmpHr & 0x00FF));
        payload.put((byte)((battUsedMilliAmpHr & 0xFF00) >> 8));

        payload.put((byte)(temp & 0x00FF));
        payload.put((byte)((temp & 0xFF00) >> 8));

        payload.put(rssi.byteValue());

        payload.put(lqi.byteValue());

        payload.put((byte)(retries & 0x000000FF));
        payload.put((byte)((retries & 0x0000FF00) >> 8));
        payload.put((byte)((retries & 0x00FF0000) >> 16));
        payload.put((byte)((retries & 0xFF000000) >> 24));

        payload.put((byte)(rejoins & 0x000000FF));
        payload.put((byte)((rejoins & 0x0000FF00) >> 8));
        payload.put((byte)((rejoins & 0x00FF0000) >> 16));
        payload.put((byte)((rejoins & 0xFF000000) >> 24));

        return payload.array();
    }
}
