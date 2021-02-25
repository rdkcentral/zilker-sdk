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

package com.comcast.xh.zith.mockzigbeecore.request;

import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;

public class NetworkHealthCheckConfigure extends Request
{
    private int intervalMillis;
    private int ccaThreshold;
    private int ccaFailureThreshold;
    private int restoreThreshold;
    private int delayBetweenThresholdRetriesMillis;

    public NetworkHealthCheckConfigure(int requestId,
                                       int intervalMillis,
                                       int ccaThreshold,
                                       int ccaFailureThreshold,
                                       int restoreThreshold,
                                       int delayBetweenThresholdRetriesMillis)
    {
        super(requestId);

        this.intervalMillis = intervalMillis;
        this.ccaThreshold = ccaThreshold;
        this.ccaFailureThreshold = ccaFailureThreshold;
        this.restoreThreshold = restoreThreshold;
        this.delayBetweenThresholdRetriesMillis = delayBetweenThresholdRetriesMillis;
    }

    @Override
    public void execute()
    {
        new Response(requestId, 0, "networkHealthCheckConfigureResponse").send();
    }
}
