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

import com.comcast.xh.zith.device.capability.Tamperable;
import com.comcast.xh.zith.mockzigbeecore.event.AlarmClusterEvent;
import com.comcast.xh.zith.mockzigbeecore.event.AttributeReportEvent;
import com.comcast.xh.zith.mockzigbeecore.event.ClearAllPinCodesResponseEvent;
import com.comcast.xh.zith.mockzigbeecore.event.OperationEventNotificationEvent;
import com.comcast.xh.zith.mockzigbeecore.event.SetPinCodeResponseEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Objects;

public class MockDoorLockCluster extends MockCluster implements Tamperable
{
    private static Logger log = LoggerFactory.getLogger(MockCluster.class);

    private static final short LOCK_DOOR_COMMAND_ID = 0x00;
    private static final short UNLOCK_DOOR_COMMAND_ID = 0x01;
    private static final short SET_PIN_CODE = 0x05;
    private static final short CLEAR_ALL_PIN_CODES = 0x08;

    private static final short ATTRIBUTE_LOCK_STATE = 0x0;
    private static final short ATTRIBUTE_USER_CODE_DISABLE_TIME = 0x31;
    private static final short ATTRIBUTE_ALARM_MASK = 0x40;
    private static final short ATTRIBUTE_NUM_PIN_USERS_SUPPORTED = 0x12;
    private static final short ATTRIBUTE_MAX_PIN_CODE_LENGTH = 0x17;
    private static final short ATTRIBUTE_MIN_PIN_CODE_LENGTH = 0x18;
    private static final short ATTRIBUTE_AUTO_RELOCK_TIME = 0x23;
    private static final short ATTRIBUTE_KEYPAD_PROGRAMMING_EVENT_MASK = 0x45;

    // Alarm codes
    private static final short BOLT_JAMMED = 0x00;
    private static final short LOCK_RESET_TO_FACTORY_DEFAULTS = 0x01;
    private static final short BATTERY_REPLACEMENT = 0x02;
    private static final short RF_MODULE_POWER_CYCLED = 0x03;
    private static final short TAMPER_ALARM_WRONG_CODE_ENTRY_LIMIT = 0x04;
    private static final short TAMPER_ALARM_FRONT_ESCUTCHEON_REMOVED = 0x05;
    private static final short DOOR_FORCED_OPEN_WHILE_LOCKED = 0x06;

    private static final byte MAX_PIN_USERS_SUPPORTED = 30;
    private static final byte MAX_PIN_CODE_LENGTH = 8;
    private static final byte MIN_PIN_CODE_LENGTH = 8;

    private boolean isLocked = false;
    private boolean tampered = false;
    private boolean invalidCodeEntryLimit = false;
    private boolean lockBoltJammed = false;
    private int autoLockSeconds = 5;

    private List<PinEntry> pins = new ArrayList<>();

    public MockDoorLockCluster(String eui64, int endpointId)
    {
        super("doorLock", eui64, endpointId, 0x101, true);

        // Lock state
        addAttribute(ATTRIBUTE_LOCK_STATE, DataType.ENUM8);
        // Lock type
        addAttribute(0x1, DataType.ENUM8);
        // Actuator enabled
        addAttribute(0x2, DataType.BOOLEAN);
        // Num lock records supported
        addAttribute(0x10, DataType.UINT16);
        // Num total users supported
        addAttribute(0x11, DataType.UINT16);
        // Num pin users supported
        addAttribute(ATTRIBUTE_NUM_PIN_USERS_SUPPORTED, DataType.UINT16);
        // Num weekday schedules supported per user
        addAttribute(0x14, DataType.UINT8);
        // Num yearday schedules supported per user
        addAttribute(0x15, DataType.UINT8);
        // max pin length
        addAttribute(ATTRIBUTE_MAX_PIN_CODE_LENGTH, DataType.UINT8);
        // min pin length
        addAttribute(ATTRIBUTE_MIN_PIN_CODE_LENGTH, DataType.UINT8);
        // language
        addAttribute(0x21, DataType.STRING);
        // auto relock time
        addAttribute(ATTRIBUTE_AUTO_RELOCK_TIME, DataType.UINT32);
        // sound volume
        addAttribute(0x24, DataType.UINT8);
        // operating mode
        addAttribute(0x25, DataType.ENUM8);
        // enable one touch locking
        addAttribute(0x29, DataType.BOOLEAN);
        // enable inside status led
        addAttribute(0x2a, DataType.BOOLEAN);
        // enable privacy mode button
        addAttribute(0x2b, DataType.BOOLEAN);
        // wrong code entry limit
        addAttribute(0x30, DataType.UINT8);
        // user code temporary disable time
        addAttribute(ATTRIBUTE_USER_CODE_DISABLE_TIME, DataType.UINT8);
        // send pin over the air
        addAttribute(0x32, DataType.BOOLEAN);
        // zigbee security level
        addAttribute(0x34, DataType.ENUM8);
        // door lock alarm mask
        addAttribute(ATTRIBUTE_ALARM_MASK, DataType.MAP16);
        // keypad operation event mask
        addAttribute(0x41, DataType.MAP16);
        // rf operation event mask
        addAttribute(0x42, DataType.MAP16);
        // manual operation event mask
        addAttribute(0x43, DataType.MAP16);
        // rfid operation event mask
        addAttribute(0x44, DataType.MAP16);
        // keypad programming event mask
        addAttribute(ATTRIBUTE_KEYPAD_PROGRAMMING_EVENT_MASK, DataType.MAP16);
        // rf operation event mask
        addAttribute(0x46, DataType.MAP16);
        // rfid operation event mask
        addAttribute(0x47, DataType.MAP16);
        // ???, just putting some datatype there....
        addAttribute( 0x80, DataType.UINT32);
    }

