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
 * Created by Thomas Lea on 9/27/19.
 */

#include <deviceService/deviceService_pojo.h>
#include "category.h"
#include "util.h"
#include <string.h>
#include <inttypes.h>
#include <resourceTypes.h>
#include <icIpc/ipcMessage.h>
#include <deviceService/deviceService_ipc.h>
#include <deviceHelper.h>
#include <icUtil/stringUtils.h>
#include <icTypes/icSortedLinkedList.h>
#include <icTypes/sbrm.h>
#include <icUtil/fileUtils.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsService_ipc.h>
#include <linenoise.h>

#define DISCOVERY_SECONDS 60

static void listDeviceEntry(DSDevice *device)
{
    printf("%s: Class: %s\n", device->id, device->deviceClass);

    icHashMapIterator *endpointsIt = hashMapIteratorCreate(device->endpointsValuesMap);
    while (hashMapIteratorHasNext(endpointsIt))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(endpointsIt, (void **) &key, &keyLen, (void **) &value);

        DSEndpoint *endpoint = get_DSEndpoint_from_DSDevice_endpoints(device, key);

        char *label = NULL;
        icHashMapIterator *resourcesIt = hashMapIteratorCreate(endpoint->resourcesValuesMap);
        while (hashMapIteratorHasNext(resourcesIt))
        {
            char *key2;
            uint16_t keyLen2;
            char *value2;
            hashMapIteratorGetNext(resourcesIt, (void **) &key2, &keyLen2, (void **) &value2);

            DSResource *resource = get_DSResource_from_DSEndpoint_resources(endpoint, key2);
            if (strcmp(resource->type, RESOURCE_TYPE_LABEL) == 0)
            {
                label = resource->value;
                break;
            }
        }
        hashMapIteratorDestroy(resourcesIt);

        printf("\tEndpoint %s: Profile: %s, Label: %s\n", endpoint->id, endpoint->profile, label);
    }
    hashMapIteratorDestroy(endpointsIt);
}

static bool historyFunc(int argc,
                        char **argv)
{
    bool result = true;
    linenoiseHistoryPrint();
    return result;
}

