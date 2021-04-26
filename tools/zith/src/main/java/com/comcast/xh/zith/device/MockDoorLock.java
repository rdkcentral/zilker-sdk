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

import java.util.Collection;

public class MockDoorLock extends MockZigbeeDevice implements BatteryPowered, Tamperable
{
    private MockDoorLockCluster doorLockCluster;

    public MockDoorLock(String name)
    {
        this(name, null);
    }

    public MockDoorLock(String name, String eui64)
    {
        super(name, eui64, false, false);

        MockEndpoint endpoint = new MockEndpoint(getEui64(), 1, HA_PROFILE, 0x000a, 0, "Yale", "YRD246 TSDB", 2);
        endpoint.addCluster(new MockPowerConfigurationCluster(getEui64(), 1));
        endpoint.addCluster(new MockIdentifyCluster(getEui64(), 1));
        endpoint.addCluster(new MockAlarmCluster(getEui64(), 1));
        // DoorLock is both a time server and a time client
        endpoint.addCluster(new MockTimeCluster(getEui64(), 1, true));
        endpoint.addCluster(new MockTimeCluster(getEui64(), 1, false));
        doorLockCluster = new MockDoorLockCluster(getEui64(), 1);
        endpoint.addCluster(doorLockCluster);
        endpoint.addCluster(new MockPollControlCluster(getEui64(), 1));

        endpoint.addCluster(new MockDiagnosticsCluster(getEui64(), 1));
        endpoint.addCluster(new MockOtaUpgradeCluster(getEui64(), 1, 0x0109002A));

        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    public boolean isLocked()
    {
        return doorLockCluster.isLocked();
    }

    // TODO add source for lock/unlock

    public void lock()
    {
        doorLockCluster.setLocked(true);
        System.out.println("Door Lock " + getName() + " is now locked");
    }

    public void unlock()
    {
        doorLockCluster.setLocked(false);
        System.out.println("Door Lock " + getName() + " is now unlocked");
    }

    public void toggle()
    {
        doorLockCluster.setLocked(!doorLockCluster.isLocked());
        System.out.println("Door Lock " + getName() + " is now " + (doorLockCluster.isLocked() ? "locked" : "unlocked"));
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

    @Override
    public boolean isTampered()
    {
        return findRequiredServerClusterOfType(Tamperable.class).isTampered();
    }

    @Override
    public void setTampered(boolean isTampered)
    {
        findRequiredServerClusterOfType(Tamperable.class).setTampered(isTampered);
    }

    public boolean isInvalidCodeEntryLimit()
    {
        return doorLockCluster.isInvalidCodeEntryLimit();
    }

    public void setInvalidCodeEntryLimit(boolean invalidCodeEntryLimit)
    {
        doorLockCluster.setInvalidCodeEntryLimit(invalidCodeEntryLimit);
    }

    public void setLockBoltJammed(boolean lockBoltJammed)
    {
        doorLockCluster.setLockBoltJammed(lockBoltJammed);
    }

    public boolean isLockBoltJammed()
    {
        return doorLockCluster.isLockBoltJammed();
    }

    public void addPin(int userId, byte status, byte type, String pin)
    {
        doorLockCluster.addPin(userId, status, type, pin);
    }

    public int getPinCount()
    {
        return doorLockCluster.getPinCount();
    }

    public int getMaxPinCodeLength()
    {
        return doorLockCluster.getMaxPinCodeLength();
    }

    public int getMinPinCodeLength()
    {
        return doorLockCluster.getMinPinCodeLength();
    }

    public int getMaxPinCodesSupported()
    {
        return doorLockCluster.getMaxPinUsersSupported();
    }

    public int getAutoRelockSeconds()
    {
        return doorLockCluster.getAutoRelockTime();
    }

    public void setAutoRelockSeconds(int seconds)
    {
        doorLockCluster.setAutoRelockTime(seconds);
    }

    public Collection<MockDoorLockCluster.PinEntry> getPins()
    {
        return doorLockCluster.getPins();
    }
}
