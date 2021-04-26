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

import com.comcast.xh.zith.mockzigbeecore.event.Event;

public class Response extends Event
{
    private int requestId;
    private int resultCode;
    private String responseType;

    public enum ZhalStatus
    {
        OK                      (0),
        FAIL                    (-1),
        INVALID_ARG             (-2),
        NOT_IMPLEMENTED         (-3),
        TIMEOUT                 (-4),
        OUT_OF_MEMORY           (-5),
        MESSAGE_DELIVERY_FAILED (-6),
        NETWORK_BUSY            (-7),
        NOT_READY               (-8),
        LPM                     (-9);

        private final int status;
        ZhalStatus(int status)
        {
            this.status = status;
        }

        public int getValue()
        {
            return status;
        }
    }

    public Response(int requestId, int resultCode, String responseType)
    {
        super("ipcResponse");

        this.requestId = requestId;
        this.resultCode = resultCode;
        this.responseType = responseType;
    }

    public int getRequestId()
    {
        return requestId;
    }

    public int getResultCode()
    {
        return resultCode;
    }

    public String getResponseType()
    {
        return responseType;
    }
}
