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

import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.device.MockEndpoint;
import com.comcast.xh.zith.device.cluster.AttributeData;
import com.comcast.xh.zith.device.cluster.MockCluster;
import com.comcast.xh.zith.device.cluster.MockCluster.*;

public class AttributesWriteRequest extends DeviceRequest
{
    private int endpointId;
    private int clusterId;
    private boolean server;
    private AttributeData[] attrWrites;

    public AttributesWriteRequest(int requestId, String eui64, int endpointId, int clusterId, boolean server, AttributeData[] attrWrites)
    {
        super(requestId, eui64, "attributesWriteResponse");
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.server = server;
        this.attrWrites = attrWrites;
    }

    @Override
    public void executeDeviceRequest(MockZigbeeDevice device)
    {
        MockEndpoint endpoint = device.getEndpoint(endpointId);
        MockCluster cluster = server ? endpoint.getServerCluster(clusterId) : endpoint.getClientCluster(clusterId);

        new Response(requestId, 0, responseType) {
            @Override
            public int getResultCode()
            {
                return setAttributeData().getValue();
            }

            private ZhalStatus setAttributeData()
            {
                boolean ok = true;
                for (AttributeData data : attrWrites)
                {
                    ok &= cluster.writeAttribute(data) == Status.SUCCESS;
                }
                return ok ? ZhalStatus.OK : ZhalStatus.FAIL;
            }
        }.send();
    }
}
