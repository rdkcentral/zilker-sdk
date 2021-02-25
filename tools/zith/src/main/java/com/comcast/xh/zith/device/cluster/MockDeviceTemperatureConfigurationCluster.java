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

package com.comcast.xh.zith.device.cluster;

import com.comcast.xh.zith.device.capability.TemperatureLevel;
import com.comcast.xh.zith.mockzigbeecore.event.AlarmClusterEvent;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class MockDeviceTemperatureConfigurationCluster extends MockCluster implements TemperatureLevel
{
    public static final int CLUSTER_ID = 0x0002;
    public static final int ATTRIBUTE_TEMPERATURE_ALARM_MASK = 0x0010;

    private static final byte DEVICE_HIGH_TEMPERATURE = 0x00;
    private boolean tempHigh = false;

    public MockDeviceTemperatureConfigurationCluster(String eui64, int endpointId)
    {
        super("deviceTemperatureConfiguration", eui64, endpointId, CLUSTER_ID, true);

        // Temp high alarm mask
        addAttribute(ATTRIBUTE_TEMPERATURE_ALARM_MASK, DataType.MAP8);
    }

    @Override
    public boolean isTemperatureHigh()
    {
        return tempHigh;
    }

    @Override
    public void setTemperatureHigh(boolean tempHigh)
    {
        if (this.tempHigh != tempHigh)
        {
            this.tempHigh = tempHigh;
            new AlarmClusterEvent(this, DEVICE_HIGH_TEMPERATURE, tempHigh == false).send();
        }
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case ATTRIBUTE_TEMPERATURE_ALARM_MASK:
            {
                result = new byte[1];
                result[0] = (byte) (tempHigh ? 1 : 0);
                break;
            }
        }

        return result;
    }

    @Override
    protected Status setAttributeData(AttributeInfo attributeInfo, byte[] data)
    {
        return Status.SUCCESS;
    }
}