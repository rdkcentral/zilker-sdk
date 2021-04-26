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

import com.comcast.xh.zith.device.MockLegacySecurityDevice;
import com.comcast.xh.zith.device.capability.BatteryBackedUp;
import com.comcast.xh.zith.device.capability.Tamperable;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MockLegacySirenRepeaterCluster extends MockLegacySecurityCluster implements Tamperable, BatteryBackedUp
{
    private Logger logger = LoggerFactory.getLogger(MockLegacySecurityCluster.class);
    private boolean tampered = true;
    private boolean acPowerLost;
    private boolean batteryBad;

    public MockLegacySirenRepeaterCluster(String eui64, int endpointId, short vBattNominal, short vBattLow)
    {
        super("legacySirenRepeater", eui64, endpointId, vBattNominal, vBattLow);
    }

    @Override
    public void handleMfgCommand(int mfgId, int commandId, byte[] payload)
    {
        switch (commandId)
        {
            case ClientMfgCommand.SIREN_MODE:
                SirenMode mode = SirenMode.valueOf(payload[0]);
                logger.info("Warning tone set to {} @ volume {}",
                            mode == null ? "invalid mode" : mode.name(),
                            payload[1]);
                break;
            case ClientMfgCommand.SIREN_STATE:
                logger.info("Panel status set to {}, armType {}, delayRemaining {}, alarmType {}, silent {}",
                            payload[0],
                            payload[1],
                            payload[2],
                            payload[3],
                            payload[4]);
                break;
            case ClientMfgCommand.SET_WHITE_LED:
                logger.info("Strobe mode set to brightness {}, onTime {}", payload[0], payload[1]);
                break;
        }
        super.handleMfgCommand(mfgId, commandId, payload);
    }

    @Override
    public byte[] getStatus()
    {
        byte[] status = { 0x00, 0x00 };
        if (isTampered())
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_TAMPER;
        }

        if (isACPowerLost())
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_EXT_PWR_FAIL;
        }

        if (isLowBattery())
        {
            status[0] |= MockLegacySecurityDevice.StatusBitmap.STATUS1_LOW_BATT;
        }

        if (isBatteryBad())
        {
            status[1] |= MockLegacySecurityDevice.StatusBitmap.STATUS2_BATTERY_BAD;
        }

        return status;
    }

    @Override
    public boolean isTampered()
    {
        return tampered;
    }

    @Override
    public void setTampered(boolean isTampered)
    {
        if (tampered != isTampered)
        {
            tampered = isTampered;
            sendStatusCommand();
        }
    }

    @Override
    public boolean isACPowerLost()
    {
        return acPowerLost;
    }

    @Override
    public void setACPowerLost(boolean acPowerLost)
    {
        if (this.acPowerLost != acPowerLost)
        {
            this.acPowerLost = acPowerLost;
            sendStatusCommand();
        }
    }

    @Override
    public boolean isBatteryBad()
    {
        return batteryBad;
    }

    @Override
    public void setBatteryBad(boolean batteryBad)
    {
        if (this.batteryBad != batteryBad)
        {
            this.batteryBad = batteryBad;
            sendStatusCommand();
        }
    }

    /* Siren repeater does not detect if battery is missing */
    @Override
    public boolean isBatteryMissing()
    {
        return false;
    }

    @Override
    public void setBatteryMissing(boolean batteryMissing)
    {
        /* Not supported */
        logger.warn("Siren repeater does not detect missing battery");
    }

    enum SirenMode
    {
        SIREN_OFF  (0x00),
        SIREN_FIRE (0x04),
        SIREN_BURG (0x03);

        private byte sirenTone;

        SirenMode(int sirenTone)
        {
            this.sirenTone = (byte) sirenTone;
        }

        byte getValue()
        {
            return sirenTone;
        }

        static SirenMode valueOf(byte sirenTone)
        {
            SirenMode value = null;
            for (SirenMode mode : SirenMode.values())
            {
                if (mode.getValue() == sirenTone)
                {
                    value = mode;
                    break;
                }
            }
            return value;
        }
    }

    /* Client commands received by this server cluster */
    private final static class ClientMfgCommand
    {
        private ClientMfgCommand() {}

        /**
         * Warning tone
         */
        final static byte SIREN_MODE    = 0x2B;
        /**
         * Security panel status
         */
        final static byte SIREN_STATE   = 0x2C;
        /**
         * Strobe mode
         */
        final static byte SET_WHITE_LED = 0x2D;
    }
}
