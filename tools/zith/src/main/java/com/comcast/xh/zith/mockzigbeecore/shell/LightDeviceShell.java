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

import com.comcast.xh.zith.device.MockLight;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.shell.DeviceNameProvider;
import org.springframework.shell.standard.*;
import org.springframework.shell.table.*;

import java.util.*;

@ShellComponent
@ShellCommandGroup("Devices: Lights")
public class LightDeviceShell
{
    private static final String DEFAULT_NAME = "__DEFAULT__";
    private long deviceNameCounter = 0;

    @ShellMethod(value="List lights", key="light-list")
    public Table list()
    {
        LinkedHashMap<String, Object> headers = new LinkedHashMap<>();
        headers.put("name", "Name");
        headers.put("eui64", "EUI64");
        headers.put("discovered", "Discovered");
        headers.put("on", "On");
        TableModel model = new BeanListTableModel(MockZigbeeCore.getInstance().getDevicesByType(MockLight.class), headers);
        TableBuilder tableBuilder = new TableBuilder(model);
        return tableBuilder.addFullBorder(BorderStyle.fancy_light).build();
    }

    @ShellMethod(value="Add a new light", key="light-add")
    public void addLight(@ShellOption(defaultValue = DEFAULT_NAME)String name)
    {
        if (name.equals(DEFAULT_NAME))
        {
            name = "Light " + ++deviceNameCounter;
        }

        MockLight light = new MockLight(name);
        MockZigbeeCore.getInstance().addDevice(light);
    }

    @ShellMethod(value="Turn on a light", key="light-turn-on")
    public void turnOn(@ShellOption(defaultValue = "Light 1", valueProvider = NameValueProvider.class)String name)
    {
        MockLight light = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockLight.class);
        if(light == null)
        {
            System.out.println("Light " + name + " not found.");
        }
        else
        {
            light.turnOn();
        }
    }

    @ShellMethod(value="Turn off a light", key="light-turn-off")
    public void turnOff(@ShellOption(defaultValue = "Light 1", valueProvider = NameValueProvider.class)String name)
    {
        MockLight light = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockLight.class);
        if(light == null)
        {
            System.out.println("Light " + name + " not found.");
        }
        else
        {
            light.turnOff();
        }
    }

    @ShellMethod(value="Toggle a light", key="light-toggle")
    public void toggle(@ShellOption(defaultValue = "Light 1", valueProvider = NameValueProvider.class)String name)
    {
        MockLight light = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockLight.class);
        if (light == null)
        {
            System.out.println("Light " + name + " not found.");
        }
        else
        {
            light.toggle();
        }
    }

    @ShellMethod(value="Set the lighting level", key="light-level")
    public void level(@ShellOption(defaultValue = "Light 1", valueProvider = NameValueProvider.class)String name, int level)
    {
        MockLight light = MockZigbeeCore.getInstance().getDeviceByNameAndType(name, MockLight.class);
        if(light == null)
        {
            System.out.println("Light " + name + " not found.");
        }
        else
        {
            if(level >= 0 && level <= 100)
            {
                light.setLightLevel(level);
            }
            else
            {
                System.out.println("Please use a level between 0 and 100");
            }
        }
    }

    @ShellComponent
    public static class NameValueProvider extends DeviceNameProvider<MockLight>
    {
        public NameValueProvider()
        {
            super(MockLight.class);
        }
    }
}
