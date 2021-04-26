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
// Created by einfochips on 12/03/2020.
//

public class MockRemoteCellModemCluster extends MockCluster
{
    private static final int REMOTE_CELL_MODEM_CLUSTER_ID   = 0xfd03;
    private static final int POWER_STATUS_ATTRIBUTE_ID      = 0x0000;

    private boolean powerStatusOn = true; // by default true;
    public MockRemoteCellModemCluster(String eui64, int endpointId)
    {
        super("remoteCellModem", eui64, endpointId, REMOTE_CELL_MODEM_CLUSTER_ID, true);

        // mfg specific power status attribute
        addMfgAttribute(COMCAST_MFG_ID, POWER_STATUS_ATTRIBUTE_ID, DataType.BOOLEAN);
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case POWER_STATUS_ATTRIBUTE_ID:
            {
                result = new byte[1];
                result[0] = (byte)(powerStatusOn == true ? 1 : 0);
                break;
            }
        }

        return result;
    }

    public void setModemPowerOn(boolean on)
    {
        this.powerStatusOn = on;
        // send the value now.
        sendMfgAttributeReport(COMCAST_MFG_ID, POWER_STATUS_ATTRIBUTE_ID);
    }
}
