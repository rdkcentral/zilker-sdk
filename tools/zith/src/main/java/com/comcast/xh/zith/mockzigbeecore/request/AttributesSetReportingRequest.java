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

import java.util.List;

public class AttributesSetReportingRequest extends DeviceRequest
{
    private int endpointId;
    private int clusterId;
    private boolean mfgSpecific;
    private List<AttributeReportingConfig> configs;

    public AttributesSetReportingRequest(int requestId, String eui64, int endpointId, int clusterId, boolean mfgSpecific, List<AttributeReportingConfig> configs)
    {
        super(requestId, eui64, "attributesSetReportingResponse");
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.mfgSpecific = mfgSpecific;
        this.configs = configs;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        MockEndpoint endpoint = device.getEndpoint(endpointId);
        if (endpoint != null)
        {
            MockCluster cluster = endpoint.getServerCluster(clusterId);
            if (cluster != null)
            {
                cluster.addReportingConfigs(configs);
            }
        }
        // TODO we should probably signal some failure if any of the above does not exist or fails
        new Response(requestId, 0, responseType).send();
    }
}
