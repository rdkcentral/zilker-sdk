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

package com.icontrol.api.client;

import com.icontrol.api.InvocationHandlerBase;
import com.icontrol.ipc.IPCSender;

public class ManagerInvocationHandler extends InvocationHandlerBase implements ServiceAvailabilityCheckable
{
    private int managerPort = 0;

    protected ManagerInvocationHandler(int managerPort)
    {
        this.managerPort = managerPort;
    }
    /**
     * Check if this service is currently available
     *
     * @return true if the service is available, false otherwise
     *
     * @see com.icontrol.api.client.ManagerInvocationHandler
     */
    public boolean isServiceAvailable()
    {
        return IPCSender.isServiceAvailable(managerPort);
    }

    /**
     * Wait for a service to become available
     *
     * @param timeoutInSeconds
     *            the number of seconds to wait until giving up. Pass 0 for no
     *            timeout
     * @return true if the service is now available, false if timed out
     *
     * @throws IllegalArgumentException
     *             if value for timeoutInSeconds is not a positive integer
     * @see com.icontrol.api.client.ManagerInvocationHandler
     */
    public boolean waitForServiceToBeAvailable(int timeoutInSeconds)
    {
        if (timeoutInSeconds < 0)
        {
            throw new IllegalArgumentException("timeoutInSeconds is out of bounds");
        }

        return IPCSender.waitForServiceAvailable(managerPort, timeoutInSeconds);
    }
}
