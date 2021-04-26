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

import com.comcast.xh.zith.device.cluster.MockCluster;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class AlarmClusterEvent extends ClusterCommandEvent
{
    private static final int ALARM_CLUSTER_ID = 0x9;
    private static final int ALARM_COMMAND_ID = 0x0;
    private static final int ALARM_CLEAR_COMMAND_ID = 0x2;
    private MockCluster cluster;
    private byte alarmCode;

    public AlarmClusterEvent(MockCluster cluster, byte alarmCode)
    {
        this(cluster, alarmCode, false);
    }

    public AlarmClusterEvent(MockCluster cluster, byte alarmCode, boolean isClear)
    {
        super(cluster.getEui64(), cluster.getEndpointId(), ALARM_CLUSTER_ID, true, isClear ? ALARM_CLEAR_COMMAND_ID : ALARM_COMMAND_ID);
        this.cluster = cluster;
        this.alarmCode = alarmCode;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer buf = ByteBuffer.allocate(3);
        buf.order(ByteOrder.LITTLE_ENDIAN);
        buf.put(alarmCode);
        buf.putShort((short)cluster.getClusterId());
        return buf.array();
    }


}
