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

import com.comcast.xh.zith.device.capability.BatteryBackedUp;
import com.comcast.xh.zith.device.capability.BatteryPowered;
import com.comcast.xh.zith.device.capability.TemperatureLevel;
import com.comcast.xh.zith.mockzigbeecore.event.AlarmClusterEvent;
import com.comcast.xh.zith.mockzigbeecore.event.AttributeReportEvent;

/**
 * TODO: Stub!
 */
public class MockPowerConfigurationCluster extends MockCluster implements BatteryPowered, BatteryBackedUp, TemperatureLevel
{
    private static final int ATTRIBUTE_BATTERY_VOLTAGE = 0x20;
    private static final int ATTRIBUTE_BATTERY_ALARM_STATE = 0x3e;
    private static final int ATTRIBUTE_BATTERY_PERCENTAGE_REMAINING = 0x21;
    private static final int ATTRIBUTE_BATTERY_RECHARGE_CYCLE_COUNT = 0x0d;

    // Alarm codes
    private static final byte AC_VOLTAGE_BELOW_MIN = 0x00;
    private static final byte BATTERY_BELOW_MIN_THRESHOLD = 0x10;
    private static final byte BATTERY_NOT_AVAILABLE = 0x3b;
    private static final byte BATTERY_BAD = 0x3c;
    private static final byte BATTERY_HIGH_TEMPERATURE = 0x3f;
    private boolean batteryLow = false;
    private boolean batteryBad = false;
    private boolean batteryMissing = false;
    private boolean acPowerLost = false;
    private boolean batteryTempHigh = false;
    private int batteryRechargeCycleCount = 0;
    private int batteryPercentage = 100;
    private short decivolts = 60;
    private boolean useBatteryAlarmState = true;

    public MockPowerConfigurationCluster(String eui64, int endpointId) {
        this(eui64, endpointId, true);
    }

    public MockPowerConfigurationCluster(String eui64, int endpointId, boolean useBatteryAlarmState) {
        super("powerConfiguration", eui64, endpointId, 0x1, true);

        this.useBatteryAlarmState = useBatteryAlarmState;

        //Mains Voltage
        addAttribute(0x0, DataType.UINT16);
        // Frequency
        addAttribute(0x1, DataType.UINT8);
        //Mains alarm mask
        addAttribute(0x10, DataType.MAP8);
        //Mains Voltage Min Threshold
        addAttribute(0x11, DataType.UINT16);
        //Mains Voltage Max Threshold
        addAttribute(0x12, DataType.UINT16);
        //Mains voltage Dwell Trip Point
        addAttribute(0x13, DataType.UINT16);
        //Battery voltage
        addAttribute(ATTRIBUTE_BATTERY_VOLTAGE, DataType.UINT8);
        //Battery Percent Remaining
        addAttribute(ATTRIBUTE_BATTERY_PERCENTAGE_REMAINING, DataType.UINT8);
        //Battery Manufacturer
        addAttribute(0x30, DataType.STRING);
        //Battery size
        addAttribute(0x31, DataType.ENUM8);
        //Battery AH remaining
        addAttribute(0x32, DataType.UINT16);
        //Battery Quantity
        addAttribute(0x33, DataType.UINT8);
        // Battery Rated Voltage (mV)
        addAttribute(0x34, DataType.UINT8);
        // Battery Alarm Mask
        addAttribute(0x35, DataType.MAP8);
        //Battery Voltage Min Threshold
        addAttribute(0x36, DataType.UINT8);
        //Battery Voltage Threshold 1
        addAttribute(0x37, DataType.UINT8);
        //Battery Voltage Threshold 2
        addAttribute(0x38, DataType.UINT8);
        //Battery Voltage Threshold 3
        addAttribute(0x39, DataType.UINT8);
        //Battery percentage min threshold
        addAttribute(0x3a, DataType.UINT8);
        //Batttery percentage threshold 1
        addAttribute(0x3b, DataType.UINT8);
        //Batttery percentage threshold 2
        addAttribute(0x3c, DataType.UINT8);
        //Batttery percentage threshold 3
        addAttribute(0x3d, DataType.UINT8);
        if (useBatteryAlarmState)
        {
            //Battery alarm state
            addAttribute(ATTRIBUTE_BATTERY_ALARM_STATE, DataType.MAP32);
        }
    }

