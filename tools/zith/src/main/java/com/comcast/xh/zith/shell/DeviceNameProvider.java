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

package com.comcast.xh.zith.shell;

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import org.springframework.core.MethodParameter;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

public class DeviceNameProvider<T extends MockZigbeeDevice> extends ValueProviderBase
{
    private Class<T> clazz;

    public DeviceNameProvider(Class<T> clazz)
    {
        this.clazz = clazz;
    }

    protected Collection<String> getAllPossibleValues(MethodParameter parameter)
    {
        List<T> devices = MockZigbeeCore.getInstance().getDevicesByType(clazz);
        List<String> deviceNames = new ArrayList<>();
        for(T device : devices)
        {
            deviceNames.add(device.getName());
        }

        return deviceNames;
    }
}
