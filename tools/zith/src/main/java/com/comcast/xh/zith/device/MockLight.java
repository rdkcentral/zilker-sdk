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
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.device.cluster.*;

public class MockLight extends MockZigbeeDevice
{
    private MockOnOffCluster onOffCluster;
    private MockElectricalMeasurementCluster electricalMeasurementCluster;
    private MockMeteringCluster meteringCluster;
    private boolean useMeteringCluster;
    private MockLevelCluster levelCluster;

    public MockLight(String name, String eui64, boolean useMeteringCluster)
    {
        super(name, eui64, true, true);

        this.useMeteringCluster = useMeteringCluster;

        MockEndpoint endpoint = new MockEndpoint(getEui64(), 1, 0x0104, 0x0101, 0, "CentraLite", "4257050-ZHAC", 3);
        endpoint.addCluster(new MockIdentifyCluster(getEui64(), 1));
        endpoint.addCluster(new MockGroupsCluster(getEui64(), 1));
        endpoint.addCluster(new MockScenesCluster(getEui64(), 1));

        onOffCluster = new MockOnOffCluster(getEui64(), 1);
        endpoint.addCluster(onOffCluster);

        levelCluster = new MockLevelCluster(getEui64(), 1);
        endpoint.addCluster(levelCluster);
        endpoint.addCluster(new MockDiagnosticsCluster(getEui64(), 1));
        endpoint.addCluster(new MockOtaUpgradeCluster(getEui64(), 1, 0x15005010));

        if(useMeteringCluster)
        {
            meteringCluster = new MockMeteringCluster(getEui64(), 1);
            endpoint.addCluster(meteringCluster);
        }
        else
        {
            electricalMeasurementCluster = new MockElectricalMeasurementCluster(getEui64(), 1);
            endpoint.addCluster(electricalMeasurementCluster);
        }

        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    public MockLight(String name, boolean useMeteringCluster)
    {
        this(name, null, useMeteringCluster);
    }

    public MockLight(String name)
    {
        this(name, null,false);
    }

    public MockLight(String name, String eui64)
    {
        this(name, eui64,false);
    }

    public boolean isOn()
    {
        return onOffCluster.isOn();
    }

    public void turnOn()
    {
        onOffCluster.setOn(true);
        System.out.println("Light " + getName() + " is now on");
    }

    public void turnOff()
    {
        onOffCluster.setOn(false);
        System.out.println("Light " + getName() + " is now off");
    }

    public void toggle()
    {
        onOffCluster.setOn(!onOffCluster.isOn());
        System.out.println("Light " + getName() + " is now " + (onOffCluster.isOn() ? "on" : "off"));
    }

    public void setLightLevel(int level)
    {
        levelCluster.setLevel((level*255)/100);
        System.out.println("Light " + getName() + " is now level " + levelCluster.getLevel() + " or " + level + " percent.");
    }

    public int getOnLevel()
    {
        return levelCluster.getOnLevel();
    }

    //returns the level as a percentage
    public int getLevel()
    {
        int level = levelCluster.getLevel();
        int percentage = (level * 100)/255;
        if((level * 100) % 255 > 127)
        {
            percentage++;
        }
        return percentage;
    }

    public void setWatts(int watts)
    {
        if(useMeteringCluster)
        {
            meteringCluster.setWatts(watts);
        }
        else
        {
            electricalMeasurementCluster.setWatts(watts);
        }

        System.out.println("Light " + getName() + " now at power " + watts);
    }

    public int getWatts()
    {
        if(useMeteringCluster)
        {
            return meteringCluster.getWatts();
        }
        else
        {
            return electricalMeasurementCluster.getWatts();
        }
    }
}
