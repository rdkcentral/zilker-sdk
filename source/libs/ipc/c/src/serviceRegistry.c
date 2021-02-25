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
 * ServiceRegistry.c
 *
 * Registry of known services.  Primarily it will
 * contain each running  on the local host, but
 * attempts to keep in sync with other services
 * running within the local premise.
 *
 * Used by IPC and Event-listeners to determine the
 * targeted service location  (same host, same network, etc)
 * to optimize the calls and skip marshaling if possible.
 *
 * Note: this correlates closely to ServiceRegistry.java
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <icTypes/icLinkedList.h>
#include "serviceRegistry.h"

// since we only support one upgrade running at a time,
// simply store the local variable and re-use it
//
static icLinkedList *registry = NULL;       // TODO: move to use Hash if applicable
static pthread_mutex_t REG_MTX = PTHREAD_MUTEX_INITIALIZER;

// forward declare the 'init'
static void initRegistry();

// search functions for the linkedList
bool searchByIpcPort(void *searchVal, void *item);
bool searchByEventPort(void *searchVal, void *item);

// TODO: need eventListener to monitor for other registry events and keep in sync
// TODO: need to broadcast events when registry changes so others can get changes
// TODO: move this into a separate service, so that there is only 1 per host.
// TODO: it could then be used as a proxy to encrypt data to/from the various hosts

/*
 * convenience function to create and clear a
 * serviceHandle instance (as well as default the
 * address to LOCAL_LOOPBACK
 */
serviceHandle *createServiceHandle()
{
    // create and clear
    //
    serviceHandle *retVal = (serviceHandle *)malloc(sizeof(serviceHandle));
    memset(retVal, 0, sizeof(serviceHandle));

    // assign IP to LOCAL_LOOPBACK
    //
    strcpy(retVal->serviceAddress, LOCAL_LOOPBACK);
    return retVal;
}

/*
 * convience function to destroy a serviceHandle
 */
void destroyServiceHandle(serviceHandle *handle)
{
    if (handle != NULL)
    {
        free(handle);
    }
}

/*
 * Called by a Service to register it's information
 * to the registry.  Used to track the available services
 * and their address.
 */
void registerService(serviceHandle *handle)
{
    if (handle == NULL)
    {
        return;
    }

    // make sure our internal variables are setup
    //
    initRegistry();



    // TODO: see if we already have this service in the list and just update if so



    // obtain the lock, then append this handle to our list
    //
    pthread_mutex_lock(&REG_MTX);
    linkedListAppend(registry, handle);
    pthread_mutex_unlock(&REG_MTX);
}

/*
 * Return the Service that is registered for this IPC port.
 * Can return NULL if the service is not known.  Note that the
 * caller should NOT free this memory.
 */
serviceHandle *getServiceForIpcPort(uint16_t ipcPort)
{
    serviceHandle *retVal = NULL;

    pthread_mutex_lock(&REG_MTX);
    if (registry != NULL)
    {
        // ask the linked list to find the handle by searching for
        // a matching ipcPort
        //
        retVal = (serviceHandle *) linkedListFind(registry, &ipcPort, searchByIpcPort);
    }
    pthread_mutex_unlock(&REG_MTX);

    return retVal;
}

/*
 * Return the Service that is registered for this Event port.
 * Can return NULL if the service is not known.  Note that the
 * caller should NOT free this memory.
 */
serviceHandle *getServiceForEventPort(uint16_t eventPort)
{
    serviceHandle *retVal = NULL;

    pthread_mutex_lock(&REG_MTX);
    if (registry != NULL)
    {
        // ask the linked list to find the handle by searching for
        // a matching ipcPort
        //
        retVal = (serviceHandle *) linkedListFind(registry, &eventPort, searchByEventPort);
    }
    pthread_mutex_unlock(&REG_MTX);

    return retVal;
}

/*
 * Return the serviceAddress to use for a particular IPC port
 * If the service is not registered, assumes LOCAL_LOOPBACK
 */
const char *getServiceAddressForIpcPort(uint16_t ipcPort)
{
    const char *retVal = NULL;

    pthread_mutex_lock(&REG_MTX);
    if (registry != NULL)
    {
        // ask the linked list to find the handle by searching for
        // a matching ipcPort, then grab the address
        //
        serviceHandle *handle = (serviceHandle *) linkedListFind(registry, &ipcPort, searchByIpcPort);
        if (handle != NULL)
        {
            retVal = handle->serviceAddress;
        }
    }
    pthread_mutex_unlock(&REG_MTX);

    // return what we found
    //
    if (retVal != NULL)
    {
        return retVal;
    }

    // not found, assume local
    //
    return LOCAL_LOOPBACK;
}

/*
 * Return the serviceAddress to use for a particular Event port
 * If the service is not registered, assumes LOCAL_LOOPBACK
 */
const char *getServiceAddressForEventPort(uint16_t eventPort)
{
    const char *retVal = NULL;

    pthread_mutex_lock(&REG_MTX);
    if (registry != NULL)
    {
        // ask the linked list to find the handle by searching for
        // a matching eventPort, then grab the address
        //
        serviceHandle *handle = (serviceHandle *) linkedListFind(registry, &eventPort, searchByEventPort);
        if (handle != NULL)
        {
            retVal = handle->serviceAddress;
        }
    }
    pthread_mutex_unlock(&REG_MTX);

    // return what we found
    //
    if (retVal != NULL)
    {
        return retVal;
    }

    // not found, assume local
    //
    return LOCAL_LOOPBACK;
}

/*
 *
 */
static void initRegistry()
{
    pthread_mutex_lock(&REG_MTX);
    if (registry == NULL)
    {
        registry = linkedListCreate();
    }
    pthread_mutex_unlock(&REG_MTX);
}

/*
 *
 */
bool searchByIpcPort(void *searchVal, void *item)
{
    // assume 'searchVal' is a short and 'item' is a serviceHandle
    //
    uint16_t *port = (uint16_t *)searchVal;
    serviceHandle *handle = (serviceHandle *)item;

    // compare the ipc port to the search port
    //
    if (handle->ipcPort == *port)
    {
        return true;
    }
    return false;
}

/*
 *
 */
bool searchByEventPort(void *searchVal, void *item)
{
    // assume 'searchVal' is a short and 'item' is a serviceHandle
    //
    uint16_t *port = (uint16_t *)searchVal;
    serviceHandle *handle = (serviceHandle *)item;

    // compare the ipc port to the search port
    //
    if (handle->eventPort == *port)
    {
        return true;
    }
    return false;
}
