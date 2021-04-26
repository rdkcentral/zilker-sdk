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


package com.comcast.xh.zith.mockzigbeecore.event;

//
// Created by tlea on 1/7/19.
//

import com.comcast.xh.zith.device.MockLegacySecurityDevice;
import com.comcast.xh.zith.device.cluster.MockZoneCluster;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

import static com.comcast.xh.zith.device.MockZigbeeDevice.HA_PROFILE;
import static com.comcast.xh.zith.device.cluster.MockCluster.UC_MFG_ID;

/*
{
	"timestamp":	"1546960002082",
	"eui64":	"000d6f00015d8757",
	"eventType":	"clusterCommandReceived",
	"sourceEndpoint":	1,
	"profileId":	260,
	"direction":	0,
	"clusterId":	1280,
	"commandId":	3,
	"mfgSpecific":	true,
	"mfgCode":	4256,
	"seqNum":	1,
	"apsSeqNum":	249,
	"rssi":	-13,
	"lqi":	255,
	"encodedBuf":	"BAAEAREAABMAHWAJABI8AAEAAAAAAAQ="
}

   deviceInfoMessage->firmwareVer[0] = message[0];
    deviceInfoMessage->firmwareVer[1] = message[1];
    deviceInfoMessage->firmwareVer[2] = message[2];

    deviceInfoMessage->devType = (uCDeviceType) message[3];

    deviceInfoMessage->devStatus.byte1 = message[4];
    deviceInfoMessage->devStatus.byte2 = message[5];

    deviceInfoMessage->config.lowTempLimit.tempFrac = message[6];
    deviceInfoMessage->config.lowTempLimit.tempInt = message[7];
    deviceInfoMessage->config.highTempLimit.tempFrac = message[8];
    deviceInfoMessage->config.highTempLimit.tempInt = message[9];
    deviceInfoMessage->config.lowBattThreshold = message[11] + (message[10] << 8);

    deviceInfoMessage->config.devNum = message[12];
    deviceInfoMessage->config.enables.byte = message[13];

    deviceInfoMessage->config.hibernateDuration = message[15] + (message[14] << 8);
    deviceInfoMessage->config.napDuration = message[16] + (message[17] << 8);

    deviceInfoMessage->config.region = message[18];

 */

public class LegacySecurityDeviceInfoEvent extends ClusterCommandEvent
{
    /**
     * Some devices emit a longer message with up to 23 bytes.
     * These extended messages have a region (byte), manufacturer id (short)
     * and model id (short)
     */
    public final static int DEVICE_INFO_LENGTH = 23;
    public final static int DEVICE_INFO_LENGTH_BASIC = 18;

    private MockLegacySecurityDevice device;

    public LegacySecurityDeviceInfoEvent(String eui64, int endpointId)
    {
        super(eui64, 260, endpointId, MockZoneCluster.CLUSTER_ID, true, true, UC_MFG_ID, 3);
    }

    public LegacySecurityDeviceInfoEvent(String eui64, int endpointId, MockLegacySecurityDevice device)
    {
        super(eui64,
              HA_PROFILE,
              endpointId,
              MockZoneCluster.CLUSTER_ID,
              true,
              true,
              UC_MFG_ID,
              0x03);

        this.device = device;
    }

    @Override
    protected byte[] getCommandPayload()
    {
        ByteBuffer payload = ByteBuffer.allocate(DEVICE_INFO_LENGTH);
        payload.order(ByteOrder.LITTLE_ENDIAN);

        long firmwareVersion = device.getZigbeeFirmwareVersion();
        payload.put((byte) ((firmwareVersion & 0x00ff0000) >> 16))
               .put((byte) ((firmwareVersion & 0x0000ff00) >> 8))
               .put((byte) (firmwareVersion & 0x000000ff))
               .put(device.getDeviceType().getId())
               .put(device.getStatus())
               .put(device.getLowTempLimit().toByteArray())
               .put(device.getHighTempLimit().toByteArray())
               .putShort(device.getLowBatteryThreshold())
               .put(device.getDeviceNumber())
               .put(device.getEnablesBitmap())
               .putShort(device.getHibernationDuration())
               .putShort(device.getNapDuration())

                // This isn't necessarily there on real devices (e.g., keypad)
               .put(device.getRegion().getRegionCode())
               .putShort(device.getMfgId())
               .putShort(device.getModelId());

        return payload.array();
    }
}
