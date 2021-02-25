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

import com.comcast.xh.zith.device.capability.BatteryPowered;
import com.comcast.xh.zith.mockzigbeecore.event.ACE.server.LegacyKeypadEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MockLegacyKeypadCluster extends MockLegacySecurityCluster implements BatteryPowered
{
    private static Logger log = LoggerFactory.getLogger(MockLegacyKeypadCluster.class);

    //BUG: Real device reports input (server) clusters 0x0000, 0x0003, but
    //transmits 0x0500 when it issues commands
    public MockLegacyKeypadCluster(String eui64, int endpointId, short vBattNominal, short vBattLow)
    {
        super("legacyKeypad", eui64, endpointId, vBattNominal, vBattLow);
    }

    /**
     * Simulate button pushes on the keypad device to request an arm/disarm
     * @param accessCode
     */
    public void requestPanelStateChange(String accessCode, KeypadButton buttonPushed)
    {
        new LegacyKeypadEvent(getEui64(),
                              getEndpointId(),
                              getDeviceNumber(),
                              buttonPushed,
                              accessCode,
                              getBatteryVoltage(),
                              (byte) (getTemperature() / 100),
                              (byte) (getTemperature() % 100)).send();
    }

    private void parseSetLEDEvent(byte[] payload)
    {
        if (payload.length == 3)
        {
            LEDMode mode = LEDMode.valueOf(payload[0]);
            int duration = payload[1];
            LEDColor color = LEDColor.valueOf(payload[2]);

            if (mode == null)
            {
                log.warn("Unsupported LED mode {}", payload[0]);
            }

            if (color == null)
            {
                log.warn("Unsupported LED color {}", payload[2]);
            }

            log.info("setLED: mode: {}, duration: {}s, color: {}", mode, duration, color);
        }
        else
        {
            log.error("Invalid set LEDs payload");
        }
    }

    @Override
    public void handleMfgCommand(int mfgId, int commandId, byte[] payload)
    {
        if (commandId == ClientMfgCommand.SET_LED)
        {
            parseSetLEDEvent(payload);
        }
        else
        {
            super.handleMfgCommand(mfgId, commandId, payload);
        }
    }

    public enum KeypadButton
    {
        NONE                (0x00),
        ARM_STAY            (0x01),
        PANIC               (0x02),
        ARM_AWAY            (0x03),
        DISARM              (0x04),
        GET_PANEL_STATUS    (0x05);

        private byte action;
        KeypadButton(int action)
        {
            this.action = (byte) action;
        }

        public byte toByte()
        {
            return this.action;
        }
    }

    enum LEDColor
    {
        RED       (0x00),
        GREEN     (0x01),
        AMBER     (0x02);

        private byte color;
        LEDColor(int color)
        {
            this.color = (byte) color;
        }

        public static LEDColor valueOf(byte value)
        {
            LEDColor found = null;
            for (LEDColor color : LEDColor.values())
            {
                if (color.toByte() == value)
                {
                    found = color;
                    break;
                }
            }
            return found;
        }

        public byte toByte()
        {
            return this.color;
        }
    }

    enum LEDMode
    {
        OFF           (0x00),
        SOLID         (0x01),
        FAST_BLINK    (0x02),
        SLOW_BLINK    (0x03);

        private byte mode;

        LEDMode(int mode)
        {
            this.mode = (byte) mode;
        }

        public static LEDMode valueOf(byte value)
        {
            LEDMode found = null;
            for (LEDMode mode : LEDMode.values())
            {
                if (mode.toByte() == value)
                {
                    found = mode;
                    break;
                }
            }
            return found;
        }

        public byte toByte()
        {
            return mode;
        }
    }

    /**
     * Server commands from keypad
     */
    public static final class ServerMfgCommand
    {
        private ServerMfgCommand() {}
        public static final byte KEYPAD_EVENT = 0x0A;
    }

    /**
     * Client commands to keypad
     */
    public static final class ClientMfgCommand
    {
        private ClientMfgCommand() {}
        public static final byte SET_LED  = 0x2A;
    }
}
