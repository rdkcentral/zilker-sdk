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
 * eventConsumer.c
 *
 * Set of functions to listen for multicast events
 * and forward to a listener to be decoded and processed.
 *
 * Closely mirrors the BaseJavaEventAdapter.java so that events
 * are process the same regardless of Native or Java implementation.
 *
 * Note: this correlates to BaseJavaEventAdapter.java
 *
 * Authors: jelderton, gfaulkner, tlea
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <icBuildtime.h>
#include <icTypes/icLinkedList.h>
#include <icConcurrent/threadPool.h>
#include <icLog/logging.h>
#include <icIpc/eventConsumer.h>
#include <icIpc/baseEvent.h>
#include <icUtil/stringUtils.h>
#include <icConcurrent/threadUtils.h>
#include "ipcCommon.h"
#include "transport/transport.h"

#define MAX_EVENT_HANDLERS 2

#undef DEBUG_IPC_DISPLAY_EVENTS

/*
 * object saved in the 'adapterList'.  represents an 'adapter'
 * for a particular service identifier.
 */
typedef struct _eventAdapter
{
    uint32_t             serviceIdNum;  // service id this adapter represents
    eventListenerAdapter handler;       // function to call to transcode and distribute events from this service
} eventAdapter;

/*
 * container to use when a raw JSON event is received.
 * this is what will be added to the threadPool to be
 * processed in the background (see processRawEventTask() function)
 */
typedef struct _rawEventContainer
{
    int32_t eventCode;
    int32_t eventValue;
    cJSON   *jsonPayload;           // not a copy, the original created during the socket 'read'
    eventListenerAdapter handler[MAX_EVENT_HANDLERS];  // eventWorkerArgs->handler to use
} rawEventContainer;

/* private function declarations */
static void internalShutdown();
static bool startReaderThread();
static void *eventReaderThread(void *args);
static void processRawJsonEvent(cJSON *json);
static void processRawEventTask(void *args);
static bool findAdapterById(void *searchVal, void *item);
static void processRawEventTaskArgFreeFunc(void *arg);

/* private variable declarations */
static icLinkedList *adapterList = NULL;                        // linked list of eventAdapter items (one item per service to listen for)
static pthread_mutex_t LIST_MTX = PTHREAD_MUTEX_INITIALIZER;    // mutex for the adapterList
static pthread_cond_t readThreadCond = PTHREAD_COND_INITIALIZER;
static icThreadPool    *eventThreadPool;                        // thread pool to background parsing and notification of events
static bool       running = false;
static int        readSock = -1;
static icHashMap *serviceSpecificPoolMap = NULL; // Map of serviceId->threadPool
static pthread_mutex_t SERVICE_POOL_MAP_MTX = PTHREAD_MUTEX_INITIALIZER;

static eventAdapter* subscribeAllAdapter = NULL;

/*
 * register the supplied 'handler' to be notified when an event comes in from
 * the service with a matching 'serviceIdNum'.  the 'handler' receives raw events
 * that need to be decoded and internally broadcasted.
 *
 * @param serviceIdNum - identifier of the service we want events from
 * @param handler - function to call as events from the service are received
 * @return false if there is a problem
 */
bool startEventListener(uint16_t serviceIdNum, eventListenerAdapter handler)
{
    icLogDebug(API_LOG_CAT, "initializing event listener for service %" PRIu32, serviceIdNum);

    // see if an eventAdapter already exists for this service
    //
    pthread_mutex_lock(&LIST_MTX);
    if (adapterList != NULL)
    {
        // look for an adapter already monitoring for events from this serviceId
        //
        if (linkedListFind(adapterList, &serviceIdNum, findAdapterById) != NULL)
        {
            icLogDebug(API_LOG_CAT, "already listening for events from service %" PRIu32, serviceIdNum);
            pthread_mutex_unlock(&LIST_MTX);
            return false;
        }
    }
    else
    {
        // first time, create the linked list
        //
        adapterList = linkedListCreate();
    }

    // not monitoring this 'serviceId' yet, so create the container and add
    // to the list (since we already have the mutex held)
    //
    eventAdapter *impl = (eventAdapter *) malloc(sizeof(eventAdapter));
    impl->serviceIdNum = serviceIdNum;
    impl->handler = handler;

    if (serviceIdNum == EVENTCONSUMER_SUBSCRIBE_ALL) {
        if (subscribeAllAdapter != NULL) {
            free(subscribeAllAdapter);
        }

        subscribeAllAdapter = impl;
    } else {
        linkedListAppend(adapterList, impl);
    }

    // create our thread pool (if needed)
    //
    if (eventThreadPool == NULL)
    {
        // originally we used min=3, max=6.  after stats collection, seems
        // that we rarely need more then 1 or 2 threads for event processing.
        // therefore putting the min=1 to reduce idle overhead
        //
        char *poolName = stringBuilder("evTP:%"PRIu16, serviceIdNum);
        eventThreadPool = threadPoolCreate(poolName, 1, 6, MAX_QUEUE_SIZE);
        free(poolName);
    }

    // create our reader thread (if needed)
    //
    if (running == false)
    {
        startReaderThread();
    }
    pthread_mutex_unlock(&LIST_MTX);

    return true;
}

