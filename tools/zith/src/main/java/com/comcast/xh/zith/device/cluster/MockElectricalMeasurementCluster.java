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

public class MockElectricalMeasurementCluster extends MockCluster
{
    private int activePower = 0;
    private int multiplier = 1;
    private int divisor = 10;

    public MockElectricalMeasurementCluster(String eui64, int endpointId)
    {
        super("electricalmeasurement", eui64, endpointId, 0x0b04, true);

        addAttribute(0, 27);
        addAttribute(768, 33);
        addAttribute(1285, 33);
        addAttribute(1288, 33);
        addAttribute(1291, 41); //active power
        addAttribute(1538, 33);
        addAttribute(1539, 33);
        addAttribute(1540, 33); //ac power multiplier
        addAttribute(1541, 33); //ac power divisor
    }

    public void setWatts(int watts)
    {
        activePower = watts * divisor / multiplier;

        sendAttributeReport( 1291);
    }

    public int getWatts()
    {
        return activePower * multiplier / divisor;
    }

    private byte[] getMyBytes(int sixteenbitvalue)
    {
        byte[] result = new byte[2];
        result[0] = (byte) (sixteenbitvalue & 0xff);
        result[1] = (byte) ((sixteenbitvalue >> 8) & 0xff);

        return result;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case 1291:
            {
                result = getMyBytes(activePower);
                break;
            }

            case 1540:
            {
                result = getMyBytes(multiplier);
                break;
            }

            case 1541:
            {
                result = getMyBytes(divisor);
                break;
            }
        }

        return result;
    }
}
