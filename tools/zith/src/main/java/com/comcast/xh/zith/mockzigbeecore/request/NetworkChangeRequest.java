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
import com.comcast.xh.zith.mockzigbeecore.event.NetworkConfigChangedEvent;

public class NetworkChangeRequest extends Request
{
    private byte channel;
    private short panId;
    private String networkKey;

    public NetworkChangeRequest(int requestId, byte channel, short panId, String networkKey)
    {
        super(requestId);
        this.channel = channel;
        this.panId = panId;
        this.networkKey = networkKey;
    }

    @Override
    public void execute()
    {
        new Response(requestId, 0, "networkChange").send();

        MockZigbeeCore.getInstance().networkChange(channel, panId, networkKey);
    }
}
