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


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <cjson/cJSON.h>
#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icQueue.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
#include <zhal/zhal.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/threadUtils.h>
#include "zhalPrivate.h"
#include "zhalAsyncReceiver.h"

/*
 * This is the core IPC processing to ZigbeeCore.  Individual requests are in zhalRequests.c
 *
 * Created by Thomas Lea on 3/14/16.
 */

#define LOG_TAG "zhalImpl"

typedef struct
{
    icQueue *queue; //WorkItems pending for this device
    pthread_mutex_t mutex;
    int isBusy;
} DeviceQueue;

typedef struct
{
    uint64_t eui64;
    uint64_t requestId;
    cJSON *request;
    cJSON *response;
    DeviceQueue *deviceQueue; //handle to the owning device queue
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    bool timedOut;
} WorkItem;

#define SOCKET_RECEIVE_TIMEOUT_SEC 10
#define SOCKET_SEND_TIMEOUT_SEC 10

static icHashMap *deviceQueues = NULL; //maps uint64_t (EUI64) to DeviceQueue pointers
static pthread_mutex_t deviceQueuesMutex = PTHREAD_MUTEX_INITIALIZER;

static uint64_t requestId = 0;
static pthread_mutex_t requestIdMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t workerThread;
static pthread_cond_t workerCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t workerMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t workerRunningMutex = PTHREAD_MUTEX_INITIALIZER;
static void *workerThreadProc(void*);
static bool workerThreadRunning = false;
static bool shouldWorkerContinue();

static icHashMap *asyncRequests = NULL; //maps uint64_t (requestId) to WorkItem
static pthread_mutex_t asyncRequestsMutex = PTHREAD_MUTEX_INITIALIZER;

static char *zigbeeIp = NULL;
static int zigbeePort = 0;

static int handleIpcResponse(cJSON *response);
static void deviceQueuesDestroy(void* key, void* value);

static zhalCallbacks *callbacks = NULL;
static void* callbackContext = NULL;

zhalCallbacks* getCallbacks()
{
    return callbacks;
}

void* getCallbackContext()
{
    return callbackContext;
}

int zhalInit(const char *host, int port, zhalCallbacks *cbs, void* ctx)
{
    icLogDebug(LOG_TAG, "zhalInit %s:%d", host, port);

    zigbeeIp = strdup(host);
    zigbeePort = port;
    callbacks = cbs;
    callbackContext = ctx;

    pthread_mutex_lock(&deviceQueuesMutex);
    deviceQueues = hashMapCreate();
    pthread_mutex_unlock(&deviceQueuesMutex);

    pthread_mutex_lock(&asyncRequestsMutex);
    asyncRequests = hashMapCreate();
    pthread_mutex_unlock(&asyncRequestsMutex);

    zhalAsyncReceiverStart(host, handleIpcResponse, zhalHandleEvent);

    pthread_mutex_lock(&workerRunningMutex);
    workerThreadRunning = true;
    pthread_mutex_unlock(&workerRunningMutex);

    createThread(&workerThread, workerThreadProc, NULL, "zhal");

    return 0;
}

int zhalTerm()
{
    icLogDebug(LOG_TAG, "zhalTerm");

    zhalNetworkTerm();

    // Shut down the receiver socket
    zhalAsyncReceiverStop();

    // Shutdown the worker
    pthread_mutex_lock(&workerRunningMutex);
    workerThreadRunning = false;
    pthread_mutex_unlock(&workerRunningMutex);
    pthread_mutex_lock(&workerMutex);
    pthread_cond_signal(&workerCond);
    pthread_mutex_unlock(&workerMutex);

    pthread_join(workerThread, NULL);

    callbacks = NULL;
    free(zigbeeIp);
    zigbeeIp = NULL;

    pthread_mutex_lock(&asyncRequestsMutex);
    hashMapDestroy(asyncRequests, standardDoNotFreeHashMapFunc);
    asyncRequests = NULL;
    pthread_mutex_unlock(&asyncRequestsMutex);

    pthread_mutex_lock(&deviceQueuesMutex);
    hashMapDestroy(deviceQueues, deviceQueuesDestroy);
    deviceQueues = NULL;
    pthread_mutex_unlock(&deviceQueuesMutex);

    return 0;
}

static uint64_t getNextRequestId()
{
    uint64_t id;
    pthread_mutex_lock(&requestIdMutex);
    id = ++requestId;
    pthread_mutex_unlock(&requestIdMutex);
    return id;
}

static bool itemQueueCompareFunc(void *searchVal, void *item)
{
    return searchVal == item ? true : false;
}

