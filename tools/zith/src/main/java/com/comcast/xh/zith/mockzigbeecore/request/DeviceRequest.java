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
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public abstract class DeviceRequest extends Request
{
    private static final Logger log = LoggerFactory.getLogger(DeviceRequest.class);
    protected String eui64;
    protected String responseType;

    public DeviceRequest(int requestId, String eui64, String responseType)
    {
        super(requestId);
        this.eui64 = eui64;
        this.responseType = responseType;
    }

    protected abstract void executeDeviceRequest(MockZigbeeDevice device);

    public final void execute()
    {
        MockZigbeeDevice device = MockZigbeeCore.getInstance().getDevice(eui64);
        if(device == null)
        {
            log.error("device " + eui64 + " not found.");
            new Response(requestId, -1, responseType).send();
        }
        else if (device.isOnline() == false)
        {
            log.error("device " + eui64 + " offline.");
            new Response(requestId, -1, responseType).send();
        }
        else
        {
            executeDeviceRequest(device);
        }
    }
}
