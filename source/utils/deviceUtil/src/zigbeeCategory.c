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

#include <stdlib.h>
#include <string.h>
#include <deviceService/deviceService_pojo.h>
#include <icIpc/ipcMessage.h>
#include <deviceService/deviceService_ipc.h>
#include <icUtil/stringUtils.h>
#include <icTypes/icStringBuffer.h>
#include <curl/curl.h>
#include <icTypes/icStringHashMap.h>
#include <icTypes/icSortedLinkedList.h>
#include "zigbeeCategory.h"

static bool isValidEui64(const char *str)
{
    uint64_t eui64 = 0;
    return stringToUnsignedNumberWithinRange(str, &eui64, 16, 0, UINT64_MAX);
}

static bool zigbeeStatusFunc(int argc,
                             char **argv)
{
    bool result = true;

    DSZigbeeSubsystemStatus *status = create_DSZigbeeSubsystemStatus();

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_GET_ZIGBEE_SUBSYSTEM_STATUS(status)) == IPC_SUCCESS)
    {
        printf("Zigbee Status:\n");
        printf("\tisAvailable: %s\n", status->isAvailable ? "true" : "false");
        printf("\tisUp: %s\n", status->isUp ? "true" : "false");
        printf("\tisOpenForJoin: %s\n", status->isOpenForJoin ? "true" : "false");
        printf("\teui64: %s\n", status->eui64);
        printf("\toriginalEui64: %s\n", status->originalEui64);
        printf("\tchannel: %d\n", status->channel);
        printf("\tPAN ID: 0x%04x\n", status->panId);
        printf("\tnetwork key: %s\n", status->networkKey);
    }
    else
    {
        fprintf(stderr, "Failed to get zigbee status: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }

    destroy_DSZigbeeSubsystemStatus(status);

    return result;
}

static bool dumpCountersFunc(int argc,
                             char **argv)
{
    bool result = true;

    char *response = NULL;
    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_GET_ZIGBEE_COUNTERS(&response)) == IPC_SUCCESS && response != NULL)
    {
        cJSON *json = cJSON_Parse(response);
        free(response);
        response = cJSON_Print(json);
        printf("%s\n", response);

        cJSON_Delete(json);
    }
    else
    {
        fprintf(stderr, "Failed to get zigbee counters: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        result = false;
    }

    free(response);

    return result;
}

size_t renderResponseFunc(void *ptr, size_t size, size_t nmemb, void *arg)
{
    // I probably should have just use a regular fifo buffer
    icStringBuffer *sb = (icStringBuffer *)arg;
    char *str = (char *)malloc(size*nmemb+1);
    memcpy(str, ptr, size*nmemb);
    str[size*nmemb] = '\0';
    stringBufferAppend(sb, str);
    free(str);

    return size*nmemb;
}

static const char *getLabelForDevice(const char *eui64, icStringHashMap *labelCache, icSortedLinkedList *resourceList)
{
    const char *retVal = stringHashMapGet(labelCache, eui64);
    if (retVal == NULL)
    {
        // Find the first non-null label for the device
        icLinkedListIterator *iter = linkedListIteratorCreate(resourceList);
        while(linkedListIteratorHasNext(iter) == true)
        {
            DSResource *res = (DSResource *)linkedListIteratorGetNext(iter);
            if (res->value != NULL && strstr(res->uri, eui64) != NULL)
            {
                char *label = stringBuilder("%s - %s", res->value, eui64);
                stringHashMapPut(labelCache, strdup(eui64), label);
                retVal = label;
                break;
            }
        }
        linkedListIteratorDestroy(iter);

        // Just use the plain eui64, no label found
        if (retVal == NULL)
        {
            stringHashMapPutCopy(labelCache, eui64, eui64);
            retVal = eui64;
        }
    }


    return retVal;
}

static int8_t resourceCompareFunc(void *newItem, void *element)
{
    DSResource *newResource = (DSResource *) newItem;
    DSResource *elementResource = (DSResource *) element;
    return stringCompare(newResource->uri, elementResource->uri, false);
}

