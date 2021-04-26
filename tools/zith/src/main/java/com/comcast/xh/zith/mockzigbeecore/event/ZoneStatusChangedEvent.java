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

import com.comcast.xh.zith.device.cluster.MockZoneCluster;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class ZoneStatusChangedEvent extends ClusterCommandEvent {
    private static final byte COMMAND_ID = 0x00;
    private static final short PAYLOAD_BYTES = 6;
    private static final byte ZONE_ID = 0x01;
    private short flags;
    private byte zoneId;

    public ZoneStatusChangedEvent(String eui64, int endpointId, byte zoneId, short flags)
    {
        super(eui64, endpointId, MockZoneCluster.CLUSTER_ID, true, COMMAND_ID);
        this.flags = flags;
        this.zoneId = zoneId;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer payload = ByteBuffer.allocate(PAYLOAD_BYTES);
        payload.order(ByteOrder.LITTLE_ENDIAN);
        // ZoneStatus
        payload.putShort(flags);
        // Extended Status (always 0 in ZCL 7)
        payload.put((byte) 0x00);
        // Zone ID
        payload.put(zoneId);
        // Delay (x250ms)
        payload.putShort((short) 0);

        return payload.array();
    }
}
