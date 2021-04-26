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

public class MockWarningDeviceCluster extends MockCluster
{
    private static final int START_WARNING_COMMAND_ID = 0x00;
    private static final int SQUAWK_COMMAND_ID = 0x01;

    public MockWarningDeviceCluster(String eui64, int endpointId)
    {
        super("warningDevice", eui64, endpointId, 0x0502, true);

        // max duration
        addAttribute(0x0000, DataType.UINT16);
    }

    public void handleCommand(int commandId, byte[] payload)
    {
        if (commandId == START_WARNING_COMMAND_ID)
        {
            // TODO
        }
        else if (commandId == SQUAWK_COMMAND_ID)
        {
            // TODO
        }
    }
}
