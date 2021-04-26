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

package com.comcast.xh.zith.mockzigbeecore.request;

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;

public class DeviceIsChildRequest extends Request
{
    private String eui64;

    public DeviceIsChildRequest(int requestId, String eui64)
    {
        super(requestId);
        this.eui64 = eui64;
    }

    @Override
    public void execute()
    {
        int resultCode = -1;
        MockZigbeeCore zc = MockZigbeeCore.getInstance();
        MockZigbeeDevice device = zc.getDevice(eui64);
        // Actual zigbeeCore doesn't return 0 for routers that are children...
        if (device != null && device.getParent().equals(zc.getEui64()) && !device.isRouter())
        {
            resultCode = 0;
        }
        new Response(requestId, resultCode, "deviceIsChildResponse").send();
    }
}
