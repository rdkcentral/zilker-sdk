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
 * Created by Thomas Lea on 9/30/15.
 */
#include <deviceHelper.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <deviceService/deviceService_ipc.h>
#include <stdio.h>
#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include <commonDeviceDefs.h>

static bool fetchResource(char *uri, char **value);
static bool writeResource(char *uri, char *value);

#define READ_RESOURCE_TIMEOUT_SECONDS 60
#define LOG_TAG "deviceHelper"

char* createDeviceUri(const char *uuid)
{
    //URI format: /[uuid]
    char *uri = (char*)malloc(1
                              + strlen(uuid)
                              + 1);
    if(uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s", uuid);

    return uri;
}

char* createEndpointUri(const char *uuid, const char *endpointId)
{
    //URI format: /[uuid]/ep/[endpoint id]
    char *uri = (char*)malloc(1
                              + strlen(uuid)
                              + 4
                              + strlen(endpointId)
                              + 1);
    if(uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s/ep/%s", uuid, endpointId);

    return uri;
}

char* createDeviceResourceUri(const char *uuid, const char *resourceId)
{
    //URI format: /[uuid]/r/[resource id]
    char *uri = (char*)malloc(1
                              + strlen(uuid)
                              + 3
                              + strlen(resourceId)
                              + 1);
    if(uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s/r/%s", uuid, resourceId);

    return uri;
}

char *createDeviceMetadataUri(const char *uuid, const char *metadataId)
{
    //URI format: /[uuid]/m/[resource id]
    char *uri = (char*)malloc(1
                              + strlen(uuid)
                              + 3
                              + strlen(metadataId)
                              + 1);
    if(uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "/%s/m/%s", uuid, metadataId);

    return uri;
}

char* createEndpointResourceUri(const char *uuid, const char *endpointId, const char *resourceId)
{
    char *epUri = createEndpointUri(uuid, endpointId);

    //URI Format: [endpointURI]/r/[resourceId]
    //The compiler will optimize the constants addition
    char *uri = (char*)malloc(strlen(epUri)
                              + 3
                              + strlen(resourceId)
                              + 1);

    sprintf(uri, "%s/r/%s", epUri, resourceId);

    free(epUri);

    return uri;
}

char* createEndpointMetadataUri(const char *uuid, const char *endpointId, const char *metadataId)
{
    char *epUri = createEndpointUri(uuid, endpointId);

    //URI Format: /[endpointUri]/m/[metadataId]
    //The compiler will optimize the constants addition
    char *uri = (char*)malloc(strlen(epUri)
                              + 3
                              + strlen(metadataId)
                              + 1);

    sprintf(uri, "%s/m/%s", epUri, metadataId);

    free(epUri);

    return uri;
}

/*
 * Read an resource on a device's endpoint.
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadEndpointResource(const char *uuid,
                                            const char *endpointId,
                                            const char *resourceId,
                                            char **value)
{
    if(uuid == NULL || endpointId == NULL || resourceId == NULL || value == NULL)
    {
        return false;
    }

    char *uri = createEndpointResourceUri(uuid, endpointId, resourceId);
    if(uri != NULL)
    {
        bool result = fetchResource(uri, value);

        free(uri);

        return result;
    }
    else
    {
        return false;
    }
}

bool deviceHelperWriteEndpointResource(const char *uuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       char *value)
{
    if(uuid == NULL || endpointId == NULL || resourceId == NULL || value == NULL)
    {
        return false;
    }

    char *uri = createEndpointResourceUri(uuid, endpointId, resourceId);
    if(uri != NULL)
    {
        bool result = writeResource(uri, value);

        free(uri);

        return result;
    }
    else
    {
        return false;
    }
}

/*
 * Read an resource
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadDeviceResource(const char *uuid,
                                    const char *resourceId,
                                    char **value)
{
    if(uuid == NULL || resourceId == NULL || value == NULL)
    {
        return false;
    }

    char *uri = createDeviceResourceUri(uuid, resourceId);
    if(uri != NULL)
    {
        bool result = fetchResource(uri, value);

        free(uri);

        return result;
    }
    else
    {
        return false;
    }
}

bool deviceHelperReadResourceByUri(const char *uri,
                                   char **value)
{
    bool result = false;

    if(uri == NULL || value == NULL)
    {
        return false;
    }

    DSReadResourceResponse *output = create_DSReadResourceResponse();
    IPCCode ipcRc;
    if((ipcRc = deviceService_request_READ_RESOURCE_timeout((char *) uri, output, READ_RESOURCE_TIMEOUT_SECONDS)) == IPC_SUCCESS && output->success == true)
    {
        if(output->response != NULL)
        {
            *value = strdup(output->response);
        }
        result = true;
    }
    else
    {
        fprintf(stderr, "Failed to read resource: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSReadResourceResponse(output);

    return result;
}

bool deviceHelperGetResourceByUri(const char *uri,
                                  DSResource *resource)
{
    bool result = false;

    if(uri == NULL || resource == NULL)
    {
        return false;
    }

    IPCCode ipcRc;
    if((ipcRc = deviceService_request_GET_RESOURCE_timeout((char *) uri, resource, READ_RESOURCE_TIMEOUT_SECONDS)) == IPC_SUCCESS)
    {
        result = true;
    }
    else
    {
        fprintf(stderr, "Failed to get resource: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    return result;
}

bool deviceHelperReadMetadataByUri(const char *uri,
                                   char **value)
{
    bool result = false;

    if(uri == NULL || value == NULL)
    {
        return false;
    }

    DSReadMetadataResponse *output = create_DSReadMetadataResponse();
    if(deviceService_request_READ_METADATA((char *) uri, output) == IPC_SUCCESS && output->success == true)
    {
        if(output->response != NULL)
        {
            *value = strdup(output->response);
        }
        result = true;
    }
    destroy_DSReadMetadataResponse(output);

    return result;
}

bool deviceHelperWriteMetadataByUri(const char *uri,
                                    const char *value)
{
    bool result = false;

    if(uri == NULL)
    {
        return false;
    }

    DSWriteMetadataRequest *input = create_DSWriteMetadataRequest();
    input->uri = strdup(uri);
    if(value != NULL)
    {
        input->value = strdup(value);
    }

    bool success = false;
    result = deviceService_request_WRITE_METADATA(input, &success) == IPC_SUCCESS && success == true;
    destroy_DSWriteMetadataRequest(input);

    return result;
}

bool deviceHelperReadMetadataByOwner(const char *ownerUri,
                                     const char *metadataId,
                                     char **value)
{
    bool result = false;

    if(ownerUri == NULL || metadataId == NULL || value == NULL)
    {
        return false;
    }

    //URI Format: [ownerUri]/m/[metadataId]
    //The compiler will optimize the constants addition
    char *uri = (char*)malloc(strlen(ownerUri)
                              + 1
                              + 1 //strlen("m")
                              + 1
                              + strlen(metadataId)
                              + 1);

    if(uri == NULL)
    {
        return NULL;
    }

    sprintf(uri, "%s/m/%s", ownerUri, metadataId);

    DSReadMetadataResponse *output = create_DSReadMetadataResponse();

    if(deviceService_request_READ_METADATA(uri, output) == IPC_SUCCESS && output->success == true)
    {
        if(output->response != NULL)
        {
            *value = strdup(output->response);
        }
        result = true;
    }
    destroy_DSReadMetadataResponse(output);

    free(uri);

    return result;
}

bool deviceHelperWriteMetadataByOwner(const char *ownerUri,
                                      const char *metadataId,
                                      const char *value)
{
    bool result = false;

    if(ownerUri == NULL || metadataId == NULL)
    {
        return false;
    }

    //URI Format: [ownerUri]/m/[metadataId]
    //The compiler will optimize the constants addition
    char *uri = (char*)malloc(strlen(ownerUri)
                              + 1
                              + 1 //strlen("m")
                              + 1
                              + strlen(metadataId)
                              + 1);

    if(uri == NULL)
    {
        return false;
    }

    sprintf(uri, "%s/m/%s", ownerUri, metadataId);

    DSWriteMetadataRequest *input = create_DSWriteMetadataRequest();
    input->uri = uri;
    if(value != NULL)
    {
        input->value = strdup(value);
    }

    bool success = false;
    result = deviceService_request_WRITE_METADATA(input, &success) == IPC_SUCCESS && success == true;
    destroy_DSWriteMetadataRequest(input);

    return result;
}

static bool fetchResource(char *uri, char **value)
{
    bool result;

    if(uri == NULL || value == NULL)
    {
        return false;
    }

    DSReadResourceResponse *output = create_DSReadResourceResponse();
    if(deviceService_request_READ_RESOURCE(uri, output) == IPC_SUCCESS && output->success == true)
    {
        if(output->response != NULL)
        {
            *value = strdup(output->response);
        }
        result = true;
    }
    else
    {
        result = false;
    }
    destroy_DSReadResourceResponse(output);

    return result;
}

static bool writeResource(char *uri, char *value)
{
    bool result;

    if(uri == NULL || value == NULL)
    {
        return false;
    }

    DSWriteResourceRequest *request = create_DSWriteResourceRequest();
    request->uri = strdup(uri);
    request->value = strdup(value);
    bool output;
    if(deviceService_request_WRITE_RESOURCE(request, &output) == IPC_SUCCESS && output == true)
    {
        result = true;
    }
    else
    {
        result = false;
    }
    destroy_DSWriteResourceRequest(request);

    return result;
}

char* createResourceUri(const char *ownerUri, const char *resourceId)
{
    //URI format: ownerUri/r/[resource id]

    return stringBuilder("%s/r/%s", ownerUri, resourceId);
}

/*
 * Extract the device UUID from the given URI, caller must free result
 */
char* getDeviceUuidFromUri(const char *uri)
{
    char *retVal = NULL;
    // Format should /deviceUuid/... or just /deviceUuid
    if (uri != NULL)
    {
        size_t len = strlen(uri);
        if (len > 1)
        {
            // If its just a device URI, then it won't have an ending /, in which case we will use the len
            char *end = strchr(uri + 1, '/');
            if (end != NULL)
            {
                len = end - uri;
            }

            retVal = (char *) malloc(len + 1);
            strncpy(retVal, uri + 1, len - 1);
            // Since we were copying part of a longer string, need to NULL terminate
            retVal[len - 1] = '\0';
        }
    }
    return retVal;
}

/*
 * Extract the endpoint ID from the given URI, caller must free result
 */
char* getEndpointIdFromUri(const char *uri)
{
    char *retVal = NULL;
    // Format should /deviceUuid/ep/endpointId/... or /deviceUuid/ep/endpointId
    if (uri != NULL)
    {
        size_t len = strlen(uri);
        if (len > 1)
        {
            // Find the start of the endpoint part
            char *start = strstr(uri + 1, "/ep/");
            if (start != NULL)
            {
                start = start + 4;
                char *end = strchr(start, '/');
                if (end != NULL)
                {
                    len = end - start;
                }
                else
                {
                    len = len - (start - uri);
                }
            }

            if (start != NULL)
            {
                retVal = (char *) malloc(len + 1);
                strncpy(retVal, start, len);
                // Since we were copying part of a longer string, need to NULL terminate
                retVal[len] = '\0';
            }
        }
    }
    return retVal;
}

/**
 * Get the value of a resource from the device
 * @param endpoint the endpoint to fetch from
 * @param resourceId the resourceId to fetch
 * @param defaultValue the default value to use if value is empty or cannot be retrieved
 * @return the value, which should NOT be freed
 */
const char *deviceHelperGetDeviceResourceValue(DSDevice *device, const char *resourceId, const char *defaultValue)
{
    if (device == NULL || resourceId == NULL)
    {
        icLogWarn(LOG_TAG, "%s:  Invalid device or resourceId passed", __FUNCTION__);
        return defaultValue;
    }

    char *resourceUri = createResourceUri(device->uri, resourceId);
    DSResource *resource = get_DSResource_from_DSDevice_resources(device, resourceUri);
    free(resourceUri);

    if (resource != NULL && resource->value != NULL && strlen(resource->value) > 0)
    {
        return resource->value;
    }
    else
    {
        return defaultValue;
    }
}

/**
 * Get the value of a resource endpoint from the endpoint
 * @param endpoint the endpoint to fetch from
 * @param resourceId the resourceId to fetch
 * @param defaultValue the default value to use if value is empty or cannot be retrieved
 * @return the value, which should NOT be freed
 */
const char *deviceHelperGetEndpointResourceValue(DSEndpoint *endpoint, const char *resourceId, const char *defaultValue)
{
    if (endpoint == NULL || resourceId == NULL)
    {
        icLogWarn(LOG_TAG, "%s:  Invalid endpoint or resourceId passed", __FUNCTION__);
        return defaultValue;
    }

    char *resourceUri = createResourceUri(endpoint->uri, resourceId);
    DSResource *resource = get_DSResource_from_DSEndpoint_resources(endpoint, resourceUri);
    free(resourceUri);

    if (resource != NULL && resource->value != NULL && strlen(resource->value) > 0)
    {
        return resource->value;
    }
    else
    {
        return defaultValue;
    }
}

/**
 * Find the first endpoint on the given device with the given profile
 * @param device the device
 * @param endpointProfile the profile
 * @return the endpoint or NULL if no endpoint found
 */
DSEndpoint *deviceHelperGetEndpointByProfile(DSDevice *device, const char *endpointProfile)
{
    DSEndpoint *endpoint = NULL;

    if (device != NULL && endpointProfile != NULL)
    {
        icHashMapIterator *iter = hashMapIteratorCreate(device->endpointsValuesMap);
        while(hashMapIteratorHasNext(iter))
        {
            char *key;
            uint16_t keyLen;
            DSEndpoint *value;
            hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&value);

            if (strcmp(value->profile, endpointProfile) == 0)
            {
                endpoint = value;
                break;
            }
        }
        hashMapIteratorDestroy(iter);
    }

    return endpoint;
}

bool deviceHelperIsMultiEndpointCapable(const DSDevice *device)
{
    return hashMapCount(device->endpointsValuesMap) > 1;
}