/*
 * un-register the handlers associated with 'serviceIdNum'.  assume that this is called
 * by the handler because it has no listeners to inform.  if this is the last handler,
 * will cleanup and close the event socket.
 *
 * @param serviceIdNum - identifier of the service we want events from
 */
void stopEventListener(uint16_t serviceIdNum)
{
    icLogDebug(API_LOG_CAT, "stopping event listener for service %" PRIu32, serviceIdNum);

    // find the worker in our list that matches this port, and remove it
    //
    pthread_mutex_lock(&LIST_MTX);
    if (adapterList != NULL)
    {
        linkedListDelete(adapterList, &serviceIdNum, findAdapterById, NULL);

        // if the list is empty, then perform our 'shutdown'
        //
        if (linkedListCount(adapterList) == 0)
        {
            internalShutdown();
        }
    }
    pthread_mutex_unlock(&LIST_MTX);
}

/*
 * force-close the event listener thread and socket.
 * Generally called during shutdown.
 */
void shutdownEventListener()
{
    pthread_mutex_lock(&LIST_MTX);
    internalShutdown();
    pthread_mutex_unlock(&LIST_MTX);
}

/*
 * assume mutex is held
 */
static void internalShutdown()
{
    // kill event reader
    //
    if (running == true)
    {
        running = false;
        transport_close(readSock);
        readSock = -1;

        pthread_cond_wait(&readThreadCond, &LIST_MTX);

    }

    // kill thread pool
    //
    if (eventThreadPool != NULL)
    {
        threadPoolDestroy(eventThreadPool);
        eventThreadPool = NULL;
    }

    // remove all handlers (use standard memory release mechanism)
    //
    if (adapterList != NULL)
    {
        linkedListDestroy(adapterList, NULL);
        adapterList = NULL;
    }
}

/*
 * collect statistics about the event listeners, and populate
 * them into the supplied runtimeStatsPojo container
 */
void collectEventStatistics(runtimeStatsPojo *container, bool thenClear)
{
    // for now, just get the thread pool stats
    //
    if (eventThreadPool != NULL)
    {
        threadPoolStats *tstats = threadPoolGetStatistics(eventThreadPool, thenClear);
        if (tstats != NULL)
        {
            // transfer each value into the container
            //
            put_long_in_runtimeStatsPojo(container, "eventTpoolTotalRan", tstats->totalTasksRan);
            put_long_in_runtimeStatsPojo(container, "eventTpoolTotalQueued", tstats->totalTasksQueued);
            put_long_in_runtimeStatsPojo(container, "eventTpoolMaxQueued", tstats->maxTasksQueued);
            put_long_in_runtimeStatsPojo(container, "eventTpoolMaxConcurrent", tstats->maxConcurrentTasks);
            free(tstats);
        }
    }
}

/*
 * Register a specific thread pool to handle events from a service
 */
bool registerServiceSpecificEventHandlerThreadPool(uint16_t serviceIdNum, icThreadPool *threadPool)
{
    bool success = false;
    pthread_mutex_lock(&SERVICE_POOL_MAP_MTX);
    if (serviceSpecificPoolMap == NULL)
    {
        serviceSpecificPoolMap = hashMapCreate();
    }
    if (hashMapContains(serviceSpecificPoolMap, &serviceIdNum, sizeof(serviceIdNum)) == false)
    {
        uint16_t *key = (uint16_t *)malloc(sizeof(uint16_t));
        *key = serviceIdNum;
        success = hashMapPut(serviceSpecificPoolMap, key, sizeof(uint16_t), threadPool);
    }
    pthread_mutex_unlock(&SERVICE_POOL_MAP_MTX);

    return success;
}

static void serviceSpecificPoolFreeFunc(void *key, void *value)
{
    free(key);
    icThreadPool *pool = (icThreadPool *)value;
    threadPoolDestroy(pool);
}

