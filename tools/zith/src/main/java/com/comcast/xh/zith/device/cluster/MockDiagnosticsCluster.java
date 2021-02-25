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

public class MockDiagnosticsCluster extends MockCluster
{
    public MockDiagnosticsCluster(String eui64, int endpointId)
    {
        super("diagnostics", eui64, endpointId, 0x0b05, true);

        addAttribute(256, 35);
        addAttribute(257, 35);
        addAttribute(258, 35);
        addAttribute(259, 35);
        addAttribute(260, 33);
        addAttribute(261, 33);
        addAttribute(262, 33);
        addAttribute(263, 33);
        addAttribute(264, 33);
        addAttribute(265, 33);
        addAttribute(266, 33);
        addAttribute(267, 33);
        addAttribute(268, 33);
        addAttribute(269, 33);
        addAttribute(270, 33);
        addAttribute(271, 33);
        addAttribute(272, 33);
        addAttribute(273, 33);
        addAttribute(274, 33);
        addAttribute(275, 33);
        addAttribute(276, 33);
        addAttribute(277, 33);
        addAttribute(278, 33);
        addAttribute(279, 33);
        addAttribute(280, 33);
        addAttribute(281, 33);
        addAttribute(282, 33);
        addAttribute(283, 33);
        // last message lqi
        addAttribute(284, 32);
        // last message rssi
        addAttribute(285, 40);
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case 284:
            {
                result = new byte[1];
                result[0] = (byte)0xff;
                break;
            }

            case 285:
            {
                result = new byte[1];
                result[0] = -50;
                break;
            }
        }

        return result;
    }
}