static bool listDevicesFunc(int argc,
                            char **argv)
{
    IPCCode ipcRc;
    bool rc = false;

    DSDeviceList *output = create_DSDeviceList();

    if (argc == 0)
    {
        ipcRc = deviceService_request_GET_DEVICES(output);
    }
    else
    {
        ipcRc = deviceService_request_GET_DEVICES_BY_DEVICE_CLASS(argv[0], output);
    }

    if (ipcRc == IPC_SUCCESS)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(output->devices);
        while (linkedListIteratorHasNext(iterator))
        {
            DSDevice *device = (DSDevice *) linkedListIteratorGetNext(iterator);
            listDeviceEntry(device);
        }
        linkedListIteratorDestroy(iterator);

        rc = true;
    }
    else
    {
        fprintf(stderr, "Failed to get devices: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSDeviceList(output);

    return rc;
}

static bool getDeviceCountBySubsystemFunc(int argc,
                            char **argv)
{
    IPCCode ipcRc;
    bool rc = false;

    DSDeviceList *output = create_DSDeviceList();

    ipcRc = deviceService_request_GET_DEVICES_BY_SUBSYSTEM(argv[0], output);

    if (ipcRc == IPC_SUCCESS)
    {
        printf("%d\n",linkedListCount(output->devices));
        rc = true;
    }
    else
    {
        fprintf(stderr, "Failed to get devices by subsystem: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSDeviceList(output);

    return rc;
}

static int8_t resourceSort(void *newItem,
                           void *element)
{
    DSResource *left = (DSResource *) newItem;
    DSResource *right = (DSResource *) element;
    return stringCompare(left->uri, right->uri, false);
}

//find the 'best' label to use for this device.  caller frees
static char *getDeviceLabel(DSDevice *device)
{
    char *result = NULL;

    icHashMapIterator *endpointsIt = hashMapIteratorCreate(device->endpointsValuesMap);
    while (hashMapIteratorHasNext(endpointsIt) && result == NULL)
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(endpointsIt, (void **) &key, &keyLen, (void **) &value);

        DSEndpoint *endpoint = get_DSEndpoint_from_DSDevice_endpoints(device, key);

        icHashMapIterator *resourcesIt = hashMapIteratorCreate(endpoint->resourcesValuesMap);
        while (hashMapIteratorHasNext(resourcesIt))
        {
            char *key2;
            uint16_t keyLen2;
            char *value2;
            hashMapIteratorGetNext(resourcesIt, (void **) &key2, &keyLen2, (void **) &value2);

            DSResource *resource = get_DSResource_from_DSEndpoint_resources(endpoint, key2);
            if (strcmp(resource->type, RESOURCE_TYPE_LABEL) == 0 && resource->value != NULL)
            {
                result = strdup(resource->value);
                break;
            }
        }
        hashMapIteratorDestroy(resourcesIt);
    }
    hashMapIteratorDestroy(endpointsIt);

    return result;
}

static void printDeviceEntry(DSDevice *device)
{
    char *label = getDeviceLabel(device);
    printf("%s: %s, Class: %s\n", device->id, label == NULL ? "(no label)" : label, device->deviceClass);

    //gather device level resources
    icSortedLinkedList *sortedResources = sortedLinkedListCreate();
    icHashMapIterator *resourcesIt = hashMapIteratorCreate(device->resourcesValuesMap);
    while (hashMapIteratorHasNext(resourcesIt))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(resourcesIt, (void **) &key, &keyLen, (void **) &value);

        DSResource *resource = get_DSResource_from_DSDevice_resources(device, key);
        sortedLinkedListAdd(sortedResources, resource, resourceSort);
    }
    hashMapIteratorDestroy(resourcesIt);

    //print device level resources
    icLinkedListIterator *it = linkedListIteratorCreate(sortedResources);
    while (linkedListIteratorHasNext(it))
    {
        DSResource *resource = linkedListIteratorGetNext(it);
        char *v = getResourceValue(resource);
        printf("\t%s = %s\n", resource->uri, v == NULL ? "(null)" : v);
    }
    linkedListIteratorDestroy(it);
    linkedListDestroy(sortedResources, standardDoNotFreeFunc);

    //loop through each endpoint
    icHashMapIterator *endpointsIt = hashMapIteratorCreate(device->endpointsValuesMap);
    while (hashMapIteratorHasNext(endpointsIt))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(endpointsIt, (void **) &key, &keyLen, (void **) &value);

        DSEndpoint *endpoint = get_DSEndpoint_from_DSDevice_endpoints(device, key);

        //gather endpoint resources
        sortedResources = sortedLinkedListCreate();
        resourcesIt = hashMapIteratorCreate(endpoint->resourcesValuesMap);
        while (hashMapIteratorHasNext(resourcesIt))
        {
            char *key2;
            uint16_t keyLen2;
            char *value2;
            hashMapIteratorGetNext(resourcesIt, (void **) &key2, &keyLen2, (void **) &value2);

            DSResource *resource = get_DSResource_from_DSEndpoint_resources(endpoint, key2);
            sortedLinkedListAdd(sortedResources, resource, resourceSort);
        }
        hashMapIteratorDestroy(resourcesIt);

        //print endpoint resources
        printf("\tEndpoint %s: Profile: %s\n", endpoint->id, endpoint->profile);
        it = linkedListIteratorCreate(sortedResources);
        while (linkedListIteratorHasNext(it))
        {
            DSResource *resource = linkedListIteratorGetNext(it);
            char *v = getResourceValue(resource);
            printf("\t\t%s = %s\n", resource->uri, v == NULL ? "(null)" : v);
        }
        linkedListIteratorDestroy(it);

        linkedListDestroy(sortedResources, standardDoNotFreeFunc);
    }
    hashMapIteratorDestroy(endpointsIt);

    free(label);
}

static bool printDeviceFunc(int argc,
                            char **argv)
{
    (void) argc; //unused

    DSDevice *device = create_DSDevice();
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_GET_DEVICE_BY_ID(argv[0], device)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to get device: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        destroy_DSDevice(device);
        return false;
    }

    printDeviceEntry(device);

    destroy_DSDevice(device);

    return true;
}

static bool printAllDevicesFunc(int argc,
                                char **argv)
{
    IPCCode ipcRc;
    bool rc = false;

    DSDeviceList *output = create_DSDeviceList();

    if (argc == 0)
    {
        ipcRc = deviceService_request_GET_DEVICES(output);
    }
    else
    {
        ipcRc = deviceService_request_GET_DEVICES_BY_DEVICE_CLASS(argv[0], output);
    }

    if (ipcRc == IPC_SUCCESS)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(output->devices);
        while (linkedListIteratorHasNext(iterator))
        {
            DSDevice *device = (DSDevice *) linkedListIteratorGetNext(iterator);
            printDeviceEntry(device);
        }
        linkedListIteratorDestroy(iterator);

        rc = true;
    }
    else
    {
        fprintf(stderr, "Failed to get devices: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSDeviceList(output);

    return rc;
}

static bool readResourceFunc(int argc,
                             char **argv)
{
    (void) argc; //unused
    bool result = true;

    char *value = NULL;
    if (deviceHelperReadResourceByUri(argv[0], &value) == true)
    {
        printf("%s\n", stringCoalesce(value));
    }
    else
    {
        printf("Failed\n");
        result = false;
    }
    free(value);

    return result;
}

static bool writeResourceFunc(int argc,
                              char **argv)
{
    bool result = false;
    DSWriteResourceRequest *request = create_DSWriteResourceRequest();
    request->uri = strdup(argv[0]);
    if (argc == 2)
    {
        request->value = strdup(argv[1]);
    }

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_WRITE_RESOURCE(request, &result)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to write resource: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }
    destroy_DSWriteResourceRequest(request);

    return result;
}

