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

//
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.device.MockLegacySecurityDevice;

public class UpgradeDeviceFirmwareLegacyRequest extends DeviceRequest
{
    private String routerEui64;
    private String appFilename;
    private String bootloaderFilename;
    private int authChallengeResponse;

    public UpgradeDeviceFirmwareLegacyRequest(int requestId, String eui64, String routerEui64, String appFilename, String bootloaderFilename, int authChallengeResponse)
    {
        super(requestId, eui64, "upgradeDeviceFirmwareLegacyResponse");
        this.routerEui64 = routerEui64;
        this.appFilename = appFilename;
        this.bootloaderFilename = bootloaderFilename;
        this.authChallengeResponse = authChallengeResponse;
    }

    public String getEui64()
    {
        return eui64;
    }

    public String getRouterEui64()
    {
        return routerEui64;
    }

    public String getAppFilename()
    {
        return appFilename;
    }

    public String getBootloaderFilename()
    {
        return bootloaderFilename;
    }

    public int getAuthChallengeResponse()
    {
        return authChallengeResponse;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        int resultCode = -1;

        if(device instanceof MockLegacySecurityDevice)
        {
            ((MockLegacySecurityDevice)device).upgrade(routerEui64, appFilename, bootloaderFilename, authChallengeResponse);
            resultCode = 0;
        }

        Response response = new Response(requestId, resultCode, "upgradeDeviceFirmwareLegacyResponse");
        response.send();
    }
}
