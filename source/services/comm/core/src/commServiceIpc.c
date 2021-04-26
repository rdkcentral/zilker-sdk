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
 * commServiceIpc.c
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton - 7/6/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icBuildtime.h>
#include <icLog/logging.h>                      // logging subsystem
#include <icIpc/ipcMessage.h>
#include <icIpc/eventConsumer.h>
#include <icIpc/ipcReceiver.h>
#include <icSystem/softwareCapabilities.h>
#include <icTime/timeUtils.h>
#include <commMgr/commService_pojo.h>
#include <watchdog/serviceStatsHelper.h>
#include "commService_ipc_handler.h"            // local IPC handler
#include "channelManager.h"
#include "commServiceCommon.h"                  // common set of defines for CommService

/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_commService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    collectEventStatistics(output, input);
    collectIpcStatistics(get_commService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

    // now stats about the channels that are enabled
    collectChannelMessageStatisticsIPC(output, input);

    output->serviceName = strdup(COMM_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_commService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
{
    // get status from all enabled channels
    //
    getChannelRuntimeStatusIPC(output);
    return IPC_SUCCESS;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   details - both the 'temp dir' the config was extracted to, and the 'target dir' of where to store
 */
IPCCode handle_commService_CONFIG_RESTORED_request(configRestoredInput *input, configRestoredOutput* output)
{
    // have ChannelManger pass along to all channels
    //
    if (channelConfigurationRestoredIPC(input) == true)
    {
        /*
         * Restart until we can handle restoration without restarting.
         */
        output->action = CONFIG_RESTORED_RESTART;
    }
    else
    {
        icLogWarn(COMM_LOG, "error restoring configuration");
        output->action = CONFIG_RESTORED_FAILED;
    }

    return IPC_SUCCESS;
}

/**
 * Ask the server for Sunrise/Sunset times
 *   output - response from service
 **/
IPCCode handle_GET_SUNRISE_SUNSET_TIME_request(sunriseSunsetTimes *output)
{
    // ask ChannelManager to get the sunset/sunrise time
    //
    if (channelGetSunriseSunsetTimeIPC(output) == true)
    {
        return IPC_SUCCESS;
    }

    icLogWarn(COMM_LOG, "unable to process GET_SUNRISE_SUNSET_TIME");
    return IPC_GENERAL_ERROR;
}

/**
 * Forward a message to the server to send, either via e-mail or SMS. Primarily used by RulesEngine to deliver messages
 *   input - send subscriber a notification based on a rule executing
 **/
IPCCode handle_SEND_MESSAGE_TO_SUBSCRIBER_request(ruleSendMessage *input)
{
    // ask ChannelManager to send the message
    //
    if (channelSendMessageToSubscriberIPC(input) == true)
    {
        return IPC_SUCCESS;
    }
    icLogWarn(COMM_LOG, "unable to process SEND_MESSAGE_TO_SUBSCRIBER");
    return IPC_GENERAL_ERROR;
}

/**
 * Returns true if ANY channel is online (bband OR cell)
 *   output - response from service (boolean)
 **/
IPCCode handle_GET_ONLINE_STATUS_request(bool *output)
{
    *output = channelIsAnythingOnline();
    return IPC_SUCCESS;
}

/**
 * Returns list of channels and their detailed status
 *   output - list of commChannelStatus objects
 **/
IPCCode handle_GET_ONLINE_DETAILED_STATUS_request(commChannelStatusList *output)
{
    // get from channel manager
    //
    channelGetOnlineDetailedStatus(output);
    return IPC_SUCCESS;
}

/**
 * query current state of "cloud association" (old code called this ACTIVATION)
 *   output - object wrapper for the cloudAssociationState enumeration
 **/
IPCCode handle_GET_CLOUD_ASSOCIATION_STATE_request(cloudAssociationValue *output)
{
    // ask each channel if it has a cloud association state
    //
    if (channelGetCloudAssociationStateIPC(output) == false)
    {
        icLogWarn(COMM_LOG, "unable to process GET_CLOUD_ASSOCIATION_STATE; no channels are active?");
        output->cloudAssState = CLOUD_ACCOC_UNKNOWN;
    }

    return IPC_SUCCESS;
}

/**
 * only applicable when CONFIG_SERVICE_COMM_AUTO_ASSOCIATE option is no set.  input hash requires specific string variables to be defined.  this should be replaced with a different mechanism once AWS IoT is in place!
 *   input - container of input parameters when manually initiating 'cloud association'
 *   output - return object when manually initiating 'cloud association'
 **/
IPCCode handle_INITIATE_MANUAL_CLOUD_ASSOCIATION_request(cloudAssociationParms *input, cloudAssociationResponse *output)
{
    if (input == NULL || output == NULL)
    {
        icLogWarn(COMM_LOG, "unable to process INITIATE_MANUAL_CLOUD_ASSOCIATION; missing input/output objects");
        return IPC_INVALID_ERROR;
    }

#ifndef CONFIG_SERVICE_COMM_AUTO_ASSOCIATE

    // pass along to ChannelManager
    //
    if (channelStartManualCloudAssociationIPC(input, output) == true)
    {
        return IPC_SUCCESS;
    }

    icLogWarn(COMM_LOG, "error starting manual activation");
    return IPC_GENERAL_ERROR;

#else
    icLogWarn(COMM_LOG, "manual activation not supported.  ignoring request");
    return IPC_INVALID_ERROR;
#endif
}

/**
 * reset many of the settings to default values - for reset to factory situations
 **/
IPCCode handle_RESET_COMM_SETTINGS_TO_DEFAULT_request(bool *output)
{
    // forward to ChannelManager
    //
    channelConfigurationResetToDefaultsIPC();
    *output = true;
    return IPC_SUCCESS;
}

/**
 * return a list of commHostConfig objects; describing all exposed hostnames within commService
 *   output - list of commHostConfig objects
 **/
IPCCode handle_GET_HOSTNAME_CONFIG_LIST_request(commHostConfigList *output)
{
    // forward to ChannelManager
    //
    if (channelGetHostnameConfigurationListIPC(output) == true)
    {
        return IPC_SUCCESS;
    }
    return IPC_GENERAL_ERROR;
}

/**
 * update each commHostConfig objects within the list.  NOTE: has to be a defined object via GET_HOSTNAME_CONFIG_LIST
 *   input - list of commHostConfig objects
 **/
IPCCode handle_SET_HOSTNAME_CONFIG_LIST_request(commHostConfigList *input)
{
    // forward to ChannelManager
    //
    if (channelSetHostnameConfigurationListIPC(input) == true)
    {
        return IPC_SUCCESS;
    }
    return IPC_GENERAL_ERROR;
}

/**
 * Needs implementation as this is channel specific.
 * Uploads a set of images to the server
 *   input - iControl Server specific
 *   output - response from service (boolean)
 **/
IPCCode handle_UPLOAD_IMAGES_TO_SERVER_request(imageUploadMessage *input, bool *output)
{
    // TODO: implement this
    icLogWarn(COMM_LOG, "UPLOAD_IMAGES_TO_SERVER not implemented yet");
    return IPC_GENERAL_ERROR;
}

/**
 * Needs implementation as this is channel specific.
 * Tells server to associate a previous event containing media to a rule execution
 *   input - iControl Server specific
 **/
IPCCode handle_ASSOCIATE_MEDIA_TO_RULE_request(associateMediaToRule *input)
{
    // TODO: implement this
    icLogWarn(COMM_LOG, "ASSOCIATE_MEDIA_TO_RULE not implemented yet");
    return IPC_GENERAL_ERROR;
}

/**
 * Needs implementation as this is channel specific.
 * Initiate captures and upload of a set of pictures from a camera to the server
 *   input - combines several IPC requests into a single reqest to execute the sequence of taking a pic and uploading to the server
 **/
IPCCode handle_UPLOAD_PICTURES_FROM_CAMERA_request(uploadPicturesFromCamera *input)
{
    // TODO: implement this
    icLogWarn(COMM_LOG, "UPLOAD_PICS_FROM_CAMERA not implemented yet");
    return IPC_GENERAL_ERROR;
}

/**
 * Needs implementation as this is channel specific.
 * Initiate capture and upload of video plus a thumbnail from a camera to the server
 *   input - combines several IPC requests into a single reqest to execute the sequence of taking a video clip and uploading to the server
 **/
IPCCode handle_UPLOAD_VIDEO_FROM_CAMERA_request(uploadVideoFromCamera *input)
{
    // TODO: implement this
    icLogWarn(COMM_LOG, "UPLOAD_VIDEO_FROM_CAMERA not implemented yet");
    return IPC_GENERAL_ERROR;
}

/**
 * Needs implementation as this is channel specific.
 * Upload local files to a server.
 *   input - create a upload message which will upload files to the server
 *   output - response from service (boolean)
 **/
IPCCode handle_UPLOAD_FILES_TO_SERVER_request(uploadMessage *input,bool *output)
{
    // TODO: implement this
    icLogWarn(COMM_LOG, "UPLOAD_FILES_TO_SERVER not implemented yet");
    return IPC_GENERAL_ERROR;
}
/**
 * Performs a connectivity test on core services, then a set of those test results.
 *   input - options to configure for the connectivity test
 **/
IPCCode handle_PERFORM_CONNECTIVITY_TEST_request(connectivityTestOptions *input, connectivityTestResultList *output)
{
    output->testResultList = linkedListCreate();
    icLinkedList *testResults = channelPerformConnectionTests(input->useCell, input->primaryChannelOnly);

    // Translate results to IPC POJOs
    sbIcLinkedListIterator *iterator = linkedListIteratorCreate(testResults);
    while (linkedListIteratorHasNext(iterator) == true)
    {
        channelTestResult *result = (channelTestResult *) linkedListIteratorGetNext(iterator);
        connectivityTestResult *resultPojo = create_connectivityTestResult();
        // let the pojo own channelId memory now, so we only need to free the result type.
        resultPojo->channelId = result->channelId;
        result->channelId = NULL;
        resultPojo->result = result->succeeded;

        linkedListAppend(output->testResultList, resultPojo);
    }

    linkedListDestroy(testResults, (linkedListItemFreeFunc) destroyChannelTestResult);

    return IPC_SUCCESS;
}

/**
 * Needs implementation as this is channel specific.
 * Send a notification to the cloud that CPE setup has completed
 **/
IPCCode handle_NOTIFY_CPE_SETUP_COMPLETE_request()
{
    // TODO: implement this
    icLogWarn(COMM_LOG, "NOTIFY_CPE_SETUP_COMPLETE not implemented yet");
    return IPC_GENERAL_ERROR;
}

