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
// Created by tlea on 1/14/19.
//

import com.comcast.xh.zith.device.MockIASSensor;
import com.comcast.xh.zith.device.MockLegacyDoorWindowSensor;
import com.comcast.xh.zith.device.MockSensor;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.shell.DeviceNameProvider;
import org.springframework.shell.standard.*;
import org.springframework.shell.table.*;

import java.util.*;

@ShellComponent
@ShellCommandGroup("Devices: Sensors")
public class SensorDeviceShell
{
    private static final String DEFAULT_NAME = "__DEFAULT__";
    private long deviceNameCounter = 0;

    @ShellMethod(value="List sensors", key="sensor-list")
    public Table list()
    {
        LinkedHashMap<String, Object> headers = new LinkedHashMap<>();
        headers.put("name", "Name");
        headers.put("eui64", "EUI64");
        headers.put("discovered", "Discovered");
        headers.put("faulted", "Faulted");
        headers.put("tampered", "Tampered");
        TableModel model = new BeanListTableModel(MockZigbeeCore.getInstance().getDevicesByType(MockSensor.class), headers);
        TableBuilder tableBuilder = new TableBuilder(model);
        return tableBuilder.addFullBorder(BorderStyle.fancy_light).build();
    }

    @ShellMethod(value="Add a new sensor", key="sensor-add")
    public void addSensor(@ShellOption(defaultValue = DEFAULT_NAME)String name)
    {
        if (name.equals(DEFAULT_NAME))
        {
            name = "Sensor " + ++deviceNameCounter;
        }

        MockIASSensor sensor = new MockIASSensor(name);
        MockZigbeeCore.getInstance().addDevice(sensor);
    }

    @ShellMethod(value="Add a new sensor", key="sensor-add-legacy")
    public void addLegacySensor(@ShellOption(defaultValue = DEFAULT_NAME)String name)
    {
        if (name.equals(DEFAULT_NAME))
        {
            name = "Sensor " + ++deviceNameCounter;
        }

        MockLegacyDoorWindowSensor sensor = new MockLegacyDoorWindowSensor(name);
        MockZigbeeCore.getInstance().addDevice(sensor);
    }

    @ShellMethod(value="Fault a sensor", key="sensor-fault")
    public void fault(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setFaulted(true);
        }
    }

    @ShellMethod(value="Restore a sensor", key="sensor-restore")
    public void restore(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setFaulted(false);
        }
    }

    @ShellMethod(value="Toggle a sensor's fault", key="sensor-fault-toggle")
    public void faultToggle(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setFaulted(!sensor.isFaulted());
        }
    }

    @ShellMethod(value="Tamper a sensor", key="sensor-tamper")
    public void tamper(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setTampered(true);
        }
    }

    @ShellMethod(value="Tamper restore a sensor", key="sensor-tamper-restore")
    public void tamperRestore(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setTampered(false);
        }
    }

    @ShellMethod(value="Toggle a sensor's tamper", key="sensor-tamper-toggle")
    public void tamperToggle(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setTampered(!sensor.isTampered());
        }
    }

    @ShellMethod(value="Set a sensor as having low battery", key="sensor-low-battery")
    public void lowBattery(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setLowBattery(true);
        }
    }

    @ShellMethod(value="Set a sensor as no longer having low battery", key="sensor-low-battery-restore")
    public void lowBatteryRestore(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setLowBattery(false);
        }
    }

    @ShellMethod(value="Toggle a sensor's low battery", key="sensor-low-battery-toggle")
    public void lowBatteryToggle(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else
        {
            sensor.setLowBattery(!sensor.isLowBattery());
        }
    }

    @ShellMethod(value="Trigger a poll control checkin", key="sensor-checkin")
    public void sendCheckin(@ShellOption(defaultValue = "Sensor 1", valueProvider = NameValueProvider.class)String name)
    {
        MockSensor sensor = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockSensor.class);
        if(sensor == null)
        {
            System.out.println("Sensor " + name + " not found.");
        }
        else if (!(sensor instanceof MockIASSensor))
        {
            System.out.println("Sensor " + name + " is not of supported type.");
        }
        else
        {
            ((MockIASSensor)sensor).sendCheckIn();
        }
    }

    @ShellComponent
    public static class NameValueProvider extends DeviceNameProvider<MockLegacyDoorWindowSensor>
    {
        public NameValueProvider()
        {
            super(MockLegacyDoorWindowSensor.class);
        }
    }
}
