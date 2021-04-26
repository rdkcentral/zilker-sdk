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
import com.comcast.xh.zith.device.cluster.AttributeInfo;
import com.comcast.xh.zith.device.cluster.MockCluster;

public class GetAttributeInfosRequest extends DeviceRequest
{
    private int endpointId;
    private int clusterId;
    private boolean server;
    private Integer mfgId;

    public GetAttributeInfosRequest(int requestId, String eui64, int endpointId, int clusterId, boolean server, Integer mfgId)
    {
        super(requestId, eui64, "getAttributeInfosResponse");
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.server = server;
        this.mfgId = mfgId;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        MockEndpoint endpoint = device.getEndpoint(endpointId);
        MockCluster cluster = server ? endpoint.getServerCluster(clusterId) : endpoint.getClientCluster(clusterId);

        //"attributeInfos":[{"id":0,"type":32},{"id":2,"type":32},{"id":3,"type":32},{"id":4,"type":66},{"id":5,"type":66},{"id":6,"type":66},{"id":7,"type":48}]}

        new Response(requestId, 0, responseType) {
            public AttributeInfo[] getAttributeInfos()
            {
                return cluster.getAttributeInfos(mfgId);
            }
        }.send();
    }
}
