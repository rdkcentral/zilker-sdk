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
import com.comcast.xh.zith.device.cluster.AttributeData;
import com.comcast.xh.zith.device.cluster.MockCluster;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class AttributesReadRequest extends DeviceRequest
{
    private static Logger log = LoggerFactory.getLogger(AttributesReadRequest.class);
    private int endpointId;
    private int clusterId;
    private boolean server;
    private int[] attributeIds;
    private Integer mfgId;

    public AttributesReadRequest(int requestId, String eui64, int endpointId, int clusterId, boolean server, int[] attributeIds, Integer mfgId)
    {
        super(requestId, eui64, "attributesReadResponse");
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.server = server;
        this.attributeIds = attributeIds;
        this.mfgId = mfgId;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        MockEndpoint endpoint = device.getEndpoint(endpointId);
        MockCluster cluster = server ? endpoint.getServerCluster(clusterId) : endpoint.getClientCluster(clusterId);

        new Response(requestId, 0, responseType)
        {
            public AttributeData[] getAttributeData()
            {
                AttributeData[] result = new AttributeData[attributeIds.length];

                int i = 0;
                for (int attributeId : attributeIds)
                {
                    if (mfgId != null)
                    {
                        result[i++] = cluster.readMfgAttribute(mfgId.intValue(), attributeId);
                    }
                    else
                    {
                        result[i++] = cluster.readAttribute(attributeId);
                    }
                }

                return result;
            }
        }.send();
    }
}
