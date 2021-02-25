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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <icLog/logging.h>
#include <device/icDeviceMetadata.h>
#include "deviceServicePrivate.h"
#include <jsonHelper/jsonHelper.h>
#include <serial/icSerDesContext.h>

#define LOG_TAG "deviceService"

/*
 * Created by Thomas Lea on 9/15/15.
 */

// Keys for metadata json representation
#define METADATA_ID_KEY "id"
#define METADATA_URI_KEY "uri"
#define METADATA_VALUE_KEY "value"

void metadataDestroy(icDeviceMetadata* metadata)
{
    if (metadata != NULL)
    {
        free(metadata->id);
        free(metadata->endpointId);
        free(metadata->uri);
        free(metadata->deviceUuid);
        free(metadata->value);

        free(metadata);
    }
}

extern inline void metadataDestroy__auto(icDeviceMetadata **metadata);

void metadataPrint(icDeviceMetadata* metadata, const char* prefix)
{
    if (metadata == NULL)
    {
        icLogDebug(LOG_TAG, "%sMetadata [NULL!]", prefix);
    }
    else
    {
        icLogDebug(LOG_TAG,
                   "%sMetadata [uri=%s] [id=%s] [endpointId=%s] [value=%s]",
                   prefix,
                   metadata->uri,
                   metadata->id,
                   metadata->endpointId,
                   metadata->value);
    }
}


/**
 * Clone a metadata object
 *
 * @param metadata the metadata to clone
 * @return the metadata object
 */
icDeviceMetadata* metadataClone(const icDeviceMetadata* metadata)
{
    icDeviceMetadata* clone = NULL;
    if (metadata != NULL)
    {
        clone = (icDeviceMetadata*) calloc(1, sizeof(icDeviceMetadata));
        if (metadata->endpointId != NULL)
        {
            clone->endpointId = strdup(metadata->endpointId);
        }
        if (metadata->uri != NULL)
        {
            clone->uri = strdup(metadata->uri);
        }
        if (metadata->id != NULL)
        {
            clone->id = strdup(metadata->id);
        }
        if (metadata->deviceUuid != NULL)
        {
            clone->deviceUuid = strdup(metadata->deviceUuid);
        }
        if (metadata->value != NULL)
        {
            clone->value = strdup(metadata->value);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Attempt to clone NULL metadata");
    }
    return clone;
}

/**
 * Convert metadata object to JSON
 *
 * @param metadata the metadata to convert
 * @return the JSON object
 */
cJSON* metadataToJSON(const icDeviceMetadata* metadata, const icSerDesContext *context)
{
    cJSON* json = cJSON_CreateObject();
    // Add metadata info
    cJSON_AddStringToObject(json, METADATA_ID_KEY, metadata->id);
    cJSON_AddStringToObject(json, METADATA_URI_KEY, metadata->uri);

    // First try to parse the metadata as json, if that works, then store it directly
    cJSON *valueJSON = cJSON_Parse(metadata->value);
    if (valueJSON != NULL && cJSON_IsObject(valueJSON))
    {
        cJSON_AddItemToObject(json, METADATA_VALUE_KEY, valueJSON);
    }
    else
    {
        // Cleanup in case we parsed to something that wasn't an object(NULL is safe to delete)
        cJSON_Delete(valueJSON);
        // Wasn't JSON, so just store it
        cJSON_AddStringToObject(json, METADATA_VALUE_KEY, metadata->value);
    }


    return json;
}

/**
 * Convert a list of metadata objects to a JSON object with id as key
 *
 * @param metadatas the list of metadata
 * @return the JSON object
 */
cJSON* metadatasToJSON(icLinkedList* metadatas, const icSerDesContext *context)
{
    // Add metadata by id
    cJSON* metadatasJson = cJSON_CreateObject();
    icLinkedListIterator* metadataIter = linkedListIteratorCreate(metadatas);
    while (linkedListIteratorHasNext(metadataIter))
    {
        icDeviceMetadata* metadata = (icDeviceMetadata*) linkedListIteratorGetNext(metadataIter);
        cJSON* metadataJson = metadataToJSON(metadata, context);
        cJSON_AddItemToObject(metadatasJson, metadata->id, metadataJson);
    }
    linkedListIteratorDestroy(metadataIter);

    return metadatasJson;
}

/**
 * Load a device metadata into memory from JSON
 *
 * @param deviceUUID the device UUID for which we are loading the metadata
 * @param endpointId the endpoint ID for which we are loading the metdata
 * @param metadataJson the JSON to load
 * @return the metadata object or NULL if there is an error
 */
icDeviceMetadata* metadataFromJSON(const char* deviceUUID, const char* endpointId, cJSON* metadataJSON)
{
    icDeviceMetadata* metadata = NULL;
    if (metadataJSON != NULL)
    {
        metadata = (icDeviceMetadata*) calloc(1, sizeof(icDeviceMetadata));
        // TODO how to best handle errors here...probably should just log and skip this metadata
        metadata->id = getCJSONString(metadataJSON, METADATA_ID_KEY);
        // Check whether it was an object, or a string
        cJSON *valueJSON = cJSON_GetObjectItem(metadataJSON, METADATA_VALUE_KEY);
        if (cJSON_IsObject(valueJSON))
        {
            // Convert the object back to string for users
            metadata->value = cJSON_Print(valueJSON);
        }
        else
        {
            // Just get the string
            metadata->value = getCJSONString(metadataJSON, METADATA_VALUE_KEY);
        }
        metadata->uri = getCJSONString(metadataJSON, METADATA_URI_KEY);
        metadata->deviceUuid = strdup(deviceUUID);
        if (endpointId != NULL)
        {
            metadata->endpointId = strdup(endpointId);
        }
    }
    else
    {
        if (endpointId != NULL)
        {
            icLogError(LOG_TAG, "Failed to find metadata json for device %s, endpoint %s", deviceUUID, endpointId);
        }
        else
        {
            icLogError(LOG_TAG, "Failed to find metadata json for device %s", deviceUUID);
        }
    }

    return metadata;
}

/**
 * Load the metadata for a device and endpoint from JSON
 *
 * @param deviceUUID  the device UUID
 * @param endpointId the endpoint ID
 * @param metadatasJson the JSON to load
 * @return linked list of metadata structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see metadataDestroy
 */
icLinkedList* metadatasFromJSON(const char* deviceUUID, const char* endpointId, cJSON* metadatasJSON)
{
    icLinkedList* metadatas = linkedListCreate();
    if (metadatasJSON != NULL)
    {
        int metadataCount = cJSON_GetArraySize(metadatasJSON);
        for (int i = 0; i < metadataCount; ++i)
        {
            cJSON* metadataJson = cJSON_GetArrayItem(metadatasJSON, i);
            icDeviceMetadata* metadata = metadataFromJSON(deviceUUID, endpointId, metadataJson);
            if (metadata != NULL)
            {
                linkedListAppend(metadatas, metadata);
            }
            else
            {
                if (endpointId != NULL)
                {
                    icLogError(LOG_TAG, "Failed to add metadata %s for device %s, endpoint %s",
                               metadataJson->string, deviceUUID, endpointId);
                }
                else
                {
                    icLogError(LOG_TAG, "Failed to add metadata %s for device %s",
                               metadataJson->string, deviceUUID);
                }
            }
        }
    }
    else
    {
        if (endpointId != NULL)
        {
            icLogError(LOG_TAG, "Failed to find metadatas json for device %s, endpoint %s", deviceUUID, endpointId);
        }
        else
        {
            icLogError(LOG_TAG, "Failed to find metadatas json for device %s", deviceUUID);
        }
    }

    return metadatas;
}