    public MockPowerConfigurationCluster(String eui64, int endpointId, boolean useBatteryAlarmState, boolean hasBatteryRechargeCycleAttributeSupport)
    {
        this(eui64, endpointId, useBatteryAlarmState);
        if (hasBatteryRechargeCycleAttributeSupport == true)
        {
            // mfg specific battery recharge cycle count attribute
            addMfgAttribute(COMCAST_MFG_ID, ATTRIBUTE_BATTERY_RECHARGE_CYCLE_COUNT, DataType.UINT8);
        }
    }

    @Override
    public void setLowBattery(boolean isLowBattery)
    {
        if (isLowBattery != batteryLow)
        {
            batteryLow = isLowBattery;
            if (useBatteryAlarmState)
            {
                sendAttributeReport(ATTRIBUTE_BATTERY_ALARM_STATE);
            }
            else
            {
                new AlarmClusterEvent(this, BATTERY_BELOW_MIN_THRESHOLD, isLowBattery == false).send();
            }
        }
    }

    @Override
    public boolean isLowBattery()
    {
        return batteryLow;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case ATTRIBUTE_BATTERY_ALARM_STATE:
            {
                result = new byte[4];
                result[0] = (byte)(batteryLow ? 1 : 0);
                break;
            }

            case ATTRIBUTE_BATTERY_VOLTAGE:
            {
                result = new byte[1];
                result[0] = (byte)decivolts;
                break;
            }

            case ATTRIBUTE_BATTERY_RECHARGE_CYCLE_COUNT:
            {
                result = new byte[2];
                result[0] = (byte)(batteryRechargeCycleCount & 0xff);
                result[1] = (byte)((batteryRechargeCycleCount >>8) & 0xff);
                break;
            }
        }

        return result;
    }

    @Override
    protected Status setAttributeData(AttributeInfo attributeInfo, byte[] data)
    {
        return Status.SUCCESS;
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
            new AlarmClusterEvent(this, AC_VOLTAGE_BELOW_MIN, acPowerLost == false).send();
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
            new AlarmClusterEvent(this, BATTERY_BAD, batteryBad == false).send();
        }
    }

    @Override
    public boolean isBatteryMissing()
    {
        return batteryMissing;
    }

    @Override
    public void setBatteryMissing(boolean batteryMissing)
    {
        if (this.batteryMissing != batteryMissing)
        {
            this.batteryMissing = batteryMissing;
            new AlarmClusterEvent(this, BATTERY_NOT_AVAILABLE, batteryMissing == false).send();
        }
    }

    @Override
    public boolean isTemperatureHigh()
    {
        return batteryTempHigh;
    }

    @Override
    public void setTemperatureHigh(boolean tempHigh)
    {
        if (batteryTempHigh != tempHigh)
        {
            batteryTempHigh = tempHigh;
            new AlarmClusterEvent(this, BATTERY_HIGH_TEMPERATURE, tempHigh == false).send();
        }
    }

    public void setBatteryPercentage(int batteryPercentage)
    {
        this.batteryPercentage = batteryPercentage;

        sendAttributeReport(ATTRIBUTE_BATTERY_PERCENTAGE_REMAINING);
    }

    @Override
    public short getBatteryVoltage()
    {
        return decivolts;
    }

    @Override
    public void setBatteryVoltage(short newVoltage)
    {
        if (this.decivolts != newVoltage)
        {
            this.decivolts = newVoltage;
            sendAttributeReport(ATTRIBUTE_BATTERY_VOLTAGE);
        }
    }
}