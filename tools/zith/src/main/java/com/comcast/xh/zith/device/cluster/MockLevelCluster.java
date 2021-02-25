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

//
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.mockzigbeecore.event.AttributeReportEvent;

public class MockLevelCluster extends MockCluster
{
    //Command IDs
    private static final short MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID = 0x4;

    //Attribute IDs
    private static final short ATTRIBUTE_ON_LEVEL =                 0x0011;
    private static final short ATTRIBUTE_CURRENT_LEVEL =            0x0000;

    private int level;
    private int onLevel;

    public MockLevelCluster(String eui64, int endpointId)
    {
        super("level", eui64, endpointId, 0x8, true);

        //currentLevel
        addAttribute(ATTRIBUTE_ON_LEVEL, DataType.UINT8);
        addAttribute(0x0010, DataType.UINT16);
        //onLevel
        addAttribute(ATTRIBUTE_CURRENT_LEVEL, DataType.UINT8);
        addAttribute(0x0012, DataType.UINT16);
        addAttribute(0x0013, DataType.UINT16);
        addAttribute(0x0014, DataType.UINT16);

        this.level = 0;
        this.onLevel = 0;
    }

    @Override
    public void handleCommand(int commandId, byte[] payload)
    {
        if(commandId == MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID)
        {
            setLevel(payload[0]&0xFF);
        }
    }

    public void setLevel(int level)
    {
        this.level = level;
        sendAttributeReport(ATTRIBUTE_ON_LEVEL);
    }

    public void setOnLevel(int level)
    {
        this.onLevel = level;
        sendAttributeReport(ATTRIBUTE_CURRENT_LEVEL);
    }

    public int getOnLevel()
    {
        return onLevel;
    }

    public int getLevel()
    {
        return level;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case ATTRIBUTE_ON_LEVEL:
            {
                result = new byte[1];
                result[0] = (byte) (level);
                break;
            }
            case ATTRIBUTE_CURRENT_LEVEL:
            {
                result = new byte[1];
                result[0] = (byte) (onLevel);
                break;
            }
        }

        return result;
    }

    @Override
    public Status setAttributeData(AttributeInfo info, byte[] payload)
    {
        switch(info.getId())
        {
            case ATTRIBUTE_CURRENT_LEVEL:
            {
                setOnLevel(payload[0]&0xFF);
                return Status.SUCCESS;
            }
            case ATTRIBUTE_ON_LEVEL:
            {
                setOnLevel(payload[0]&0xFF);
                return Status.SUCCESS;
            }
        }
        return Status.FAILURE;
    }
}