static void queueDoNotFreeFunc(void *item) {}

static void destroyItem(WorkItem *item)
{
    pthread_cond_destroy(&item->cond);
    pthread_mutex_destroy(&item->mtx);
    free(item);
}

static void deviceQueuesDestroy(void* key, void* value)
{
    DeviceQueue *deviceQueue = (DeviceQueue*) value;
    queueDestroy(deviceQueue->queue, NULL);
    free(deviceQueue);
    free(key);
}
/*
 * This blocks until the full operation is complete or it times out
 */
cJSON * zhalSendRequest(uint64_t targetEui64, cJSON *requestJson, int timeoutSecs)
{
    cJSON *result = NULL;

    pthread_mutex_lock(&deviceQueuesMutex);

    //get the queue for the target device and create it if it is missing
    DeviceQueue *deviceQueue = (DeviceQueue*)hashMapGet(deviceQueues, &targetEui64, sizeof(uint64_t));
    if(deviceQueue == NULL)
    {
        icLogDebug(LOG_TAG, "Creating device queue for %016" PRIx64, targetEui64);

        deviceQueue = (DeviceQueue*)calloc(1, sizeof(DeviceQueue));
        deviceQueue->queue = queueCreate();
        pthread_mutex_init(&deviceQueue->mutex, NULL);
        uint64_t *key = (uint64_t*)malloc(sizeof(targetEui64));
        *key = targetEui64;
        hashMapPut(deviceQueues, key, sizeof(uint64_t), deviceQueue);
    }
    pthread_mutex_unlock(&deviceQueuesMutex);

    WorkItem *item = (WorkItem*)calloc(1, sizeof(WorkItem));
    item->eui64 = targetEui64;
    item->requestId = getNextRequestId();
    item->request = requestJson;
    item->deviceQueue = deviceQueue;
    item->timedOut = false;
    initTimedWaitCond(&item->cond);
    pthread_mutex_init(&item->mtx, NULL);

    cJSON_AddNumberToObject(requestJson, "requestId", item->requestId);

    //lock the item before the worker can get it so we wont miss its completion
    pthread_mutex_lock(&item->mtx);

    //enqueue the work item
    pthread_mutex_lock(&deviceQueue->mutex);
    queuePush(deviceQueue->queue, item);
    pthread_mutex_unlock(&deviceQueue->mutex);

    //signal our worker that there is stuff to do
    pthread_mutex_lock(&workerMutex);
    pthread_cond_signal(&workerCond);
    pthread_mutex_unlock(&workerMutex);

    if(ETIMEDOUT == incrementalCondTimedWait(&item->cond, &item->mtx, timeoutSecs))
    {
        icLogWarn(LOG_TAG, "requestId %" PRIu64 " timed out", item->requestId);

        //remove from asyncRequests
        pthread_mutex_lock(&asyncRequestsMutex);
        bool didDeleteFromAsyncRequests = hashMapDelete(asyncRequests, &item->requestId, sizeof(uint64_t), standardDoNotFreeHashMapFunc);
        pthread_mutex_unlock(&asyncRequestsMutex);

        //lock the device queue and remove this item if its still there
        pthread_mutex_lock(&deviceQueue->mutex);
        bool didDeleteFromQueue = queueDelete(deviceQueue->queue, item, itemQueueCompareFunc, queueDoNotFreeFunc);

        // Two cases here, could have been never removed from queue to be sent, or it could have been removed and
        // sent, but we didn't get a reply in time.  The busy counter is only incremented in the latter case.  So if we
        // removed it from the queue, we don't need to decrement busy.  But if we are deleting it from our pending
        // async requests, then we should decrement busy.

        if (didDeleteFromAsyncRequests)
        {
            deviceQueue->isBusy--;
        }
        else
        {
            icLogDebug(LOG_TAG, "requestId %" PRIu64 " was not pending, so not changing busy counter", item->requestId);
        }

        // If this item exists in neither place, then it was taken as available work.  We can't clean it up because
        // the worker thread still holds a pointer, so instead we just mark it as timedOut and the other thread will
        // take care of cleanup.  This doesn't feel like a great way of handling this, but I couldn't come up with
        // anything cleaner.
        if (!didDeleteFromAsyncRequests && !didDeleteFromQueue)
        {
            item->timedOut = true;
        }
        pthread_mutex_unlock(&deviceQueue->mutex);

        //There may have been other requests queued up, so signal our worker that there might be stuff to do
        pthread_mutex_lock(&workerMutex);
        pthread_cond_signal(&workerCond);
        pthread_mutex_unlock(&workerMutex);
    }
    else
    {
        result = item->response;
    }

    //finally unlock the item and clean up
    pthread_mutex_unlock(&item->mtx);
    // If we are set as timedOut its a signal to the worker thread to do the clean up
    if (!item->timedOut)
    {
        destroyItem(item);
    }

    return result;
}

