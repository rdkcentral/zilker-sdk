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

package com.comcast.xh.zith.mockzigbeecore.event.ACE.client;

import com.comcast.xh.zith.device.cluster.MockACECluster;
import com.comcast.xh.zith.device.cluster.MockACECluster.*;
import com.comcast.xh.zith.mockzigbeecore.event.ClusterCommandEvent;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Simulated command from the ACE client to arm/disarm the system
 */
public class ArmCommandEvent extends ClusterCommandEvent
{
    /**
     * This is the fixed length (mode + access code length + zone ID)
     */
    private static final int PAYLOAD_LENGTH = 3;
    private static final byte COMMAND_ID    = 0x00;

    private final ArmMode mode;
    private final byte zoneId;
    private final String accessCode;

    public ArmCommandEvent(String eui64, int endpointId, ArmMode mode, String accessCode, byte zoneId)
    {
        super(eui64, endpointId, MockACECluster.CLUSTER_ID, false, 0x00);
        this.mode = mode;
        this.accessCode = accessCode;
        this.zoneId = zoneId;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer payloadBuf = ByteBuffer.allocate(PAYLOAD_LENGTH + accessCode.length());
        payloadBuf.order(ByteOrder.LITTLE_ENDIAN)
                .put(mode.toByte())
                .put((byte) accessCode.length())
                .put(accessCode.getBytes())
                .put(zoneId);

        return payloadBuf.array();
    }
}
