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
// Created by tlea on 2/19/19.
//

import com.comcast.xh.zith.mockzigbeecore.event.AttributeReportEvent;

public class MockMeteringCluster extends MockCluster
{
    private int instantaneousDemand = 0;
    private int multiplier = 1;
    private int divisor = 1000;

    public MockMeteringCluster(String eui64, int endpointId)
    {
        super("metering", eui64, endpointId, 0x0702, true);

        addAttribute(0x0400, 0x2a); //instantaneous demand.  signed 24 bit
        addAttribute(0x0301, 0x22); //multiplier.  unsigned 24 bit
        addAttribute(0x0302, 0x22); //divisor.  unsigned 24 bit
    }

    public void setInstantaneousDemand(int instantaneousDemand)
    {
        this.instantaneousDemand = instantaneousDemand;
    }

    public void setMultiplier(int multiplier)
    {
        this.multiplier = multiplier;
    }

    public void setDivisor(int divisor)
    {
        this.divisor = divisor;
    }

    public void setWatts(int watts)
    {
        instantaneousDemand = watts;

        sendAttributeReport(0x0400);
    }

    public int getWatts()
    {
        return instantaneousDemand;
    }

    private byte[] getMyBytes(int twentyfourbitvalue)
    {
        byte[] result = new byte[3];
        result[0] = (byte) (twentyfourbitvalue & 0xff);
        result[1] = (byte) ((twentyfourbitvalue >> 8) & 0xff);
        result[2] = (byte) ((twentyfourbitvalue >> 16) & 0xff);

        return result;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case 0x0400: //instantaneous demand
            {
                result = getMyBytes(instantaneousDemand);
                break;
            }

            case 0x0301:
            {
                result = getMyBytes(multiplier);
                break;
            }

            case 0x0302:
            {
                result = getMyBytes(divisor);
                break;
            }
        }

        return result;
    }
}
