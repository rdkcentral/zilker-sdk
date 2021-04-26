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

package com.comcast.xh.zith.device;

//
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.device.cluster.MockBasicCluster;
import com.comcast.xh.zith.device.cluster.MockCluster;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

public class MockEndpoint
{
    private int endpointId;
    private int profileId;
    private int deviceId;
    private int deviceVersion;
    private boolean enabled = true;

    protected Map<Integer, MockCluster> serverClusters;
    protected Map<Integer, MockCluster> clientClusters;

    public MockEndpoint(String eui64, int endpointId, int profileId, int deviceId, int deviceVersion, String manufacturer, String model, int hardwareVersion)
    {
        this(eui64, endpointId, profileId, deviceId, deviceVersion, manufacturer, model, hardwareVersion, false);
    }

    public MockEndpoint(String eui64, int endpointId, int profileId, int deviceId, int deviceVersion, String manufacturer, String model, int hardwareVersion, boolean hasMfgSpecificAttribute)
    {
        this.endpointId = endpointId;
        this.profileId = profileId;
        this.deviceId = deviceId;
        this.deviceVersion = deviceVersion;

        serverClusters = new HashMap<Integer, MockCluster>();
        clientClusters = new HashMap<Integer, MockCluster>();

        //All endpoints have the basic cluster
        MockBasicCluster basicCluster = new MockBasicCluster(eui64, endpointId, manufacturer, model, hardwareVersion, hasMfgSpecificAttribute);
        serverClusters.put(0, basicCluster);
    }

    public MockEndpoint(String eui64, int endpointId, int profileId, int deviceId, int deviceVersion)
    {
        this(eui64, endpointId, profileId, deviceId, deviceVersion, null, null, 0);
    }

    public void addCluster(MockCluster cluster)
    {
        if(cluster.isServer())
        {
            serverClusters.put(cluster.getClusterId(), cluster);
        }
        else
        {
            clientClusters.put(cluster.getClusterId(), cluster);
        }
    }

    public MockCluster getServerCluster(int clusterId)
    {
        return serverClusters.get(clusterId);
    }

    public MockCluster getClientCluster(int clusterId)
    {
        return clientClusters.get(clusterId);
    }

    public Integer[] getServerClusterIds()
    {
        Integer[] result = serverClusters.keySet().toArray(new Integer[0]);
        Arrays.sort(result);
        return result;
    }

    public Integer[] getClientClusterIds()
    {
        Integer[] result = clientClusters.keySet().toArray(new Integer[0]);
        Arrays.sort(result);
        return result;
    }

    public int getEndpointId()
    {
        return endpointId;
    }

    public int getProfileId()
    {
        return profileId;
    }

    public int getDeviceId()
    {
        return deviceId;
    }

    public int getDeviceVersion()
    {
        return deviceVersion;
    }

    public boolean isEnabled()
    {
        return enabled;
    }

    public void setEnabled(boolean enabled)
    {
        this.enabled = enabled;
    }
}