    public void handleCommand(int commandId, byte[] payload)
    {
        switch (commandId)
        {
            case LOCK_DOOR_COMMAND_ID:
            {
                if (isLocked == false)
                {
                    setLocked(true);
                }
            }
            break;

            case UNLOCK_DOOR_COMMAND_ID:
            {
                if (isLocked)
                {
                    setLocked(false);
                }
            }
            break;

            case SET_PIN_CODE:
            {
                int userId = payload[0] + (payload[1] << 8);
                byte userStatus = payload[2];
                byte userType = payload[3];
                byte pinLength = payload[4];
                StringBuilder sb = new StringBuilder();
                for(int i = 0; i < pinLength; i++)
                {
                    sb.append((char)payload[5+i]);
                }

                PinEntry entry = new PinEntry(userId, userStatus, userType, sb.toString());

                //first check to see if we are full, then make sure this pin isnt already in the list
                byte status = 0; //default to success
                if (pins.size() == MAX_PIN_USERS_SUPPORTED)
                {
                    status = 2; //memory full
                }
                else
                {
                    // does this pin already exist in our list?
                    for(PinEntry e : pins)
                    {
                        if (e.pin.equals(entry.pin) == true)
                        {
                            log.error("Pin " + entry.pin + " already exists");
                            status = 3; //duplicate code
                            break;
                        }
                    }
                }

                if (status == 0)
                {
                    log.debug("adding pin " + entry.pin);
                    pins.add(entry);
                }

                new SetPinCodeResponseEvent(this, status).send();
            }
            break;

            case CLEAR_ALL_PIN_CODES:
            {
                pins.clear();
                new ClearAllPinCodesResponseEvent(this, (byte)0).send();
            }
            break;

            default:

        }
    }

    @Override
    protected byte[] getAttributeData(AttributeInfo info)
    {
        byte[] result = null;

        switch(info.getId())
        {
            case ATTRIBUTE_LOCK_STATE:
            {
                result = new byte[1];
                result[0] = (byte) (isLocked() ? 1 : 2);
                break;
            }

            case ATTRIBUTE_USER_CODE_DISABLE_TIME:
            {
                result = new byte[1];
                result[0] = 1;
                break;
            }

            case ATTRIBUTE_NUM_PIN_USERS_SUPPORTED:
            {
                result = new byte[1];
                result[0] = MAX_PIN_USERS_SUPPORTED;
                break;
            }

            case ATTRIBUTE_MAX_PIN_CODE_LENGTH:
            {
                result = new byte[1];
                result[0] = MAX_PIN_CODE_LENGTH;
                break;
            }

            case ATTRIBUTE_MIN_PIN_CODE_LENGTH:
            {
                result = new byte[1];
                result[0] = MIN_PIN_CODE_LENGTH;
                break;
            }

            case ATTRIBUTE_AUTO_RELOCK_TIME:
            {
                result = new byte[4];
                result[0] = (byte) (autoLockSeconds & 0xff);
                result[1] = (byte) ((autoLockSeconds >> 8) & 0xff);
                result[2] = (byte) ((autoLockSeconds >> 16) & 0xff);
                result[3] = (byte) ((autoLockSeconds >> 24) & 0xff);
                break;
            }
        }

        return result;
    }

    @Override
    protected Status setAttributeData(AttributeInfo attributeInfo, byte[] data)
    {
        Status result = Status.FAILURE;
        switch(attributeInfo.getId())
        {
            case ATTRIBUTE_KEYPAD_PROGRAMMING_EVENT_MASK:
            case ATTRIBUTE_ALARM_MASK: // TODO record this and then only send alarms if set
            {
                result = Status.SUCCESS;
                break;
            }

            default:
                break;
        }

        return result;
    }