/*
 * Unregister a specific thread pool to handle events from a service
 */
bool unregisterServiceSpecificEventHandlerThreadPool(uint16_t serviceIdNum)
{
    pthread_mutex_lock(&SERVICE_POOL_MAP_MTX);
    bool success = hashMapDelete(serviceSpecificPoolMap, &serviceIdNum, sizeof(serviceIdNum),
                                 serviceSpecificPoolFreeFunc);
    if (hashMapCount(serviceSpecificPoolMap) == 0)
    {
        hashMapDestroy(serviceSpecificPoolMap, serviceSpecificPoolFreeFunc);
        serviceSpecificPoolMap = NULL;
    }
    pthread_mutex_unlock(&SERVICE_POOL_MAP_MTX);

    return success;
}

/*
 * internal, assumes mutex is held
 */
static bool startReaderThread()
{
    int sockFD = transport_sub_register(TRANSPORT_DEFAULT_PUBSUB);
    if (sockFD < 0)
    {
        icLogError(API_LOG_CAT, "unable to create event listening socket : %s", strerror(errno));
        return false;
    }

    /* Currently the only thing supported is subscribe all. This is
     * due to the underlying transports not supporting filtering very well.
     *
     * TODO: This should be revisited with transport layer filtering enabled.
     */
    transport_subscribe(sockFD, TRANSPORT_SUBSCRIBE_ALL);

    // save the socket we just opened and set state to 'run'
    //
    readSock = sockFD;
    running = true;

    // start the reader thread
    //
    /*
     * This little wonderful piece of code is to silence the -Wint-to-void-pointer warning.
     * We know we are casting this thing that way, and even want to!
     */
    createDetachedThread(eventReaderThread, (void *) ((unsigned long) sockFD), "eventReader");

    return true;
}

/*
 * used when searching our adapterList
 */
static bool findAdapterById(void *searchVal, void *item)
{
    // searchVal should be the serviceIdNum we supplied
    // item should be an eventAdapter struct
    //
    uint16_t serviceIdNum = *((uint16_t *)searchVal);
    eventAdapter *worker = (eventAdapter *)item;

    // ideally we should mutex lock on this item, but
    // simply comparing so probably not worth the overhead
    // note that the list itself has a mutex lock
    //
    if (worker->serviceIdNum == serviceIdNum)
    {
        // found match
        //
        return true;
    }

    // not the one we're looking for
    //
    return false;
}

static void* eventReaderThread(void* args)
{
    int sockfd = readSock;
    int shutdownfd = transport_get_shutdown_sock_readfd(sockfd);
    bool is_running;

    pthread_mutex_lock(&LIST_MTX);
    is_running = running;
    pthread_mutex_unlock(&LIST_MTX);

    while (is_running)
    {
        cJSON* json = NULL;

        if (canReadFromServiceSocket(sockfd, shutdownfd, 10) != 0)
        {
            // loop back around
            pthread_mutex_lock(&LIST_MTX);
            is_running = running;
            pthread_mutex_unlock(&LIST_MTX);
            continue;
        }

        if (transport_sub_recv(sockfd, &json) < 0)
        {
            is_running = false;
            continue;
        }

        // process the json and potentially deliver to listeners.
        // this will destroy our json object on completion.
        //
        processRawJsonEvent(json);

        pthread_mutex_lock(&LIST_MTX);
        is_running = running;
        pthread_mutex_unlock(&LIST_MTX);
    }

    icLogInfo(API_LOG_CAT, "event receiver thread is exiting");

    pthread_mutex_lock(&LIST_MTX);
    running = false;

    if (readSock != -1) {
        readSock = -1;
        transport_close(sockfd);
    }
    pthread_cond_broadcast(&readThreadCond);
    pthread_mutex_unlock(&LIST_MTX);

    return NULL;
}

/*
 * extract sender from the json structure to see if this is event
 * can be decoded and forwarded to any listeners.
 *
 * this will destroy the json object on completion
 */