/*
 * Create a copy of the ReceivedAttributeReport
 */
ReceivedAttributeReport *cloneReceivedAttributeReport(ReceivedAttributeReport *report)
{
    ReceivedAttributeReport *reportCopy = NULL;

    if (report != NULL)
    {
        reportCopy = (ReceivedAttributeReport*) calloc(1, sizeof(ReceivedAttributeReport));
        memcpy(reportCopy, report, sizeof(ReceivedAttributeReport));

        reportCopy->reportData = (uint8_t *) calloc(report->reportDataLen, 1);      // since sizeof(uint8_t) is 1
        memcpy(reportCopy->reportData, report->reportData, report->reportDataLen);
    }

    return reportCopy;
}

/*
 * Creates a copy of the ReceivedClusterCommand
 */
ReceivedClusterCommand *cloneReceivedClusterCommand(ReceivedClusterCommand *command)
{
    ReceivedClusterCommand *commandCopy = NULL;

    if (command != NULL)
    {
        commandCopy = (ReceivedClusterCommand *) calloc(1, sizeof(ReceivedClusterCommand));
        memcpy(commandCopy, command, sizeof(ReceivedClusterCommand));

        commandCopy->commandData = (uint8_t *) calloc(command->commandDataLen, 1);      // since sizeof(uint8_t) is 1
        memcpy(commandCopy->commandData, command->commandData, command->commandDataLen);
    }

    return commandCopy;
}

/*
 * Free a ReceivedAttributeReport
 */
void freeReceivedAttributeReport(ReceivedAttributeReport* report)
{
    if(report != NULL)
    {
        free(report->reportData);
        free(report);
    }
}

ReceivedAttributeReport *receivedAttributeReportClone(const ReceivedAttributeReport *report)
{
    ReceivedAttributeReport *result = NULL;

    if(report != NULL)
    {
        result = malloc(sizeof(ReceivedAttributeReport));
        memcpy(result, report, sizeof(ReceivedAttributeReport));
        result->reportData = malloc(report->reportDataLen);
        memcpy(result->reportData, report->reportData, report->reportDataLen);
    }

    return result;
}

/*
 * Free a ReceivedClusterCommand
 */
void freeReceivedClusterCommand(ReceivedClusterCommand* command)
{
    if(command != NULL)
    {
        free(command->commandData);
        free(command);
    }
}

ReceivedClusterCommand *receivedClusterCommandClone(const ReceivedClusterCommand *command)
{
    ReceivedClusterCommand *result = NULL;

    if(command != NULL)
    {
        result = malloc(sizeof(ReceivedClusterCommand));
        memcpy(result, command, sizeof(ReceivedClusterCommand));
        result->commandData = malloc(command->commandDataLen);
        memcpy(result->commandData, command->commandData, command->commandDataLen);
    }

    return result;
}

bool zhalEndpointHasServerCluster(zhalEndpointInfo* endpointInfo, uint16_t clusterId)
{
    bool result = false;

    if(endpointInfo != NULL)
    {
        for(int i = 0; i < endpointInfo->numServerClusterIds; i++)
        {
            if(endpointInfo->serverClusterIds[i] == clusterId)
            {
                result = true;
                break;
            }
        }
    }

    return result;
}

/*
 * Put any work items that are ready into the provided queue.  Return zero if there was no work to do.
 */
static int getAvailableWork(icQueue *availableWork)
{
    int result = 0;

    pthread_mutex_lock(&deviceQueuesMutex);
    icHashMapIterator *queuesIterator = hashMapIteratorCreate(deviceQueues);
    while(hashMapIteratorHasNext(queuesIterator))
    {
        uint64_t *eui64;
        uint16_t keyLen;
        DeviceQueue *dq;
        if(hashMapIteratorGetNext(queuesIterator, (void **) &eui64, &keyLen, (void **) &dq) == true)
        {
            pthread_mutex_lock(&dq->mutex);

            WorkItem *item = NULL;

            //if a device queue is busy, dont schedule another item
            if(dq->isBusy == 0)
            {
                item = (WorkItem*)queuePop(dq->queue);
            }

            if(item != NULL)
            {
                queuePush(availableWork, item);
                result++;
            }

            pthread_mutex_unlock(&dq->mutex);
        }
    }
    hashMapIteratorDestroy(queuesIterator);
    pthread_mutex_unlock(&deviceQueuesMutex);

    return result;
}

