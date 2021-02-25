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

public class OperationEventNotificationEvent extends ClusterCommandEvent
{
    private static final short OPERATION_EVENT_NOTIFICATION_COMMAND_ID = 0x20;
    private boolean isLocked;

    public OperationEventNotificationEvent(MockDoorLockCluster cluster, boolean isLocked)
    {
        super(cluster.getEui64(), MockZigbeeDevice.HA_PROFILE, cluster.getEndpointId(), cluster.getClusterId(), cluster.isServer(), false, 0, OPERATION_EVENT_NOTIFICATION_COMMAND_ID);
        this.isLocked = isLocked;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        byte[] payload = new byte[10];
        // Source, hardcoded to manual for right now
        payload[0] = 0x02;
        // operation event code, hardcoded to lock/unlock right now
        payload[1] = (byte)(isLocked ? 0x1 : 0x2);
        // I dunno what's in the rest, but whatever
        return payload;
    }
}
