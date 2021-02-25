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

package com.comcast.xh.zith.device;

import com.comcast.xh.zith.device.cluster.MockLegacyKeypadCluster;

public class MockLegacyKeypad extends MockLegacySecurityDevice implements MockPeripheral, MockSecurityController
{
    private static final short V_BATT_LOW_MV = 2400;
    private static final short V_BATT_NOMINAL_MV = 3000;
    private MockLegacyKeypadCluster keypadCluster;

    public MockLegacyKeypad(String name)
    {
        this(name, null);
    }

    public MockLegacyKeypad(String name, String eui64)
    {
        super(name,
              eui64,
              false,
              false,
              "SMC",
              "SMCWK01-Z",
              DeviceType.KEYPAD_1,
              0x00000307,
              (byte) 1,
              new UCTemp(19, 0),
              new UCTemp(29, 0),
              V_BATT_LOW_MV,
              (byte) (EnablesBitmap.MAGNETIC_SW | EnablesBitmap.EXT_CONTACT | EnablesBitmap.ARMED),
              (short) 1800,
              (byte) 1,
              LegacyRegion.US,
              (short) 0,
              (short) 0);

        MockEndpoint endpoint = new MockEndpoint(getEui64(), 1, HA_PROFILE, 65535, 0);
        keypadCluster = new MockLegacyKeypadCluster(getEui64(), 1, V_BATT_NOMINAL_MV, V_BATT_LOW_MV);

        endpoint.addCluster(keypadCluster);
        endpoints.put(endpoint.getEndpointId(), endpoint);
    }

    public void requestPanelStateChange(String accessCode, MockLegacyKeypadCluster.KeypadButton actionButton)
    {
        keypadCluster.requestPanelStateChange(accessCode, actionButton);
    }
}
