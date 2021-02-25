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

package com.comcast.xh.zith.mockzigbeecore.event.ACE.server;

import com.comcast.xh.zith.device.cluster.MockLegacyKeypadCluster.*;
import com.comcast.xh.zith.device.cluster.MockLegacySecurityCluster;
import com.comcast.xh.zith.mockzigbeecore.event.ClusterCommandEvent;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import static com.comcast.xh.zith.device.MockZigbeeDevice.HA_PROFILE;
import static com.comcast.xh.zith.device.cluster.MockCluster.UC_MFG_ID;

public class LegacyKeypadEvent extends ClusterCommandEvent
{
    private static final short FIXED_PAYLOAD_LENGTH = 8;
    private KeypadButton buttonPushed;
    private String accessCode;
    private byte deviceNum;
    private short vBatt_mv;
    private byte tempInt;
    private byte tempFrac;

    public LegacyKeypadEvent(String eui64,
                             int endpointId,
                             byte deviceNum,
                             KeypadButton buttonPushed,
                             String accessCode,
                             short vBatt_mv,
                             byte tempInt,
                             byte tempFrac)
    {
        super(eui64,
              HA_PROFILE,
              endpointId,
              MockLegacySecurityCluster.CLUSTER_ID,
              true,
              true,
              UC_MFG_ID,
              ServerMfgCommand.KEYPAD_EVENT);

        this.buttonPushed   = buttonPushed;
        this.accessCode     = accessCode;
        this.deviceNum      = deviceNum;
        this.vBatt_mv       = vBatt_mv;
        this.tempInt        = tempInt;
        this.tempFrac       = tempFrac;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer payloadBuf = ByteBuffer.allocate(FIXED_PAYLOAD_LENGTH + accessCode.length());
        payloadBuf.order(ByteOrder.BIG_ENDIAN)
                .put(deviceNum)
                .put(buttonPushed.toByte())
                .put(accessCode.getBytes())
                .putShort(vBatt_mv)
                .put((byte) getRssi())
                .put((byte) getLqi())
                .put(tempInt)
                .put(tempFrac);

        return payloadBuf.array();
    }
}
