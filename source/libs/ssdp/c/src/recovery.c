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

#include <pthread.h>
#include <string.h>

#include <icLog/logging.h>
#include <icConcurrent/timedWait.h>
#include <ssdp/ssdp.h>
#include <icUtil/macAddrUtils.h>
#include "discoverSearch.h"

/*-----------------------------------------------
 * recovery.c
 *
 * Implementation of the public 'ssdpRecoverIpAddress' function,
 * which uses ssdp to locate a device that was already paired to the system.
 * Generally called when communication with the IP-based device failed
 * and we are looking to see if DHCP lease expired and changed the IP address.
 *
 * Author: jelderton - 2/2/16
 *-----------------------------------------------*/

// object container for each recover requester
typedef struct _recoverSearch
{
    pthread_mutex_t mutex;      // allow call to ssdpRecoverIpAddress to be synchronous
    pthread_cond_t  cond;
    uint32_t        handle;     // "discover start" handle
    char            *macAddr;   // MAC we're looking for
    uint8_t macAddrBytes[6];    // MAC we're looking for, represented in bytes
    char            *ipAddr;    // IP of the discovered device with the same MAC address
} recoverSearch;

/*
 * private variables
 */
static pthread_mutex_t  ipRecoveryMutex = PTHREAD_MUTEX_INITIALIZER;
static icLinkedList     *recoverList = NULL;

/*
 * linkedList 'search' function.
 */
static bool findByMacAddress(void *searchVal, void *item)
{
    // the search val should be the "mac address byte array"
    //
    uint8_t *macAddress = (uint8_t *)searchVal;
    recoverSearch *node = (recoverSearch *)item;

    // compare this mac against the one in 'node'
    //
    if (macAddress != NULL && compareMacAddrs(macAddress, node->macAddrBytes) == 0)
    {
        // found the device this 'recoverSearch' was looking for
        //
        return true;
    }

    return false;
}

/*
 * callback from ssdp
 */
static void ipRecoveryCallback(SsdpDevice *device)
{
    icLogInfo(SSDP_LOG_TAG, "ssdpRecoverIpAddress: found %s at IP %s",
              (device->macAddress != NULL) ? device->macAddress : "unknown",
              (device->ipAddress != NULL) ? device->ipAddress : "unknown");

    // convert the macAddress string to an array of bytes
    //
    uint8_t macAddrBytes[6];
    if (macAddrToBytes((const char *)device->macAddress, macAddrBytes, true) == false)
    {
        // unable to convert the mac address to bytes
        //
        icLogWarn(SSDP_LOG_TAG, "ssdpRecoverIpAddress: device %s has invalid mac, unable to correlate to known devices",
              (device->ipAddress != NULL) ? device->ipAddress : "unknown");
        return;
    }

    // find the 'recoverSearch' object in our 'recoverList' that
    // has a matching mac address (byte comparison)
    //
    pthread_mutex_lock(&ipRecoveryMutex);
    recoverSearch *match = (recoverSearch *)linkedListFind(recoverList, macAddrBytes, findByMacAddress);
    pthread_mutex_unlock(&ipRecoveryMutex);

    if (match != NULL)
    {
        // save the IP address of the discovered device, then
        // notify the blocked 'ssdpRecoverIpAddress' function
        //
        icLogDebug(SSDP_LOG_TAG, "ssdpRecoverIpAddress: located device %s/%s seems to resolve our search for %s",
                   device->macAddress, device->ipAddress, match->macAddr);
        pthread_mutex_lock(&match->mutex);
        match->ipAddr = strdup(device->ipAddress);
        pthread_cond_broadcast(&match->cond);
        pthread_mutex_unlock(&match->mutex);
    }
    else
    {
        icLogDebug(SSDP_LOG_TAG, "ssdpRecoverIpAddress: located device %s/%s is not something we are searching for...",
                   device->macAddress, device->ipAddress);
    }
}

/*
 * cleanup
 */
static void destroyRecovery(recoverSearch *search)
{
    if (search != NULL)
    {
        if (search->macAddr != NULL)
        {
            free(search->macAddr);
        }
        if (search->ipAddr != NULL)
        {
            free(search->ipAddr);
        }
        pthread_mutex_destroy(&search->mutex);
        pthread_cond_destroy(&search->cond);
    }
}

