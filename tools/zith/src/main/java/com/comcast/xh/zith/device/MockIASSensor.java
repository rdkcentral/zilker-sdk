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

import com.comcast.xh.zith.device.cluster.MockDiagnosticsCluster;
import com.comcast.xh.zith.device.cluster.MockOtaUpgradeCluster;
import com.comcast.xh.zith.device.cluster.MockPollControlCluster;
import com.comcast.xh.zith.device.cluster.MockPowerConfigurationCluster;
import com.comcast.xh.zith.device.cluster.MockTemperatureMeasurementCluster;
import com.comcast.xh.zith.device.cluster.MockZoneCluster;
import com.comcast.xh.zith.device.cluster.MockZoneCluster.*;

public class MockIASSensor extends MockZigbeeDevice implements MockSensor
{
    private static final short DEVICE_ID = 0x0402;
    private MockZoneCluster zoneCluster;
    private MockPollControlCluster pollControlCluster;
    private MockTemperatureMeasurementCluster temperatureMeasurementCluster;
    private MockPowerConfigurationCluster powerConfigurationCluster;

    public MockIASSensor(String name)
    {
        this(name, null);
    }

    public MockIASSensor(String name, String eui64)
    {
        this(name, eui64, ZoneType.CONTACT_SW);
    }

    public MockIASSensor(String name, String eui64, ZoneType zoneClusterType)
    {
        super(name, eui64);

        MockEndpoint endpoint = new MockEndpoint(getEui64(), 1, HA_PROFILE, DEVICE_ID, 0, "Visonic", "MCT-370 SMA", 1);
        zoneCluster = new MockZoneCluster(getEui64(), 1, zoneClusterType);
        endpoint.addCluster(zoneCluster);
        endpoint.addCluster(new MockOtaUpgradeCluster(getEui64(), 1, 0x10000003));
        pollControlCluster = new MockPollControlCluster(getEui64(), 1);
        endpoint.addCluster(pollControlCluster);
        endpoint.addCluster(new MockDiagnosticsCluster(getEui64(), 1));
        temperatureMeasurementCluster = new MockTemperatureMeasurementCluster(getEui64(), 1);
        endpoint.addCluster(temperatureMeasurementCluster);
        powerConfigurationCluster = new MockPowerConfigurationCluster(getEui64(), 1);
        endpoint.addCluster(powerConfigurationCluster);

        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    @Override
    public void setLowBattery(boolean isLowBattery)
    {
        zoneCluster.setLowBattery(isLowBattery);
    }

    @Override
    public boolean isLowBattery()
    {
        return zoneCluster.isLowBattery();
    }

    @Override
    public void setBatteryVoltage(short newVoltage)
    {
        zoneCluster.setBatteryVoltage(newVoltage);
    }

    @Override
    public short getBatteryVoltage()
    {
        return zoneCluster.getBatteryVoltage();
    }

    @Override
    public void setFaulted(boolean isFaulted)
    {
        zoneCluster.setFaulted(isFaulted);
    }

    @Override
    public boolean isFaulted()
    {
        return zoneCluster.isFaulted();
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
    public void setMeasuredTemperature(short temp)
    {
        temperatureMeasurementCluster.setMeasuredTemperature(temp);
    }

    @Override
    public short getMeasuredTemperature()
    {
        return temperatureMeasurementCluster.getMeasuredTemperature();
    }

    @Override
    public boolean isTroubled()
    {
        return zoneCluster.isTroubled();
    }

    @Override
    public void setTroubled(boolean isTroubled)
    {
        zoneCluster.setTroubled(isTroubled);
    }

    public void sendCheckIn()
    {
        pollControlCluster.sendCheckIn();
    }

    public void sendComcastCheckIn(Integer battVoltage, Integer battUsedMilliAmpHr, Integer temp, Integer rssi, Integer lqi,
                                   Integer retries, Integer rejoins)
    {
        pollControlCluster.sendComcastCheckIn(battVoltage, battUsedMilliAmpHr, temp, rssi, lqi, retries, rejoins);
    }
}
