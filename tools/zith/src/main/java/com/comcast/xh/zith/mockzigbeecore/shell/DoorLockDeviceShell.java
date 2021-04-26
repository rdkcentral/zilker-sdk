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

package com.comcast.xh.zith.mockzigbeecore.shell;

//
// Created by mkoch on 1/31/19.
//

import com.comcast.xh.zith.device.MockDoorLock;
import com.comcast.xh.zith.device.cluster.MockDoorLockCluster;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.shell.DeviceNameProvider;
import org.springframework.shell.standard.*;
import org.springframework.shell.table.*;

import java.util.*;

@ShellComponent
@ShellCommandGroup("Devices: Door Locks")
public class DoorLockDeviceShell
{
    private static final String DEFAULT_NAME = "__DEFAULT__";
    private long deviceNameCounter = 0;

    @ShellMethod(value="List door locks", key="lock-list")
    public Table list()
    {
        LinkedHashMap<String, Object> headers = new LinkedHashMap<>();
        headers.put("name", "Name");
        headers.put("eui64", "EUI64");
        headers.put("discovered", "Discovered");
        headers.put("locked", "Locked");
        TableModel model = new BeanListTableModel(MockZigbeeCore.getInstance().getDevicesByType(MockDoorLock.class), headers);
        TableBuilder tableBuilder = new TableBuilder(model);
        return tableBuilder.addFullBorder(BorderStyle.fancy_light).build();
    }

    @ShellMethod(value="Add a new door lock", key="lock-add")
    public void addDoorLock(@ShellOption(defaultValue = DEFAULT_NAME)String name)
    {
        if (name.equals(DEFAULT_NAME))
        {
            name = "DoorLock " + ++deviceNameCounter;
        }

        MockDoorLock lock = new MockDoorLock(name);
        MockZigbeeCore.getInstance().addDevice(lock);
    }

    @ShellMethod(value="Lock a door lock", key="lock-lock")
    public void lock(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.lock();
        }
    }

    @ShellMethod(value="Unlock a door lock", key="lock-unlock")
    public void unlock(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.unlock();
        }
    }

    @ShellMethod(value="Toggle a door lock", key="lock-toggle")
    public void toggle(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if (lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.toggle();
        }
    }

    @ShellMethod(value="Set a door lock as having low battery", key="lock-low-battery")
    public void lowBattery(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.setLowBattery(true);
        }
    }

    @ShellMethod(value="Set a door lock as no longer having low battery", key="lock-low-battery-restore")
    public void lowBatteryRestore(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.setLowBattery(false);
        }
    }

    @ShellMethod(value="Toggle a door lock's low battery", key="lock-low-battery-toggle")
    public void lowBatteryToggle(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.setLowBattery(!lock.isLowBattery());
        }
    }

    @ShellMethod(value="Add a PIN user to the lock", key="lock-add-pin")
    public void addPin(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name,
                       @ShellOption(defaultValue = "0")int id,
                       @ShellOption(defaultValue = "1")byte status,
                       @ShellOption(defaultValue = "3")byte type,
                       @ShellOption(defaultValue = "1234", valueProvider = NameValueProvider.class)String pin)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            lock.addPin(id, status, type, pin);
        }
    }

    @ShellMethod(value="Get the PIN codes on the lock", key="lock-get-pins")
    public void getPins(@ShellOption(defaultValue = "DoorLock 1", valueProvider = NameValueProvider.class)String name)
    {
        MockDoorLock lock = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockDoorLock.class);
        if(lock == null)
        {
            System.out.println("Door Lock " + name + " not found.");
        }
        else
        {
            Collection<MockDoorLockCluster.PinEntry> pins = lock.getPins();
            for(MockDoorLockCluster.PinEntry entry : pins)
            {
                System.out.println(entry.toString());
            }
        }
    }

    @ShellComponent
    public static class NameValueProvider extends DeviceNameProvider<MockDoorLock>
    {
        public NameValueProvider()
        {
            super(MockDoorLock.class);
        }
    }
}