    public boolean isLocked()
    {
        return isLocked;
    }

    public void setLocked(boolean isLocked)
    {
        this.isLocked = isLocked;

        // TODO add source
        new OperationEventNotificationEvent(this, isLocked).send();

        // Match what we assume with the lock
        this.tampered = false;
        this.lockBoltJammed = false;
    }

    // TODO add source for lock/unlock

    @Override
    public boolean isTampered()
    {
        return tampered;
    }

    @Override
    public void setTampered(boolean isTampered)
    {
        this.tampered = isTampered;

        if (this.tampered)
        {
            byte alarmCode = TAMPER_ALARM_FRONT_ESCUTCHEON_REMOVED;
            new AlarmClusterEvent(this, alarmCode).send();
        }
    }

    public boolean isInvalidCodeEntryLimit()
    {
        return invalidCodeEntryLimit;
    }

    public void setInvalidCodeEntryLimit(boolean invalidCodeEntryLimit)
    {
        this.invalidCodeEntryLimit = invalidCodeEntryLimit;

        if (this.invalidCodeEntryLimit)
        {
            byte alarmCode = TAMPER_ALARM_WRONG_CODE_ENTRY_LIMIT;
            new AlarmClusterEvent(this, alarmCode).send();
            // TODO we should auto clear this based on ATTRIBUTE_USER_CODE_DISABLE_TIME
        }
    }

    public void setLockBoltJammed(boolean lockBoltJammed)
    {
        this.lockBoltJammed = lockBoltJammed;

        if (this.lockBoltJammed)
        {
            byte alarmCode = BOLT_JAMMED;
            new AlarmClusterEvent(this, alarmCode).send();
        }
    }

    public boolean isLockBoltJammed()
    {
        return lockBoltJammed;
    }

    public void addPin(int userId, byte status, byte type, String pin)
    {
        pins.add(userId, new PinEntry(userId, status, type, pin));
    }

    public int getPinCount()
    {
        return pins.size();
    }

    public Collection<PinEntry> getPins()
    {
        return pins;
    }

    public byte getMaxPinCodeLength()
    {
        return MAX_PIN_CODE_LENGTH;
    }

    public byte getMinPinCodeLength()
    {
        return MIN_PIN_CODE_LENGTH;
    }

    public byte getMaxPinUsersSupported()
    {
        return MAX_PIN_USERS_SUPPORTED;
    }

    public int getAutoRelockTime()
    {
        return autoLockSeconds;
    }

    public void setAutoRelockTime(int seconds)
    {
        autoLockSeconds = seconds;

        byte[] payload = new byte[4];
        payload[0] = (byte) (autoLockSeconds & 0xff);
        payload[1] = (byte) ((autoLockSeconds >> 8) & 0xff);
        payload[2] = (byte) ((autoLockSeconds >> 16) & 0xff);
        payload[3] = (byte) ((autoLockSeconds >> 24) & 0xff);

        new AttributeReportEvent(getEui64(),
                getEndpointId(),
                getClusterId(),
                ATTRIBUTE_AUTO_RELOCK_TIME,
                DataType.UINT32,
                payload).send();
    }

    public static class PinEntry
    {
        int userId;
        byte status;
        byte type;
        String pin;

        public PinEntry(int userId, byte status, byte type, String pin)
        {
            this.userId = userId;
            this.status = status;
            this.type = type;
            this.pin = pin;
        }

        public int getUserId()
        {
            return userId;
        }

        public byte getStatus()
        {
            return status;
        }

        public byte getType()
        {
            return type;
        }

        public String getPin()
        {
            return pin;
        }

        public String toString()
        {
            return "id=" + userId + ", status=" + status + ", type=" + type + ", pin=" + pin;
        }

        @Override
        public boolean equals(Object o)
        {
            if (this == o)
            {
                return true;
            }
            if (o == null || getClass() != o.getClass())
            {
                return false;
            }
            PinEntry pinEntry = (PinEntry) o;

            // treat -1 user id as a 'wildcard'
            if (userId == -1 || pinEntry.userId == -1)
            {
                return status == pinEntry.status &&
                        type == pinEntry.type &&
                        pin.equals(pinEntry.pin);
            }
            else
            {
                return userId == pinEntry.userId &&
                        status == pinEntry.status &&
                        type == pinEntry.type &&
                        pin.equals(pinEntry.pin);
            }
        }

        @Override
        public int hashCode()
        {
            return Objects.hash(userId, status, type, pin);
        }
    }
}