static void processRawJsonEvent(cJSON *json)
{
    if (json == NULL)
    {
        return;
    }

#ifdef DEBUG_IPC_DISPLAY_EVENTS
    char *eventJsonString = cJSON_Print(json);
    icLogDebug(API_LOG_CAT, "eventReaderThread(): event=%s", eventJsonString);
    free(eventJsonString);
#endif

    // extract the serviceIdNum, to identify what service this came from
    //
    uint32_t sourceServiceId = extractServiceIdFromRawEvent(json);
    icThreadPool *pool = eventThreadPool;

    // Check if we have a service specific pool to use
    pthread_mutex_lock(&SERVICE_POOL_MAP_MTX);
    if (serviceSpecificPoolMap != NULL)
    {
        icThreadPool *serviceSpecificPool = (icThreadPool *) hashMapGet(serviceSpecificPoolMap,
                                                                        &sourceServiceId,
                                                                        sizeof(uint16_t));
        if (serviceSpecificPool != NULL)
        {
            pool = serviceSpecificPool;
        }
    }

    // find the eventAdapter for this serviceIdNum
    //
    pthread_mutex_lock(&LIST_MTX);
    eventAdapter *adapter = (eventAdapter *) linkedListFind(adapterList, &sourceServiceId, findAdapterById);
    if ((adapter == NULL) && (subscribeAllAdapter == NULL))
    {
        // not listening for events from this service.  move along...
        //
#ifdef DEBUG_IPC_DEEP
        icLogWarn(API_DEEP_LOG_CAT, "received event from service %"PRIu32"; no adapters registered so unable to deliver %s", sourceServiceId, ptr);
#endif
        cJSON_Delete(json);
        json = NULL;
    }
    else
    {
        // extract BaseEvent information from the JSON buffer so we know the code & value
        //
        BaseEvent event = {
                .base.context = NULL
        };
        if (baseEvent_fromJSON(&event, json) == 0)
        {
            int index = 0;
#ifdef CONFIG_DEBUG_IPC
            icLogDebug(API_LOG_CAT, "received event; service=%" PRIu32 " code=%" PRIu32 " value=%" PRIu32,
                    sourceServiceId, event.eventCode, event.eventValue);
#endif
            // pass to all of the information to the handler so it can
            // properly decode and process the event.  do this via our
            // eventThreadPool so we can quickly loop back around to receive
            // the next UDP event
            //
            rawEventContainer *taskArgs = (rawEventContainer *) calloc(1, sizeof(rawEventContainer));
            taskArgs->eventCode = event.eventCode;
            taskArgs->eventValue = event.eventValue;
            taskArgs->jsonPayload = json;   // NOTE: handing json to the pool to destroy

            if (adapter)
            {
                taskArgs->handler[index++] = adapter->handler;
            }
            if (subscribeAllAdapter)
            {
                taskArgs->handler[index++] = subscribeAllAdapter->handler;
            }

            // place in the thread pool
            //
            if (threadPoolAddTask(pool, processRawEventTask, taskArgs, processRawEventTaskArgFreeFunc) == false)
            {
                // failed.  'taskArgs' should be freed by the pool
                //
                icLogWarn(API_LOG_CAT, "error handling event.  thread pool is FULL!  service=%"PRIu16" code=%"PRIi32" value=%"PRIi32,
                          sourceServiceId, event.eventCode, event.eventValue);
            }
        }
        else
        {
            // memory cleanup the JSON buffer since it wasn't transferred to a task
            //
            cJSON_Delete(json);
            json = NULL;
            icLogWarn(API_LOG_CAT, "error parsing event code/value received from service %"PRIu16, sourceServiceId);
        }
    }
    pthread_mutex_unlock(&LIST_MTX);
    pthread_mutex_unlock(&SERVICE_POOL_MAP_MTX);
}

/*
 * task called via the 'eventThreadPool', to process an incoming
 * raw event - by passing to the worker's handler so it can be
 * parsed and internally distributed WITHOUT interrupting the worker thread
 */
static void processRawEventTask(void *args)
{
    int i;

    rawEventContainer *raw = (rawEventContainer *)args;

    for (i = 0; i < MAX_EVENT_HANDLERS; i++) {
        // pass to all of the information to the handler so it can
        // properly decode and process the event
        //
        if (raw->handler[i]) {
            raw->handler[i](raw->eventCode, raw->eventValue, raw->jsonPayload);
        }
    }
}

static void processRawEventTaskArgFreeFunc(void *arg)
{
    rawEventContainer *raw = (rawEventContainer *)arg;

    // cleanup the JSON object created by the eventWorker
    //
    cJSON_Delete(raw->jsonPayload);
    free(arg);
}

/*
 * mechanism to direct-inject events (in raw json) through the reader so they
 * can be delivered to listeners as-if they arrived over the socket.
 */
void directlyProcessRawEvent(const char *eventJsonString)
{
    if (eventJsonString != NULL)
    {
        // parse as json, then pass along to our 'processRawJsonEvent' as if this came from the socket.
        // that function will cleanup our json object after completion
        //
        cJSON *event = cJSON_Parse(eventJsonString);
        processRawJsonEvent(event);
    }
}