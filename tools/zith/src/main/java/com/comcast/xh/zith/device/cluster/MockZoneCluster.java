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
import com.comcast.xh.zith.device.capability.Faultable;
import com.comcast.xh.zith.device.capability.Tamperable;
import com.comcast.xh.zith.device.capability.Troubleable;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.mockzigbeecore.event.ZoneStatusChangedEvent;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Fake Zigbee door/window sensor (IAS Zone cluster 0x0500)
 */
public class MockZoneCluster extends MockCluster implements Tamperable, Faultable, BatteryPowered, Troubleable
{
    public static final int CLUSTER_ID = 0x500;
    private static final byte ATTR_ZONE_STATUS = 0x02;
    private static final byte ATTR_ZONE_TYPE = 0x01;
    private static final byte ATTR_CIE_ADDRESS = 0x10;

    protected boolean isOpen = true;
    protected boolean isTampered = true;
    private boolean isEnrolled;
    protected boolean isBatteryLow;
    protected boolean isTroubled;
    private byte zoneId;
    protected ZoneType zoneType;
    private long cieAddress;
    private short voltage;

    public MockZoneCluster(String eui64, int endpointId, ZoneType zoneType)
    {
        super("zone", eui64, endpointId, CLUSTER_ID, true);
        // Zone Info Set
        // ZoneState
        addAttribute(0x0, DataType.ENUM8);
        //ZoneType
        addAttribute(ATTR_ZONE_TYPE, DataType.ENUM16);
        //ZoneStatus
        addAttribute(ATTR_ZONE_STATUS, DataType.MAP16);

        //Zone Settings Set
        //IAS Control and Indication Equipment Address
        addAttribute(ATTR_CIE_ADDRESS, DataType.EUI64);

        // Zone ID
        addAttribute(0x11, DataType.UINT8);
        // Number of zone sensitivity levels supported
        addAttribute(0x12, DataType.UINT8);
        // Current zone sensitivity level
        addAttribute(0x13, DataType.UINT8);

        this.zoneType = zoneType;
    }

    public MockZoneCluster(String eui64, int endpointId, ZoneType zoneType, boolean initialIsOpen)
    {
        this(eui64, endpointId, zoneType);
        isOpen = initialIsOpen;
    }

    private ZoneStatus getZoneStatus()
    {
        return new ZoneStatus().setTampered(isTampered).setAlarm1(isOpen).setBatteryLow(isBatteryLow).setTrouble(isTroubled);
    }

    private void onZoneStatusChanged()
    {
        // Unpaired devices are null - only send if paired
        if (MockZigbeeCore.getInstance().getDevice(getEui64()) != null)
        {
            new ZoneStatusChangedEvent(
                    getEui64(),
                    getEndpointId(),
                    (byte) 0x01,
                    getZoneStatus().getFlags()
            ).send();
        }
    }

    public void setFaulted(boolean open)
    {
        if (isOpen != open)
        {
            isOpen = open;
            onZoneStatusChanged();
        }
    }

    public void setTampered(boolean tampered)
    {
        if (isTampered != tampered)
        {
            isTampered = tampered;
            onZoneStatusChanged();
        }
    }

    public boolean isFaulted()
    {
        return isOpen;
    }

    public boolean isTampered()
    {
        return isTampered;
    }

    public boolean isTroubled()
    {
        return isTroubled;
    }

    public void setTroubled(boolean troubled)
    {
        if (isTroubled != troubled)
        {
            isTroubled = troubled;
            onZoneStatusChanged();
        }
    }

    public boolean isEnrolled()
    {
        return isEnrolled;
    }

    public long getCieAddress()
    {
        return cieAddress;
    }

    public ZoneType getZoneType()
    {
        return zoneType;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch (info.getId())
        {
            case ATTR_ZONE_STATUS:
                result = getZoneStatus().toByteArray();
                break;
            case ATTR_ZONE_TYPE:
                result = zoneType.toByteArray();
                break;
            default:
                result = super.getAttributeData(info);
        }

        return result;
    }

    @Override
    protected Status setAttributeData(AttributeInfo info, byte[] data)
    {
        Status status;
        switch (info.getId())
        {
            case ATTR_CIE_ADDRESS:
                ByteBuffer byteBuffer = ByteBuffer.allocate(Long.BYTES);
                byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
                byteBuffer.put(data);
                byteBuffer.flip();
                cieAddress = byteBuffer.getLong();
                status = Status.SUCCESS;
                break;
            default:
                status = super.setAttributeData(info, data);
        }
        return status;
    }

    @Override
    public void handleCommand(int commandId, byte[] payload)
    {
        switch(commandId)
        {
            case ClientCommand.ZONE_ENROLL_RESPONSE:
                if (payload[0] == Status.SUCCESS.getValue() && payload.length == 2)
                {
                    zoneId = payload[1];
                    isEnrolled = true;
                }
                break;
            default:
                super.handleCommand(commandId, payload);
                break;
        }
    }

