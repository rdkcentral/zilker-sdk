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

import com.comcast.xh.zith.device.MockKeypad;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.shell.DeviceNameProvider;
import org.springframework.shell.standard.ShellCommandGroup;
import org.springframework.shell.standard.ShellComponent;
import org.springframework.shell.standard.ShellMethod;
import org.springframework.shell.standard.ShellOption;
import org.springframework.shell.table.*;

import java.util.LinkedHashMap;

@ShellComponent
@ShellCommandGroup("Devices: Keypads")
public class KeypadDeviceShell
{
    private static final String DEFAULT_NAME = "__DEFAULT__";
    private long deviceNameCounter = 0;

    @ShellMethod(value="List keypads", key="keypad-list")
    public Table list()
    {
        LinkedHashMap<String, Object> headers = new LinkedHashMap<>();
        headers.put("name", "Name");
        headers.put("eui64", "EUI64");
        headers.put("discovered", "Discovered");
        headers.put("tampered", "Tampered");
        TableModel model = new BeanListTableModel(MockZigbeeCore.getInstance().getDevicesByType(MockKeypad.class), headers);
        TableBuilder tableBuilder = new TableBuilder(model);
        return tableBuilder.addFullBorder(BorderStyle.fancy_light).build();
    }

    @ShellMethod(value="Add a new keypad", key="keypad-add")
    public void addKeypad(@ShellOption(defaultValue = DEFAULT_NAME)String name)
    {
        if (name.equals(DEFAULT_NAME))
        {
            name = "Keypad " + ++deviceNameCounter;
        }

        MockKeypad keypad = new MockKeypad(name);
        MockZigbeeCore.getInstance().addDevice(keypad);
    }

    @ShellMethod(value="Tamper a keypad", key="keypad-tamper")
    public void tamper(@ShellOption(defaultValue = "Keypad 1", valueProvider = NameValueProvider.class)String name)
    {
        MockKeypad keypad = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockKeypad.class);
        if(keypad == null)
        {
            System.out.println("Keypad " + name + " not found.");
        }
        else
        {
            keypad.setTampered(true);
        }
    }

    @ShellMethod(value="Tamper restore a keypad", key="keypad-tamper-restore")
    public void tamperRestore(@ShellOption(defaultValue = "Keypad 1", valueProvider = NameValueProvider.class)String name)
    {
        MockKeypad keypad = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockKeypad.class);
        if(keypad == null)
        {
            System.out.println("Keypad " + name + " not found.");
        }
        else
        {
            keypad.setTampered(false);
        }
    }

    @ShellMethod(value="Toggle a keypad's tamper", key="keypad-tamper-toggle")
    public void tamperToggle(@ShellOption(defaultValue = "Keypad 1", valueProvider = NameValueProvider.class)String name)
    {
        MockKeypad keypad = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockKeypad.class);
        if(keypad == null)
        {
            System.out.println("Keypad " + name + " not found.");
        }
        else
        {
            keypad.setTampered(!keypad.isTampered());
        }
    }

    @ShellMethod(value="Set a keypad as having low battery", key="keypad-low-battery")
    public void lowBattery(@ShellOption(defaultValue = "Keypad 1", valueProvider = NameValueProvider.class)String name)
    {
        MockKeypad keypad = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockKeypad.class);
        if(keypad == null)
        {
            System.out.println("keypad " + name + " not found.");
        }
        else
        {
            keypad.setLowBattery(true);
        }
    }

    @ShellMethod(value="Set a keypad as no longer having low battery", key="keypad-low-battery-restore")
    public void lowBatteryRestore(@ShellOption(defaultValue = "Keypad 1", valueProvider = NameValueProvider.class)String name)
    {
        MockKeypad keypad = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockKeypad.class);
        if(keypad == null)
        {
            System.out.println("keypad " + name + " not found.");
        }
        else
        {
            keypad.setLowBattery(false);
        }
    }

    @ShellMethod(value="Toggle a keypad's low battery", key="keypad-low-battery-toggle")
    public void lowBatteryToggle(@ShellOption(defaultValue = "keypad 1", valueProvider = NameValueProvider.class)String name)
    {
        MockKeypad keypad = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockKeypad.class);
        if(keypad == null)
        {
            System.out.println("keypad " + name + " not found.");
        }
        else
        {
            keypad.setLowBattery(!keypad.isLowBattery());
        }
    }

    @ShellComponent
    public static class NameValueProvider extends DeviceNameProvider<MockKeypad>
    {
        public NameValueProvider()
        {
            super(MockKeypad.class);
        }
    }
}
