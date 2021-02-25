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
 * channel.h
 *
 * define the common definition and functions that each 'channel'
 * implementation needs to implement.  helps to keep the various
 * instances separated but maintain similar signatures.
 *
 * channel objects are owned by the channelManager, and
 * can be obtained via channelManager->getChannelById().
 *
 * Author: jelderton - 9/10/15
 *-----------------------------------------------*/

#ifndef ZILKER_CHANNEL_H
#define ZILKER_CHANNEL_H

#include <stdbool.h>
#include <icIpc/ipcStockMessagesPojo.h>
#include <commMgr/commService_pojo.h>
#include "message.h"

/*
 * channel overall state
 */
typedef enum
{
    CHANNEL_STATE_DOWN,
    CHANNEL_STATE_CONNECTING,       // starting connection sequence
    CHANNEL_STATE_CONNECTED,        // connected, but not established
    CHANNEL_STATE_ESTABLISHING,     // establishing the connection with server
    CHANNEL_STATE_COMPLETE,         // complete, ready for use
} channelState;

/*
 * labels for channel overall state values.  can use the
 * channelOverallState value as the index into the array
 */
static const char *channelStateLabels[] = {
    "DOWN",         // CHANNEL_STATE_DOWN
    "CONNECTING",   // CHANNEL_STATE_CONNECTING
    "CONNECTED",    // CHANNEL_STATE_CONNECTED
    "ESTABLISHING", // CHANNEL_STATE_ESTABLISHING
    "COMPLETE",     // CHANNEL_STATE_COMPLETE
    NULL
};

/*
 * channel connection state (different from overall state)
 */
typedef enum
{
    CHANNEL_CONNECT_INTERNAL_ERROR, // init or mem problem
    CHANNEL_CONNECT_IO_ERROR,       // network problem
    CHANNEL_CONNECT_AUTH_ERROR,     // login failure
    CHANNEL_CONNECT_SUCCESS         // success
} channelConnectionState;

/*
 * labels for channelConnectionState values.  can use the
 * channelConnectionState value as the index into the array
 */
static const char *channelConnectionStateLabels[] = {
    "INIT_ERROR",   // CHANNEL_CONNECT_INTERNAL_ERROR
    "IO_ERROR",     // CHANNEL_CONNECT_IO_ERROR
    "AUTH_ERROR",   // CHANNEL_CONNECT_AUTH_ERROR
    "SUCCESS",      // CHANNEL_CONNECT_SUCCESS
    NULL
};

/*
 * function prototype to shutdown a channel (different from a disconnect).
 * each channel should perform memory cleanup, stop all threads, etc as this
 * is called during service shutdown.
 *
 * due to the nature of the call, this will block until the channel has completed
 * the shutdown procedure and cleaned it's memory.
 */
typedef void (*channelShutdownFunc)();

/*
 * function prototype to ask a channel if it's enabled
 */
typedef bool (*channelIsEnabledFunc)();

/*
 * function prototype to tell a channel if it's enabled
 */
typedef void (*channelSetEnabledFunc)(bool enabled);

/*
 * function prototype to get the current 'overall' state
 */
typedef channelState (*channelGetStateFunc)();

/*
 * function prototype to get the current 'connection' state (or failed reason)
 */
typedef channelConnectionState (*channelGetConnectStateFunc)();

/*
 * function prototype to ask a channel to "connect to the server"
 * if supported and the useCell is true, the connection should
 * be made over the cellular network interface.
 *
 * this call will most likely be backgrounded.  caller can use the
 * channelState or channelConnectState to determine
 */
typedef void (*channelConnectFunc)(bool useCell);

/*
 * function prototype to ask a channel to "disconnect from the server"
 *
 * this call will most likely be backgrounded
 */
typedef void (*channelDisconnectFunc)();

/*
 * function prototype to ask the channel to make a server request
 */
typedef bool (*channelRequestFunc)(message *msg);

/*
 * collect status about the channel, and populate them into the supplied container
 */
typedef void (*channelGetStatusDetailsFunc)(commChannelStatus *output);

/*
 * collect runtime status about the channel, and populate them into the supplied container
 */
