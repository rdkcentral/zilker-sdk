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

/*
 * Created by Thomas Lea on 9/19/16.
 */

#include <icLog/logging.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <cjson/cJSON.h>
#include <icUtil/base64.h>
#include <zhal/zhal.h>
#include "zhalPrivate.h"

static void handleClusterCommandReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *sourceEndpointJson = cJSON_GetObjectItem(event, "sourceEndpoint");
    cJSON *profileIdJson = cJSON_GetObjectItem(event, "profileId");
    cJSON *directionJson = cJSON_GetObjectItem(event, "direction");
    cJSON *clusterIdJson = cJSON_GetObjectItem(event, "clusterId");
    cJSON *commandIdJson = cJSON_GetObjectItem(event, "commandId");
    cJSON *mfgSpecificJson = cJSON_GetObjectItem(event, "mfgSpecific");
    cJSON *mfgCodeJson = cJSON_GetObjectItem(event, "mfgCode");
    cJSON *seqNumJson = cJSON_GetObjectItem(event, "seqNum");
    cJSON *apsSeqNumJson = cJSON_GetObjectItem(event, "apsSeqNum");
    cJSON *rssiJson = cJSON_GetObjectItem(event, "rssi");
    cJSON *lqiJson = cJSON_GetObjectItem(event, "lqi");
    cJSON *encodedBufJson = cJSON_GetObjectItem(event, "encodedBuf");

    if(eui64Json == NULL ||
       sourceEndpointJson == NULL ||
       profileIdJson == NULL ||
       directionJson == NULL ||
       clusterIdJson == NULL ||
       commandIdJson == NULL ||
       mfgSpecificJson == NULL ||
       mfgCodeJson == NULL ||
       seqNumJson == NULL ||
       apsSeqNumJson == NULL ||
       rssiJson == NULL ||
       lqiJson == NULL ||
       encodedBufJson == NULL)
    {
        icLogError(LOG_TAG, "handleClusterCommandReceived: received incomplete event JSON");
    }
    else
    {
        ReceivedClusterCommand* command = (ReceivedClusterCommand*)calloc(1, sizeof(ReceivedClusterCommand));

        command->sourceEndpoint = (uint8_t) sourceEndpointJson->valueint;
        command->profileId = (uint16_t) profileIdJson->valueint;
        command->fromServer = directionJson->valueint == 1 ? true : false;
        command->clusterId = (uint16_t) clusterIdJson->valueint;
        command->commandId = (uint8_t) commandIdJson->valueint;
        command->mfgSpecific = (uint8_t) mfgSpecificJson->valueint;
        command->mfgCode = (uint16_t) mfgCodeJson->valueint;
        command->seqNum = (uint8_t) seqNumJson->valueint;
        command->apsSeqNum = (uint8_t) apsSeqNumJson->valueint;
        command->rssi = (int8_t) rssiJson->valueint;
        command->lqi = (uint8_t) lqiJson->valueint;

        sscanf(eui64Json->valuestring, "%016" PRIx64, &command->eui64);

        if(icDecodeBase64(encodedBufJson->valuestring, &command->commandData, &command->commandDataLen))
        {
            getCallbacks()->clusterCommandReceived(getCallbackContext(), command);
        }

        freeReceivedClusterCommand(command);
    }
}

static void handleAttributeReportReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *sourceEndpointJson = cJSON_GetObjectItem(event, "sourceEndpoint");
    cJSON *clusterIdJson = cJSON_GetObjectItem(event, "clusterId");
    cJSON *encodedBufJson = cJSON_GetObjectItem(event, "encodedBuf");
    cJSON *rssiJson = cJSON_GetObjectItem(event, "rssi");
    cJSON *lqiJson = cJSON_GetObjectItem(event, "lqi");
    cJSON *mfgCodeJson = cJSON_GetObjectItem(event, "mfgCode");

    if(eui64Json == NULL ||
       sourceEndpointJson == NULL ||
       clusterIdJson == NULL ||
       encodedBufJson == NULL ||
       rssiJson == NULL ||
       lqiJson == NULL)
    {
        icLogError(LOG_TAG, "handleAttributeReportReceived: received incomplete event JSON");
    }
    else
    {
        ReceivedAttributeReport* report = (ReceivedAttributeReport*)calloc(1, sizeof(ReceivedAttributeReport));

        report->eui64 = 0;
        report->sourceEndpoint = (uint8_t) sourceEndpointJson->valueint;
        report->clusterId = (uint16_t) clusterIdJson->valueint;
        report->rssi = (int8_t) rssiJson->valueint;
        report->lqi = (uint8_t) lqiJson->valueint;

        sscanf(eui64Json->valuestring, "%016" PRIx64, &report->eui64);

        if (mfgCodeJson != NULL)
        {
            report->mfgId = (uint16_t) mfgCodeJson->valueint;
        }

        if (icDecodeBase64(encodedBufJson->valuestring, &report->reportData, &report->reportDataLen))
        {
            getCallbacks()->attributeReportReceived(getCallbackContext(), report);
        }

        freeReceivedAttributeReport(report);
    }
}

static void handleFirmwareVersionNotifyEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *currentVersionJson = cJSON_GetObjectItem(event, "currentVersion");

    if(eui64Json == NULL ||
            currentVersionJson == NULL)
    {
        icLogError(LOG_TAG, "handleFirmwareVersionNotifyEventReceived: received incomplete event JSON");
    }
    else
    {
        uint64_t eui64 = 0;
        uint32_t currentVersion = (uint32_t) currentVersionJson->valueint;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceFirmwareVersionNotifyEventReceived(getCallbackContext(), eui64, currentVersion);
    }
}

static void handleFirmwareUpgradingEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if(eui64Json == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __FUNCTION__);
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceFirmwareUpgradingEventReceived(getCallbackContext(), eui64);
    }
}

static void handleFirmwareUpgradeCompletedEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if(eui64Json == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __FUNCTION__);
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceFirmwareUpgradeCompletedEventReceived(getCallbackContext(), eui64);
    }
}

static void handleFirmwareUpgradeFailedEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if(eui64Json == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __FUNCTION__);
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceFirmwareUpgradeFailedEventReceived(getCallbackContext(), eui64);
    }
}

static void handleDeviceCommunicationSucceededEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if(eui64Json == NULL)
    {
        icLogError(LOG_TAG, "handleDeviceCommunicationSucceededEventReceived: received incomplete event JSON");
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceCommunicationSucceeded(getCallbackContext(), eui64);
    }
}

static void handleDeviceCommunicationFailedEventRecieved(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");

    if (eui64Json == NULL)
    {
        icLogError(LOG_TAG, "%s: received incomplete event JSON", __FUNCTION__);
    }
    else
    {
        uint64_t eui64 = 0;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        getCallbacks()->deviceCommunicationFailed(getCallbackContext(), eui64);
    }
}

static void handleDeviceAnnounced(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *deviceTypeJson = cJSON_GetObjectItem(event, "deviceType");
    cJSON *powerSourceJson = cJSON_GetObjectItem(event, "powerSource");
    if(eui64Json != NULL && deviceTypeJson != NULL && powerSourceJson != NULL)
    {
        uint64_t eui64;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        zhalDeviceType deviceType = deviceTypeUnknown;
        if(strcmp(deviceTypeJson->valuestring, "endDevice") == 0)
        {
            deviceType = deviceTypeEndDevice;
        }
        else if(strcmp(deviceTypeJson->valuestring, "router") == 0)
        {
            deviceType = deviceTypeRouter;
        }

        zhalPowerSource powerSource = powerSourceUnknown;
        if(strcmp(powerSourceJson->valuestring, "mains") == 0)
        {
            powerSource = powerSourceMains;
        }
        else if(strcmp(powerSourceJson->valuestring, "battery") == 0)
        {
            powerSource = powerSourceBattery;
        }

        getCallbacks()->deviceAnnounced(getCallbackContext(), eui64, deviceType, powerSource);
    }
}

static void handleDeviceRejoinedEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *isSecureJson = cJSON_GetObjectItem(event, "isSecure");
    if (eui64Json != NULL && isSecureJson != NULL)
    {
        uint64_t eui64;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        bool isSecure = false;
        if (cJSON_IsBool(isSecureJson))
        {
            isSecure = (bool) cJSON_IsTrue(isSecureJson);
        }

        getCallbacks()->deviceRejoined(getCallbackContext(), eui64, isSecure);
    }
}

static void handleLinkKeyUpdatedEventReceived(cJSON *event)
{
    cJSON *eui64Json = cJSON_GetObjectItem(event, "eui64");
    cJSON *isUsingHashBasedKeyJson = cJSON_GetObjectItem(event, "isUsingHashBasedKey");
    if (eui64Json != NULL)
    {
        uint64_t eui64;
        sscanf(eui64Json->valuestring, "%016" PRIx64, &eui64);

        bool isUsingHashBasedKey = false;
        if (isUsingHashBasedKeyJson != NULL && cJSON_IsBool(isUsingHashBasedKeyJson) == true)
        {
            isUsingHashBasedKey = (bool) cJSON_IsTrue(isUsingHashBasedKeyJson);
        }

        getCallbacks()->linkKeyUpdated(getCallbackContext(), eui64, isUsingHashBasedKey);
    }
}

static void handleNetworkHealthProblemEventReceived(cJSON *event)
{
    getCallbacks()->networkHealthProblem(getCallbackContext());
}

static void handleNetworkHealthProblemRestoredEventReceived(cJSON *event)
{
    getCallbacks()->networkHealthProblemRestored(getCallbackContext());
}

static void handlePanIdAttackEventReceived(cJSON *event)
{
    getCallbacks()->panIdAttackDetected(getCallbackContext());
}

