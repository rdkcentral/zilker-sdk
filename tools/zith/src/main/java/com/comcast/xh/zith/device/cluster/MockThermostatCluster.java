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

import java.nio.ByteOrder;
import java.util.EnumSet;
import java.nio.ByteBuffer;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;


public class MockThermostatCluster extends MockCluster
{
    private static Logger log = LoggerFactory.getLogger(MockThermostatCluster.class);

    private static final short RUNNING_STATE_ATTRIBUTE_ID =  0x29;
    private static final short SYSTEM_MODE_ATTRIBUTE_ID = 0x1c;
    private static final short SETPOINT_HOLD_ATTRIBUTE_ID = 0x23;
    private static final short LOCAL_TEMPERATURE_ATTRIBUTE_ID = 0x0;
    private static final short ABS_MIN_HEAT_SETPOINT_ATTRIBUTE_ID = 0x3;
    private static final short ABS_MAX_HEAT_SETPOINT_ATTRIBUTE_ID = 0x4;
    private static final short ABS_MIN_COOL_SETPOINT_ATTRIBUTE_ID = 0x5;
    private static final short ABS_MAX_COOL_SETPOINT_ATTRIBUTE_ID = 0x6;
    private static final short LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID = 0x10;
    private static final short OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID = 0x12;
    private static final short OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID = 0x11;
    private static final short CONTROL_SEQUENCE_OF_OPERATION_ATTRIBUTE_ID = 0x1b;

    private EnumSet<ThermostatRunningState> runningStates = EnumSet.noneOf(ThermostatRunningState.class);
    private ThermostatSystemMode systemMode = ThermostatSystemMode.off;
    private ThermostatControlSequence controlSequence = ThermostatControlSequence.coolingAndHeating;
    private boolean holdOn = false;
    private int localTemperature = 2222;
    private int absMinHeatSetpoint = 400;
    private int absMaxHeatSetpoint = 3730;
    private int absMinCoolSetpoint = 400;
    private int absMaxCoolSetpoint = 3730;
    private short localTempCalibration = 0;
    private int coolSetpoint = 2300;
    private int heatSetpoint = 2200;

    private enum ThermostatRunningState
    {
        HeatStateOn(0x0001),
        CoolStateOn(0x0002),
        FanStateOn(0x0004),
        HeatSecondStageStateOn(0x0008),
        CoolSecondStateStateOn(0x0010),
        FanSecondStageStateOn(0x0020),
        FanThirdStateStateOn(0x0040);

        private int value;

        ThermostatRunningState(int value)
        {
            this.value = value;
        }
    }

    public enum ThermostatSystemMode
    {
        off((byte)0x0),
        auto((byte)0x1),
        cool((byte)0x3),
        heat((byte)0x4),
        emergencyHeating((byte)0x5),
        precooling((byte)0x6),
        fanOnly((byte)0x7);

        private byte value;

        ThermostatSystemMode(byte value)
        {
            this.value = value;
        }

        public static ThermostatSystemMode fromId(byte id)
        {
            for(ThermostatSystemMode mode: values())
            {
                if(mode.value == id)
                {
                    return mode;
                }
            }
            return null;
        }
    }

    private enum ThermostatControlSequence
    {
        coolingOnly((byte)0x0),
        coolingWithReheat((byte)0x1),
        heatingOnly((byte)0x2),
        heatingWithReheat((byte)0x3),
        coolingAndHeating((byte)0x4),
        coolingAndHeatingWithReheat((byte)0x5);

        private byte value;

        ThermostatControlSequence(byte value) {
            this.value = value;
        }
    }

    public MockThermostatCluster(String eui64, int endpointId)
    {
        super("thermostat", eui64, endpointId, 0x201, true);

        // local temperature
        addAttribute(LOCAL_TEMPERATURE_ATTRIBUTE_ID, DataType.INT16);
        // abs min heat setpoint limit
        addAttribute(ABS_MIN_HEAT_SETPOINT_ATTRIBUTE_ID, DataType.INT16);
        // abs max heat setpoint limit
        addAttribute(ABS_MAX_HEAT_SETPOINT_ATTRIBUTE_ID, DataType.INT16);
        // abs min cool setpoint limit
        addAttribute(ABS_MIN_COOL_SETPOINT_ATTRIBUTE_ID, DataType.INT16);
        // abs max cool setpoint limit
        addAttribute(ABS_MAX_COOL_SETPOINT_ATTRIBUTE_ID, DataType.INT16);
        // local temperature calibrations
        addAttribute(LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID, DataType.INT8);
        // occupied cooling setpoint
        addAttribute(OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID, DataType.INT16);
        // occupied heating setpoint
        addAttribute(OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID, DataType.INT16);
        // min heat setpoint limit
        addAttribute(0x15, DataType.INT16);
        // max heat setpoint limit
        addAttribute(0x16, DataType.INT16);
        // min cool setpoint limit
        addAttribute(0x17, DataType.INT16);
        // max cool setpoint limit
        addAttribute(0x18, DataType.INT16);
        // min setpoint dead band
        addAttribute(0x19, DataType.INT8);
        // control sequence of operation
        addAttribute(CONTROL_SEQUENCE_OF_OPERATION_ATTRIBUTE_ID, DataType.ENUM8);
        // system mode
        addAttribute(SYSTEM_MODE_ATTRIBUTE_ID, DataType.ENUM8);
        // thermostat running mode
        addAttribute(0x1e, DataType.ENUM8);
        // thermostat setpoint hold
        addAttribute(SETPOINT_HOLD_ATTRIBUTE_ID, DataType.ENUM8);
        // thermostat running state
        addAttribute(RUNNING_STATE_ATTRIBUTE_ID, DataType.MAP16);
    }

