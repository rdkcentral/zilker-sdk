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

import com.comcast.xh.zith.device.capability.BatteryPowered;
import com.comcast.xh.zith.device.capability.TemperatureMeasurable;
import com.comcast.xh.zith.device.cluster.MockLegacySecurityCluster;
import com.comcast.xh.zith.mockzigbeecore.event.LegacySecurityDeviceInfoEvent;

public class MockLegacySecurityDevice extends MockZigbeeDevice implements BatteryPowered, TemperatureMeasurable
{
    private String manufacturer;
    private String model;
    private long firmwareVersion;
    private DeviceType deviceType;
    private byte hardwareVer;
    private UCTemp lowTempLimit;
    private UCTemp highTempLimit;
    private short lowBatteryThreshold;
    private byte enablesBitmap;
    private short hibernationDuration;
    private byte napDuration;
    LegacyRegion region;
    private short mfgId;
    private short modelId;
    private MockLegacySecurityCluster cluster;

    //These are set when this device is upgraded
    private String upgradeRouterEui64;
    private String upgradeAppFilename;
    private String upgradeBootloaderFilename;
    private int upgradeAuthChallengeResponse;

    public MockLegacySecurityDevice(String name,
            boolean isRouter,
            boolean isMainsPowered,
            String manufacturer,
            String model,
            DeviceType deviceType,
            long firmwareVersion,
            byte hardwareVersion,
            UCTemp lowTempLimit,
            UCTemp highTempLimit,
            short lowBatteryThreshold,
            byte enablesBitmap,
            short hibernationDuration,
            byte napDuration,
            LegacyRegion region,
            short mfgId,
            short modelId)
    {
        this(name, null, isRouter, isMainsPowered, manufacturer, model, deviceType, firmwareVersion, hardwareVersion,
                lowTempLimit, highTempLimit, lowBatteryThreshold, enablesBitmap, hibernationDuration, napDuration,
                region, mfgId, modelId);
    }

    public MockLegacySecurityDevice(String name,
            String eui64,
            boolean isRouter,
            boolean isMainsPowered,
            String manufacturer,
            String model,
            DeviceType deviceType,
            long firmwareVersion,
            byte hardwareVersion,
            UCTemp lowTempLimit,
            UCTemp highTempLimit,
            short lowBatteryThreshold,
            byte enablesBitmap,
            short hibernationDuration,
            byte napDuration,
            LegacyRegion region,
            short mfgId,
            short modelId)
    {
        super(name, eui64, isRouter, isMainsPowered);

        this.manufacturer = manufacturer;
        this.model = model;
        this.deviceType = deviceType;
        this.firmwareVersion = firmwareVersion;
        this.hardwareVer = hardwareVersion;
        this.lowTempLimit = lowTempLimit;
        this.highTempLimit = highTempLimit;
        this.lowBatteryThreshold = lowBatteryThreshold;
        this.enablesBitmap = enablesBitmap;
        this.hibernationDuration = hibernationDuration;
        this.napDuration = napDuration;
        this.region = region;
        this.mfgId = mfgId;
        this.modelId = modelId;
    }

    /**
     * Get the firmware version number. It will be converted to the legacy
     * 3 byte format for you.
     *
     * @return the firmware version at boot
     */
    public long getZigbeeFirmwareVersion()
    {
        return firmwareVersion;
    }

    /**
     * Update the device firmware version
     */
    public void setFirmwareVersion(long firmwareVersion)
    {
        this.firmwareVersion = firmwareVersion;
    }

    /**
     * Get the device type id
     *
     * @return the type id
     */
    public DeviceType getDeviceType()
    {
        return deviceType;
    }

    /**
     * Get status bitmaps
     *
     * @return A byte string with status1, status2 bitmaps.
     * @see StatusBitmap
     */
    public byte[] getStatus()
    {
        return getMockSecurityCluster().getStatus();
    }


    /**
     * Get the low temperature limit
     *
     * @return a legacy temperature value (may not be null)
     */
    public UCTemp getLowTempLimit()
    {
        return lowTempLimit;
    }

    /**
     * Get the upper temperature limit
     *
     * @return a legacy temperature value (may not be null)
     */
    public UCTemp getHighTempLimit()
    {
        return highTempLimit;
    }

    /**
     * Get the low battery warning voltage
     *
     * @return voltage in millivolts
     */
    public short getLowBatteryThreshold()
    {
        return lowBatteryThreshold;
    }

    /**
     * Get the device number
     *
     * @return the assigned device number.
     */
    public byte getDeviceNumber()
    {
        return getMockSecurityCluster().getDeviceNumber();
    }

    /**
     * Get the 'enables' bitmap
     *
     * @return a byte with any applicable {@link EnablesBitmap} bits set
     */
    public byte getEnablesBitmap()
    {
        return enablesBitmap;
    }

    /**
     * Get the hibernation duration
     *
     * @return time in seconds
     */
    public short getHibernationDuration()
    {
        return hibernationDuration;
    }

    /**
     * Get the nap duration
     *
     * @return time in seconds
     */
    public short getNapDuration()
    {
        return napDuration;
    }

    /**
     * Get the region code
     *
     * @return a region code
     */
    public LegacyRegion getRegion()
    {
        return region;
    }