/* send over the socket and await initial synchronous response (quick).  Returns true if that was successful */
static bool xmit(WorkItem *item)
{
    int sock;
    struct sockaddr_in addr;
    int sendFlags = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(zigbeePort);
    inet_aton(zigbeeIp, &(addr.sin_addr));

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        icLogError(LOG_TAG, "error opening socket: %s", strerror(errno));
        return false;
    }

#ifdef SO_NOSIGPIPE
    // set socket option to not SIGPIPE on write errors.  not available on some platforms.
    // hopefully if not, they support MSG_NOSIGNAL - which should be used on the send() call
    //
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(LOG_TAG, "unable to set SO_NOSIGPIPE flag on socket");
    }
#endif

    struct timeval timeout = {0, 0};

#ifdef SO_RCVTIMEO
    timeout.tv_sec = SOCKET_RECEIVE_TIMEOUT_SEC;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) == -1)
    {
        char *errstr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "failed setting receive timeout on socket: %s", errstr);
        free(errstr);
    }
#endif

#ifdef SO_SNDTIMEO
    timeout.tv_sec = SOCKET_SEND_TIMEOUT_SEC;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval)) == -1)
    {
        char *errstr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "failed setting send timeout on socket: %s", errstr);
        free(errstr);
    }
#endif

    if (connect(sock, (const struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        // failed to connect to service, close up the socket
        //
        icLogWarn(LOG_TAG, "error connecting to socket: %s", strerror(errno));
        close(sock);
        return false;
    }

    char *payload = cJSON_PrintUnformatted(item->request);
    uint16_t payloadLen = (uint16_t) strlen(payload);

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint16_t msgLen = payloadLen;
#else
    uint16_t msgLen = (uint16_t) (((payloadLen & 0xff) << 8) + ((payloadLen & 0xff00) >> 8));
#endif
    if (send(sock, &msgLen, sizeof(uint16_t), sendFlags) < 0)
    {
        icLogError(LOG_TAG, "error sending payload length: %s", strerror(errno));
        close(sock);
        return false;
    }

    if (send(sock, payload, payloadLen, sendFlags) < 0)
    {
        icLogError(LOG_TAG, "error sending payload: %s", strerror(errno));
        close(sock);
        free(payload);
        return false;
    }

    free(payload);

    // recv taints msgLen but we can only receive 2 bytes into a unit16 - there are no sane bounds checks
    // coverity[tainted_data_argument]
    if (recv(sock, &msgLen, sizeof(uint16_t), MSG_WAITALL) < 0)
    {
        icLogError(LOG_TAG, "error receiving payload length: %s", strerror(errno));
        close(sock);
        return false;
    }

    msgLen = ntohs(msgLen);

    char *reply = (char*)calloc(1, msgLen + 1U);
    errno = 0;
    ssize_t recvLen = recv(sock, reply, msgLen, MSG_WAITALL);
    if (recvLen < 0 || recvLen != msgLen)
    {
        if (recvLen != msgLen)
        {
            icLogError(LOG_TAG, "error receiving payload: message truncated");
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "error receiving payload: %s", errStr);
            free(errStr);
        }

        free(reply);
        close(sock);
        return false;
    }

    cJSON *response = cJSON_Parse(reply);
    free(reply);

    if(response != NULL)
    {
        cJSON *resultCode = cJSON_GetObjectItem(response, "resultCode");
        if (resultCode != NULL && resultCode->valueint == 0)
        {
            close(sock);
            cJSON_Delete(response);
            return true;
        }
    }

    cJSON_Delete(response);

    close(sock);
    return false;
}