static char *formatMapAsDot(DSZigbeeNetworkMap *map)
{
    // Get all label resources for pretty names
    DSResourceList *resourceList = create_DSResourceList();
    deviceService_request_QUERY_RESOURCES_BY_URI("*/label", resourceList);

    // TODO Make it sorted, this just helps for PIM/PRM where you have multiple labels, and hopefully gets the PIM/PRM
    // label rather than a zone
    icSortedLinkedList *sortedResources = sortedLinkedListCreate();

    icLinkedListIterator *resourceIter = linkedListIteratorCreate(resourceList->resourceList);
    while (linkedListIteratorHasNext(resourceIter))
    {
        DSResource *resource = (DSResource *) linkedListIteratorGetNext(resourceIter);
        sortedLinkedListAdd(sortedResources, resource, resourceCompareFunc);
    }
    linkedListIteratorDestroy(resourceIter);

    // For local device's eui64
    DSZigbeeSubsystemStatus *zigbeeStatus = create_DSZigbeeSubsystemStatus();
    deviceService_request_GET_ZIGBEE_SUBSYSTEM_STATUS(zigbeeStatus);

    // Keep a cache of resolved labels
    icStringHashMap *labelCache = stringHashMapCreate();
    if (zigbeeStatus->eui64 != NULL)
    {
        char *label = stringBuilder("Touchscreen - %s", zigbeeStatus->eui64);
        stringHashMapPut(labelCache, strdup(zigbeeStatus->eui64), label);
    }

    // Build the dot file
    icStringBuffer *sb = stringBufferCreate(1024);
    stringBufferAppend(sb, "digraph {\n");
    stringBufferAppend(sb, "rankdir=TB;\n\n");
    icLinkedListIterator *iter = linkedListIteratorCreate(map->entries);
    while (linkedListIteratorHasNext(iter))
    {
        DSZigbeeNetworkMapEntry *item = (DSZigbeeNetworkMapEntry *) linkedListIteratorGetNext(iter);

        char *entryStr = stringBuilder("\"%s\" -> \"%s\" [ label = \"%d\" ];\n",
                                       getLabelForDevice(item->address, labelCache, sortedResources),
                                       getLabelForDevice(item->nextCloserHop, labelCache, sortedResources), item->lqi);
        stringBufferAppend(sb, entryStr);
        free(entryStr);
    }
    linkedListIteratorDestroy(iter);
    stringBufferAppend(sb, "}");

    char *dot = stringBufferToString(sb);

    // Cleanup
    stringBufferDestroy(sb);
    destroy_DSResourceList(resourceList);
    destroy_DSZigbeeSubsystemStatus(zigbeeStatus);
    stringHashMapDestroy(labelCache, NULL);
    linkedListDestroy(sortedResources, standardDoNotFreeFunc);

    return dot;
}

static void renderZigbeeNetworkMap(char *dot)
{
    // Call the webservice to translate to ascii art
    CURL *curl = curl_easy_init();
    if(curl != NULL)
    {
        //printf("src=%s\n",srcArg);
        char *encodedSrcArg = curl_easy_escape(curl, dot, strlen(dot));
        // TODO would be nice if we could host this ourselves, its open source: https://github.com/ggerganov/dot-to-ascii
        char *url = stringBuilder("https://dot-to-ascii.ggerganov.com/dot-to-ascii.php?boxart=1&src=%s", encodedSrcArg);
        free(encodedSrcArg);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, renderResponseFunc);
        icStringBuffer *response = stringBufferCreate(1024);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "Failed fetching rendered graph: %s\n",
                    curl_easy_strerror(res));
        }
        else
        {
            char *out = stringBufferToString(response);
            printf("%s\n",out);
            free(out);
        }

        stringBufferDestroy(response);
        curl_easy_cleanup(curl);
        free(url);
    }
}

