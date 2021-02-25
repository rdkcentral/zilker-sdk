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
import com.comcast.xh.zith.device.capability.Tamperable;
import com.comcast.xh.zith.device.cluster.*;
import com.comcast.xh.zith.device.cluster.MockZoneCluster.*;
import com.comcast.xh.zith.device.cluster.MockACECluster.*;

public class MockACEDevice extends MockZigbeeDevice
        implements MockPeripheral, Tamperable, BatteryPowered, MockSecurityController
{

    private MockZoneCluster zoneCluster;
    private MockACECluster aceCluster;

    public MockACEDevice(String name, String eui64, String manufacturer, String model, int hardwareVersion, long firmwareVersion, ZoneType zoneType)
    {
        super(name, eui64);

        MockEndpoint endpoint = new MockEndpoint(getEui64(), 1, HA_PROFILE, 0x401, 0, manufacturer, model, hardwareVersion);

        endpoint.addCluster(new MockPowerConfigurationCluster(getEui64(), 1));
        endpoint.addCluster(new MockIdentifyCluster(getEui64(), 1));
        endpoint.addCluster(new MockDiagnosticsCluster(getEui64(), 1));
        endpoint.addCluster(new MockOtaUpgradeCluster(getEui64(), 1, firmwareVersion));
        endpoint.addCluster(new MockPollControlCluster(getEui64(), 1));
        endpoint.addCluster(new MockTemperatureMeasurementCluster(getEui64(), 1));

        zoneCluster = new MockZoneCluster(getEui64(), 1, zoneType, false);
        aceCluster = new MockACECluster(getEui64(), 1);

        //TODO: Support panic/emergency if necessary

        endpoint.addCluster(zoneCluster);
        endpoint.addCluster(aceCluster);

        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    public MockACEDevice(String name, String manufacturer, String model, int hardwareVersion, long firmwareVersion, ZoneType zoneType)
    {
        this(name, null, manufacturer, model, hardwareVersion, firmwareVersion, zoneType);
    }

    /**
     * Simulate button pushes to arm the system
     * @param accessCode
     * @param armMode
     * @see com.comcast.xh.zith.device.cluster.MockACECluster.ArmMode
     */
    public void requestArm(String accessCode, ArmMode armMode)
    {
        aceCluster.requestPanelStateChange(accessCode, armMode);
    }

    /**
     * Simulate a panel status poll (typical for wakeup)
     */
    public void requestPanelStatus()
    {
        aceCluster.requestPanelStatus();
    }

    @Override
    public boolean isTampered()
    {
        return zoneCluster.isTampered();
    }

    @Override
    public void setTampered(boolean isTampered)
    {
        zoneCluster.setTampered(isTampered);
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

}