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

import com.comcast.xh.zith.device.capability.TemperatureMeasurable;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * TODO: Stub!
 */
public class MockTemperatureMeasurementCluster extends MockCluster implements TemperatureMeasurable
{
    public static final int CLUSTER_ID = 0x0402;
    public static final int MEASURED_TEMPERATURE_ATTRIBUTE_ID = 0x0;
    private short measuredTemp = 2600;

    public MockTemperatureMeasurementCluster(String eui64, int endpointId)
    {
        super("temperatureMeasurement", eui64, endpointId, CLUSTER_ID, true);

        //Measured Value (1/100 °C) -273.15..327.67 °C (0x954d..0x7fff)
        addAttribute(MEASURED_TEMPERATURE_ATTRIBUTE_ID, DataType.INT16);
        //Min Measured Value
        addAttribute(0x1, DataType.INT16);
        //Max Measured Value
        addAttribute(0x2, DataType.INT16);
        // Tolerance (uncertainty)
        addAttribute(0x3, DataType.UINT16);
    }

    public void setMeasuredTemperature(short temp)
    {
        measuredTemp = temp;
    }

    public short getMeasuredTemperature()
    {
        return measuredTemp;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case MEASURED_TEMPERATURE_ATTRIBUTE_ID:
            {
                ByteBuffer buf = ByteBuffer.allocate(Short.BYTES);
                buf.order(ByteOrder.LITTLE_ENDIAN);
                buf.putShort(measuredTemp);
                result = buf.array();
                break;
            }
        }

        return result;
    }
}
