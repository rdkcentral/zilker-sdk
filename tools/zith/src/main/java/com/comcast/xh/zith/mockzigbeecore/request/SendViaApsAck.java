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
// Created by tlea on 7/18/19.
//

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.mockzigbeecore.event.LegacyApsAckedCommandEvent;

/*
{
	"request":	"sendViaApsAck",
	"address":	"3675c42e22131640",
	"endpointId":	1,
	"clusterId":	1280,
	"sequenceNum":	249,
	"encodedMessage":	"FKAQADPO/w==",
	"requestId":	15
}
 */

/**
 * These are treated basically like a SendCommandRequest, but we also notify anyone waiting for the ack (sequenceNum)
 */
public class SendViaApsAck extends SendCommandRequest
{
    private int sequenceNum;

    public SendViaApsAck(int requestId, String eui64, int endpointId, int clusterId, int sequenceNum, int mfgId, int commandId, String payload)
    {
        super(requestId,
              eui64,
              endpointId,
              clusterId,
              true,
              true,
              mfgId,
              commandId,
              payload,
                "sendViaApsAckResponse");

        this.sequenceNum = sequenceNum;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        super.executeDeviceRequest(device);

        LegacyApsAckedCommandEvent.apsAckReceived(sequenceNum, getCommandId());
    }
}
