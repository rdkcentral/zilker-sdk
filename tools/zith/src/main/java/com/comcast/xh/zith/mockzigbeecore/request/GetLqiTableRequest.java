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

import java.util.ArrayList;
import java.util.List;

public class GetLqiTableRequest extends Request
{
    private String eui64;

    public GetLqiTableRequest(int requestId, String eui64)
    {
        super(requestId);
        this.eui64 = eui64;
    }

    @Override
    public void execute()
    {
        // TODO need to double check what is return when
        int resultCode = -1;
        MockZigbeeCore zc = MockZigbeeCore.getInstance();
        final List<LqiData> lqiData = new ArrayList<>();
        MockZigbeeDevice d = zc.getDevice(eui64);
        if (eui64.equals(zc.getEui64()) || (d != null && d.isRouter()))
        {
            resultCode = 0;
            for(MockZigbeeDevice device : zc.getAllDevices())
            {
                if (device.getParent().equals(eui64))
                {
                    // TODO get the LQI data from the device
                    lqiData.add(new LqiData(device.getEui64(), 255));
                }
            }
        }
        new Response(requestId, resultCode, "getLqiTableResponse")
        {
            public List<LqiData> getEntries() { return lqiData; }
        }.send();
    }

    private static class LqiData
    {
        private String eui;
        private int lqi;

        LqiData(String eui64, int lqi)
        {
            this.eui = eui64;
            this.lqi = lqi;
        }

        public String getEui()
        {
            return eui;
        }

        public int getLqi()
        {
            return lqi;
        }
    }
}
