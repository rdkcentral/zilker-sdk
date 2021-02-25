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
// Created by tlea on 1/8/19.
//

import com.comcast.xh.zith.device.MockLegacySecurityDevice;
import com.comcast.xh.zith.device.capability.BatteryPowered;
import com.comcast.xh.zith.device.capability.Faultable;
import com.comcast.xh.zith.device.capability.Tamperable;
import com.comcast.xh.zith.device.capability.Troubleable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MockLegacyDoorWindowCluster extends MockLegacySecurityCluster implements Tamperable, Faultable, BatteryPowered,
        Troubleable
{
    private static Logger log = LoggerFactory.getLogger(MockLegacyDoorWindowCluster.class);
    private boolean faulted;
    private boolean tampered;
    private boolean lowBattery;
    private boolean troubled;

    public MockLegacyDoorWindowCluster(String eui64, int endpointId, short vBattNominal, short vBattLow)
    {
        super("legacydoorwindowsensor", eui64, endpointId, vBattNominal, vBattLow);

        addAttribute(0, 32);
        addAttribute(7, 48);

        //this sensor will pair faulted and tampered
        faulted = false;
        tampered = false;
        lowBattery = false;
        troubled = false;
    }

    @Override
    public boolean isFaulted()
    {
        return faulted;
    }

    @Override
    public boolean isTampered()
    {
        return tampered;
    }

    @Override
    public void setFaulted(boolean faulted)
    {
        if (this.faulted != faulted)
        {
            this.faulted = faulted;

            log.info(getEui64() + " is now " + (faulted ? "faulted" : "restored"));

            sendStatusCommand();
        }
        else
        {
            log.warn(getEui64() + " is already " + (faulted ? "faulted" : "restored"));
        }
    }

    public void toggleFaulted()
    {
        setFaulted(!isFaulted());
    }

    @Override
    public void setTampered(boolean tampered)
    {
        if (this.tampered != tampered)
        {
            this.tampered = tampered;

            log.info(getEui64() + " is now " + (tampered ? "tampered" : "tamper restored"));

            sendStatusCommand();
        }
        else
        {
            log.warn(getEui64() + " is already " + (tampered ? "tampered" : "tamper restored"));
        }
    }

    @Override
    public boolean isTroubled()
    {
        return troubled;
    }

    @Override
    public void setTroubled(boolean isTroubled)
    {
        if (troubled != isTroubled)
        {
            troubled = isTroubled;

            log.info(getEui64() + " is now " + (troubled ? "troubled" : "trouble restored"));

            sendStatusCommand();
        }
        else
        {
            log.warn(getEui64() + " is already " + (troubled ? "troubled" : "trouble restored"));
        }
    }

    public void toggleTampered()
    {
        setTampered(!isTampered());
    }

    public void toggleLowBattery()
    {
        setLowBattery(!isLowBattery());
    }

    public void sendInfoCommmand()
    {
    }

    public void sendSerialNumberCommmand()
    {
    }

   @Override
    public void handleMfgCommand(int mfgId, int commandId, byte[] payload)
    {
        switch (commandId)
        {
            default:
                super.handleMfgCommand(mfgId, commandId, payload);
                break;
        }
    }

    @Override
    public byte[] getStatus()
    {
        byte[] status = { 0x00, 0x00 };
        if (tampered)
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_TAMPER;
        }

        if (faulted)
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_ALARM1;
        }

        if (lowBattery)
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_LOW_BATT;
        }

        if (troubled)
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_TROUBLE;
        }

        return status;
    }
}
