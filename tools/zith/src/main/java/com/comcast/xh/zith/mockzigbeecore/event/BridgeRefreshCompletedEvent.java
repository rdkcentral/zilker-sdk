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
import com.comcast.xh.zith.device.cluster.MockBridgeCluster;

import static com.comcast.xh.zith.device.cluster.MockBridgeCluster.BRIDGE_MFG_ID;

public class BridgeRefreshCompletedEvent extends ClusterCommandEvent
{
    private static final int REFRESH_COMPLETED_COMMAND_ID = 0x01;

    public BridgeRefreshCompletedEvent(MockBridgeCluster cluster)
    {
        super(cluster.getEui64(), MockZigbeeDevice.HA_PROFILE, cluster.getEndpointId(), cluster.getClusterId(), true, true,
                BRIDGE_MFG_ID, REFRESH_COMPLETED_COMMAND_ID);
    }

    protected byte[] getCommandPayload()
    {
        return new byte[]{};
    }
}
