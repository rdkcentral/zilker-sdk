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

//
// Created by tlea on 1/4/19.
//

public class MockOtaUpgradeCluster extends MockCluster
{
    private long firmwareVersion;

    public MockOtaUpgradeCluster(String eui64, int endpointId, long firmwareVersion)
    {
        super("otaupgrade", eui64, endpointId, 0x19, false);

        this.firmwareVersion = firmwareVersion;

        addAttribute(0, 240);
        addAttribute(1, 35);
        addAttribute(0x2, 35); //current file version
        addAttribute(3, 33);
        addAttribute(4, 35);
        addAttribute(5, 33);
        addAttribute(6, 48);
        addAttribute(7, 33);
        addAttribute(8, 33);
        addAttribute(9, 33);
        addAttribute(10, 35);
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case 0x2:
            {
                result = new byte[4];
                result[0] = (byte)(firmwareVersion & 0xff);
                result[1] = (byte)((firmwareVersion >>8) & 0xff);
                result[2] = (byte)((firmwareVersion >>16) & 0xff);
                result[3] = (byte)((firmwareVersion >>24) & 0xff);
                break;
            }
        }

        return result;
    }

    public long getFirmwareVersion()
    {
        return firmwareVersion;
    }
}
