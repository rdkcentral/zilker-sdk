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
 * discoverSearch.h
 *
 * Define a set of input parameters used during a SSDP discovery.
 * Allows us to search for multiple targets concurrently.
 *
 * Author: jelderton - 1/28/16
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include "discoverSearch.h"

/*
 * helper function to allocate and clear a discoverSearch object
 *
 * @see destroyDiscoverSearch
 */
discoverSearch *createDiscoverSearch()
{
    discoverSearch *retVal = (discoverSearch *)malloc(sizeof(discoverSearch));
    if (retVal != NULL)
    {
        // clear mem & create the linked list
        //
        memset(retVal, 0, sizeof(discoverSearch));
        retVal->stList = linkedListCreate();
        retVal->processedList = linkedListCreate();
    }

    return retVal;
}

/*
 * helper to safely release memory associated with the discoverSearch object
 */
void destroyDiscoverSearch(discoverSearch *obj)
{
    if (obj != NULL)
    {
        if (obj->stList != NULL)
        {
            linkedListDestroy(obj->stList, NULL);
            obj->stList = NULL;
        }
        if (obj->processedList != NULL)
        {
            linkedListDestroy(obj->processedList, NULL);
            obj->processedList = NULL;
        }

        free(obj);
    }
}


/*
 * 'linkedListCompareFunc' implementation to search a linked list
 * for a particular ipAddress string.
 */
static bool findIpAddressInList(void *searchVal, void *item)
{
    // typecast the input vars
    //
    char *searchIpAddr = (char *)searchVal;
    char *currElement = (char *)item;

    if (currElement != NULL && searchIpAddr != NULL)
    {
        // NOTE: case-ignore compare for when we support IPV6 addresses
        //
        if (strcasecmp(searchIpAddr, currElement) == 0)
        {
            // found the host/ip we're looking for
            //
            return true;
        }
    }

    // no match
    //
    return false;
}

/*
 * helper function to see if an IP address is stored in the 'processedList'.
 */
bool discoverSearchDidProcessedIp(discoverSearch *obj, char *ipAddress)
{
    if (linkedListFind(obj->processedList, ipAddress, findIpAddressInList) != NULL)
    {
        // we have this 'ipAddress' string in our processedList
        //
        return true;
    }

    return false;
}

/*
 * add an IP address to the 'processedList' so that the device is not examined
 * again by this entity (until the next search attempt).
 */
void discoverSearchAddProcessedIp(discoverSearch *obj, char *ipAddress)
{
    if (ipAddress != NULL)
    {
        linkedListAppend(obj->processedList, strdup(ipAddress));
    }
}