    @Override
    public void setLowBattery(boolean isLowBattery)
    {
        if (isLowBattery != isBatteryLow)
        {
            isBatteryLow = isLowBattery;
            onZoneStatusChanged();
        }
    }

    @Override
    public boolean isLowBattery()
    {
        return isBatteryLow;
    }

    @Override
    public void setBatteryVoltage(short newVoltage)
    {
        this.voltage = newVoltage;
    }

    @Override
    public short getBatteryVoltage()
    {
        return voltage;
    }

    public enum ZoneType
    {
        CIE                 (0x0000),
        MOTION              (0x000d),
        CONTACT_SW          (0x0015),
        DOOR_WINDOW_HANDLE  (0x0016),
        FIRE_SENSOR         (0x0028),
        WATER_SENSOR        (0x002a),
        CO_SENSOR           (0x002b),
        PERS_EMERG          (0x002c),
        VIBRATION           (0x002d),
        REMOTE_CONTROL      (0x010f),
        KEYFOB              (0x0115),
        KEYPAD              (0x021d),
        WD                  (0x0225),
        GLASS_BREAK         (0x0226),
        SEC_REPEATER        (0x0229),
        MFR_SPECIFIC_START  (0x8000),
        MFR_SPECIFIC_END    (0xfffe),
        INVALID             (0xffff);

        private final short zoneType;

        ZoneType(int zoneType)
        {
            this.zoneType = (short) zoneType;
        }

        public short getValue()
        {
            return zoneType;
        }

        public byte[] toByteArray()
        {
            ByteBuffer buf = ByteBuffer.allocate(2);
            buf.order(ByteOrder.LITTLE_ENDIAN);
            buf.putShort(zoneType);
            return buf.array();
        }
    }

    public static final class ZoneStatus
    {
        static final short ALARM1               = 1;
        static final short ALARM2               = 1 << 1;
        static final short TAMPER               = 1 << 2;
        static final short LOW_BATTERY          = 1 << 3;
        static final short SUPERVISION_NOTIFY   = 1 << 4;
        static final short RESTORE_NOTIFY       = 1 << 5;
        static final short TROUBLE              = 1 << 6;
        static final short AC_POWERED           = 1 << 7;
        static final short TEST                 = 1 << 8;
        static final short BATTERY_DEFECTIVE    = 1 << 9;

        private short flags;

        private ZoneStatus()
        {
        }

        public short getFlags()
        {
            return flags;
        }

        public byte[] toByteArray()
        {
            ByteBuffer buf = ByteBuffer.allocate(2);
            buf.order(ByteOrder.LITTLE_ENDIAN);
            buf.putShort(flags);
            return buf.array();
        }

        public ZoneStatus setAlarm1(boolean inAlarm1)
        {
            flags |= inAlarm1 ? ALARM1 : 0;
            return this;
        }

        public ZoneStatus setAlarm2(boolean inAlarm2)
        {
            flags |= inAlarm2 ? ALARM2 : 0;
            return this;
        }

        public ZoneStatus setTampered(boolean isTampered)
        {
            flags |= isTampered ? TAMPER : 0;
            return this;
        }

        public ZoneStatus setBatteryLow(boolean isBatteryLow)
        {
            flags |= isBatteryLow ? LOW_BATTERY : 0;
            return this;
        }

        public ZoneStatus setShouldSupervisionNotify(boolean shouldSupervisionNotify)
        {
            flags |= shouldSupervisionNotify ? SUPERVISION_NOTIFY : 0;
            return this;
        }

        public ZoneStatus setShouldRestoreNotify(boolean shouldRestoreNotify)
        {
            flags |= shouldRestoreNotify ? RESTORE_NOTIFY : 0;
            return this;
        }

        public ZoneStatus setTrouble(boolean inTrouble)
        {
            flags |= inTrouble ? TROUBLE : 0;
            return this;
        }

        public ZoneStatus setAC(boolean isACPowered)
        {
            flags |= isACPowered ? ZoneStatus.AC_POWERED : 0;
            return this;
        }

        public ZoneStatus setTest(boolean inTestMode)
        {
            flags |= inTestMode ? TEST : 0;
            return this;
        }

        public ZoneStatus setBatteryDefective(boolean isDefective)
        {
            flags |= isDefective ? BATTERY_DEFECTIVE : 0;
            return this;
        }
    }

    public static final class ServerCommand
    {
        public static final int ZONE_STATUS_CHANGED = 0x00;
        public static final int ZONE_ENROLL_REQUEST = 0x01;
    }

    public static final class ClientCommand
    {
        public static final int ZONE_ENROLL_RESPONSE = 0x00;
        public static final int INITIATE_NORMAL_MODE = 0x01;
        public static final int INITIATE_TEST_MODE   = 0x02;
    }
}