/*
 * linkedList 'search' function.
 */
static bool findSearchObject(void *searchVal, void *item)
{
    // since we have the object, just do a pointer comparison
    //
    if (searchVal == item)
    {
        return true;
    }
    return false;
}

/*
 * linkedList 'delete' function
 */
static void deleteSearchObject(void *item)
{
    recoverSearch *tmp = (recoverSearch *)item;
    destroyRecovery(tmp);
}

/*
 * remove a search item from the linkedList
 */
static void cleanupList(recoverSearch *search)
{
    pthread_mutex_lock(&ipRecoveryMutex);
    linkedListDelete(recoverList, search, findSearchObject, deleteSearchObject);
    if (linkedListCount(recoverList) == 0)
    {
        linkedListDestroy(recoverList, NULL);
        recoverList = NULL;
    }
    pthread_mutex_unlock(&ipRecoveryMutex);
}

/*---------------------------------------------------------------------------------------
 * ssdpRecoverIpAddress : Attempt to locate a device matching the provided mac address via SSDP discovery.
 *
 * This function is typically used to locate devices that had their ip address change due to DHCP.
 * If the device is found, the ipAddress will point to the new/current ip address.
 *
 * @return true if the device was found
 *---------------------------------------------------------------------------------------*/
bool ssdpRecoverIpAddress(SearchType type, const char *macAddress, char **ipAddress, uint32_t timeoutSeconds)
{
    bool result = false;

    // sanity check
    //
    if (macAddress == NULL || strlen(macAddress) == 0)
    {
        return false;
    }
    icLogInfo(SSDP_LOG_TAG, "ssdpRecoverIpAddress: attempting recovery of %s", macAddress);

    // perform init (if necessary)
    //
    pthread_mutex_lock(&ipRecoveryMutex);
    if (recoverList == NULL)
    {
        recoverList = linkedListCreate();
    }

    // create a new 'recoverSearch' object
    //
    recoverSearch *search = (recoverSearch *)malloc(sizeof(recoverSearch));
    memset(search, 0, sizeof(recoverSearch));
    search->macAddr = strdup(macAddress);
    if (macAddrToBytes((const char *)macAddress, search->macAddrBytes, true) == false)
    {
        pthread_mutex_unlock(&ipRecoveryMutex);
        icLogError(SSDP_LOG_TAG, "%s: Unable to parse macAddress '%s'", __FUNCTION__, macAddress);
        cleanupList(search);
        return false;
    }

    // setup mutex & condition for this object
    //
    pthread_mutex_init(&search->mutex, NULL);
    initTimedWaitCond(&search->cond);

    // add this new search directive to our recoverList
    //
    linkedListAppend(recoverList, search);
    pthread_mutex_unlock(&ipRecoveryMutex);

    // start the discovery for this specific device
    //
    search->handle = ssdpDiscoverStart(type, ipRecoveryCallback);
    if (search->handle == 0)
    {
        // discover didn't start
        //
        icLogWarn(SSDP_LOG_TAG, "ssdpRecoverIpAddress: error starting the recovery of %s", macAddress);
        cleanupList(search);
        return false;
    }

    // wait around (up to 'timeoutSeconds') for the discover reply
    //
    pthread_mutex_lock(&search->mutex);
    incrementalCondTimedWait(&search->cond, &search->mutex, timeoutSeconds);

    // at this point we hit the timeout, or found the match
    // look at the ipAddress of 'search' to see if we found the device
    //
    if (search->ipAddr != NULL)
    {
        *ipAddress = search->ipAddr;
        search->ipAddr = NULL;
        result = true;
    }

    // stop discovery
    //
    pthread_mutex_unlock(&search->mutex);
    ssdpDiscoverStop(search->handle);

    // remove 'search' from our list
    //
    cleanupList(search);
    icLogInfo(SSDP_LOG_TAG, "ssdpRecoverIpAddress: completed search of %s, rc=%s", macAddress, (result == true) ? "true" : "false");

    return result;
}


