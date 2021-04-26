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

public class MockFanControlCluster extends MockCluster
{
    private static final short FAN_MODE_ATTRIBUTE_ID = 0x0;

    private FanMode fanMode = FanMode.auto;

    public enum FanMode
    {
        off((byte)0x0),
        low((byte)0x1),
        medium((byte)0x2),
        high((byte)0x3),
        on((byte)0x4),
        auto((byte)0x5),
        smart((byte)0x6);

        private byte value;

        FanMode(byte value)
        {
            this.value = value;
        }
    }

    public MockFanControlCluster(String eui64, int endpointId)
    {
        super("fancontrol", eui64, endpointId, 0x202, true);

        // Fan mode
        addAttribute(FAN_MODE_ATTRIBUTE_ID, DataType.ENUM8);
        // Fan mode sequence
        addAttribute(0x1, DataType.ENUM8);
    }

    private byte[] get8BitValue(int value)
    {
        byte[] result = new byte[1];
        result[0] = (byte) (value & 0xff);

        return result;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case FAN_MODE_ATTRIBUTE_ID:
            {
                result = get8BitValue(fanMode.value);
                break;
            }

        }

        return result;
    }

    public void setFanMode(FanMode fanMode)
    {
        if (fanMode != this.fanMode)
        {
            this.fanMode = fanMode;
            sendAttributeReport(FAN_MODE_ATTRIBUTE_ID);
        }
    }

    public FanMode getFanMode()
    {
        return fanMode;
    }
}
