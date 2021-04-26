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
import com.comcast.xh.zith.device.cluster.MockOtaUpgradeCluster;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;

import java.util.*;

abstract public class MockZigbeeDevice extends BaseMockDevice
{
    public static final short HA_PROFILE = 0x0104;

    private String name;
    private boolean discovered;
    protected Map<Integer, MockEndpoint> endpoints;
    private boolean isRouter;
    private boolean isMainsPowered;
    private String parentEui64 = MockZigbeeCore.getInstance().getEui64();

    public MockZigbeeDevice(String name, boolean isRouter, boolean isMainsPowered)
    {
        this(name, null, isRouter, isMainsPowered);
    }

    public MockZigbeeDevice(String name, String eui64, boolean isRouter, boolean isMainsPowered)
    {
        super(name, eui64 != null ? eui64 : getRandomEui64());
        discovered = false;
        this.isRouter = isRouter;
        this.isMainsPowered = isMainsPowered;
        this.name = name;
        endpoints = new HashMap<Integer, MockEndpoint>();

        System.out.println(name + " MockDevice created with eui64 " + getEui64());
    }

    private static String getRandomEui64()
    {
        Random r = new Random();
        long l = r.nextLong();
        return String.format("%016x", l);
    }

    public MockZigbeeDevice(String name)
    {
        this(name, false, false);
    }

    public MockZigbeeDevice(String name, String eui64)
    {
        this(name, eui64, false, false);
    }

    public boolean isDiscovered()
    {
        return discovered;
    }

    public void setDiscovered(boolean discovered)
    {
        this.discovered = discovered;
    }

    public boolean isRouter()
    {
        return isRouter;
    }

    public boolean isMainsPowered()
    {
        return isMainsPowered;
    }

    public MockEndpoint getEndpoint(int endpointId)
    {
        return endpoints.get(endpointId);
    }

    public String getEui64()
    {
        return getUUID();
    }

    public boolean isDefaulted()
    {
        return findServerClusterOfType(MockBasicCluster.class).isDefaulted();
    }

    public void setDefaulted(boolean isDefaulted)
    {
        findServerClusterOfType(MockBasicCluster.class).setDefaulted(isDefaulted);
    }

    public Integer[] getEndpointIds()
    {
        List<Integer> enabledEndpointIds = new ArrayList<>();
        for(Map.Entry<Integer,MockEndpoint> entry : endpoints.entrySet())
        {
            if (entry.getValue().isEnabled())
            {
                enabledEndpointIds.add(entry.getKey());
            }
        }
        Integer[] result = enabledEndpointIds.toArray(new Integer[0]);
        Arrays.sort(result);
        return result;
    }

    public List<MockEndpoint> getEndpointsByZigbeeDeviceId(int deviceId)
    {
        List<MockEndpoint> eps = new ArrayList<>();
        for(MockEndpoint endpoint : endpoints.values())
        {
            if (endpoint.getDeviceId() == deviceId)
            {
                eps.add(endpoint);
            }
        }

        return eps;
    }

    public void joinComplete()
    {
    }

    public String getParent()
    {
        return parentEui64;
    }

    public void setParent(String parentEui64)
    {
        this.parentEui64 = parentEui64;
    }

    /* TODO: These may need to return a collection of clusters, first implementer wins */
    public <T> T findServerClusterOfType(Class<T> clusterClass)
    {
        for(MockEndpoint endpoint : endpoints.values())
        {
            for(int id : endpoint.getServerClusterIds())
            {
                MockCluster cluster = endpoint.getServerCluster(id);
                if (clusterClass.isInstance(cluster))
                {
                    return clusterClass.cast(cluster);
                }
            }
        }

        return null;
    }

    public <T> T findRequiredServerClusterOfType(Class<T> clusterClass)
    {
        T cluster = findServerClusterOfType(clusterClass);
        if (cluster == null)
        {
            throw new UnsupportedOperationException("No " + clusterClass.getSimpleName() + " cluster found in " + getClass());
        }
        return cluster;
    }

    public <T> T findClientClusterOfType(Class<T> clusterClass)
    {
        for(MockEndpoint endpoint : endpoints.values())
        {
            for(int id : endpoint.getClientClusterIds())
            {
                MockCluster cluster = endpoint.getClientCluster(id);
                if (clusterClass.isInstance(cluster))
                {
                    return clusterClass.cast(cluster);
                }
            }
        }

        return null;
    }

    public String getManufacturer()
    {
        return findServerClusterOfType(MockBasicCluster.class).getManufacturer();
    }

    public String getModel()
    {
        return findServerClusterOfType(MockBasicCluster.class).getModel();
    }

    public int getZigbeeHardwareVersion()
    {
        return findServerClusterOfType(MockBasicCluster.class).getHardwareVersion();
    }

    public long getZigbeeFirmwareVersion()
    {
        return findClientClusterOfType(MockOtaUpgradeCluster.class).getFirmwareVersion();
    }

    public String getFirmwareVersion()
    {
        return String.format("0x%08x", getZigbeeFirmwareVersion());
    }

    public String getHardwareVersion()
    {
        return Integer.toString(getZigbeeHardwareVersion());
    }
}
