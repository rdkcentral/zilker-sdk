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
#include <device/icDeviceResource.h>
#include <deviceDriver.h>
#include "deviceServicePrivate.h"
#include <jsonHelper/jsonHelper.h>
#include <icUtil/stringUtils.h>
#include <icConfig/simpleProtectConfig.h>

#define LOG_TAG "deviceService"

/*
 * Created by Thomas Lea on 8/10/15.
 */

// Keys for resource json representation
#define RESOURCE_ID_KEY "id"
#define RESOURCE_URI_KEY "uri"
#define RESOURCE_MODE_KEY "mode"
#define RESOURCE_CACHING_POLICY_KEY "cachingPolicy"
#define RESOURCE_DATE_OF_LAST_SYNC_MILLIS_KEY "dateOfLastSyncMillis"
#define RESOURCE_VALUE_KEY "value"
#define RESOURCE_ENCRYPTED_VALUE_KEY "value_enc"
#define RESOURCE_TYPE_KEY "type"
#define RESOURCE_NAMESPACE_KEY "namespace"

extern inline void resourceDestroy__auto(icDeviceResource **resource);

void resourceDestroy(icDeviceResource* resource)
{
    if (resource != NULL)
    {
        free(resource->id);
        free(resource->endpointId);
        free(resource->uri);
        free(resource->type);
        free(resource->deviceUuid);
        free(resource->value);

        free(resource);
    }
}

void resourcePrint(icDeviceResource* resource, const char* prefix)
{
    if (resource == NULL)
    {
        icLogDebug(LOG_TAG, "%sResource [NULL!]", prefix);
    }
    else
    {
        icLogDebug(LOG_TAG,
                   "%sResource [uri=%s] [id=%s] [endpointId=%s] [type=%s] [mode=0x%x] [cache policy=%d]: %s",
                   prefix,
                   resource->uri,
                   resource->id,
                   resource->endpointId,
                   resource->type,
                   resource->mode,
                   resource->cachingPolicy,
                   (resource->mode & RESOURCE_MODE_SENSITIVE) ? ("(encrypted)") : (resource->value));
    }
}

/**
 * Clone a device resource object
 *
 * @param resource the resource to clone
 * @return the cloned resource object
 */
icDeviceResource* resourceClone(const icDeviceResource* resource)
{
    icDeviceResource* clone = NULL;
    if (resource != NULL)
    {
        clone = (icDeviceResource*) calloc(1, sizeof(icDeviceResource));
        if (resource->deviceUuid != NULL)
        {
            clone->deviceUuid = strdup(resource->deviceUuid);
        }
        if (resource->id != NULL)
        {
            clone->id = strdup(resource->id);
        }
        if (resource->uri != NULL)
        {
            clone->uri = strdup(resource->uri);
        }
        if (resource->type != NULL)
        {
            clone->type = strdup(resource->type);
        }
        if (resource->value != NULL)
        {
            clone->value = strdup(resource->value);
        }
        clone->dateOfLastSyncMillis = resource->dateOfLastSyncMillis;
        clone->cachingPolicy = resource->cachingPolicy;
        clone->mode = resource->mode;
        if (resource->endpointId != NULL)
        {
            clone->endpointId = strdup(resource->endpointId);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Attempt to clone NULL resource");
    }
    return clone;
}

/**
 * Convert resource object to JSON
 *
 * @param resource the resource to convert
 * @return the JSON object
 */
cJSON* resourceToJSON(const icDeviceResource* resource, const icSerDesContext *context)
{
    cJSON* json = cJSON_CreateObject();
    char *resourceValue = NULL;
    bool resourceValueEncrypted = false;

    if (resource->mode & RESOURCE_MODE_SENSITIVE)
    {
        if (serDesHasContextValue(context, RESOURCE_NAMESPACE_KEY) == true)
        {
            resourceValue = simpleProtectConfigData(serDesGetContextValue(context, RESOURCE_NAMESPACE_KEY),
                                                                          resource->value);
            resourceValueEncrypted = true;
        }
        else
        {
            icLogWarn(LOG_TAG, "Cannot encrypt resource \"%s\": missing namespace context value", resource->id);
        }
    }
    if (resourceValue == NULL && resource->value != NULL)
    {
        resourceValue = strdup(resource->value);
    }

    // Add resource info
    cJSON_AddStringToObject(json, RESOURCE_ID_KEY, resource->id);
    cJSON_AddStringToObject(json, RESOURCE_URI_KEY, resource->uri);
    cJSON_AddNumberToObject(json, RESOURCE_MODE_KEY, resource->mode);
    cJSON_AddNumberToObject(json, RESOURCE_CACHING_POLICY_KEY, resource->cachingPolicy);
    cJSON_AddNumberToObject(json, RESOURCE_DATE_OF_LAST_SYNC_MILLIS_KEY, resource->dateOfLastSyncMillis);

    // Whoever stores in the resource, if there is binary data they have to make sure to encode it, as a resource
    // doesn't support binary data

    if (resourceValueEncrypted == true)
    {
        cJSON_AddStringToObject(json, RESOURCE_ENCRYPTED_VALUE_KEY, resourceValue);
    }
    else
    {
        cJSON_AddStringToObject(json, RESOURCE_VALUE_KEY, resourceValue);
    }
    cJSON_AddStringToObject(json, RESOURCE_TYPE_KEY, resource->type);

    // Cleanup
    free(resourceValue);

    return json;
}

/**
 * Convert a list of resources objects to a JSON object with id as key
 *
 * @param resources the list of resources
 * @return the JSON object
 */
cJSON* resourcesToJSON(icLinkedList* resources, const icSerDesContext *context)
{
    cJSON* resourcesJSON = cJSON_CreateObject();
    icLinkedListIterator* iter = linkedListIteratorCreate(resources);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceResource* resource = (icDeviceResource*) linkedListIteratorGetNext(iter);
        cJSON* resourceJson = resourceToJSON(resource, context);
        cJSON_AddItemToObject(resourcesJSON, resource->id, resourceJson);
    }
    linkedListIteratorDestroy(iter);

    return resourcesJSON;
}