typedef void (*channelGetRuntimeStatusFunc)(serviceStatusPojo *output);

/*
 * collect statistics about the channel, and populate them into the supplied container
 */
typedef void (*channelGetRuntimeStatisticsFunc)(runtimeStatsPojo *container, bool thenClear);

/*
 * pass along "configuration restored" notification to each
 * of the allocated channels (regardless of enabled state).
 * supports the "CONFIG_RESTORED" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelConfigurationRestoredFunc)(configRestoredInput *input);

/*
 * pass along "configuration reset" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "RESET_COMM_SETTINGS_TO_DEFAULT" IPC call to commService.
 */
typedef void (*channelConfigurationResetToDefaultsFunc)();

/*
 * pass along "get sunrise/sunset" request to each enabled channel.
 * supports the "GET_SUNRISE_SUNSET_TIME" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelGetSunriseSunsetTimeFunc)(sunriseSunsetTimes *output);

/*
 * pass along "send message to subscriber" request to each enabled channel.
 * supports the "SEND_MESSAGE_TO_SUBSCRIBER" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelSendMessageToSubscriberFunc)(ruleSendMessage *input);

/*
 * pass along "get cloud association state" request to each enabled channel.
 * supports the "GET_CLOUD_ASSOCIATION_STATE" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelGetCloudAssociationStateFunc)(cloudAssociationValue *input);

/*
 * pass along "manually start cloud association" request to each enabled channel.
 * supports the "INITIATE_MANUAL_CLOUD_ASSOCIATION" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelStartManualCloudAssociationFunc)(cloudAssociationParms *input, cloudAssociationResponse *output);

/*
 * pass along "get hostname configuration" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "GET_HOSTNAME_CONFIG_LIST" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelGetHostnameConfigurationListFunc)(commHostConfigList *output);

/*
 * pass along "set hostname configuration" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "SET_HOSTNAME_CONFIG_LIST" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
typedef bool (*channelSetHostnameConfigurationListFunc)(commHostConfigList *input);

/*
 * Perform a connection test for the channel.
 */
typedef bool (*channelPerformConnectionTestFunc)(bool useCell);


/*
 * basic object representation of a single 'channel of communication'
 * that can be used to send messages and receive requests/replies.
 * each instance has a set of 'characteristics' that describe the
 * set of formats and network a channel operates in.
 */
typedef struct _channel
{
    uint8_t      id;                // internal identifier

    // main functions (some are optional and can be NULL)
    channelGetStateFunc             getStateFunc;
    channelGetConnectStateFunc      getConnectStateFunc;
    channelIsEnabledFunc            isEnabledFunc;
    channelSetEnabledFunc           setEnabledFunc;
    channelConnectFunc              connectFunc;
    channelDisconnectFunc           disconnectFunc;
    channelShutdownFunc             shutdownFunc;
    channelRequestFunc              requestFunc;              // optional
    channelGetStatusDetailsFunc     getStatusDetailsFunc;     // required for PRIMARY channel, optional for rest
    channelGetRuntimeStatusFunc     getRuntimeStatusFunc;     // optional
    channelGetRuntimeStatisticsFunc getRuntimeStatisticsFunc; // optional

    // IPC functions (all are optional and can be NULL)
    channelConfigurationRestoredFunc        configRestoredIpcFunc;
    channelConfigurationResetToDefaultsFunc configResetToDefaultsIpcFunc;
    channelGetSunriseSunsetTimeFunc         getSunriseSunsetTimeIpcFunc;
    channelSendMessageToSubscriberFunc      sendMessageToSubscriberIpcFunc;
    channelGetCloudAssociationStateFunc     getCloudAssociationStateIpcFunc;
    channelStartManualCloudAssociationFunc  startManualCloudAssociationIpcFunc;
    channelGetHostnameConfigurationListFunc getHostnameConfigurationListIpcFunc;
    channelSetHostnameConfigurationListFunc setHostnameConfigurationListIpcFunc;
    channelPerformConnectionTestFunc        performConnectionTestFunc;
} channel;

#endif // ZILKER_CHANNEL_H
