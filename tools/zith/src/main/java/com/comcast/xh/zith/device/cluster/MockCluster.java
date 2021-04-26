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

package com.comcast.xh.zith.device.cluster;

//
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.device.EventWaitHelper;
import com.comcast.xh.zith.mockzigbeecore.MockZigbeeCore;
import com.comcast.xh.zith.mockzigbeecore.event.AttributeReportEvent;
import com.comcast.xh.zith.mockzigbeecore.request.AttributeReportingConfig;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.*;

public class MockCluster
{
    private static Logger log = LoggerFactory.getLogger(MockCluster.class);

    private static final int STD_MFG_ID = -1;
    public static final short UC_MFG_ID         = 0x10A0;
    public static final short UC_MFG_ID_WRONG   = 0x4256;
    public static final short COMCAST_MFG_ID    = 0x111d;

    private String clusterName;
    private String eui64;
    private int endpointId;
    private int clusterId;
    private boolean server;
    private Map<Integer, Map<Integer, AttributeInfo>> attributeInfos; //mfgId to map of attribute id to attribute info
    private Map<Integer, Map<Integer, EventWaitHelper>> mfgIdWaits; //mfgId to map of commandId to EventWaitHelper
    private Map<Integer, AttributeReportingConfig> reportingConfigs;

    public MockCluster(String clusterName, String eui64, int endpointId, int clusterId, boolean isServer)
    {
        this.clusterName = clusterName;
        this.eui64 = eui64;
        this.endpointId = endpointId;
        this.clusterId = clusterId;
        this.server = isServer;

        attributeInfos = new HashMap<>();
        mfgIdWaits = new HashMap<>();
        reportingConfigs = new HashMap<>();
    }

    public enum Status
    {
        SUCCESS                 (0x0),
        FAILURE                 (0x01),
        NOT_AUTHORIZED          (0x7e),
        MALFORMED_COMMAND       (0x80),
        UNSUPPORTED_ATTRIBUTE   (0x86);

        private final short status;
        Status (int status)
        {
            this.status = (short) status;
        }

        public short getValue()
        {
            return status;
        }
    }

    public String getClusterName()
    {
        return clusterName;
    }

    public String getEui64()
    {
        return eui64;
    }

    public int getEndpointId()
    {
        return endpointId;
    }

    public int getClusterId()
    {
        return clusterId;
    }

    public boolean isServer()
    {
        return server;
    }

    public AttributeData readAttribute(int attributeId)
    {
        return readMfgAttribute(STD_MFG_ID, attributeId);
    }

    public AttributeData readMfgAttribute(int mfgId, int attributeId)
    {
        AttributeData result = null;

        AttributeInfo info = getMfgAttributeInfo(mfgId, attributeId);

        if(info != null)
        {
            byte[] data = getAttributeData(info);

            if (data != null)
            {
                result = new AttributeData(attributeId, info.getType(), data);
            }
            else
            {
                log.error("cluster returned null data for read attribute " + attributeId + " on cluster " + clusterName);
            }
        }
        else
        {
            log.error("attribute info not found");
        }

        return result;
    }

    public Status writeAttribute(AttributeData data)
    {
        int attributeId = data.getId();
        int mfgId = STD_MFG_ID;
        if (data.getMfgId() != null)
        {
            mfgId = data.getMfgId();
        }
        AttributeInfo info = getMfgAttributeInfo(mfgId, attributeId);
        Status status = Status.FAILURE;
        if(info != null)
        {
            status = setAttributeData(info, data.getRawData());
            if (status != Status.SUCCESS)
            {
                log.error("Failed to write attribute " + attributeId);
            }
        }
        else
        {
            log.error("Attribute info not found for mfgId " + mfgId + ", attributeId " + attributeId);
        }
        return status;
    }

    protected byte[] getAttributeData(AttributeInfo attributeInfo) { return null; }

    protected Status setAttributeData(AttributeInfo attributeInfo, byte[] data)
    {
        return Status.FAILURE;
    }

    protected void addAttribute(int id, int type)
    {
        addMfgAttribute(STD_MFG_ID, id, type);
    }

    protected void addMfgAttribute(int mfgId, int id, int type)
    {
        Map<Integer,AttributeInfo> attributeMap = attributeInfos.computeIfAbsent(mfgId, k -> new HashMap<Integer, AttributeInfo>());
        attributeMap.put(id, new AttributeInfo(id, type, mfgId));
    }