    private byte[] get8BitValue(boolean value)
    {
        byte[] result = new byte[1];
        result[0] = (byte) (value ? 1 : 0);

        return result;
    }

    private byte[] get8BitValue(int value)
    {
        byte[] result = new byte[1];
        result[0] = (byte) (value & 0xff);

        return result;
    }

    private byte[] get16BitValue(int value)
    {
        byte[] result = new byte[2];
        result[0] = (byte) (value & 0xff);
        result[1] = (byte) ((value >> 8) & 0xff);

        return result;
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case RUNNING_STATE_ATTRIBUTE_ID:
            {
                int bitset = 0;
                for(ThermostatRunningState state : runningStates)
                {
                    bitset |= state.value;
                }

                result = get16BitValue(bitset);
                break;
            }

            case SYSTEM_MODE_ATTRIBUTE_ID:
            {
                result = get8BitValue(systemMode.value);
                break;
            }

            case SETPOINT_HOLD_ATTRIBUTE_ID:
            {
                result = get8BitValue(holdOn);
                break;
            }

            case LOCAL_TEMPERATURE_ATTRIBUTE_ID:
            {
                result = get16BitValue(localTemperature);
                break;
            }

            case ABS_MIN_HEAT_SETPOINT_ATTRIBUTE_ID:
            {
                result = get16BitValue(absMinHeatSetpoint);
                break;
            }

            case ABS_MAX_HEAT_SETPOINT_ATTRIBUTE_ID:
            {
                result = get16BitValue(absMaxHeatSetpoint);
                break;
            }

            case ABS_MIN_COOL_SETPOINT_ATTRIBUTE_ID:
            {
                result = get16BitValue(absMinCoolSetpoint);
                break;
            }

            case ABS_MAX_COOL_SETPOINT_ATTRIBUTE_ID:
            {
                result = get16BitValue(absMaxCoolSetpoint);
                break;
            }

            case LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID:
            {
                result = get8BitValue(localTempCalibration);
                break;
            }

            case OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID:
            {
                result = get16BitValue(coolSetpoint);
                break;
            }

            case OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID:
            {
                result = get16BitValue(heatSetpoint);
                break;
            }

            case CONTROL_SEQUENCE_OF_OPERATION_ATTRIBUTE_ID:
            {
                result = get8BitValue(controlSequence.value);
                break;
            }

        }

        return result;
    }

    @Override
    public Status setAttributeData(AttributeInfo info, byte[] payload)
    {
        Status retVal = Status.FAILURE;
        switch(info.getId())
        {
            case SYSTEM_MODE_ATTRIBUTE_ID:
            {
                ThermostatSystemMode temp = ThermostatSystemMode.fromId(payload[0]);

                if(temp != null)
                {
                    systemMode = temp;
                    retVal = Status.SUCCESS;
                }
                else
                {
                    retVal = Status.FAILURE;
                }
                break;
            }

            case OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID:
            case OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID:
            {
                ByteBuffer buf = ByteBuffer.allocate(Short.BYTES);
                buf.order(ByteOrder.LITTLE_ENDIAN);
                buf.put(payload);
                buf.flip();
                int setpoint = buf.getShort() & 0xffff;
                if (info.getId() == OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID)
                {
                    setCoolSetpoint(setpoint);
                }
                else
                {
                    setHeatSetpoint(setpoint);
                }
                retVal = Status.SUCCESS;
                break;
            }

            // TODO:  Write the attributes as needed for tests
            case RUNNING_STATE_ATTRIBUTE_ID:
            case SETPOINT_HOLD_ATTRIBUTE_ID:
            case LOCAL_TEMPERATURE_ATTRIBUTE_ID:
            case ABS_MIN_HEAT_SETPOINT_ATTRIBUTE_ID:
            case ABS_MAX_HEAT_SETPOINT_ATTRIBUTE_ID:
            case ABS_MIN_COOL_SETPOINT_ATTRIBUTE_ID:
            case ABS_MAX_COOL_SETPOINT_ATTRIBUTE_ID:
            case LOCAL_TEMPERATURE_CALIBRATION_ATTRIBUTE_ID:
            case CONTROL_SEQUENCE_OF_OPERATION_ATTRIBUTE_ID:
            default:
            {
                break;
            }
        }
        return retVal;
    }

    public void setHeatSetpoint(int heatSetpoint)
    {
        if (heatSetpoint != this.heatSetpoint)
        {
            this.heatSetpoint = heatSetpoint;
            sendAttributeReport(OCCUPIED_HEATING_SETPOINT_ATTRIBUTE_ID);
        }
    }

    public int getHeatSetpoint()
    {
        return heatSetpoint;
    }

    public void setCoolSetpoint(int coolSetpoint)
    {
        if (coolSetpoint != this.coolSetpoint)
        {
            this.coolSetpoint = coolSetpoint;
           sendAttributeReport(OCCUPIED_COOLING_SETPOINT_ATTRIBUTE_ID);
        }
    }

    public int getCoolSetpoint()
    {
        return coolSetpoint;
    }

    public void setSystemMode(ThermostatSystemMode systemMode)
    {
        if (systemMode != this.systemMode)
        {
            this.systemMode = systemMode;
            sendAttributeReport(SYSTEM_MODE_ATTRIBUTE_ID);
        }
    }

    public ThermostatSystemMode getSystemMode()
    {
        return systemMode;
    }

    public void setHoldOn(boolean holdOn)
    {
        this.holdOn = holdOn;
        sendAttributeReport(SETPOINT_HOLD_ATTRIBUTE_ID);
    }

    public boolean getHoldOn()
    {
        return holdOn;
    }
}
