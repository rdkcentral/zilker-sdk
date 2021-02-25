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
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.mockzigbeecore.event.LegacyGodparentInfoMessageEvent;
import com.comcast.xh.zith.mockzigbeecore.event.LegacyPingMessageEvent;
import com.comcast.xh.zith.mockzigbeecore.event.LegacySecurityDeviceStatusEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Common security device cluster (complex IAS zone)
 */
public abstract class MockLegacySecurityCluster extends MockCluster implements BatteryPowered
{
    public static final int CLUSTER_ID = MockZoneCluster.CLUSTER_ID;
    private static final byte NOMINAL_RSSI_DBM = -30;
    private static final byte LQI_MAX = (byte) 255;
    private static final short TYPICAL_ROOM_TEMP_CENTI_C = 2500;

    private static Logger log = LoggerFactory.getLogger(MockLegacySecurityCluster.class);
    private byte deviceNumber;

    private short vBatt;
    private short vBattNominal;
    private short vBattLow;
    private byte rssi_dBm = NOMINAL_RSSI_DBM;
    private byte lqi = LQI_MAX;
    private short temperature = TYPICAL_ROOM_TEMP_CENTI_C;

    MockLegacySecurityCluster(String clusterName, String eui64, int endpointId, short vBattNominal, short vBattLow)
    {
        super(clusterName, eui64, endpointId, CLUSTER_ID, true);
        vBatt = vBattNominal;
        this.vBattLow = vBattLow;
        this.vBattNominal = vBattNominal;
    }

    private final static class ServerMfgCommand
    {
        final static byte SET_DEVICE_NUMBER = 0x25;
        final static byte PING = 0x33;
    }

    public byte getDeviceNumber()
    {
        return deviceNumber;
    }

    public void sendPingCommand()
    {
        // Only if we are paired
        if (MockZigbeeCore.getInstance().getDevice(getEui64()) != null)
        {
            new LegacyPingMessageEvent(getEui64(), getEndpointId(), deviceNumber, rssi_dBm, lqi).send();
        }
    }

    /**
     * Send a status command and wait for APS ack.
     * @param waitMillis
     * @return the command id in the APS ack
     */
    public int sendStatusCommand(int waitMillis)
    {
        int result = -1;

        // Only if we are paired
        if (MockZigbeeCore.getInstance().getDevice(getEui64()) != null)
        {
            LegacySecurityDeviceStatusEvent event = new LegacySecurityDeviceStatusEvent(getEui64(), getEndpointId(), this);
            if(waitMillis > 0)
            {
                result = event.sendAndWait(waitMillis);
            }
            else
            {
                event.send();
            }
        }

        return result;
    }

    /**
     * Send a status command and dont wait or care about ack response
     */
    public void sendStatusCommand()
    {
        sendStatusCommand(0);
    }

    /**
     * Send a godparent info command and wait for APS ack.  The rssi and lqi will make the specified
     * godparent look perfect so it will be identified as the target's godparent.
     *
     * @param godparentDeviceNumber the device number of the device sending the message
     * @param targetDeviceNumber the device number that this info message is about
     * @param waitMillis
     * @return the command id in the APS ack
     */
    public int sendGodparentInfo(byte godparentDeviceNumber, byte targetDeviceNumber, int waitMillis)
    {
        int result = -1;

        // Only if we are paired
        if (MockZigbeeCore.getInstance().getDevice(getEui64()) != null)
        {
            LegacyGodparentInfoMessageEvent event = new LegacyGodparentInfoMessageEvent(getEui64(),
                                                                                        getEndpointId(),
                                                                                        godparentDeviceNumber,
                                                                                        targetDeviceNumber);
            if(waitMillis > 0)
            {
                result = event.sendAndWait(waitMillis);
            }
            else
            {
                event.send();
            }
        }

        return result;
    }

    public void sendCheckinCommmand()
    {
        new LegacySecurityDeviceStatusEvent(getEui64(), getEndpointId(), this, true).send();
    }

    @Override
    public void handleMfgCommand(int mfgId, int commandId, byte[] payload)
    {
        switch (commandId)
        {
            case ServerMfgCommand.SET_DEVICE_NUMBER:
                deviceNumber = payload[0];
                log.info("device number set to " + deviceNumber);
                break;
            case ServerMfgCommand.PING:
                log.info("Received a ping");
                break;
            default:
                super.handleMfgCommand(mfgId, commandId, payload);
                break;
        }
    }

    /**
     * Get the status bytes. See STATUS1/STATUS2 constants for bits
     * Defaults to all clear
     * @return
     */
    public byte[] getStatus()
    {
        return new byte[] { 0x00, 0x00 };
    }

    public void setvBattNominal(short vBattNominal)
    {
        this.vBattNominal = vBattNominal;
    }

    public void setvBattLow(short vBattLow)
    {
        this.vBattLow = vBattLow;
    }

    @Override
    public void setLowBattery(boolean isLowBattery)
    {
        short newVBatt = isLowBattery ? (short) (vBattLow - 1) : vBattNominal;
        if (vBatt != newVBatt)
        {
            setBatteryVoltage(newVBatt);
            sendStatusCommand(1000);
        }
    }

    @Override
    public boolean isLowBattery()
    {
        return vBatt <= vBattNominal;
    }

    /**
     * Set the battery voltage
     * @param newVoltage voltage in millivolts
     */
    @Override
    public void setBatteryVoltage(short newVoltage)
    {
        if (newVoltage > 0 && newVoltage < 3200)
        {
            vBatt = newVoltage;
        }
        else
        {
            log.warn("Ignoring out of range voltage {}, currently {} mV", newVoltage, vBatt);
        }
    }

    /**
     * Get the battery voltage
     * @return voltage in millivolts
     */
    @Override
    public short getBatteryVoltage()
    {
        return vBatt;
    }

    /**
     * Set the current temperature
     * @param temperature A temperature in centi-degrees celsius
     */
    public void setTemperature(short temperature)
    {
        /* ZCL valid temperature range */
        if (temperature >= (short) 0x954d && temperature < (short) 0xFFFF)
        {
            this.temperature = temperature;
        }
        else
        {
            log.warn("Ignoring invalid temperature {}", temperature);
        }
    }

    /**
     * Get the current ambient temperature
     * @return temperature in centi-degrees celsius
     */
    public short getTemperature()
    {
        return temperature;
    }

    /**
     * Get the Received Signal Strength Indication
     * @return RSSI in dB(m)
     */ 
    public byte getRssi()
    {
        return rssi_dBm;
    }

    public void setRssi(byte rssi)
    {
        this.rssi_dBm = rssi;
    }

    /**
     * Get the Link Quality Index
     * 255: Laboratory conditions / ideal
     * >200: "High"
     * == 200: "Fair:" about 80% reliable
     * <200: "Poor"
     * @return a nondimensional quality rating
     */
    public byte getLqi()
    {
        return lqi;
    }

    public void setLqi(byte lqi)
    {
        this.lqi = lqi;
    }
}
