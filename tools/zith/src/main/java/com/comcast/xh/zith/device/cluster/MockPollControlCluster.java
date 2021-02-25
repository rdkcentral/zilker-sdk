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

import com.comcast.xh.zith.mockzigbeecore.event.PollControlClusterEvent;
import com.comcast.xh.zith.mockzigbeecore.event.ComcastPollControlCheckinEvent;
/**
 * TODO: stub!
 */
public class MockPollControlCluster extends MockCluster
{
    public MockPollControlCluster(String eui64, int endpointId) {
        super("pollControl", eui64, endpointId, 0x0020, true);

        //Check-in interval
        addAttribute(0x0, DataType.UINT32);
        //Long Poll interval
        addAttribute(0x1, DataType.UINT32);
        //Short Poll Interval
        addAttribute(0x2, DataType.UINT16);
        //Fast Poll Timeout
        addAttribute(0x3, DataType.UINT16);
        //Check-in Interval Min
        addAttribute(0x4, DataType.UINT32);
        //Long Poll Interval Min
        addAttribute(0x5, DataType.UINT32);
        //Fast Poll Timeout Max
        addAttribute(0x6, DataType.UINT16);
    }

    public void sendCheckIn()
    {
        new PollControlClusterEvent(this).send();
    }

    public void sendComcastCheckIn(Integer battVoltage, Integer battUsedMilliAmpHr, Integer temp, Integer rssi, Integer lqi, Integer retries, Integer rejoins)
    {
        new ComcastPollControlCheckinEvent(this, battVoltage,  battUsedMilliAmpHr, temp, rssi, lqi, retries, rejoins).send();
    }

    @Override
    protected Status setAttributeData(AttributeInfo attributeInfo, byte[] data)
    {
        return Status.SUCCESS;
    }
}
