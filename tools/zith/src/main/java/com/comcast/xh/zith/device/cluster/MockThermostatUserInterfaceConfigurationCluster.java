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

package com.comcast.xh.zith.device.cluster;

public class MockThermostatUserInterfaceConfigurationCluster extends MockCluster
{
    public MockThermostatUserInterfaceConfigurationCluster(String eui64, int endpointId)
    {
        super("thermostatUserInterfaceConfiguration", eui64, endpointId, 0x204, true);

        // temperature display mode
        addAttribute(0x0, DataType.ENUM8);
        // keypad lockout
        addAttribute(0x1, DataType.ENUM8);
    }
}
