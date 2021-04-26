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

package com.comcast.xh.zith.mockzigbeecore.event;

//
// Created by tlea on 7/18/19.
//

import com.comcast.xh.zith.device.EventWaitHelper;

import java.util.HashMap;
import java.util.Map;

/**
 * This base class is for sending a command event up and waiting for an APS ack response containing
 * a command id.
 */
public class LegacyApsAckedCommandEvent extends ClusterCommandEvent
{
    private static Map<Integer, ApsAckEventWaitHelper> apsAckEventWaitHelpers = new HashMap<Integer, ApsAckEventWaitHelper>();

    public LegacyApsAckedCommandEvent(String eui64, int endpointId, int commandId, int rssi, int lqi)
    {
        super(eui64, 260, endpointId, 1280, true, true, 4256, commandId, rssi, lqi);
    }

    public static void apsAckReceived(int sequenceNum, int command)
    {
        ApsAckEventWaitHelper eventWaitHelper = apsAckEventWaitHelpers.remove(sequenceNum);
        if(eventWaitHelper != null)
        {
            eventWaitHelper.command = command;
            eventWaitHelper.notifyWaiters();
        }
    }

    public int sendAndWait(int waitMillis)
    {
        ApsAckEventWaitHelper waitHelper = new ApsAckEventWaitHelper();
        apsAckEventWaitHelpers.put(getApsSeqNum(), waitHelper);
        send();
        waitHelper.waitForEvent(waitMillis);
        return waitHelper.command;
    }

    private class ApsAckEventWaitHelper extends EventWaitHelper
    {
        int command = -1;
    }
}
