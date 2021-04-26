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
import com.comcast.xh.zith.mockzigbeecore.event.BridgeRefreshCompletedEvent;

import java.util.Collections;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class MockBridgeCluster extends MockCluster implements Tamperable
{
    // Server commands received
    private static final int REFRESH_COMMAND_ID = 0x00;
    private static final int RESET_COMMAND_ID = 0x01;
    private static final int START_ZONE_CONFIGURATION_COMMAND_ID  = 0x02;
    private static final int STOP_ZONE_CONFIGURATION_COMMAND_ID = 0x03;

    // Server commands generated
    private static final int REFRESH_REQUESTED_COMMAND_ID = 0x00;

    // MFG ID for commands
    public static final int BRIDGE_MFG_ID = 0x1022;

    // Alarm codes
    private static final byte BRIDGE_TAMPER_ALARM_CODE = 0x00;

    private Set<BridgeListener> listeners = Collections.newSetFromMap(new ConcurrentHashMap<>());
    private int timeToRefreshMillis = 50;
    private boolean tampered = false;

    public MockBridgeCluster(String eui64, int endpointId)
    {
        super("bridge", eui64, endpointId, 0xFD00, true);

        // AlarmMask
        addMfgAttribute(BRIDGE_MFG_ID, 0x0000, DataType.MAP8);
    }

    public void handleMfgCommand(int mfgId, int commandId, byte[] payload)
    {
        if (mfgId == BRIDGE_MFG_ID)
        {
            if (commandId == REFRESH_COMMAND_ID)
            {
                // Let listeners do their thing
                for (BridgeListener listener : listeners)
                {
                    listener.refreshRequested();
                }
                // Spend some time doing the refresh
                try
                {
                    Thread.sleep(timeToRefreshMillis);
                } catch (InterruptedException e)
                {
                    // Don't care
                }
                // Complete the refresh
                new BridgeRefreshCompletedEvent(this).send();
            }
            else if (commandId == RESET_COMMAND_ID)
            {
                // TODO
            }
            else if (commandId == START_ZONE_CONFIGURATION_COMMAND_ID)
            {
                // TODO
            }
            else if (commandId == STOP_ZONE_CONFIGURATION_COMMAND_ID)
            {
                // TODO
            }
        }
    }

    public interface BridgeListener
    {
        void refreshRequested();
    }

    public void addBridgeListener(BridgeListener listener)
    {
        listeners.add(listener);
    }

    public void removeBridgeListener(BridgeListener listener)
    {
        listeners.remove(listener);
    }

    @Override
    public boolean isTampered()
    {
        return tampered;
    }

    @Override
    public void setTampered(boolean isTampered)
    {
        if (isTampered != tampered)
        {
            tampered = isTampered;
            new AlarmClusterEvent(this, BRIDGE_TAMPER_ALARM_CODE, tampered == false).send();
        }
    }

    @Override
    protected Status setAttributeData(AttributeInfo attributeInfo, byte[] data)
    {
        return Status.SUCCESS;
    }
}