static void handlePanIdAttackClearedEventReceived(cJSON *event)
{
    getCallbacks()->panIdAttackCleared(getCallbackContext());
}

int zhalHandleEvent(cJSON *event)
{
    void* callbackContext = getCallbackContext();
    char *eventString = cJSON_PrintUnformatted(event);
    icLogDebug(LOG_TAG, "got event: %s", eventString);
    free(eventString);

    cJSON *eventType = cJSON_GetObjectItem(event, "eventType");
    if(eventType == NULL)
    {
        icLogError(LOG_TAG, "Invalid event received (missing eventType)");
        return 0;
    }

    if(strcmp(eventType->valuestring, "zhalStartup") == 0 && getCallbacks()->startup != NULL)
    {
        getCallbacks()->startup(callbackContext);
    }
    else if(strcmp(eventType->valuestring, "networkConfigChanged") == 0 && getCallbacks()->networkConfigChanged != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(event, "networkConfigData");
        if(data != NULL)
        {
            getCallbacks()->networkConfigChanged(callbackContext, data->valuestring);
        }
    }
    else if(strcmp(eventType->valuestring, "deviceAnnounced") == 0 && getCallbacks()->deviceAnnounced != NULL)
    {
        handleDeviceAnnounced(event);
    }
    else if(strcmp(eventType->valuestring, "deviceJoined") == 0 && getCallbacks()->deviceJoined != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(event, "eui64");
        if(data != NULL)
        {
            uint64_t eui64;
            sscanf(data->valuestring, "%016" PRIx64, &eui64);
            getCallbacks()->deviceJoined(callbackContext, eui64);
        }
    }
    else if(strcmp(eventType->valuestring, "deviceRejoined") == 0 && getCallbacks()->deviceRejoined != NULL)
    {
        handleDeviceRejoinedEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "linkKeyUpdated") == 0 && getCallbacks()->linkKeyUpdated != NULL)
    {
        handleLinkKeyUpdatedEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "apsAckFailure") == 0 && getCallbacks()->apsAckFailure != NULL)
    {
        cJSON *data = cJSON_GetObjectItem(event, "eui64");
        if(data != NULL)
        {
            uint64_t eui64;
            sscanf(data->valuestring, "%016" PRIx64, &eui64);
            getCallbacks()->apsAckFailure(callbackContext, eui64);
        }
    }
    else if(strcmp(eventType->valuestring, "clusterCommandReceived") == 0 && getCallbacks()->clusterCommandReceived != NULL)
    {
        handleClusterCommandReceived(event);
    }
    else if(strcmp(eventType->valuestring, "attributeReport") == 0 && getCallbacks()->attributeReportReceived != NULL)
    {
        handleAttributeReportReceived(event);
    }
    else if(strcmp(eventType->valuestring, "deviceFirmwareVersionNotifyEvent") == 0 && getCallbacks()->deviceFirmwareVersionNotifyEventReceived != NULL)
    {
        handleFirmwareVersionNotifyEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "deviceFirmwareUpgradingEvent") == 0 && getCallbacks()->deviceFirmwareUpgradingEventReceived != NULL)
    {
        handleFirmwareUpgradingEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "deviceFirmwareUpgradeCompletedEvent") == 0 && getCallbacks()->deviceFirmwareUpgradeCompletedEventReceived != NULL)
    {
        handleFirmwareUpgradeCompletedEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "deviceFirmwareUpgradeFailedEvent") == 0 && getCallbacks()->deviceFirmwareUpgradeFailedEventReceived != NULL)
    {
        handleFirmwareUpgradeFailedEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "deviceCommunicationSucceededEvent") == 0 && getCallbacks()->deviceCommunicationSucceeded != NULL)
    {
        handleDeviceCommunicationSucceededEventReceived(event);
    }
    else if (strcmp(eventType->valuestring, "deviceCommunicationFailedEvent") == 0 && getCallbacks()->deviceCommunicationFailed != NULL)
    {
        handleDeviceCommunicationFailedEventRecieved(event);
    }
    else if(strcmp(eventType->valuestring, "networkHealthProblem") == 0 && getCallbacks()->networkHealthProblem != NULL)
    {
        handleNetworkHealthProblemEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "networkHealthProblemRestored") == 0 && getCallbacks()->networkHealthProblemRestored != NULL)
    {
        handleNetworkHealthProblemRestoredEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "panIdAttack") == 0 && getCallbacks()->panIdAttackDetected != NULL)
    {
        handlePanIdAttackEventReceived(event);
    }
    else if(strcmp(eventType->valuestring, "panIdAttackCleared") == 0 && getCallbacks()->panIdAttackCleared != NULL)
    {
        handlePanIdAttackClearedEventReceived(event);
    }

    // Cleanup since we are saying we handled it
    cJSON_Delete(event);

    return 1; //we handled it
}