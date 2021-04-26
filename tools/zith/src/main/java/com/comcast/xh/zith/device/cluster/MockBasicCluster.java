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

public class MockBasicCluster extends MockCluster
{
    private static final int MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID = 0x0000;
    private static final int FIRMWARE_VERSION_ATTRIBUTE_ID = 0x0001;

    private String manufacturer;
    private String model;
    private int hardwareVersion;
    private int firmwareVersion = 0x1;
    private boolean isDefaulted = true;

    public MockBasicCluster(String eui64, int endpointId, String manufacturer, String model, int hardwareVersion, boolean hasMfgSpecificAttribute)
    {
        super("basic", eui64, endpointId, 0, true);

        this.manufacturer = manufacturer;
        this.model = model;
        this.hardwareVersion = hardwareVersion;

        addAttribute(FIRMWARE_VERSION_ATTRIBUTE_ID, DataType.UINT16); //firmware version
        addAttribute(0x3, DataType.UINT8); //hwver
        addAttribute(0x4, DataType.STRING); //manufacturer
        addAttribute(0x5, DataType.STRING); //model

        if (hasMfgSpecificAttribute == true)
        {
            // mfg specific modem reboot reason attribute for m1lte
            addMfgAttribute(COMCAST_MFG_ID, MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID, DataType.ENUM8);
        }
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID:
                // make sure it is mfg specific.
                if (info.getMfgId() == COMCAST_MFG_ID)
                {
                    result = new byte[1];
                    result[0] = 0;
                }
                break;

            case FIRMWARE_VERSION_ATTRIBUTE_ID:
                result = new byte[4];
                result[0] = (byte)(firmwareVersion & 0xff);
                result[1] = (byte)((firmwareVersion >>8) & 0xff);
                result[2] = (byte)((firmwareVersion >>16) & 0xff);
                result[3] = (byte)((firmwareVersion >>24) & 0xff);
                break;

            case 0x3:
            {
                result = new byte[1];
                result[0] = (byte) hardwareVersion;
                break;
            }

            case 0x4:
            {
                result = getAttributeBytes(manufacturer);
                break;
            }

            case 0x5:
            {
                result = getAttributeBytes(model);
                break;
            }
        }

        return result;
    }

    @Override
    public void handleCommand(int commandId, byte[] payload)
    {
        switch(commandId)
        {
            case 0: //reset to factory
                isDefaulted = true;
                break;

            default:
                break;
        }
    }

    public String getManufacturer()
    {
        return manufacturer;
    }

    public String getModel()
    {
        return model;
    }

    public int getHardwareVersion()
    {
        return hardwareVersion;
    }

    public void setDefaulted(boolean isDefaulted)
    {
        this.isDefaulted = isDefaulted;
    }

    public boolean isDefaulted()
    {
        return isDefaulted;
    }
}