    /**
     * Get the manufacturer ID
     * //TODO: what are they and what devices send them?
     *
     * @return
     */
    public short getMfgId()
    {
        return mfgId;
    }

    /**
     * Get the model ID
     * //TODO: What are they and what devices send them?
     *
     * @return
     */
    public short getModelId()
    {
        return modelId;
    }

    @Override
    public String getModel()
    {
        return model;
    }

    @Override
    public String getManufacturer()
    {
        return manufacturer;
    }

    @Override
    public int getZigbeeHardwareVersion()
    {
        return hardwareVer;
    }

    @Override
    public void joinComplete()
    {
        new LegacySecurityDeviceInfoEvent(getEui64(), 1, this).send();
    }

    private MockLegacySecurityCluster getMockSecurityCluster()
    {
        if (cluster == null)
        {
            cluster = findServerClusterOfType(MockLegacySecurityCluster.class);

            if (cluster == null)
            {
                throw new IllegalStateException("A MockSecurityCluster is required but not registered to" + this);
            }
        }

        return cluster;
    }

    @Override
    public void setLowBattery(boolean isLowBattery)
    {
        getMockSecurityCluster().setLowBattery(isLowBattery);
    }

    public void sendCheckin()
    {
        getMockSecurityCluster().sendCheckinCommmand();
    }

    @Override
    public boolean isLowBattery()
    {
        return getMockSecurityCluster().isLowBattery();
    }

    @Override
    public void setBatteryVoltage(short newVoltage)
    {
        getMockSecurityCluster().setBatteryVoltage(newVoltage);
    }

    @Override
    public short getBatteryVoltage()
    {
        return getMockSecurityCluster().getBatteryVoltage();
    }

    public void upgrade(String routerEui64, String appFilename, String bootloaderFilename, int authChallengeResponse)
    {
        this.upgradeRouterEui64 = routerEui64;
        this.upgradeAppFilename = appFilename;
        this.upgradeBootloaderFilename = bootloaderFilename;
        this.upgradeAuthChallengeResponse = authChallengeResponse;
    }

    public String getUpgradeRouterEui64()
    {
        return upgradeRouterEui64;
    }

    @Override
    public short getMeasuredTemperature()
    {
        return cluster.getTemperature();
    }

    @Override
    public void setMeasuredTemperature(short temp)
    {
        cluster.setTemperature(temp);
    }

    /**
     * Bits for the 'enables' bitmap (1 byte)
     */
    public static final class EnablesBitmap
    {
        /**
         * Not used
         */
        public static final int NONE = 1;
        public static final int MAGNETIC_SW = 1 << 1;
        public static final int EXT_CONTACT = 1 << 2;
        public static final int MAGNETIC_TAMPER = 1 << 3;
        public static final int ARMED = 1 << 4;
        public static final int TEMP_LOW_FAULT = 1 << 5;
        public static final int TEMP_HIGH_FAULT = 1 << 6;
        public static final int SENSOR_PAIRED = 1 << 7;
    }

    /**
     * Bits for the 'status' bitmaps (2 byte string)
     */
    public static final class StatusBitmap
    {
        public static final int STATUS1_ALARM1 = 1;
        public static final int STATUS1_ALARM2 = 1 << 1;
        public static final int STATUS1_LOW_TEMP = 1 << 2;
        public static final int STATUS1_HIGH_TEMP = 1 << 3;
        public static final int STATUS1_TAMPER = 1 << 4;
        public static final int STATUS1_LOW_BATT = 1 << 5;
        public static final int STATUS1_TROUBLE = 1 << 6;
        public static final int STATUS1_EXT_PWR_FAIL = 1 << 7;

        public static final int STATUS2_COMM_FAIL = 1;
        public static final int STATUS2_TEST = 1 << 1;
        public static final int STATUS2_BATTERY_BAD = 1 << 2;
        public static final int STATUS2_BOOTLOAD_FAIL = 1 << 3;
        /* most significant bits not used */
    }

    public enum DeviceType
    {
        //TODO: copy the rest from uc_common.h
        DOOR_WINDOW_1(0x01),
        SMOKE_1(0x02),
        KEYFOB_1(0x07),
        KEYPAD_1(0x0B),
        TAKEOVER_1(0x0C),
        MICRO_DOOR_WINDOW(0x0D),
        MTL_REPEATER_SIREN(0x0E);

        private byte id;

        DeviceType(int id)
        {
            this.id = (byte) id;
        }

        public byte getId()
        {
            return id;
        }
    }

    public static final class UCTemp
    {
        private byte degreesC;
        private byte tenths;

        public UCTemp(int degreesC, int tenths)
        {
            this.degreesC = (byte) degreesC;
            this.tenths = (byte) tenths;
        }

        public byte[] toByteArray()
        {
            byte[] raw = new byte[2];

            raw[0] = tenths;
            raw[1] = degreesC;

            return raw;
        }
    }

    public enum LegacyRegion
    {
        /**
         * FCC countries
         */
        US(0),
        /**
         * ETSI (Europe) countries
         */
        CH(1),
        /**
         * Japan
         */
        JP(2),
        /**
         * Australia
         */
        AU(3);

        private byte regionCode;

        LegacyRegion(int regionCode)
        {
            this.regionCode = (byte) regionCode;
        }

        public byte getRegionCode()
        {
            return regionCode;
        }
    }
}
