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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class MockPRMZoneCluster extends MockZoneCluster
{
    private static final int CUSTOM_MFG_ID = 0x1022;

    private static final int ATTR_CONFIGURED_ZONE_TYPE = 0x0;
    private static final int ATTR_ZONE_CONFIGURATION = 0x1;

    public MockPRMZoneCluster(String eui64, int endpointId, ZoneType defaultZoneType)
    {
        super(eui64, endpointId, defaultZoneType);

        addMfgAttribute(CUSTOM_MFG_ID, ATTR_CONFIGURED_ZONE_TYPE, DataType.ENUM16);
        addMfgAttribute(CUSTOM_MFG_ID, ATTR_ZONE_CONFIGURATION, DataType.INT16);
        // For PRM zones don't start faulted or tampered
        isOpen = false;
        isTampered = false;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        if (info.getMfgId() != null && info.getMfgId() == CUSTOM_MFG_ID)
        {
            switch (info.getId())
            {
                case ATTR_CONFIGURED_ZONE_TYPE:
                    ByteBuffer buf = ByteBuffer.allocate(Short.BYTES);
                    buf.order(ByteOrder.LITTLE_ENDIAN);
                    buf.putShort(getZoneType().getValue());
                    result = buf.array();
                    break;
                default:
                    result = super.getAttributeData(info);
            }
        }
        else
        {
            result = super.getAttributeData(info);
        }
        return result;
    }

    @Override
    protected Status setAttributeData(AttributeInfo info, byte[] data)
    {
        Status status = Status.FAILURE;
        if (info.getMfgId() != null && info.getMfgId() == CUSTOM_MFG_ID)
        {
            switch (info.getId())
            {
                case ATTR_CONFIGURED_ZONE_TYPE:
                    ByteBuffer buf = ByteBuffer.allocate(Short.BYTES);
                    buf.order(ByteOrder.LITTLE_ENDIAN);
                    buf.put(data);
                    buf.flip();
                    short zoneTypeVal = buf.getShort();
                    for (ZoneType type : ZoneType.values())
                    {
                        if (type.getValue() == zoneTypeVal)
                        {
                            zoneType = type;
                            status = Status.SUCCESS;
                            break;
                        }
                    }
                    break;
                default:
                    status = super.setAttributeData(info, data);
            }
        }
        else
        {
            status = super.setAttributeData(info, data);
        }
        return status;
    }
}
