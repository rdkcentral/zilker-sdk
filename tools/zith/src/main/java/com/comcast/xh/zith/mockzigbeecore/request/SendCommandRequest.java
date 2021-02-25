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

//
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.device.MockEndpoint;
import com.comcast.xh.zith.device.cluster.MockCluster;

import java.util.Base64;

/*
{
	"request":	"sendCommand",
	"address":	"000d6f00015d8757",
	"endpointId":	1,
	"clusterId":	1280,
	"direction":	"clientToServer",
	"isMfgSpecific":	1,
	"mfgId":	4256,
	"commandId":	34,
	"encodedMessage":	"AQAAAA==",
	"requestDefaultResponse":	0,
	"requestId":	9
}

 */
public class SendCommandRequest extends DeviceRequest
{
    private int endpointId;
    private int clusterId;
    private boolean server;
    private boolean mfgSpecific;
    private int mfgId;
    private int commandId;
    private byte[] payload;

    public SendCommandRequest(int requestId, String eui64, int endpointId, int clusterId, boolean server, boolean mfgSpecific, int mfgId, int commandId, String payload)
    {
        this(requestId, eui64, endpointId, clusterId, server, mfgSpecific, mfgId, commandId, payload, "sendCommandResponse");
    }

    public SendCommandRequest(int requestId, String eui64, int endpointId, int clusterId, boolean server, boolean mfgSpecific, int mfgId, int commandId, String payload, String responseType)
    {
        super(requestId, eui64, responseType);
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.server = server;
        this.mfgSpecific = mfgSpecific;
        this.mfgId = mfgId;
        this.commandId = commandId;
        this.payload = Base64.getDecoder().decode(payload);
    }

    public int getCommandId()
    {
        return commandId;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        MockEndpoint endpoint = device.getEndpoint(endpointId);
        MockCluster cluster = server ? endpoint.getServerCluster(clusterId) : endpoint.getClientCluster(clusterId);

        if(mfgSpecific)
        {
            cluster.handleMfgCommand(mfgId, commandId, payload);
        }
        else
        {
            cluster.handleCommand(commandId, payload);
        }

        new Response(requestId, 0, responseType).send();
    }
}
