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

package com.comcast.xh.zith.device;

//
// Created by tlea on 1/8/19.
//

import com.comcast.xh.zith.device.cluster.*;

public class MockLegacyDoorWindowSensor extends MockLegacySecurityDevice implements MockSensor
{
    private static final short V_BATT_LOW_MV = 2400;
    private static final short V_BATT_NOMINAL_MV = 3000;
    private MockLegacyDoorWindowCluster mockLegacySensorCluster;

    public MockLegacyDoorWindowSensor(String name)
    {
        this(name, null);
    }

    public MockLegacyDoorWindowSensor(String name, String eui64)
    {
        super(name,
              eui64,
              false,
              false,
              "SMC",
              "SMCDW01-Z",
              DeviceType.DOOR_WINDOW_1,
              0x00040004,
              (byte) 3,
              new UCTemp(19, 0),
              new UCTemp(29, 0),
              V_BATT_LOW_MV,
              (byte) (EnablesBitmap.ARMED | EnablesBitmap.MAGNETIC_SW),
              (short) 60,
              (byte) 1,
              LegacyRegion.US,
              (short) 0x0000,
              (short) 0x0004);

        MockEndpoint endpoint = new MockEndpoint(getEui64(), 1, 0x0104, 65535, 0);
        mockLegacySensorCluster = new MockLegacyDoorWindowCluster(getEui64(), 1, V_BATT_NOMINAL_MV, V_BATT_LOW_MV);
        endpoint.addCluster(mockLegacySensorCluster);
        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    public MockLegacyDoorWindowCluster getMockLegacySensorCluster()
    {
        return mockLegacySensorCluster;
    }

    public void fault()
    {
        if(mockLegacySensorCluster.isFaulted())
        {
            System.out.println("Sensor " + getName() + " is already faulted");
        }
        else
        {
            mockLegacySensorCluster.setFaulted(true);
            System.out.println("Sensor " + getName() + " is now faulted");
        }
    }

    public void restore()
    {
        if(!mockLegacySensorCluster.isFaulted())
        {
            System.out.println("Sensor " + getName() + " is already restored");
        }
        else
        {
            mockLegacySensorCluster.setFaulted(false);
            System.out.println("Sensor " + getName() + " is now restored");
        }
    }

    public void faultToggle()
    {
        if(mockLegacySensorCluster.isFaulted())
        {
            mockLegacySensorCluster.setFaulted(false);
            System.out.println("Sensor " + getName() + " is now restored");
        }
        else
        {
            mockLegacySensorCluster.setFaulted(true);
            System.out.println("Sensor " + getName() + " is now faulted");
        }
    }

    public void tamper()
    {
        mockLegacySensorCluster.setTampered(true);
        System.out.println("Sensor " + getName() + " is now tampered");
    }

    public void restoreTamper()
    {
        mockLegacySensorCluster.setTampered(false);
        System.out.println("Sensor " + getName() + " is no longer tampered");
    }

    public void tamperToggle()
    {
        if(mockLegacySensorCluster.isTampered())
        {
            mockLegacySensorCluster.setTampered(false);
            System.out.println("Sensor " + getName() + " is no longer tampered");
        }
        else
        {
            mockLegacySensorCluster.setTampered(true);
            System.out.println("Sensor " + getName() + " is now tampered");
        }
    }

    public void lowBattery()
    {
        mockLegacySensorCluster.setLowBattery(true);
        System.out.println("Sensor " + getName() + " now has a low battery");
    }

    public void restoreLowBattery()
    {
        mockLegacySensorCluster.setLowBattery(false);
        System.out.println("Sensor " + getName() + " no longer has a low battery");
    }

    public void lowBatteryToggle()
    {
        if(mockLegacySensorCluster.isLowBattery())
        {
            mockLegacySensorCluster.setLowBattery(false);
            System.out.println("Sensor " + getName() + " no longer has a low battery");
        }
        else
        {
            mockLegacySensorCluster.setLowBattery(true);
            System.out.println("Sensor " + getName() + " now has a low battery");
        }
    }

    @Override
    public void setFaulted(boolean isFaulted)
    {
        if (isFaulted)
        {
            fault();
        }
        else
        {
            restore();
        }
    }

    @Override
    public boolean isFaulted()
    {
        return mockLegacySensorCluster.isFaulted();
    }

    @Override
    public boolean isTampered()
    {
        return mockLegacySensorCluster.isTampered();
    }

    @Override
    public void setTampered(boolean isTampered)
    {
        if (isTampered)
        {
            tamper();
        }
        else
        {
            restoreTamper();
        }
    }

    @Override
    public void setMeasuredTemperature(short temp)
    {
        mockLegacySensorCluster.setTemperature(temp);
    }


    @Override
    public short getMeasuredTemperature()
    {
        return mockLegacySensorCluster.getTemperature();
    }

    @Override
    public boolean isTroubled()
    {
        return mockLegacySensorCluster.isTroubled();
    }

    @Override
    public void setTroubled(boolean isTroubled)
    {
        mockLegacySensorCluster.setTampered(isTroubled);
        System.out.println("Sensor " + getName() + " is now " + (isTroubled ? "troubled" : "trouble restored"));
    }
}
