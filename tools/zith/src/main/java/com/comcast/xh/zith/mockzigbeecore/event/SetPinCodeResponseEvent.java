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

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.device.cluster.MockDoorLockCluster;

public class SetPinCodeResponseEvent extends ClusterCommandEvent
{
    private static final short SET_PIN_CODE_RESPONSE_COMMAND_ID = 0x05;
    private byte status;

    public SetPinCodeResponseEvent(MockDoorLockCluster cluster, byte status)
    {
        super(cluster.getEui64(), MockZigbeeDevice.HA_PROFILE, cluster.getEndpointId(), cluster.getClusterId(), cluster.isServer(), false, 0, SET_PIN_CODE_RESPONSE_COMMAND_ID);
        this.status = status;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        byte[] payload = new byte[1];

        payload[0] = status;

        return payload;
    }
}