static bool zigbeeNetworkMapFunc(int argc,
                                 char **argv)
{
    bool success = false;
    bool dumpDot = false;
    bool render = false;
    if (argc == 1)
    {
        if (strcmp(argv[0],"-d") == 0)
        {
            dumpDot = true;
        }
        if (strcmp(argv[0],"-r") == 0)
        {
            render = true;
        }

    }

    DSZigbeeNetworkMap *map = create_DSZigbeeNetworkMap();

    IPCCode ipcRc;
    if ((ipcRc = deviceService_request_GET_ZIGBEE_NETWORK_MAP(map)) == IPC_SUCCESS)
    {
        success = true;

        if (dumpDot == true)
        {
            char *dot = formatMapAsDot(map);
            printf("%s\n", dot);
            free(dot);
        }
        else if (render == true)
        {
            char *dot = formatMapAsDot(map);
            renderZigbeeNetworkMap(dot);
            free(dot);
        }
        else
        {
            printf("EUI64            Next Hop EUI64   LQI\n");
            printf("---------------- ---------------- ----\n");

            icLinkedListIterator *iter = linkedListIteratorCreate(map->entries);
            while (linkedListIteratorHasNext(iter))
            {
                DSZigbeeNetworkMapEntry *item = (DSZigbeeNetworkMapEntry *) linkedListIteratorGetNext(iter);
                printf("%s %s %d\n", item->address, item->nextCloserHop, item->lqi);
            }
            linkedListIteratorDestroy(iter);
        }
    }
    else
    {
        fprintf(stderr, "Failed to set zigbee channel: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    destroy_DSZigbeeNetworkMap(map);

    return success;
}

static bool zigbeeChangeChannelFunc(int argc,
                                    char **argv)
{
    bool success = false;

    IPCCode ipcRc;

    DSZigbeeChangeChannelRequest *request = create_DSZigbeeChangeChannelRequest();
    if (stringToInt16(argv[0], &request->channel) == false)
    {
        printf("Invalid channel\n");
        destroy_DSZigbeeChangeChannelRequest(request);
        return false;
    }

    if (argc == 2 && stringCompare(argv[1], "dryrun", true) == 0)
    {
        request->dryRun = true;
        printf("This will be a dry run\n");
    }

    DSZigbeeChangeChannelResponse *response = create_DSZigbeeChangeChannelResponse();

    if((ipcRc = deviceService_request_CHANGE_ZIGBEE_CHANNEL_timeout(request, response, 60)) == IPC_SUCCESS)
    {
        switch(response->status)
        {
            case CHANNEL_CHANGE_STATUS_SUCCESS:
                printf("channel change successfully requested\n");
                break;

            case CHANNEL_CHANGE_STATUS_FAILED:
                printf("channel change request failed\n");
                break;

            case CHANNEL_CHANGE_STATUS_NOT_ALLOWED:
                printf("channel change not allowed\n");
                break;

            case CHANNEL_CHANGE_STATUS_INVALID_CHANNEL:
                printf("invalid channel\n");
                break;

            case CHANNEL_CHANGE_STATUS_IN_PROGRESS:
                printf("channel change already in progress\n");
                break;

            case CHANNEL_CHANGE_STATUS_FAILED_TO_CALCULATE:
                printf("failed to scan/calculate new channel number\n");
                break;

            case CHANNEL_CHANGE_STATUS_UNKNOWN:
            default:
                printf("unsupported status code received (%d)\n", response->status);
                break;
        }
    }
    else
    {
        fprintf(stderr, "Failed to request zigbee channel change: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    destroy_DSZigbeeChangeChannelRequest(request);
    destroy_DSZigbeeChangeChannelResponse(response);

    return success;
}

static bool zigbeeEnergyScanFunc(int argc,
                                 char **argv)
{
    bool success = true;

    //valid args are none or more than 3
    if (argc != 0 && argc < 3)
    {
        fprintf(stderr, "Invalid arguments\n");
        return false;
    }

    DSZigbeeEnergyScanRequest *request = create_DSZigbeeEnergyScanRequest();

    if (argc == 0)
    {
        //apply defaults from legacy
        request->durationMs = 30;
        request->numScans = 16;
        put_channel_in_DSZigbeeEnergyScanRequest_channels(request, 15);
        put_channel_in_DSZigbeeEnergyScanRequest_channels(request, 19);
        put_channel_in_DSZigbeeEnergyScanRequest_channels(request, 20);
        put_channel_in_DSZigbeeEnergyScanRequest_channels(request, 25);
    }
    else
    {
        //get from argv
        success &= stringToInt32(argv[0], &request->durationMs);
        success &= stringToInt32(argv[1], &request->numScans);

        if (success == true)
        {
            for (int i = 2; i < argc; i++)
            {
                int32_t channel = 0;
                success &= stringToInt32(argv[i], &channel);
                if (success == false)
                {
                    break;
                }

                put_channel_in_DSZigbeeEnergyScanRequest_channels(request, channel);
            }
        }
    }

    if (success == true)
    {
        IPCCode ipcRc;
        DSZigbeeEnergyScanResponse *response = create_DSZigbeeEnergyScanResponse();
        if((ipcRc = deviceService_request_ZIGBEE_ENERGY_SCAN_timeout(request, response, 60)) == IPC_SUCCESS)
        {
            icLinkedListIterator *it = linkedListIteratorCreate(response->scanResults);
            while(linkedListIteratorHasNext(it))
            {
                DSZigbeeEnergyScanResult *scanResult = linkedListIteratorGetNext(it);
                printf("EnergyData [chan=%d, max=%d, min=%d, avg=%d, score=%d]\n",
                        scanResult->channel,
                        scanResult->maxRssi,
                        scanResult->minRssi,
                        scanResult->averageRssi,
                        scanResult->score);
            }
            linkedListIteratorDestroy(it);
        }
        else
        {
            printf("Scan failed (invalid input?)\n");
            success = false;
        }
        destroy_DSZigbeeEnergyScanResponse(response);
    }
    else
    {
        printf("Invalid input\n");
    }

    destroy_DSZigbeeEnergyScanRequest(request);

    return success;
}

static bool zigbeeRequestLeaveFunc(int argc,
                                   char **argv)
{
    bool success = false;

    IPCCode ipcRc;

    if (!isValidEui64(argv[0]))
    {
        printf("Invalid eui64: %s\n", argv[0]);
        return false;
    }

    DSZigbeeRequestLeave *request = create_DSZigbeeRequestLeave();

    request->eui64 = strdup(argv[0]);
    request->withRejoin = false;
    request->isEndDevice = false;

    for (int i = 1; i < argc; i++)
    {
        if (stringCompare(argv[i], "rejoin", true) == 0)
        {
            request->withRejoin = true;
            fprintf(stderr, "zigbeeRequestLeave: Rejoin after leaving\n");
        }
        else if (stringCompare(argv[i], "endDevice", true) == 0)
        {
            request->isEndDevice = true;
            fprintf(stderr, "zigbeeRequestLeave: Is end device\n");
        }
        else 
        {
            fprintf(stderr, "zigbeeRequestLeave: Ignoring invalid argument '%s'\n", argv[i]);
        }
    }

    if ((ipcRc = deviceService_request_ZIGBEE_TEST_REQUEST_LEAVE(request)) == IPC_SUCCESS)
    {
        success = true;
    }
    else
    {
        fprintf(stderr, "zigbeeRequestLeave: Failed IPC request: %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
    }

    destroy_DSZigbeeRequestLeave(request);

    return success;
}

Category *buildZigbeeCategory(void)
{
    Category *cat = categoryCreate("Zigbee", "Zigbee specific commands");

    //get zigbee system status
    Command *command = commandCreate("zigbeeStatus",
                                     "zs",
                                     NULL,
                                     "get the status of the zigbee subsystem",
                                     0,
                                     0,
                                     zigbeeStatusFunc);
    categoryAddCommand(cat, command);

    //dump zigbee counters
    command = commandCreate("zigbeeDumpCounters",
                            NULL,
                            NULL,
                            "dump the current zigbee counters",
                            0,
                            0,
                            dumpCountersFunc);
    categoryAddCommand(cat, command);

    //print zigbee network map
    command = commandCreate("zigbeeNetworkMap",
                            NULL,
                            NULL,
                            "Print the Zigbee network map",
                            0,
                            1,
                            zigbeeNetworkMapFunc);
    commandAddExample(command, "zigbeeNetworkMap");
    commandAddExample(command, "zigbeeNetworkMap -d");
    commandAddExample(command, "zigbeeNetworkMap -r");
    categoryAddCommand(cat, command);

    //change the zigbee channel
    command = commandCreate("zigbeeSetChannel",
                            NULL,
                            "<channel number or 0 to pick best> [dryrun]",
                            "Change the Zigbee channel",
                            1,
                            2,
                            zigbeeChangeChannelFunc);
    commandAddExample(command, "zigbeeSetChannel 25");
    commandAddExample(command, "zigbeeSetChannel 0 dryrun");
    categoryAddCommand(cat, command);

    //perform an energy scan
    command = commandCreate("zigbeeEnergyScan",
                            NULL,
                            "[<duration mS> <number of scans> <channels to scan...>]",
                            "Scan for background noise on Zigbee channels",
                            0,
                            -1,
                            zigbeeEnergyScanFunc);
    commandAddExample(command, "zigbeeEnergyScan");
    commandAddExample(command, "zigbeeEnergyScan 30 16 11 20 25");
    categoryAddCommand(cat, command);

    // request leave
    command = commandCreate("zigbeeRequestLeave",
                            NULL,
                            "<EUI64> [rejoin] [endDevice]",
                            "Send the Leave command for the specified device",
                            1,
                            3,
                            zigbeeRequestLeaveFunc);
    commandAddExample(command, "zigbeeRequestLeave 001bad19a700af6e");
    commandAddExample(command, "zigbeeRequestLeave 001bad19a700af6e rejoin");
    commandAddExample(command, "zigbeeRequestLeave 001bad19a700af6e rejoin endDevice");
    commandSetAdvanced(command);
    categoryAddCommand(cat, command);

    return cat;
}