/**
* Load a device resource into memory from JSON
*
* @param deviceUUID the deviceUUID for which we are loading the metadata
* @param endpointId the endpointId for which we are loading the metdata
* @param resourceJSON the JSON to load
* @return the resource object or NULL if there is an error
*/
icDeviceResource* resourceFromJSON(const char* deviceUUID, const char* endpointId, cJSON* resourceJSON,
                                   const icSerDesContext *context)
{
    icDeviceResource* resource = NULL;
    char *resourceValue = NULL;

    if (resourceJSON != NULL)
    {
        resource = (icDeviceResource*) calloc(1, sizeof(icDeviceResource));

        // TODO how to best handle errors here...probably should just log and skip this resource
        resource->deviceUuid = strdup(deviceUUID);
        if (endpointId != NULL)
        {
            resource->endpointId = strdup(endpointId);
        }

        resource->uri = getCJSONString(resourceJSON, RESOURCE_URI_KEY);
        resource->id = getCJSONString(resourceJSON, RESOURCE_ID_KEY);

        int mode = RESOURCE_MODE_READABLE;
        getCJSONInt(resourceJSON, RESOURCE_MODE_KEY, &mode);
        resource->mode = (uint8_t) mode;

        if (resource->mode & RESOURCE_MODE_SENSITIVE)
        {
            char *encryptedResourceValue = getCJSONString(resourceJSON, RESOURCE_ENCRYPTED_VALUE_KEY);

            if (encryptedResourceValue != NULL)
            {
                if (serDesHasContextValue(context, RESOURCE_NAMESPACE_KEY) == true)
                {
                    resourceValue = simpleUnprotectConfigData(serDesGetContextValue(context, RESOURCE_NAMESPACE_KEY),
                                                              encryptedResourceValue);
                }
                else
                {
                    icLogWarn(LOG_TAG, "Cannot decrypt value for resource \"%s\": missing namespace context value",
                              resource->id);
                }
                free(encryptedResourceValue);
            }
            else
            {
                icLogWarn(LOG_TAG, "Cannot find encrypted value for resource \"%s\" (using unencrypted value)",
                          resource->id);
            }
        }
        if (resourceValue != NULL)
        {
            resource->value = resourceValue;
        }
        else
        {
            resource->value = getCJSONString(resourceJSON, RESOURCE_VALUE_KEY);
        }

        resource->type = getCJSONString(resourceJSON, RESOURCE_TYPE_KEY);

        int cachingPolicy = CACHING_POLICY_NEVER;
        getCJSONInt(resourceJSON, RESOURCE_CACHING_POLICY_KEY, &cachingPolicy);

        resource->cachingPolicy = cachingPolicy;

        double dateOfLastSyncMillis = 0;
        // coverity[check_return]
        getCJSONDouble(resourceJSON, RESOURCE_DATE_OF_LAST_SYNC_MILLIS_KEY, &dateOfLastSyncMillis);
        resource->dateOfLastSyncMillis = (uint64_t) dateOfLastSyncMillis;
    }
    else
    {
        if (endpointId != NULL)
        {
            icLogError(LOG_TAG, "Failed to find resource json for device %s, endpoint %s", deviceUUID, endpointId);
        }
        else
        {
            icLogError(LOG_TAG, "Failed to find resource json for device %s", deviceUUID);
        }
    }

    return resource;
}

/**
 * Load the resources for a device and endpoint from JSON
 *
 * @param device  the device
 * @param endpoint the endpoint
 * @param resourcesJson the JSON to load
 * @return linked list of resource structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see resourceDestroy
 */
icLinkedList* resourcesFromJSON(const char* deviceUUID, const char* endpointId, cJSON* resourcesJSON,
                                const icSerDesContext *context)
{
    icLinkedList* resources = linkedListCreate();
    if (resourcesJSON != NULL)
    {
        int resourceCount = cJSON_GetArraySize(resourcesJSON);
        for (int i = 0; i < resourceCount; ++i)
        {
            cJSON* resourceJson = cJSON_GetArrayItem(resourcesJSON, i);
            icDeviceResource* resource = resourceFromJSON(deviceUUID, endpointId, resourceJson, context);
            if (resource != NULL)
            {
                linkedListAppend(resources, resource);
            }
            else
            {
                if (endpointId != NULL)
                {
                    icLogError(LOG_TAG, "Failed to add resource %s for device %s, endpoint %s",
                               resourceJson->string, deviceUUID, endpointId);
                }
                else
                {
                    icLogError(LOG_TAG, "Failed to add resource %s for device %s",
                               resourceJson->string, deviceUUID);
                }
            }
        }
    }
    else
    {
        if (endpointId != NULL)
        {
            icLogError(LOG_TAG, "Failed to find resources json for device %s, endpoint %s", deviceUUID, endpointId);
        }
        else
        {
            icLogError(LOG_TAG, "Failed to find resources json for device %s", deviceUUID);
        }
    }
    return resources;
}