    public AttributeInfo getAttributeInfo(int id)
    {
        return getMfgAttributeInfo(STD_MFG_ID, id);
    }

    public AttributeInfo getMfgAttributeInfo(int mfgId, int id)
    {
        AttributeInfo info = null;
        Map<Integer,AttributeInfo> attributeMap = attributeInfos.get(mfgId);
        if (attributeMap != null)
        {
            info = attributeMap.get(id);
        }

        return info;
    }

    public AttributeInfo[] getAttributeInfos(Integer mfgId)
    {
        int mfgLookupId = STD_MFG_ID;
        if (mfgId != null)
        {
            mfgLookupId = mfgId;
        }
        Map<Integer,AttributeInfo> attributeMap = attributeInfos.get(mfgLookupId);
        if (attributeMap != null)
        {
            return attributeMap.values().toArray(new AttributeInfo[0]);
        }
        else
        {
            return new AttributeInfo[]{};
        }
    }

    protected byte[] getAttributeBytes(String str)
    {
        //1 byte length prefixed
        byte[] result = new byte[str.length() + 1];
        result[0] = (byte)str.length();
        System.arraycopy(str.getBytes(), 0, result, 1, result[0]);
        return result;
    }

    public void handleCommand(int commandId, byte[] payload)
    {
        log.error("unhandled command " + Integer.toHexString(commandId));
    }

    public void handleMfgCommand(int mfgId, int commandId, byte[] payload)
    {
        log.error("unhandled mfg command " + Integer.toHexString(commandId));

        notifyMfgCommandWait(mfgId, commandId);
    }

    public EventWaitHelper getMfgCommandWait(int mfgId, int commandId)
    {
        EventWaitHelper deviceEventWait = new EventWaitHelper();

        Map<Integer, EventWaitHelper> waits = mfgIdWaits.computeIfAbsent(mfgId,
                                                                         k -> new HashMap<Integer, EventWaitHelper>());

        waits.put(commandId, deviceEventWait);

        return deviceEventWait;
    }

    public void addReportingConfigs(List<AttributeReportingConfig> configs)
    {
        for(AttributeReportingConfig config : configs)
        {
            reportingConfigs.put(config.getInfo().getId(), config);
        }
    }

    public Collection<AttributeReportingConfig> getReportingConfigs()
    {
        return reportingConfigs.values();
    }

    public AttributeReportingConfig getReportingConfig(int attributeId)
    {
        return reportingConfigs.get(attributeId);
    }

    protected void notifyMfgCommandWait(int mfgId, int commandId)
    {
        Map<Integer, EventWaitHelper> waits = mfgIdWaits.get(mfgId);
        if(waits != null)
        {
            EventWaitHelper deviceEventWait = waits.remove(commandId);
            if(deviceEventWait != null)
            {
                deviceEventWait.notifyWaiters();
            }
        }
    }

    protected void sendAttributeReport(int attributeId)
    {
        sendMfgAttributeReport(STD_MFG_ID, attributeId);
    }

    protected void sendMfgAttributeReport(int mfgId, int attributeId)
    {
        // Only send if we are already paired
        if (MockZigbeeCore.getInstance().getDevice(getEui64()) != null)
        {
            AttributeInfo attrInfo = getMfgAttributeInfo(mfgId, attributeId);
            if (attrInfo == null)
            {
                throw new RuntimeException(
                        "Cannot send attribute report for unknown attribute " + attributeId + ", device: " + getEui64()
                                + ", endpoint: " + getEndpointId() + ", cluster: " + getClusterId());
            }
            // TODO pass mfgId
            new AttributeReportEvent(getEui64(), getEndpointId(), getClusterId(), attributeId, attrInfo.getType(),
                    getAttributeData(attrInfo), mfgId).send();
        }
    }

    final static class DataType {
        private DataType() {}

        static final int NULL       = 0x00;

        static final int BOOLEAN    = 0x10;
        static final int MAP8       = 0x18;
        static final int MAP16      = 0x19;
        static final int MAP24      = 0x1a;
        static final int MAP32      = 0x1b;

        static final int UINT8      = 0x20;
        static final int UINT16     = 0x21;
        static final int UINT32     = 0x23;

        static final int INT8       = 0x28;
        static final int INT16      = 0x29;
        static final int INT24      = 0x2a;
        static final int INT32      = 0x2b;

        static final int ENUM8      = 0x30;
        static final int ENUM16     = 0x31;

        static final int STRING     = 0x42;

        static final int EUI64      = 0xf0;
    }
}
