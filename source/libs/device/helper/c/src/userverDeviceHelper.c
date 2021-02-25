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

//
// Created by mkoch201 on 5/16/19.
//

#include <commonDeviceDefs.h>
#include <deviceHelper.h>
#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <string.h>
#include "userverDeviceHelper.h"

#define LOG_TAG "userverDeviceHelper"

#define MAX_MAC_ADDRESS 281474976710656UL
#define ZIGBEE_HEX_EUI64_LENGTH 16
#define MAC_WITHOUT_COLONS_LENGTH 12

/**
 * Helper that searches for a camera whose migrated id matches the preZilkerCameraId
 * @param preZilkerCameraId the id to search for
 * @param cameraDeviceId output for the id of the device that matches.  Caller must free.
 * @return true if the id was found, false otherwise
 */
static bool getCameraDeviceIdForPreZilkerCameraDeviceId(char *preZilkerCameraId, char **cameraDeviceId)
{
    bool retVal = false;
    if (preZilkerCameraId != NULL)
    {
        char *uri = createEndpointMetadataUri("*","*", CAMERA_MIGRATED_ID_METADATA);
        DSMetadataUriMap *metadataUriMap = create_DSMetadataUriMap();
        IPCCode ipcRc;
        if ((ipcRc = deviceService_request_QUERY_METADATA_BY_URI(uri, metadataUriMap)) == IPC_SUCCESS)
        {
            icHashMapIterator *iter = hashMapIteratorCreate(metadataUriMap->metadataByUriValuesMap);
            while(hashMapIteratorHasNext(iter))
            {
                char *metadataUri;
                uint16_t keyLen;
                char *value;
                hashMapIteratorGetNext(iter, (void**)&metadataUri, &keyLen, (void**)&value);

                if (stringCompare(preZilkerCameraId, value, false) == 0)
                {
                    *cameraDeviceId = getDeviceUuidFromUri(metadataUri);
                    retVal = true;
                    break;
                }
            }
            hashMapIteratorDestroy(iter);
        }
        else
        {
            icLogError(LOG_TAG, "Failed to find camera migrated id metadata, ipc error=%s", IPCCodeLabels[ipcRc]);
        }

        destroy_DSMetadataUriMap(metadataUriMap);
        free(uri);
    }

    return retVal;
}

/**
 * Convert a SMAP DeviceId to a device UUID/endpointId combo.  Handles the 5 different formats:
 * PreZdif(zigbee) - eui64 in decimal format
 * Zdif(zigbee) - eui64.endpointId where eui64 is 0 padded 16 digit hex, and endpointId is decimal
 * Zilker(zigbee) - premiseId.eui64 where premiseId is decimal number and eui64 is 0 padded 16 digit hex
 * PreZilker(camera) - premiseId.cameraId where cameraId is a decimal number
 * Zilker(camera) - premiseId.cameraId where cameraId is camera mac address without colons
 * @param input the SMAP device ID
 * @param deviceId the device UUID will be populated here, caller must free
 * @param endpointId the endpointId will be populated here(may be wildcard), caller must free
 * @return true if success, false if valid isn't in one of the known formats
 */
bool mapUserverDeviceId(const char *input, char **deviceId, char **endpointId)
{
    if (input == NULL || deviceId == NULL)
    {
        return false;
    }

    bool retVal = false;

    icLogDebug(LOG_TAG, "%s: Got request with deviceId %s", __FUNCTION__, input);
    // Create a copy so we can safely modify things
    char *inputCopy = strdup(input);
    // Look for a delimiter
    char *ptr = strchr(inputCopy, '.');
    if (ptr == NULL)
    {
        // Try to extract it as a base 10 uint64_t, as this was the format for pre zdif devices
        uint64_t eui64;
        if (stringToUnsignedNumberWithinRange(inputCopy, &eui64, 10, 0, UINT64_MAX) == true)
        {
            *deviceId = stringBuilder("%016"PRIx64, eui64);
            icLogDebug(LOG_TAG, "%s: Evaluated as pre-zdif format, converted to %s", __FUNCTION__, *deviceId);
            retVal = true;
        }
    }
    else
    {
        // Null terminate at the delimiter to make things simpler
        *ptr = '\0';

        // Just a dummy value so that we can test parsing things
        uint64_t testNum;

        // For zdif devices the id was in format DeviceId.EndpointId where DeviceId was a 0 padded 16 digit hex
        // eui64 and endpointId was the decimal endpointId, which is a uint8_t.  See if it matches this format
        if (ptr - inputCopy == ZIGBEE_HEX_EUI64_LENGTH &&
            stringToUnsignedNumberWithinRange(ptr + 1, &testNum, 10, 0, UINT8_MAX) &&
            stringToUnsignedNumberWithinRange(inputCopy, &testNum, 16, 0, UINT64_MAX) == true)
        {
            *deviceId = strdup(inputCopy);
            icLogDebug(LOG_TAG, "%s: Evaluated as zdif format, converted to %s", __FUNCTION__, *deviceId);
            retVal = true;
        }
        // Zilker zigbee should have 16 digit eui64 after the '.'
        else if (strlen(ptr + 1) == ZIGBEE_HEX_EUI64_LENGTH &&
                 stringToUnsignedNumberWithinRange(ptr + 1, &testNum, 16, 0, UINT64_MAX) == true)
        {
            *deviceId = strdup(ptr + 1);
            icLogDebug(LOG_TAG, "%s: Evaluated as zilker zigbee format, converted to %s", __FUNCTION__,
                       *deviceId);
            retVal = true;
        }
        // Zilker camera should have 12 digit mac after the '.'
        else if (strlen(ptr + 1) == MAC_WITHOUT_COLONS_LENGTH &&
                 stringToUnsignedNumberWithinRange(ptr + 1, &testNum, 16, 0, MAX_MAC_ADDRESS) == true)
        {
            *deviceId = strdup(ptr + 1);
            icLogDebug(LOG_TAG, "%s: Evaluated as zilker camera format, converted to %s", __FUNCTION__,
                       *deviceId);
            retVal = true;
        }
        // Pre-Zilker camera
        else if (getCameraDeviceIdForPreZilkerCameraDeviceId(ptr + 1, deviceId) == true)
        {
            icLogDebug(LOG_TAG, "%s: Evaluated as pre-zilker camera format, converted to %s", __FUNCTION__,
                       *deviceId);
            retVal = true;
        }
    }
    free(inputCopy);

    if (retVal == false)
    {
        icLogError(LOG_TAG, "%s: Unknown format for input %s", __FUNCTION__, input);
    }
    else if (endpointId != NULL)
    {
        // Just wildcard the endpoint
        *endpointId = strdup("*");
    }

    return retVal;
}