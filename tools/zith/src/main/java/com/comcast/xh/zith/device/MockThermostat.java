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

import com.comcast.xh.zith.device.capability.BatteryPowered;
import com.comcast.xh.zith.device.cluster.*;

public class MockThermostat extends MockZigbeeDevice implements BatteryPowered
{
    private MockThermostatCluster thermostatCluster;
    private MockFanControlCluster fanControlCluster;

    public MockThermostat(String name)
    {
        this(name, null);
    }

    public MockThermostat(String name, String eui64)
    {
        this(name, eui64, 1);
    }

    public MockThermostat(String name, String eui64, int endpointId)
    {
        super(name, eui64, false, false);

        MockEndpoint endpoint = new MockEndpoint(getEui64(), endpointId, HA_PROFILE, 0x0301, 0, "Zen Within", "Zen-01", 3);
        endpoint.addCluster(new MockPowerConfigurationCluster(getEui64(), endpointId));
        endpoint.addCluster(new MockIdentifyCluster(getEui64(), endpointId));
        endpoint.addCluster(new MockGroupsCluster(getEui64(), endpointId));
        endpoint.addCluster(new MockScenesCluster(getEui64(), endpointId));
        endpoint.addCluster(new MockPollControlCluster(getEui64(), endpointId));
        thermostatCluster = new MockThermostatCluster(getEui64(), endpointId);
        endpoint.addCluster(thermostatCluster);
        fanControlCluster = new MockFanControlCluster(getEui64(), endpointId);
        endpoint.addCluster(fanControlCluster);
        endpoint.addCluster(new MockThermostatUserInterfaceConfigurationCluster(getEui64(), endpointId));
        endpoint.addCluster(new MockDiagnosticsCluster(getEui64(), endpointId));
        // Some custom cluster with no attributes
        endpoint.addCluster(new MockCluster("ZenCustom", getEui64(), endpointId, 0xfe11, true));

        endpoint.addCluster(new MockTimeCluster(getEui64(), endpointId, false));
        endpoint.addCluster(new MockOtaUpgradeCluster(getEui64(), endpointId, 0x0000021F));

        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    @Override
    public boolean isLowBattery()
    {
        return findRequiredServerClusterOfType(BatteryPowered.class).isLowBattery();
    }

    @Override
    public void setLowBattery(boolean isLowBattery)
    {
        findRequiredServerClusterOfType(BatteryPowered.class).setLowBattery(isLowBattery);
    }

    @Override
    public void setBatteryVoltage(short newVoltage)
    {
        findRequiredServerClusterOfType(BatteryPowered.class).setBatteryVoltage(newVoltage);
    }

    @Override
    public short getBatteryVoltage()
    {
        return findRequiredServerClusterOfType(BatteryPowered.class).getBatteryVoltage();
    }

    public void setHeatSetpoint(int heatSetpoint)
    {
        thermostatCluster.setHeatSetpoint(heatSetpoint);
    }

    public int getHeatSetpoint()
    {
        return thermostatCluster.getHeatSetpoint();
    }

    public void setCoolSetpoint(int coolSetpoint)
    {
        thermostatCluster.setCoolSetpoint(coolSetpoint);
    }

    public int getCoolSetpoint()
    {
        return thermostatCluster.getCoolSetpoint();
    }

    public void setSystemMode(MockThermostatCluster.ThermostatSystemMode systemMode)
    {
        thermostatCluster.setSystemMode(systemMode);
    }

    public MockThermostatCluster.ThermostatSystemMode getSystemMode()
    {
        return thermostatCluster.getSystemMode();
    }

    public void setHoldOn(boolean holdOn)
    {
        thermostatCluster.setHoldOn(holdOn);
    }

    public boolean getHoldOn()
    {
        return thermostatCluster.getHoldOn();
    }

    public void setFanMode(MockFanControlCluster.FanMode fanMode)
    {
        fanControlCluster.setFanMode(fanMode);
    }

    public MockFanControlCluster.FanMode getFanMode()
    {
        return fanControlCluster.getFanMode();
    }
}
