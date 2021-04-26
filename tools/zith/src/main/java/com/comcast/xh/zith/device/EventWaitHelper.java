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

//
// Created by tlea on 2/11/19.
//

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import static org.junit.jupiter.api.Assertions.fail;

/**
 * Something that a test will use to wait for something to happen to a device.
 */
public class EventWaitHelper
{
    private CountDownLatch latch = new CountDownLatch(1);

    public EventWaitHelper() {}

    /**
     * Wait for the event
     * @param millis
     * @return true if the event was received, false if we timed out
     */
    public boolean waitForEvent(long millis)
    {
        boolean result = true;
        try
        {
            if (latch.await(millis, TimeUnit.MILLISECONDS) == false)
            {
                fail("Timed out waiting for device event");
                result = false;
            }
        } catch(InterruptedException e)
        {
        }

        return result;
    }

    public void notifyWaiters()
    {
        latch.countDown();
    }

    public void reset()
    {
        latch = new CountDownLatch(1);
    }
}