static bool execResourceFunc(int argc,
                             char **argv)
{
    bool result;

    DSExecuteResourceRequest *request = create_DSExecuteResourceRequest();
    request->uri = strdup(argv[0]);
    if (argc == 2)
    {
        request->arg = strdup(argv[1]);
    }

    DSExecuteResourceResponse *response = create_DSExecuteResourceResponse();

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_EXECUTE_RESOURCE(request, response)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to execute resource: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }
    else
    {
        result = response->success;
    }

    destroy_DSExecuteResourceRequest(request);
    destroy_DSExecuteResourceResponse(response);

    return result;
}

static int8_t queryResourcesCompareFunc(void *newItem,
                                        void *element)
{
    DSResource *newResource = (DSResource *) newItem;
    DSResource *elementResource = (DSResource *) element;
    return stringCompare(newResource->uri, elementResource->uri, false);
}

static bool queryResourcesFunc(int argc,
                               char **argv)
{
    (void) argc; //unused
    IPCCode ipcRc;
    bool rc = false;
    DSResourceList *resourceList = create_DSResourceList();
    ipcRc = deviceService_request_QUERY_RESOURCES_BY_URI(argv[0], resourceList);

    if (ipcRc == IPC_SUCCESS)
    {
        if (linkedListCount(resourceList->resourceList) > 0)
        {
            printf("resources:\n");
            icSortedLinkedList *sortedResources = sortedLinkedListCreate();

            icLinkedListIterator *iter = linkedListIteratorCreate(resourceList->resourceList);
            while (linkedListIteratorHasNext(iter))
            {
                DSResource *resource = (DSResource *) linkedListIteratorGetNext(iter);
                sortedLinkedListAdd(sortedResources, resource, queryResourcesCompareFunc);
            }
            linkedListIteratorDestroy(iter);

            iter = linkedListIteratorCreate(sortedResources);
            while (linkedListIteratorHasNext(iter))
            {
                DSResource *resource = (DSResource *) linkedListIteratorGetNext(iter);
                char *v = getResourceValue(resource);
                printf("\t%s = %s\n", resource->uri, v == NULL ? "(null)" : v);
            }
            linkedListIteratorDestroy(iter);

            // Cleanup
            linkedListDestroy(sortedResources, standardDoNotFreeFunc);
        }
        else
        {
            printf("No resources found\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to query resources: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSResourceList(resourceList);

    return rc;
}

static bool changeResourceModeFunc(int argc,
                                   char **argv)
{
    (void) argc; //unused
    bool result = false;

    DSChangeResourceModeRequest *request = create_DSChangeResourceModeRequest();
    request->uri = strdup(argv[0]);
    if (stringToUint16(argv[1], &request->newMode) == false)
    {
        fprintf(stderr, "Invalid mode value\n");
    }
    else
    {
        bool didWork = false;
        IPCCode ipcRc;
        if ((ipcRc = deviceService_request_CHANGE_RESOURCE_MODE(request, &didWork)) != IPC_SUCCESS || didWork == false)
        {
            fprintf(stderr, "Failed to change resource mode: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        }
        else
        {
            printf("Resource mode changed\n");
            result = true;
        }
    }

    destroy_DSChangeResourceModeRequest(request);

    return result;
}

static bool readMetadataFunc(int argc,
                             char **argv)
{
    (void) argc; //unused
    DSReadMetadataResponse *response = create_DSReadMetadataResponse();
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_READ_METADATA(argv[0], response)) == IPC_SUCCESS && response->success == true)
    {
        if (response->response != NULL)
        {
            printf("%s\n", response->response);
        }
        else
        {
            printf("(null)\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to read metadata: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    destroy_DSReadMetadataResponse(response);

    return true;
}

static bool writeMetadataFunc(int argc,
                              char **argv)
{
    bool result = false;
    DSWriteMetadataRequest *request = create_DSWriteMetadataRequest();
    request->uri = strdup(argv[0]);
    if (argc == 2)
    {
        request->value = strdup(argv[1]);
    }

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_WRITE_METADATA(request, &result)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to set metadata: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }
    destroy_DSWriteMetadataRequest(request);

    return result;
}

static bool queryMetadataFunc(int argc,
                              char **argv)
{
    (void) argc; //unused
    IPCCode ipcRc;
    bool rc = false;
    DSMetadataUriMap *metadataUriMap = create_DSMetadataUriMap();
    ipcRc = deviceService_request_QUERY_METADATA_BY_URI(argv[0], metadataUriMap);

    if (ipcRc == IPC_SUCCESS)
    {
        if (get_count_of_DSMetadataUriMap_metadataByUri(metadataUriMap) > 0)
        {
            printf("metadata:\n");
            icHashMapIterator *iter = hashMapIteratorCreate(metadataUriMap->metadataByUriValuesMap);
            while (hashMapIteratorHasNext(iter))
            {
                char *metadataUri;
                uint16_t keyLen;
                char *value;
                hashMapIteratorGetNext(iter, (void **) &metadataUri, &keyLen, (void **) &value);
                printf("\t%s=%s\n", metadataUri, value);

            }
            hashMapIteratorDestroy(iter);
        }
        else
        {
            printf("No metadata found\n");
        }
    }
    else
    {
        fprintf(stderr, "Failed to query metadata: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSMetadataUriMap(metadataUriMap);

    return rc;
}

static bool discoverRepairStartFunc(int argc, char **argv)
{
    (void) argc; //unused
    IPCCode ipcRc;
    bool rc = false;

    DSDiscoverDevicesByClassRequest *request = create_DSDiscoverDevicesByClassRequest();
    request->deviceClass = argv[0];
    request->timeoutSeconds = DISCOVERY_SECONDS;

    fprintf(stdout, "Starting discovery of orphaned %s for %d seconds\n", argv[0], DISCOVERY_SECONDS);

    if ((ipcRc = deviceService_request_DISCOVER_ORPHANED_DEVICES_BY_CLASS(request, &rc)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Unable to communicate with deviceService : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    else if (rc != true)
    {
        fprintf(stderr, "Unable to start discovery of orphaned %s\n", argv[0]);
    }
    else
    {
        rc = true;
    }
    request->deviceClass = NULL;
    destroy_DSDiscoverDevicesByClassRequest(request);

    return rc;
}

static bool discoverStartFunc(int argc,
                              char **argv)
{
    (void) argc; //unused
    IPCCode ipcRc;
    bool rc = false;

    DSDiscoverDevicesByClassRequest *request = create_DSDiscoverDevicesByClassRequest();
    request->deviceClass = argv[0];
    request->timeoutSeconds = DISCOVERY_SECONDS;

    fprintf(stdout, "Starting discovery of %s for %d seconds\n", argv[0], DISCOVERY_SECONDS);

    if ((ipcRc = deviceService_request_DISCOVER_DEVICES_BY_CLASS(request, &rc)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Unable to communicate with deviceService : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    else if (rc != true)
    {
        fprintf(stderr, "Unable to start discovery of %s\n", argv[0]);
    }
    else
    {
        rc = true;
    }
    request->deviceClass = NULL;
    destroy_DSDiscoverDevicesByClassRequest(request);

    return rc;
}

static bool discoverStopFunc(int argc,
                             char **argv)
{
    (void) argc; //unused
    (void) argv; //unused
    IPCCode ipcRc;
    bool rc = false;

    if ((ipcRc = deviceService_request_STOP_DISCOVERING_DEVICES(&rc)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Unable to communicate with deviceService : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    else if (rc != true)
    {
        fprintf(stderr, "Failed to stop discovery\n");
    }
    else
    {
        rc = true;
    }

    return rc;
}

static bool removeDeviceFunc(int argc,
                             char **argv)
{
    (void) argc; //unused
    bool result = true;

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_REMOVE_DEVICE(argv[0], &result)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to remove device: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    return result;
}

static bool removeEndpointFunc(int argc,
                               char **argv)
{
    (void) argc; //unused
    bool result = true;

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_REMOVE_ENDPOINT_BY_URI(argv[0], &result)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to remove endpoint: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    return result;
}

static bool removeDevicesFunc(int argc,
                              char **argv)
{
    bool result = true;

    if (argc == 0)
    {
        printf("This will remove ALL devices!  Are you sure? (y/n) \n");
    }
    else
    {
        printf("This will remove ALL %s devices!  Are you sure? (y/n) \n", argv[0]);
    }

    int yn = getchar();
    if (yn != 'y')
    {
        fprintf(stderr, "Not removing devices\n");
        return true;
    }

    DSDeviceList *deviceList = create_DSDeviceList();
    if (argc == 0 || strlen(argv[0]) == 0)
    {
        IPCCode ipcRc;
        if ((ipcRc = deviceService_request_GET_DEVICES(deviceList)) != IPC_SUCCESS)
        {
            fprintf(stderr, "Failed to get devices to remove: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        }
    }
    else
    {
        IPCCode ipcRc;
        if ((ipcRc = deviceService_request_GET_DEVICES_BY_DEVICE_CLASS(argv[0], deviceList)) != IPC_SUCCESS)
        {
            fprintf(stderr, "Failed to get devices to remove: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        }
    }

    icLinkedListIterator *iter = linkedListIteratorCreate(deviceList->devices);
    while (linkedListIteratorHasNext(iter))
    {
        DSDevice *item = (DSDevice *) linkedListIteratorGetNext(iter);
        printf("Removing %s\n", item->id);

        IPCCode ipcRc;
        bool removeResult = true;
        if ((ipcRc = deviceService_request_REMOVE_DEVICE(item->id, &removeResult)) != IPC_SUCCESS)
        {
            fprintf(stderr, "Failed to remove device %s: %d - %s\n", item->id, ipcRc, IPCCodeLabels[ipcRc]);
        }
        result &= removeResult;
    }
    linkedListIteratorDestroy(iter);

    // Cleanup
    destroy_DSDeviceList(deviceList);

    return result;
}

static void dumpResource(DSResource *resource,
                         char *prefix)
{
    if (resource == NULL)
    {
        return;
    }
    AUTO_CLEAN(free_generic__auto) const char *modeStr = stringifyMode(resource->mode);

    printf("%s%s\n", prefix, resource->uri);
    printf("%s\tvalue=%s\n", prefix, getResourceValue(resource));
    printf("%s\townerId=%s\n", prefix, resource->ownerId);
    printf("%s\townerClass=%s\n", prefix, resource->ownerClass);
    printf("%s\ttype=%s\n", prefix, resource->type);
    printf("%s\tmode=0x%x (%s)\n", prefix, resource->mode, modeStr);
}

static void dumpEndpoint(DSEndpoint *endpoint,
                         char *prefix)
{
    if (endpoint == NULL)
    {
        return;
    }

    printf("%s%s\n", prefix, endpoint->uri);
    printf("%s\tprofile=%s\n", prefix, endpoint->profile);
    printf("%s\tprofileVersion=%d\n", prefix, endpoint->profileVersion);
    printf("%s\townerId=%s\n", prefix, endpoint->ownerId);

    //resources
    printf("%s\tresources:\n", prefix);
    icHashMapIterator *hiterator = hashMapIteratorCreate(endpoint->resourcesValuesMap);
    while (hashMapIteratorHasNext(hiterator))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(hiterator, (void **) &key, &keyLen, (void **) &value);

        DSResource *resource = get_DSResource_from_DSEndpoint_resources(endpoint, key);
        dumpResource(resource, "\t\t\t\t");
    }
    hashMapIteratorDestroy(hiterator);

    //metadata
    if (get_count_of_DSEndpoint_metadata(endpoint) > 0)
    {
        printf("\t\t\tmetadata:\n");
        hiterator = hashMapIteratorCreate(endpoint->metadataValuesMap);
        while (hashMapIteratorHasNext(hiterator))
        {
            char *key;
            uint16_t keyLen;
            char *value;
            hashMapIteratorGetNext(hiterator, (void **) &key, &keyLen, (void **) &value);

            value = get_metadataValue_from_DSEndpoint_metadata(endpoint, key);
            printf("%s\t%s=%s\n", prefix, key, value);
        }
        hashMapIteratorDestroy(hiterator);
    }
}

static bool dumpDeviceFunc(int argc,
                           char **argv)
{
    (void) argc; //unused
    DSDevice *device = create_DSDevice();
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_GET_DEVICE_BY_ID(argv[0], device)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to get device: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        destroy_DSDevice(device);
        return false;
    }

    printf("%s\n", device->uri);
    printf("\tdeviceClass=%s\n", device->deviceClass);
    printf("\tdeviceClassVersion=%d\n", device->deviceClassVersion);
    printf("\tmanagingDeviceDriver=%s\n", device->managingDeviceDriver);

    //device resources
    printf("\tresources:\n");
    icHashMapIterator *hiterator = hashMapIteratorCreate(device->resourcesValuesMap);
    while (hashMapIteratorHasNext(hiterator))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(hiterator, (void **) &key, &keyLen, (void **) &value);

        DSResource *resource = get_DSResource_from_DSDevice_resources(device, key);
        dumpResource(resource, "\t\t");
    }
    hashMapIteratorDestroy(hiterator);

    //endpoints
    printf("\tendpoints:\n");
    hiterator = hashMapIteratorCreate(device->endpointsValuesMap);
    while (hashMapIteratorHasNext(hiterator))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(hiterator, (void **) &key, &keyLen, (void **) &value);

        DSEndpoint *endpoint = get_DSEndpoint_from_DSDevice_endpoints(device, key);
        dumpEndpoint(endpoint, "\t\t");
    }
    hashMapIteratorDestroy(hiterator);

    //metadata
    if (get_count_of_DSDevice_metadata(device) > 0)
    {
        printf("\tmetadata:\n");
        hiterator = hashMapIteratorCreate(device->metadataValuesMap);
        while (hashMapIteratorHasNext(hiterator))
        {
            char *key;
            uint16_t keyLen;
            char *value;
            hashMapIteratorGetNext(hiterator, (void **) &key, &keyLen, (void **) &value);

            value = get_metadataValue_from_DSDevice_metadata(device, key);
            printf("\t\t%s=%s\n", key, value);
        }
        hashMapIteratorDestroy(hiterator);
    }

    destroy_DSDevice(device);

    return true;
}

static bool dumpDevicesFunc(int argc,
                            char **argv)
{
    (void) argc; //unused
    (void) argv; //unused
    IPCCode ipcRc;
    bool rc = false;

    DSDeviceList *output = create_DSDeviceList();

    ipcRc = deviceService_request_GET_DEVICES(output);

    if (ipcRc == IPC_SUCCESS)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(output->devices);
        while (linkedListIteratorHasNext(iterator))
        {
            DSDevice *data = (DSDevice *) linkedListIteratorGetNext(iterator);
            dumpDeviceFunc(1, &data->id);
        }
        linkedListIteratorDestroy(iterator);

        rc = true;
    }
    else
    {
        fprintf(stderr, "Failed to get devices: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }
    destroy_DSDeviceList(output);

    return rc;
}

static bool readSystemPropertyFunc(int argc,
                                   char **argv)
{
    (void) argc; //unused
    bool result = true;

    DSGetSystemPropertyResponse *response = create_DSGetSystemPropertyResponse();
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_GET_SYSTEM_PROPERTY(argv[0], response)) == IPC_SUCCESS &&
        response->success == true)
    {
        printf("%s\n", response->response);
    }
    else
    {
        fprintf(stderr, "Failed to read system property: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }

    destroy_DSGetSystemPropertyResponse(response);

    return result;
}

static bool writeSystemPropertyFunc(int argc,
                                    char **argv)
{
    (void) argc; //unused
    DSSetSystemPropertyRequest *request = create_DSSetSystemPropertyRequest();
    request->key = strdup(argv[0]);
    if (argv[1] != NULL)
    {
        request->value = strdup(argv[1]);
    }

    bool result;
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_SET_SYSTEM_PROPERTY(request, &result)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to set system property: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }
    destroy_DSSetSystemPropertyRequest(request);
    return result;
}

static bool reloadDatabaseFunc(int argc,
                               char **argv)
{
    (void) argc; //unused
    (void) argv; //unused
    bool result = false;
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_RELOAD_DATABASE(&result)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to reload database: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }

    return result;
}

static bool setDDlOverride(char *input)
{
    if (input == NULL)
    {
        fprintf(stderr, "Invalid url for ddl override\n");
        return false;
    }

    if (setPropertyValue(DEVICE_DESC_WHITELIST_URL_OVERRIDE, input, true, PROPERTY_SRC_DEVICE) == true)
    {
        printf("ddl override set to %s\n", input);
        return true;
    }
    else
    {
        fprintf(stderr, "Failed to set ddl override\n");
        return false;
    }
}

static bool ddlFunc(int argc,
                    char **argv)
{
    bool result = false;

    if (stringCompare(argv[0], "override", true) == 0)
    {
        if (argc == 2)
        {
            // see if this is a file
            //
            if (doesFileExist(argv[1]) == true)
            {
                // check to see if the file is not empty
                //
                if (doesNonEmptyFileExist(argv[1]) == true)
                {
                    // need to add "file://" to the front
                    // of the file path to be a valid url request
                    //
                    char *filePath = stringBuilder("file://%s", argv[1]);
                    result = setDDlOverride(filePath);

                    // cleanup
                    free(filePath);
                }
                else
                {
                    fprintf(stderr, "File %s is empty\n", argv[1]);
                }
            }
            else if (stringStartsWith(argv[1], "http", true) == true ||
                     stringStartsWith(argv[1], "file:///", true) == true)
            {
                // since this is a url just set prop
                //
                result = setDDlOverride(argv[1]);
            }
            else
            {
                fprintf(stderr, "Input %s is not a valid url or file request\n", argv[1]);
            }
        }
        else
        {
            fprintf(stderr, "Invalid input for ddl override\n");
        }
    }
    else if (stringCompare(argv[0], "clearoverride", true) == 0)
    {
        if (propsService_request_DEL_CPE_PROPERTY(DEVICE_DESC_WHITELIST_URL_OVERRIDE) == IPC_SUCCESS)
        {
            printf("Cleared ddl override (if one was set)\n");
            result = true;
        }
        else
        {
            fprintf(stderr, "Failed to clear any previous ddl override\n");
        }
    }
    else if (stringCompare(argv[0], "process", true) == 0)
    {
        bool success;
        int ipcRc = deviceService_request_PROCESS_DEVICE_DESCRIPTORS(&success);
        if (ipcRc != IPC_SUCCESS)
        {
            fprintf(stderr, "Failed to process device descriptors: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        }
        else if (!success)
        {
            fprintf(stderr, "Failed while processing device descriptors\n");
        }
        return ipcRc == IPC_SUCCESS && success;
    }
    else if (stringCompare(argv[0], "bypass", true) == 0 ||
             stringCompare(argv[0], "clearbypass", true) == 0)
    {
        bool bypass = stringCompare(argv[0], "bypass", true) == 0;

        DSSetSystemPropertyRequest *request = create_DSSetSystemPropertyRequest();
        request->key = strdup("deviceDescriptorBypass");
        request->value = bypass ? strdup("true") : strdup("false");

        IPCCode ipcRc;
        if ((ipcRc = deviceService_request_SET_SYSTEM_PROPERTY(request, &result)) != IPC_SUCCESS)
        {
            fprintf(stderr, "Failed to set system property: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
            result = false;
        }
        else
        {
            printf("ddl %s\n", bypass ? "bypassed" : "no longer bypassed");
        }
        destroy_DSSetSystemPropertyRequest(request);
    }
    else
    {
        fprintf(stderr, "invalid ddl subcommand\n");
    }

    return result;
}

static void printListWithCommas(icLinkedList *list)
{
    sbIcLinkedListIterator *it = linkedListIteratorCreate(list);
    while(linkedListIteratorHasNext(it))
    {
        char *item = linkedListIteratorGetNext(it);
        printf("%s", item);

        if (linkedListIteratorHasNext(it) == true)
        {
            printf(", ");
        }
    }
}

static bool getStatusFunc(int argc,
                          char **argv)
{
    bool result = true;

    IPCCode ipcRc;
    DSStatus *response = create_DSStatus();

    if ((ipcRc = deviceService_request_GET_STATUS(response)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Failed to get status: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }
    else
    {
        printf("Device Service Status:\n");
        printf("\tZigbee Ready: %s\n", stringValueOfBool(response->zigbeeReady));
        printf("\tSupported Device Classes: ");
        printListWithCommas(response->supportedDeviceClasses);
        printf("\n");
        printf("\tDiscovery Running: %s\n", stringValueOfBool(response->discoveryRunning));

        if (response->discoveryRunning == true)
        {
            printf("\tRequested Discovery Timeout: %"PRIu16"\n", response->discoveryTimeoutSeconds);
            printf("\tDiscovering Device Classes: ");
            printListWithCommas(response->discoveringDeviceClasses);
            printf("\n");
        }
        result = true;
    }

    destroy_DSStatus(response);

    return result;
}

Category *buildCoreCategory(void)
{
    Category *cat = categoryCreate("Core", "Core/standard commands");

    //history
    Command *command = commandCreate("history",
                                     NULL,
                                     NULL,
                                     "print the history of commands run interactively",
                                     0,
                                     0,
                                     historyFunc);
    categoryAddCommand(cat, command);

    //list devices
    command = commandCreate("listDevices",
                            "list",
                            "[device class]",
                            "list all devices, or all devices in a class",
                            0,
                            1,
                            listDevicesFunc);
    categoryAddCommand(cat, command);

    //get number of devices by sub system
    command = commandCreate("getDeviceCountBySubsystem",
                            NULL,
                            "<subsystem>",
                            "Get the number of devices by subsystem",
                            1,
                            1,
                            getDeviceCountBySubsystemFunc);
    commandAddExample(command, "getDeviceCountBySubsystem zigbee");
    categoryAddCommand(cat, command);

    //print a device
    command = commandCreate("printDevice",
                            "pd",
                            "<uuid>",
                            "print information for a device",
                            1,
                            1,
                            printDeviceFunc);
    categoryAddCommand(cat, command);

    //print all devices
    command = commandCreate("printAllDevices",
                            "pa",
                            "[device class]",
                            "print information for all devices, or all devices in a class",
                            0,
                            1,
                            printAllDevicesFunc);
    categoryAddCommand(cat, command);

    //read resource
    command = commandCreate("readResource",
                            "rr",
                            "<uri>",
                            "read the value of a resource",
                            1,
                            1,
                            readResourceFunc);
    commandAddExample(command, "readResource /000d6f000aae8410/r/communicationFailure");
    categoryAddCommand(cat, command);

    //write resource
    command = commandCreate("writeResource",
                            "wr",
                            "<uri> [value]",
                            "write the value of a resource",
                            1,
                            2,
                            writeResourceFunc);
    commandAddExample(command, "writeResource /000d6f000aae8410/ep/1/r/label \"Front Door\"");
    categoryAddCommand(cat, command);

    //execute resource
    command = commandCreate("execResource",
                            "er",
                            "<uri> [value]",
                            "execute a resource",
                            1,
                            2,
                            execResourceFunc);
    categoryAddCommand(cat, command);

    //query resources
    command = commandCreate("queryResources",
                            "qr",
                            "<uri pattern>",
                            "query resources with a pattern",
                            1,
                            1,
                            queryResourcesFunc);
    commandAddExample(command, "qr */lowBatt");
    categoryAddCommand(cat, command);

    //change resource mode (advanced)
    command = commandCreate("changeResourceMode",
                            NULL,
                            "<uri> <new mode value>",
                            "change modes of a resource",
                            2,
                            2,
                            changeResourceModeFunc);
    commandAddExample(command, "changeResourceMode /000d6f000aae8410/ep/1/r/label 0x3b");
    commandSetAdvanced(command);
    categoryAddCommand(cat, command);

    //read metadata
    command = commandCreate("readMetadata",
                            "rm",
                            "<uri>",
                            "read metadata",
                            1,
                            1,
                            readMetadataFunc);
    commandAddExample(command, "rm /000d6f000aae8410/m/lpmPolicy");
    categoryAddCommand(cat, command);

    //write metadata
    command = commandCreate("writeMetadata",
                            "wm",
                            "<uri>",
                            "write metadata",
                            1,
                            2,
                            writeMetadataFunc);
    commandAddExample(command, "wm /000d6f000aae8410/m/lpmPolicy never");
    categoryAddCommand(cat, command);

    //query metadata
    command = commandCreate("queryMetadata",
                            "qm",
                            "<uri pattern>",
                            "query metadata through a uri pattern",
                            1,
                            1,
                            queryMetadataFunc);
    commandAddExample(command, "qm */rejoins");
    categoryAddCommand(cat, command);

    //discover devices
    command = commandCreate("discoverStart",
                            "dstart",
                            "<device class>",
                            "Start discovery for a device class",
                            1,
                            1,
                            discoverStartFunc);
    categoryAddCommand(cat, command);

    //discover orphaned devices
    command = commandCreate("discoverRepairStart",
                            "drstart",
                            "<device class>",
                            "Start discovery for orphaned devices in a device class",
                            1,
                            1,
                            discoverRepairStartFunc);
    categoryAddCommand(cat, command);

    //stop discovering devices
    command = commandCreate("discoverStop",
                            "dstop",
                            NULL,
                            "Stop device discovery",
                            0,
                            0,
                            discoverStopFunc);
    categoryAddCommand(cat, command);

    //remove device
    command = commandCreate("removeDevice",
                            "rd",
                            "<uuid>",
                            "Remove a device by uuid",
                            1,
                            1,
                            removeDeviceFunc);
    categoryAddCommand(cat, command);

    //remove endpoint
    command = commandCreate("removeEndpoint",
                            "re",
                            "<uri>",
                            "Remove an endpoint by uri",
                            1,
                            1,
                            removeEndpointFunc);
    categoryAddCommand(cat, command);

    //remove devices (advanced)
    command = commandCreate("removeDevices",
                            NULL,
                            "[device class]",
                            "Remove devices (all or by class)",
                            0,
                            1,
                            removeDevicesFunc);
    commandSetAdvanced(command);
    categoryAddCommand(cat, command);

    //dump device
    command = commandCreate("dumpDevice",
                            "dd",
                            "<uuid>",
                            "Dump all details about a device",
                            1,
                            1,
                            dumpDeviceFunc);
    categoryAddCommand(cat, command);

    //dump devices
    command = commandCreate("dumpAllDevices",
                            "dump",
                            NULL,
                            "Dump all details about all devices",
                            0,
                            0,
                            dumpDevicesFunc);
    categoryAddCommand(cat, command);

    //system prop read
    command = commandCreate("readSystemProperty",
                            NULL,
                            "<key>",
                            "Read a device service system property",
                            1,
                            1,
                            readSystemPropertyFunc);
    categoryAddCommand(cat, command);

    //system prop write
    command = commandCreate("writeSystemProperty",
                            NULL,
                            "<key> [value]",
                            "Write a device service system property",
                            1,
                            2,
                            writeSystemPropertyFunc);
    categoryAddCommand(cat, command);

    //reload database (advanced)
    command = commandCreate("reloadDatabase",
                            NULL,
                            NULL,
                            "Instruct device service to reload its device database",
                            0,
                            0,
                            reloadDatabaseFunc);
    commandSetAdvanced(command);
    categoryAddCommand(cat, command);

    //device descriptor control
    command = commandCreate("ddl",
                            NULL,
                            "override <path> | clearoverride | process | bypass | clearbypass",
                            "Configure and control device descriptor processing",
                            1,
                            2,
                            ddlFunc);
    commandAddExample(command, "ddl override /opt/etc/WhiteList.xml.override");
    commandAddExample(command, "ddl clearoverride");
    commandAddExample(command, "ddl process");
    commandAddExample(command, "ddl bypass");
    commandAddExample(command, "ddl clearbypass");
    categoryAddCommand(cat, command);

    // get the status of device service
    command = commandCreate("getStatus",
                            "gs",
                            NULL,
                            "Get the status of device service",
                            0,
                            0,
                            getStatusFunc);
    categoryAddCommand(cat, command);

    return cat;
}
