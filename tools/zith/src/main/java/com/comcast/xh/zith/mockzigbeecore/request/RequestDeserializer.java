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

import com.comcast.xh.zith.device.cluster.AttributeData;
import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.core.ObjectCodec;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.DeserializationContext;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.ObjectReader;
import com.fasterxml.jackson.databind.deser.std.StdDeserializer;
import org.apache.commons.codec.binary.Base64;
import org.json.JSONArray;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.util.List;

public class RequestDeserializer extends StdDeserializer<Request>
{
    private static Logger log = LoggerFactory.getLogger(RequestDeserializer.class);
    private static ObjectMapper mapper = new ObjectMapper();

    public RequestDeserializer()
    {
        this(null);
    }

    public RequestDeserializer(Class<?> vc)
    {
        super(vc);
    }

    @Override
    public Request deserialize(JsonParser parser, DeserializationContext deserializer)
    {
        Request request = null;

        try
        {
            ObjectCodec codec = parser.getCodec();
            JsonNode node = codec.readTree(parser);

            log.debug("Received Request: " + node);

            JsonNode requestNode = node.get("request");
            JsonNode requestIdNode = node.get("requestId");
            int requestId = requestIdNode.asInt();

            if (requestNode.asText().equals("heartbeat"))
            {
                request = new HeartbeatRequest(requestId);
            }
            else if (requestNode.asText().equals("networkInit"))
            {
                request = new NetworkInitRequest(requestId);
            }
            else if (requestNode.asText().equals("networkTerm"))
            {
                request = new NetworkTermRequest(requestId);
            }
            else if (requestNode.asText().equals("networkEnableJoin"))
            {
                int durationSeconds = node.get("durationSeconds").asInt();
                request = new NetworkEnableJoinRequest(requestId, durationSeconds);
            }
            else if (requestNode.asText().equals("networkDisableJoin"))
            {
                request = new NetworkDisableJoinRequest(requestId);
            }
            else if (requestNode.asText().equals("setDeviceAddresses"))
            {
                request = new SetDeviceAddressesRequest(requestId);
            }
            else if (requestNode.asText().equals("setAutoApsAckedAddresses"))
            {
                request = new SetAutoApsAckedAddressesRequest(requestId);
            }
            else if (requestNode.asText().equals("setDevices"))
            {
                request = new SetDevicesRequest(requestId);
            }
            else if (requestNode.asText().equals("getEndpointIds"))
            {
                String eui64 = node.get("address").asText();
                request = new GetEndpointIdsRequest(requestId, eui64);
            }
            else if (requestNode.asText().equals("getClustersInfo"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                request = new GetClustersInfoRequest(requestId, eui64, endpointId);
            }
            else if (requestNode.asText().equals("attributesRead"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                int clusterId = node.get("clusterId").asInt();
                boolean server = node.get("clientToServer").asBoolean();
                boolean isMfgSpecific = node.get("isMfgSpecific").asBoolean();
                Integer mfgId = null;
                if (isMfgSpecific)
                {
                    mfgId = node.get("mfgId").asInt();
                }

                JsonNode infos = node.get("infos");

                int[] attributeIds = new int[infos.size()];
                for (int i = 0; i < infos.size(); i++)
                {
                    JsonNode info = infos.get(i);
                    attributeIds[i] = info.get("id").asInt();
                }

                request = new AttributesReadRequest(requestId, eui64, endpointId, clusterId, server, attributeIds, mfgId);
            }
            else if (requestNode.asText().equals("attributesWrite"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                int clusterId = node.get("clusterId").asInt();
                boolean server = node.get("clientToServer").asBoolean();
                boolean isMfgSpecific = node.get("isMfgSpecific").asBoolean();
                Integer mfgId = null;
                if (isMfgSpecific)
                {
                    mfgId = node.get("mfgId").asInt();
                }

                JsonNode attributes = node.get("attributes");
                AttributeData[] attrWrites = new AttributeData[attributes.size()];
                for (int i = 0; i < attributes.size(); i++)
                {
                    JsonNode attrData = attributes.get(i);
                    attrWrites[i] = new AttributeData(attrData.get("id").asInt(),
                                                      attrData.get("type").asInt(),
                                                      attrData.get("data").asText(),
                                                      mfgId);
                }

                request = new AttributesWriteRequest(requestId, eui64, endpointId, clusterId, server, attrWrites);
            }
            else if (requestNode.asText().equals("getAttributeInfos"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                int clusterId = node.get("clusterId").asInt();
                boolean server = node.get("clientToServer").asBoolean();
                Integer mfgId = null;
                if (node.has("mfgId"))
                {
                    mfgId = node.get("mfgId").asInt();
                }

                request = new GetAttributeInfosRequest(requestId, eui64, endpointId, clusterId, server, mfgId);
            }
            else if (requestNode.asText().equals("bindingSet"))
            {
                request = new BindingSetRequest(requestId);
            }
            else if (requestNode.asText().equals("attributesSetReporting"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                int clusterId = node.get("clusterId").asInt();
                boolean mfgSpecific = node.get("isMfgSpecific").asBoolean();
                ObjectReader reader = mapper.readerFor(new TypeReference<List<AttributeReportingConfig>>(){});
                List<AttributeReportingConfig> configs = reader.readValue(node.get("configs"));

                request = new AttributesSetReportingRequest(requestId, eui64, endpointId, clusterId, mfgSpecific, configs);
            }
            else if (requestNode.asText().equals("sendCommand"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                int clusterId = node.get("clusterId").asInt();
                String direction = node.get("direction").asText();
                int isMfgSpecific = node.get("isMfgSpecific").asInt();
                int mfgId = node.get("mfgId").asInt();
                int commandId = node.get("commandId").asInt();
                String encodedMessage = node.get("encodedMessage").asText();

                request = new SendCommandRequest(requestId,
                                                 eui64,
                                                 endpointId,
                                                 clusterId,
                                                 ("clientToServer".equals(direction)),
                                                 (isMfgSpecific == 1),
                                                 mfgId,
                                                 commandId,
                                                 encodedMessage);
            }
            else if (requestNode.asText().equals("sendViaApsAck"))
            {
                String eui64 = node.get("address").asText();
                int endpointId = node.get("endpointId").asInt();
                int clusterId = node.get("clusterId").asInt();
                int sequenceNum = node.get("sequenceNum").asInt();
                String encodedMessage = node.get("encodedMessage").asText();

                // Convert it into a send command request
                byte[] msg = Base64.decodeBase64(encodedMessage);
                int mfgId = ((msg[1] << 8) + msg[2]) & 0xFFFF;
                int commandId = msg[4];
                byte[] payload = new byte[msg.length - 5];
                for(int i = 5; i < msg.length; ++i)
                {
                    payload[i-5] = msg[i];
                }

                request = new SendViaApsAck(requestId,
                                            eui64,
                                            endpointId,
                                            clusterId,
                                            sequenceNum,
                                            mfgId,
                                            commandId,
                                            Base64.encodeBase64String(payload));
            }
            else if (requestNode.asText().equals("getSystemStatus"))
            {
                request = new GetSystemStatusRequest(requestId);
            }
            else if (requestNode.asText().equals("networkChange"))
            {
                byte channel = (byte)node.get("channel").asInt();
                short panId = (short)node.get("panId").asInt();
                String networkKey = node.get("netKey").asText();
                request = new NetworkChangeRequest(requestId, channel, panId, networkKey);
            }
            else if (requestNode.asText().equals("deviceIsChild"))
            {
                String eui64 = node.get("address").asText();
                request = new DeviceIsChildRequest(requestId, eui64);
            }
            else if (requestNode.asText().equals("getSourceRoute"))
            {
                String eui64 = node.get("address").asText();
                request = new GetSourceRouteRequest(requestId, eui64);
            }
            else if (requestNode.asText().equals("getLqiTable"))
            {
                String eui64 = node.get("address").asText();
                request = new GetLqiTableRequest(requestId, eui64);
            }
            else if (requestNode.asText().equals("refreshOtaFiles"))
            {
                request = new RefreshOtaFilesRequest(requestId);
            }
            else if (requestNode.asText().equals("addDeviceAddress"))
            {
                String eui64 = node.get("address").asText();
                request = new AddDeviceAddressRequest(requestId, eui64);
            }
            else if (requestNode.asText().equals("removeDeviceAddress"))
            {
                String eui64 = node.get("address").asText();
                request = new RemoveDeviceAddressRequest(requestId, eui64);
            }
            else if (requestNode.asText().equals("upgradeDeviceFirmwareLegacy"))
            {
                String eui64 = node.get("address").asText();
                String routerAddress = node.get("routerAddress").asText();
                String appFilename = node.get("appFilename").asText();
                int authChallengeResponse = node.get("authChallengeResponse").asInt();
                String bootloaderFilename = null;
                JsonNode bootloaderFilenameNode = node.get("bootloaderFilename");
                if(bootloaderFilenameNode != null)
                {
                    bootloaderFilename = bootloaderFilenameNode.asText();
                }
                request = new UpgradeDeviceFirmwareLegacyRequest(requestId, eui64, routerAddress, appFilename, bootloaderFilename, authChallengeResponse);
            }
            else if (requestNode.asText().equals("requestLeave"))
            {
                String eui64 = node.get("address").asText();

                boolean withRejoin = false;
                if (node.has("withRejoin"))
                {
                    withRejoin = node.get("withRejoin").asBoolean();
                }

                boolean isEndDevice = false;
                if (node.has("isEndDevice"))
                {
                    isEndDevice = node.get("isEndDevice").asBoolean();
                }

                request = new DeviceLeaveRequest(requestId, eui64, withRejoin, isEndDevice);
            }
            else if (requestNode.asText().equals("networkHealthCheckConfigure"))
            {
                int intervalMillis = node.get("intervalMillis").asInt();
                int ccaThreshold = node.get("ccaThreshold").asInt();
                int ccaFailureThreshold = node.get("ccaFailureThreshold").asInt();
                int restoreThreshold = node.get("restoreThreshold").asInt();
                int delayBetweenThresholdRetriesMillis = node.get("delayBetweenThresholdRetriesMillis").asInt();

                request = new NetworkHealthCheckConfigure(requestId, intervalMillis, ccaThreshold, ccaFailureThreshold, restoreThreshold, delayBetweenThresholdRetriesMillis);
            }
            else if (requestNode.asText().equals("setCommunicationFailTimeout"))
            {
                request = new SetCommunicationFailTimeoutRequest(requestId);
            }
            else if (requestNode.asText().equals("defenderConfigure"))
            {
                int panIdChangeThreshold = node.get("panIdChangeThreshold").asInt();
                int panIdChangeWindowMillis = node.get("panIdChangeWindowMillis").asInt();
                int panIdChangeRestoreMillis = node.get("panIdChangeRestoreMillis").asInt();
                request = new DefenderConfigureRequest(requestId, panIdChangeThreshold, panIdChangeWindowMillis, panIdChangeRestoreMillis);
            }
            else
            {
                log.error("Unsupported request: " + node);
            }
        } catch (IOException e)
        {
            e.printStackTrace();
        }

        return request;
    }
}
