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

public class MockOnOffCluster extends MockCluster
{
    private static final int COMMAND_ID_ON = 0x01;
    private static final int COMMAND_ID_OFF = 0X00;

    private boolean isOn = false;

    public MockOnOffCluster(String eui64, int endpointId)
    {
        super("onoff", eui64, endpointId, 0x6, true);

        addAttribute(0, 16);
    }

    public boolean isOn()
    {
        return isOn;
    }

    public void setOn(boolean on)
    {
        if(on != isOn)
        {
            isOn = on;

            sendAttributeReport(0);
        }
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case 0:
            {
                result = new byte[1];
                result[0] = (byte) (isOn() ? 1 : 0);
                break;
            }
        }

        return result;
    }

    @Override
    public void handleCommand(int commandId, byte[] payload)
    {
        switch(commandId)
        {
            case COMMAND_ID_ON:
                setOn(true);
                break;
            case COMMAND_ID_OFF:
                setOn(false);
                break;
        }
    }
}