static bool workOnItem(WorkItem *item, void *arg)
{
    pthread_mutex_lock(&item->mtx);

    // The item timedOut but couldn't be cleaned up by zhalSendRequest because we were in the midst of trying to
    // work on the item.  So now we must do the clean up.
    if (item->timedOut)
    {
        pthread_mutex_unlock(&item->mtx);
        destroyItem(item);
        // continue to iterate
        return true;
    }

    char *json = cJSON_PrintUnformatted(item->request);
    icLogDebug(LOG_TAG, "Worker processing JSON: %s", json);
    free(json);

    //put in asyncRequests
    pthread_mutex_lock(&asyncRequestsMutex);
    hashMapPut(asyncRequests, &item->requestId, sizeof(uint64_t), item);
    pthread_mutex_unlock(&asyncRequestsMutex);

    //set device queue busy
    pthread_mutex_lock(&item->deviceQueue->mutex);
    if(item->deviceQueue->isBusy > 0)
    {
        icLogError(LOG_TAG, "device queue for %016" PRIx64 " is already busy (%d)!", item->eui64, item->deviceQueue->isBusy);
    }
    item->deviceQueue->isBusy++;
    pthread_mutex_unlock(&item->deviceQueue->mutex);

    //send to ZigbeeCore and wait for immediate/synchronous response
    // if successful, our async receiver will find and complete it via asyncRequests hash map
    if(xmit(item) == true)
    {
        pthread_mutex_unlock(&item->mtx);
    }
    else
    {
        icLogWarn(LOG_TAG, "xmit failed... aborting work item %" PRIu64, item->requestId);

        //remove from asyncRequests
        pthread_mutex_lock(&asyncRequestsMutex);
        hashMapDelete(asyncRequests, &item->requestId, sizeof(uint64_t), standardDoNotFreeHashMapFunc);
        pthread_mutex_unlock(&asyncRequestsMutex);

        //clear busy for this device queue
        pthread_mutex_lock(&item->deviceQueue->mutex);
        item->deviceQueue->isBusy--;
        if(item->deviceQueue->isBusy != 0)
        {
            icLogError(LOG_TAG, "device queue for %016" PRIx64 " is still busy (%d)!", item->eui64, item->deviceQueue->isBusy);
        }
        pthread_mutex_unlock(&item->deviceQueue->mutex);

        //since it failed, unlock the item here
        pthread_cond_signal(&item->cond);
        pthread_mutex_unlock(&item->mtx);
    }

    return true; // continue to iterate
}

static void *workerThreadProc(void *arg)
{
    icQueue *availableWork = queueCreate();

    while(shouldWorkerContinue())
    {
        //wait to be woken up
        pthread_mutex_lock(&workerMutex);
        while(!getAvailableWork(availableWork))
        {
            if(pthread_cond_wait(&workerCond, &workerMutex) != 0)
            {
                icLogError(LOG_TAG, "failed to wait on workerCond, terminating workerThreadProc!");
                break;
            }
            else if (!shouldWorkerContinue())
            {
                break;
            }
        }
        pthread_mutex_unlock(&workerMutex);

        //Process any items that were ready
        queueIterate(availableWork, (queueIterateFunc) workOnItem, NULL);

        //All items have been processed... clear the queue
        queueClear(availableWork, queueDoNotFreeFunc);
    }

    icLogInfo(LOG_TAG, "workerThreadProc exiting");

    queueDestroy(availableWork, NULL); //TODO this could leak during shutdown

    return NULL;
}

static bool shouldWorkerContinue()
{
     bool keepRunning;

     pthread_mutex_lock(&workerRunningMutex);
     keepRunning = workerThreadRunning;
     pthread_mutex_unlock(&workerRunningMutex);

     return keepRunning;
}

static int handleIpcResponse(cJSON *response)
{
    int result = 0;

    char *responseString = cJSON_PrintUnformatted(response);
    icLogDebug(LOG_TAG, "got response: %s", responseString);
    free(responseString);

    cJSON *requestId = cJSON_GetObjectItem(response, "requestId");
    if (requestId != NULL)
    {
        pthread_mutex_lock(&asyncRequestsMutex);
        WorkItem *item = hashMapGet(asyncRequests, &requestId->valueint, sizeof(uint64_t));
        hashMapDelete(asyncRequests, &requestId->valueint, sizeof(uint64_t), standardDoNotFreeHashMapFunc);
        pthread_mutex_unlock(&asyncRequestsMutex);

        if(item != NULL)
        {
            //clear busy for this device queue
            pthread_mutex_lock(&item->deviceQueue->mutex);
            item->deviceQueue->isBusy--;
            if (item->deviceQueue->isBusy != 0)
            {
                icLogError(LOG_TAG, "device queue for %016"
                        PRIx64
                        " is still busy (%d)!", item->eui64, item->deviceQueue->isBusy);
            }
            pthread_mutex_unlock(&item->deviceQueue->mutex);

            pthread_mutex_lock(&item->mtx);
            item->response = response;
            pthread_cond_signal(&item->cond);
            pthread_mutex_unlock(&item->mtx);
        }
        else
        {
            icLogDebug(LOG_TAG, "handleIpcResponse did not find %d", requestId->valueint);
        }

        result = 1; //we handled it
    }

    //this might have freed up a device queue, let the worker thread take another look.
    pthread_mutex_lock(&workerMutex);
    pthread_cond_signal(&workerCond);
    pthread_mutex_unlock(&workerMutex);

    return result;
}

