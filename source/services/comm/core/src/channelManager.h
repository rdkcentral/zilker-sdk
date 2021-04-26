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

/*-----------------------------------------------
 * channelManager.h
 *
 * centralized point-of-contact for communicating with servers.
 * contains 1-or-more "channel" objects that know how to speak
 * a specific protocol to a particular server.
 *
 * The picture below shows the flow (goes from left to right)
 * of how internal requests & events are processed and eventually
 * make their way to a hypothetical channel:
 *
 *                              +---------+   +------------+      +----------+
 *                              | Channel |___| TCP / UDP  |<---->|          |
 * +----------+                 | One     |   | Connection |      |          |
 * | Requests |                /+---------+   +------------+      |          |
 * +----------+\   +---------+/ +---------+   +------------+      |          |
 *              \__| Channel |__| Channel |___| HTTPS      |<---->| Cloud    |
 *              /  | Manager |  | Two     |   | Connection |      | Servers  |
 *   +--------+/   +---------+\ +---------+   +------------+      |          |
 *   | Events |                \+---------+   +------------+      |          |
 *   +--------+                 | Channel |___| MQTT       |<---->|          |
 *                              | Three   |   | Connection |      |          |
 *                              +---------+   +------------+      +----------+
 *
 * Author: jelderton - 9/10/15
 *-----------------------------------------------*/

#ifndef ZILKER_CHANNELMANAGER_H
#define ZILKER_CHANNELMANAGER_H

#include <stdint.h>

#include <icIpc/ipcStockMessagesPojo.h>
#include <commMgr/commService_pojo.h>
#include "channel.h"
#include "message.h"

/*
 * cruddy, but assign positive identifiers to each of
 * the supported channels to ensure uniqueness.
 * NOTE: none of these are using 0 (on purpose)
 */
#define SAMPLE_CHANNEL_ID   1
#define ANOTHER_CHANNEL_ID  2


/*
 * one-time initialization that is generally called during startup
 */
void initChannelManager();

/*
 * called during shutdown
 */
void shutdownChannelManager();

/*
 * return true if we are within the startup window
 */
bool inChannelStartupWindow();

/*
 * return the channel with this unique identifier.
 * can be NULL if the channel is not supported or created
 */
channel *getChannelById(uint8_t channelId);

/*
 * returns if any channel is connected to the server.
 * primarily used by "GET_ONLINE_STATUS" IPC handler
 */
bool channelIsAnythingOnline();

/*
 * populates the 'output' list with commChannelStatus objects
 * that depict the detailed online status of each known channel.
 * primarily used by "GET_ONLINE_DETAILED_STATUS" IPC handler
 */
void channelGetOnlineDetailedStatus(commChannelStatusList *output);

/*
 * create and broadcast the commOnlineChangedEvent.  it will collect
 * status from all known channels, then send the event with those details
 */
void channelSendCommOnlineChangedEvent();

/*
 * obtain current status, and shove into the commStatusPojo
 * for external processes to gather details about our state.
 * supports the "GET_SERVICE_STATUS" IPC call to commService.
 */
void getChannelRuntimeStatusIPC(serviceStatusPojo *output);

/*
 * collect statistics about the messages to/from the server,
 * and populate them into the supplied runtimeStatsPojo container.
 * supports the "GET_RUNTIME_STATS" IPC call to commService.
 */
void collectChannelMessageStatisticsIPC(runtimeStatsPojo *container, bool thenClear);

/*
 * pass along "configuration restored" notification to each
 * of the allocated channels (regardless of enabled state).
 * supports the "CONFIG_RESTORED" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelConfigurationRestoredIPC(configRestoredInput *input);

/*
 * pass along "configuration reset" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "RESET_COMM_SETTINGS_TO_DEFAULT" IPC call to commService.
 */
void channelConfigurationResetToDefaultsIPC();

/*
 * pass along "get sunrise/sunset" request to each enabled channel.
 * supports the "GET_SUNRISE_SUNSET_TIME" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelGetSunriseSunsetTimeIPC(sunriseSunsetTimes *output);

/*
 * pass along "send message to subscriber" request to each enabled channel.
 * supports the "SEND_MESSAGE_TO_SUBSCRIBER" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelSendMessageToSubscriberIPC(ruleSendMessage *input);

/*
 * pass along "get cloud association state" request to each enabled channel.
 * supports the "GET_CLOUD_ASSOCIATION_STATE" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 *
 * TODO: look into changing up how this the state is gathered when implementing iotCore
 */
bool channelGetCloudAssociationStateIPC(cloudAssociationValue *output);

/*
 * pass along "manually start cloud association" request to each enabled channel.
 * supports the "INITIATE_MANUAL_CLOUD_ASSOCIATION" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelStartManualCloudAssociationIPC(cloudAssociationParms *input, cloudAssociationResponse *output);

/*
 * pass along "get hostname configuration" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "GET_HOSTNAME_CONFIG_LIST" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelGetHostnameConfigurationListIPC(commHostConfigList *output);

/*
 * pass along "set hostname configuration" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "SET_HOSTNAME_CONFIG_LIST" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelSetHostnameConfigurationListIPC(commHostConfigList *input);

//TODO: Expand the output to a full report?
typedef struct {
    char *channelId;
    bool succeeded;
} channelTestResult;

/*
 * Destructor for channelTestResult types
 * @param testResult
 */
void destroyChannelTestResult(channelTestResult *testResult);

/*
 * Requests each channel to perform a connection test and return the results in the form of a list of
 * channelTestResults. Caller is responsible for freeing the list and the channelTestResults inside.
 *
 * @see destroyChannelTestResult
 */
icLinkedList *channelPerformConnectionTests(bool useCell, bool primaryOnly);

#endif // ZILKER_CHANNELMANAGER_H

