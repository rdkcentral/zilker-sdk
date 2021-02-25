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
 * troubleState.c
 *
 * Track the set of troubles throughout the system.
 * Note that this is an in-memory store, and not persisted
 * in any way.
 *
 * Authors: jelderton, mkoch - 7/9/15
 *-----------------------------------------------*/

#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icTime/timeUtils.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icSortedLinkedList.h>
#include <icSystem/softwareCapabilities.h>
#include <icIpc/ipcSender.h>
#include <icIpc/eventIdSequence.h>
#include <icSystem/softwareCapabilities.h>
#include <icConcurrent/repeatingTask.h>
#include <icConfig/storage.h>
#include <xhCron/cronEventRegistrar.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <securityService/troubleEventHelper.h>
#include <securityService/deviceTroubleEventHelper.h>
#include <securityService/sensorTroubleEventHelper.h>
#include <deviceService/deviceService_ipc.h>
#include <resourceTypes.h>
#include <commonDeviceDefs.h>
#include <deviceHelper.h>
#include <deviceService/deviceService_event.h>
#include <icTypes/icStringBuffer.h>

#include "trouble/troubleState.h"
#include "trouble/troubleStatePrivate.h"
#include "trouble/replayTracker.h"
#include "trouble/troubleStatePrivate.h"
#include "trouble/troubleState.h"
#include "trouble/troubleContainer.h"
#include "trouble/commFailTimer.h"
#include "zone/securityZonePrivate.h"
#include "alarm/alarmPanel.h"
#include "common.h"
#include "broadcastEvent.h"
#include "internal.h"
#include "securityProps.h"
#include "troubleContainer.h"

/* metadata tag used to save detailed trouble information into the device/zone */
#define DEVICE_TROUBLE_METADATA     "troubles"

// Name for our cron schedule that does low battery elevation
#define LOW_BATTERY_ELEVATION_CRON_NAME "securityServiceLowBatElevate"

// Run every hour at a random minute.  This means we aren't exact in our X days of prelow battery, but it should
// within an hour of X days.
#define LOW_BATTERY_ELEVATION_CRON_SCHEDULE_FORMAT "%d * * * *"

// Fire every minute
#define LOW_BATTERY_ELEVATION_CRON_SCHEDULE_DEV "* * * * *"
#define SECONDS_IN_A_DAY (60*60*24)

// Admiral, if we go by the book, like Lieutenant Saavik, minutes could seem like days.
#define SECONDS_IN_A_MINUTE (60)

// Buffer size of device IDs for trouble reporting in telemetry
#define TELEMETRY_TROUBLE_BUFFER_SIZE 128


// Storage namespace for non device troubles
#define NON_DEVICE_TROUBLES_NAMESPACE "nonDeviceTroubles"

// used as the 'searchVal' parameter for the
// findTroubleContainerBySearchParms linked-list
// search function
//
typedef struct {
    troubleType     type;
    troubleReason   reason;
    troublePayloadCompareFunc compareFunc;
    cJSON *payload;
} troubleSearchParms;

typedef struct {
    troubleType     type;
    troubleReason   reason;
    const char      *deviceId;
} troubleSearchDevice;

// used as 'arg' parameter for the countTroublesForCategory() function
typedef struct {
    troubleFilterConstraints    *constraints;    // trouble constraints to match against
    indicationCategory          category;        // the category to match on
    uint32_t                    outputCounter;   // output object
} countByCategoryArg;


/**
 * Handler function which is responsible for populating isTrouble, critical, and reason given its inputs. Returns if
 * this is a clear.
 */
typedef bool (*TroubleResourceHandlerFunc)(const DSResource *resource,
                                           const DSDevice *parentDevice,
                                           const char *deviceId,
                                           bool *isTrouble,
                                           troubleCriticalityType *critical,
                                           troubleReason *reason,
                                           void *troubleResourceHandlerArg);

/**
 * Function used to cleanup the argument to the handler func
 */
typedef void (*TroubleResourceHandlerArgFreeFunc)(void *arg);

/**
 * Trouble Resource Handler structure stored in our handler map
 */
typedef struct
{
    TroubleResourceHandlerFunc handlerFunc;
    void *handlerArg;
    TroubleResourceHandlerArgFreeFunc handlerArgFreeFunc;

} TroubleResourceHandler;

/**
 * A special simple handler exists that just uses a set of static values to populate isTrouble, critical, reason.  This
 * structure contains those static values for the handler.
 */
typedef struct
{
    bool isTrouble;
    troubleCriticalityType critical;
    troubleReason reason;
} SimpleTroubleResourceHandlerArg;

/*
 * private functions
 */
static bool findTroubleContainerByTroubleId(void *searchVal, void *item);
static bool findTroubleContainerByTroubleOrEventId(void *searchVal, void *item);
static bool findTroubleContainerBySearchParms(void *searchVal, void *item);
static bool findTroubleContainerBySpecificDevice(void *searchVal, void *item);
static bool countUnackedTroubles(void *item, void *arg);
static bool updateSystemTroubleFlag(void *item, void *arg);
static void assignIndicationType(troubleContainer *container);
static void schedulePreLowBatteryCron(bool devMode);
static bool lowBatElevateCallback(const char *name);
static void cpePropListener(cpePropertyEvent *event);
static void addTroubleTaskRun(void *taskObj, void *taskArg);
static void addTroubleTaskFree(void *taskObj, void *taskArg);
static void updateTroubleTaskRun(void *taskObj, void *taskArg);
static void updateTroubleTaskFree(void *taskObj, void *taskArg);
static void updateTroubleNoSendTaskRun(void *taskObj, void *taskArg);
static void updateTroubleNoSendTaskFree(void *taskObj, void *taskArg);
static void clearTroubleTaskRun(void *taskObj, void *taskArg);
static void clearTroubleTaskFree(void *taskObj, void *taskArg);
static void addTroubleToListPrivate(troubleContainer *container);
// This is public in the sense it will take the lock, but we aren't exposing this externally
static bool addTroubleToListPublic(troubleContainer *container, troublePayloadCompareFunc compareFunc);
static bool hasExistingTrouble(troubleContainer *container, troublePayloadCompareFunc compareFunc);
static void initTroubleResourceHandlers();
static void destroyTroubleResourceHandlers();

/*
 * private variables
 */

// array of 'troubleContainer' objects.  each is 'unique' through a composite key
// of "deviceId + troubleType + troubleReason", since the assumption is that
// a single device cannot have more then 1 trouble for a given type & reason
// (i.e. only one sensor tamper, but could have sensor tamper and low-battery)
//
static icLinkedList *troubleList = NULL;

static bool didInit = false;
static bool haveSystemTroubles = false;     // quick indicator if any troubles in 'troubleList' are "system" troubles
static bool haveSystemTamper = false;       // quick indicator if system tampered is in 'troubleList'

// We don't need to lock our whole security mutex when dealing with values in the map, but just need some basic safety.
// So use this mutex for access into the troubleResourceHandlers map
static pthread_mutex_t troubleResourceHandlersMtx = PTHREAD_MUTEX_INITIALIZER;
static icHashMap *troubleResourceHandlers = NULL; // Map of trouble resource id -> TroubleResourceHandler


/*
 * one-time init to setup for troubles
 */
void initTroubleStatePublic()
{
    lockSecurityMutex();
    if (didInit == false)
    {
        // init by allocating our list
        //
        troubleList = linkedListCreate();

        didInit = true;
        haveSystemTroubles = false;
        haveSystemTamper = false;

        // Init our trouble resource handlers
        initTroubleResourceHandlers();

        // init trouble replay tracker
        //
        initTroubleReplayTrackers();

        // init trouble comm fail timer
        //
        initCommFailTimer();

        // Register to dev mode property updates so we can update our schedule, then setup
        // the pre-low battery cron job for refreshing
        //
        bool devMode = getPropertyAsBool(PRELOW_BATTERY_DAYS_DEV_PROPERTY, false);
        schedulePreLowBatteryCron(devMode);
        register_cpePropertyEvent_eventListener(cpePropListener);
    }
    unlockSecurityMutex();
}

/*
 * called during shutdown
 */
void destroyTroubleStatePublic()
{
    // Unregister first outside of mutex
    unregister_cpePropertyEvent_eventListener(cpePropListener);
    unregisterForCronEvent(LOW_BATTERY_ELEVATION_CRON_NAME, false);

    // cleanup
    //
    destroyTroubleResourceHandlers();
    lockSecurityMutex();
    shutdownCommFailTimer();
    shutdownTroubleReplayTrackers();
    linkedListDestroy(troubleList, destroyTroubleContainerFromList);

    didInit = false;
    unlockSecurityMutex();
}

/*
 * retrieve & decode the troubleEvent objects stored in deviceService as
 * metadata (the 'metadatas->troubles' tag).
 * used when loading the existing troubles from deviceService.
 */
static icLinkedList *decodeTroublesMetadataForUri(const char *uri)
{
    icLinkedList *troubles = linkedListCreate();

    // get 'trouble' metadata from the device identified by this uri.
    // if it's there, should be a list of troubleEvent objects in JSON format
    //
    char *metadataStr = NULL;
    if (deviceHelperReadMetadataByOwner(uri, DEVICE_TROUBLE_METADATA, &metadataStr))
    {
        if (metadataStr != NULL)
        {
            // parse the metadata string as JSON
            cJSON *troubleJson = cJSON_Parse(metadataStr);
            if (troubleJson != NULL)
            {
                if (cJSON_IsObject(troubleJson))
                {
                    // extract each troubleEvent that is encoded in this JSON object, and add to the return list
                    int count = cJSON_GetArraySize(troubleJson);
                    for(int i = 0; i < count; ++i)
                    {
                        cJSON *item = cJSON_GetArrayItem(troubleJson, i);
                        troubleEvent *event = create_troubleEvent();
                        decode_troubleEvent_fromJSON(event, item);
                        linkedListAppend(troubles, event);
                    }
                }
                else
                {
                    icLogWarn(SECURITY_LOG, "Failed to parse trouble json as object for uri %s", uri);
                }
            }
            else
            {
                icLogWarn(SECURITY_LOG, "Failed to parse trouble json for uri %s", uri);
            }

            // Cleanup
            cJSON_Delete(troubleJson);
            free(metadataStr);
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "Failed to get trouble metadata for uri %s", uri);
    }

    return troubles;
}

/*
 * decode details from the 'extra' section of the trouble
 * used when loading the existing troubles from deviceService.
 */
static DeviceTroublePayload *extractDeviceTroublePayload(troubleObj *trouble)
{
    DeviceTroublePayload *payload = NULL;
    if (trouble->extra != NULL)
    {
        payload = decodeDeviceTroublePayload(trouble->extra);
    }

    return payload;
}

/*
 * Directly adds teh trouble to our trouble list, not sending any events,
 * but does take care of keeping our internal state in check
 */
static void addTroubleToListPrivate(troubleContainer *container)
{
    // Make sure the indication type is set appropriately
    assignIndicationType(container);

    // Keep our state up to date
    if (container->event->trouble->type == TROUBLE_TYPE_SYSTEM)
    {
        if (container->event->trouble->reason == TROUBLE_REASON_TAMPER)
        {
            haveSystemTamper = true;
        }
        haveSystemTroubles = true;
    }

    linkedListAppend(troubleList, container);

    // Filter out hidden troubles from going into the replay tracker (troubles that cannot be acknowledged).
    if (container->event->trouble->indication == INDICATION_BOTH || container->event->trouble->indication == INDICATION_VISUAL)
    {
        // inform the replay tracker
        //
        startTroubleReplayTrackers(container->event);
    }
}

/*
 * Takes the lock and directly adds the trouble to the our trouble list,
 * not sending any events, but does take care of keeping our internal state in check
 */
static bool addTroubleToListPublic(troubleContainer *container, troublePayloadCompareFunc compareFunc)
{
    bool didAdd = false;
    lockSecurityMutex();
    if (hasExistingTrouble(container, compareFunc) == false)
    {
        addTroubleToListPrivate(container);
        didAdd = true;
    }
    else
    {
        icLogDebug(SECURITY_LOG, "not adding trouble eventId %" PRIu64", already have a trouble for type=%s, reason=%s",
                   container->event->baseEvent.eventId,
                   troubleTypeLabels[container->event->trouble->type],
                   troubleReasonLabels[container->event->trouble->reason]);
    }
    unlockSecurityMutex();

    return didAdd;
}

static void loadInitialDeviceTroublesForUri(const char *uri)
{
    icLinkedList *troubles = decodeTroublesMetadataForUri(uri);

    icLinkedListIterator *troubleIter = linkedListIteratorCreate(troubles);
    while(linkedListIteratorHasNext(troubleIter))
    {
        troubleEvent *event = (troubleEvent *)linkedListIteratorGetNext(troubleIter);

        // make the troubleContainer for this event
        //
        troubleContainer *container = createTroubleContainer();
        container->event = event;

        DeviceTroublePayload *deviceTroublePayload = extractDeviceTroublePayload(event->trouble);

        // look at the deviceClass to re-parse the payload in troubleObj->extra
        // and save in our container->payload union
        //
        if (stringCompare(deviceTroublePayload->deviceClass, SENSOR_DC, false) == 0)
        {
            // extract the zone data
            //
            container->payloadType = TROUBLE_DEVICE_TYPE_ZONE;
            container->extraPayload.zone = decodeSensorTroublePayload(event->trouble->extra);
        }
        else if (stringCompare(deviceTroublePayload->deviceClass, CAMERA_DC, false) == 0)
        {
            // extract the camera data
            //
            container->payloadType = TROUBLE_DEVICE_TYPE_CAMERA;
            container->extraPayload.camera = decodeCameraTroublePayload(event->trouble->extra);
        }
        else
        {
            // steal the deviceTroublePayload
            //
            container->payloadType = TROUBLE_DEVICE_TYPE_IOT;
            container->extraPayload.device = deviceTroublePayload;
            deviceTroublePayload = NULL;
        }

        // per UL 985 (6th edition), we should NOT persist acknowledgements for life-safety
        // or system troubles across reboots.
        //
        if (container->event->trouble->indicationGroup == INDICATION_CATEGORY_SAFETY ||
            container->event->trouble->indicationGroup == INDICATION_CATEGORY_SYSTEM)
        {
            container->event->trouble->acknowledged = false;
        }

        // append our troubleContainer to the global list
        //
        if (addTroubleToListPublic(container, isMatchingDeviceTroublePayload) == false)
        {
            // Trouble already existed in the system, clean it out of the metadata
            bool *sendEventArg = (bool *)malloc(sizeof(bool));
            *sendEventArg = false;
            if (appendSecurityTask(container, sendEventArg, clearTroubleTaskRun, clearTroubleTaskFree) == false)
            {
                /* executor called clearTroubleTaskFree to free troubleEvent */
                icLogWarn(SECURITY_LOG, "Failed queueing trouble clear task: executor rejected job");
            }
        }

        // Cleanup
        deviceTroublePayloadDestroy(deviceTroublePayload);
    }
    linkedListIteratorDestroy(troubleIter);

    // All the entries now belong to the containers in our list, so just cleanup the list itself
    linkedListDestroy(troubles, standardDoNotFreeFunc);
}

/**
 * Extract stored troubles from metadata and put into our internal list of troubles
 * @param currDevice the device
 */
static void loadInitialDeviceTroubles(DSDevice *currDevice)
{
    // Load the troubles rooted on the device
    loadInitialDeviceTroublesForUri(currDevice->uri);

    // Load the troubles rooted on the endpoint
    icHashMapIterator *iter = hashMapIteratorCreate(currDevice->endpointsValuesMap);
    while(hashMapIteratorHasNext(iter))
    {
        char *uri;
        uint16_t keyLen;
        DSEndpoint *value;
        hashMapIteratorGetNext(iter, (void**)&uri, &keyLen, (void**)&value);

        loadInitialDeviceTroublesForUri(uri);
    }
    hashMapIteratorDestroy(iter);

}

/*
 * Load initial device troubles and reconcile against current state
 */
static void processInitialDeviceTroubles()
{
    // wait for deviceService to be available.  probably not necessary, but it is possible
    // watchdog told all services to startup because it waited too long for a single service
    //
    waitForServiceAvailable(DEVICESERVICE_IPC_PORT_NUM, ONE_MINUTE_SECS);
    icLogDebug(SECURITY_LOG, "Loading initial troubles...");

    // get all devices
    //
    IPCCode ipcRc;
    DSDeviceList *tmpList = create_DSDeviceList();
    if ((ipcRc = deviceService_request_GET_DEVICES(tmpList)) == IPC_SUCCESS)
    {
        // loop through all of them, getting their troubles and reconciling against current state
        //
        icLinkedListIterator *loop = linkedListIteratorCreate(tmpList->devices);
        while (linkedListIteratorHasNext(loop) == true)
        {
            // Get the troubleEvents for this device stored in the metadata,
            // then reconcile them and add to our global troubleList
            DSDevice *currDevice = (DSDevice *) linkedListIteratorGetNext(loop);

            // Load any stored troubles into our internal list of troubles, then we can determine whether they are
            // now cleared or still exist.  There is a little bit of extra thrash to load in troubles that then might
            // clear, but we want to process that clear just like a normal clear so doing it this way keeps the logic
            // simpler
            loadInitialDeviceTroubles(currDevice);

            // Check the device for troubles
            checkDeviceForInitialTroubles(currDevice->id, true, false);
        }
        linkedListIteratorDestroy(loop);
    }
    else
    {
        icLogWarn(SECURITY_LOG, "Failed to load devices to check for troubles: %s", IPCCodeLabels[ipcRc]);
    }
    destroy_DSDeviceList(tmpList);
}

/*
 * Load any persisted non device troubles
 */
static void loadNonDeviceTroubles()
{
    // read from persistent storage
    icLinkedList *troubles = storageGetKeys(NON_DEVICE_TROUBLES_NAMESPACE);
    if (troubles != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(troubles);
        while(linkedListIteratorHasNext(iter))
        {
            char *key = (char *)linkedListIteratorGetNext(iter);
            char *troubleStr = NULL;
            if (storageLoad(NON_DEVICE_TROUBLES_NAMESPACE, key, &troubleStr) == true)
            {
                // Parse out the trouble event
                cJSON *troubleJSON = cJSON_Parse(troubleStr);
                if (troubleJSON != NULL)
                {
                    troubleEvent *troubleEvent = create_troubleEvent();
                    decode_troubleEvent_fromJSON(troubleEvent, troubleJSON);

                    // place in a container
                    //
                    troubleContainer *container = createTroubleContainer();
                    container->event = troubleEvent;
                    container->payloadType = TROUBLE_DEVICE_TYPE_NONE;

                    // per UL 985 (6th edition), we should NOT persist acknowledgements for life-safety
                    // or system troubles across reboots.
                    //
                    if (container->event->trouble->indicationGroup == INDICATION_CATEGORY_SAFETY ||
                        container->event->trouble->indicationGroup == INDICATION_CATEGORY_SYSTEM)
                    {
                        container->event->trouble->acknowledged = false;
                    }

                    // Add it into our list
                    addTroubleToListPublic(container, NULL);

                    // Cleanup
                    cJSON_Delete(troubleJSON);
                }
                else
                {
                    icLogError(SECURITY_LOG, "Failed to parse non device trouble %s", key);
                }
                free(troubleStr);
            }
            else
            {
                icLogError(SECURITY_LOG, "Failed to load non device trouble %s", key);
            }

        }
        linkedListIteratorDestroy(iter);
    }

    // Cleanup
    linkedListDestroy(troubles, NULL);
}

/*
 * should be called once all of the services are online
 * so that each can be probed to gather initial troubles.
 */
void loadInitialTroublesPublic()
{
    // ensure we've initialized
    //
    lockSecurityMutex();
    bool didWeInit = didInit;
    unlockSecurityMutex();
    if (didWeInit == false)
    {
        return;
    }

    // Devices
    processInitialDeviceTroubles();

    // Load non device troubles back from storage
    loadNonDeviceTroubles();
}

/*
 * return the number of troubles that are known
 */
uint32_t getTroubleCountPublic(bool includeAckTroubles)
{
    uint32_t retVal = 0;

    lockSecurityMutex();
    retVal = getTroubleCountPrivate(includeAckTroubles);
    unlockSecurityMutex();
    return retVal;
}

/*
 * Private version, assumes mutex is already held
 */
uint32_t getTroubleCountPrivate(bool includeAckTroubles)
{
    uint32_t retVal = 0;

    if (didInit == true)
    {
        if (includeAckTroubles == true)
        {
            // include the ack events, so just get
            // the total count
            //
            retVal = linkedListCount(troubleList);
        }
        else
        {
            // need to loop through all of them, and count
            // up the ones that are not acknowledged.  the
            // simply way to do this is via the linkedListIterate
            //
            linkedListIterate(troubleList, countUnackedTroubles, &retVal);
        }
    }
    return retVal;
}

troubleFilterConstraints *createTroubleFilterConstraints(void)
{
    troubleFilterConstraints *retval = calloc(1, sizeof(troubleFilterConstraints));
    retval->ackValue = TROUBLE_ACK_EITHER;
    retval->allowedIndicationTypes = calloc(1, sizeof(troubleIndicationTypes));

    return retval;
}
void destroyTroubleFilterConstraints(troubleFilterConstraints *constraints)
{
    if (constraints != NULL)
    {
        free(constraints->allowedIndicationTypes);
        free(constraints);
    }
}

static bool troubleMatchesAckValue(const troubleContainer *container, const troubleAckValue ackValue)
{
    bool retVal = false;

    switch (ackValue)
    {
        case TROUBLE_ACK_YES:
            if (container->event->trouble->acknowledged == true)
            {
                retVal = true;
            }
            break;

        case TROUBLE_ACK_NO:
            if (container->event->trouble->acknowledged == false)
            {
                retVal = true;
            }
            break;

        case TROUBLE_ACK_EITHER:
            retVal = true;
            break;
    }

    return retVal;
}

static bool troubleMatchesIndicationTypes(const troubleContainer *container, const troubleIndicationTypes *allowedTypes)
{
    if (allowedTypes == NULL || allowedTypes->types == NULL)
    {
        // Presume no constraint on indication type desired.
        return true;
    }

    bool retVal = false;

    for (int i = 0; i<allowedTypes->size; i++)
    {
        if (allowedTypes->types[i] == container->event->trouble->indication)
        {
            retVal = true;
            break;
        }
    }

    return retVal;
}

/*
 * linkedListIterate function to get a count of troubles for
 * a specific indicationCategory.  assumes 'arg' is a
 */
static bool countTroublesForCategory(void *item, void *arg)
{
    // typecast the input arg.  we're expecting a 'countByCategoryArg'
    //
    countByCategoryArg *input = (countByCategoryArg *)arg;

    // examine the trouble 'item'
    //
    troubleContainer *container = (troubleContainer *)item;
    if (container->event->trouble->indicationGroup == input->category)
    {
        // same category.  now check constraints
        //
        bool matches = true;
        matches &= troubleMatchesAckValue(container, input->constraints->ackValue);
        matches &= troubleMatchesIndicationTypes(container, input->constraints->allowedIndicationTypes);

        if (matches == true)
        {
            // If we meet all the constraints, count this trouble
            input->outputCounter++;
        }
    }

    return true;
}

/*
 * returns the number of troubles for a given indication 'category'
 * and matches the 'constraints' provided
 */
uint32_t getTroubleCategoryCountPrivate(indicationCategory category, troubleFilterConstraints *constraints)
{
    // setup an object for the linked list iterate function countTroublesForCategory to use
    //
    countByCategoryArg arg;
    arg.outputCounter = 0;
    arg.category = category;
    arg.constraints = constraints;

    // loop through all trouble containers
    //
    linkedListIterate(troubleList, countTroublesForCategory, &arg);

    // return the counter
    //
    return arg.outputCounter;
}

/*
 * used during 'getTroubles' to perform a sort of troubleObj by 'date' (the time the trouble occurred).
 */
static int8_t listAppendTroubleObjSortByDate(void *newItem, void *element)
{
    // both should be troubleObj objects
    //
    troubleObj *exists = (troubleObj *)element;
    troubleObj *new = (troubleObj *)newItem;

    // it should return -1 if the 'element' is less then 'newItem'
    //           return  0 if the 'element' is equal to 'newItem'
    //           return  1 if the 'element' is greater then 'newItem'
    //
    if (exists->eventTime < new->eventTime)
    {
        return -1;
    }
    else if (exists->eventTime > new->eventTime)
    {
        return 1;
    }
    return 0;
}

/*
 * used during 'getTroubles' to perform a sort of troubleEvents by 'date' (the time the trouble occurred).
 */
static int8_t listAppendTroubleEventSortByDate(void *newItem, void *element)
{
    // both should be troubleEvent objects
    //
    troubleEvent *exists = (troubleEvent *)element;
    troubleEvent *new = (troubleEvent *)newItem;

    // it should return -1 if the 'element' is less then 'newItem'
    //           return  0 if the 'element' is equal to 'newItem'
    //           return  1 if the 'element' is greater then 'newItem'
    //
    uint64_t existsMillis = convertTimespecToUnixTimeMillis(&exists->baseEvent.eventTime);
    uint64_t newMillis = convertTimespecToUnixTimeMillis(&new->baseEvent.eventTime);
    if (existsMillis < newMillis)
    {
        return -1;
    }
    else if (existsMillis > newMillis)
    {
        return 1;
    }
    return 0;
}

/*
 * used during 'getTroubles' to perform a sort of troubleContainers by 'date' (the time the trouble occurred).
 */
static int8_t listAppendTroubleContainerSortByDate(void *newItem, void *element)
{
    // both should be troubleContainer objects
    //
    troubleContainer *exists = (troubleContainer *)element;
    troubleContainer *new = (troubleContainer *)newItem;

    return listAppendTroubleEventSortByDate(exists->event, new->event);
}

/*
 * used during 'getTroubles' to perform a sort of troubleObjs by 'critical'
 * (critical flag, then by 'type' and 'subtype').
 */
static int8_t listAppendTroubleObjSortByCritical(void *newItem, void *element)
{
    // both should be troubleObj objects
    //
    troubleObj *exists = (troubleObj *)element;
    troubleObj *new = (troubleObj *)newItem;

    // it should return -1 if the 'element' is less then 'newItem'
    //           return  0 if the 'element' is equal to 'newItem'
    //           return  1 if the 'element' is greater then 'newItem'
    if (exists->critical < new->critical)
    {
        return -1;
    }
    else if (exists->critical > new->critical)
    {
        return 1;
    }

    // same critical flag, so look at type
    //
    if (exists->type < new->type)
    {
        return -1;
    }
    else if (exists->type > new->type)
    {
        return 1;
    }

    return 0;
}

/*
 * used during 'getTroubles' to perform a sort of troubleEvents by 'critical'
 * (critical flag, then by 'type' and 'subtype').
 */
static int8_t listAppendTroubleEventSortByCritical(void *newItem, void *element)
{
    // both should be troubleEvent objects
    //
    troubleEvent *exists = (troubleEvent *) element;
    troubleEvent *new = (troubleEvent *) newItem;

    return listAppendTroubleObjSortByCritical(exists->trouble, new->trouble);
}

/*
 * used during 'getTroubles' to perform a sort of troubleContainers by 'critical'
 * (critical flag, then by 'type' and 'subtype').
 */
static int8_t listAppendTroubleContainerSortByCritical(void *newItem, void *element)
{
    // both should be troubleContainer objects
    //
    troubleContainer *exists = (troubleContainer *) element;
    troubleContainer *new = (troubleContainer *) newItem;

    return listAppendTroubleObjSortByCritical(exists->event->trouble, new->event->trouble);
}

/*
 * used during 'getTroubles' to perform a sort of troubleObjs by indicationGroup
 */
static int8_t listAppendTroubleObjSortByIndicationGroup(void *newItem, void *element)
{
    // both should be troubleObj objects
    //
    troubleObj *new = (troubleObj *)newItem;
    troubleObj *exists = (troubleObj *)element;

    // NOTE: doing a DESCENDING sort to return the order:
    //        LIFE_SAFETY
    //        SYSTEM
    //        BURG
    //        IOT
    //
    if (exists->indicationGroup < new->indicationGroup)
    {
        return -1;
    }
    else if (exists->indicationGroup > new->indicationGroup)
    {
        return 1;
    }

    return 0;
}

/*
 * used during 'getTroubles' to perform a sort of troubleEvents by indicationGroup
 */
static int8_t listAppendTroubleEventSortByIndicationGroup(void *newItem, void *element)
{
    // both should be troubleEvent objects
    //
    troubleEvent *new = (troubleEvent *) newItem;
    troubleEvent *exists = (troubleEvent *) element;

    return listAppendTroubleObjSortByIndicationGroup(new->trouble, exists->trouble);
}

/*
 * used during 'getTroubles' to perform a sort of troubleContainers by indicationGroup
 */
static int8_t listAppendTroubleContainerSortByIndicationGroup(void *newItem, void *element)
{
    // both should be troubleContainer objects
    //
    troubleContainer *new = (troubleContainer *) newItem;
    troubleContainer *exists = (troubleContainer *) element;

    return listAppendTroubleObjSortByIndicationGroup(new->event->trouble, exists->event->trouble);
}

/*
 * return the correct sort function for adding to a sortedLinkedList
 */
static linkedListSortCompareFunc getSortFunc(troubleSortAlgo sort, troubleFormat format)
{
    switch (sort)
    {
        case TROUBLE_SORT_BY_CREATE_DATE:
        {
            switch(format)
            {
                case TROUBLE_FORMAT_CONTAINER:
                    return listAppendTroubleContainerSortByDate;

                case TROUBLE_FORMAT_EVENT:
                    return listAppendTroubleEventSortByDate;

                case TROUBLE_FORMAT_OBJ:
                    return listAppendTroubleObjSortByDate;
            }
            break;
        }

        case TROUBLE_SORT_BY_CRITICALITY:
        {
            switch(format)
            {
                case TROUBLE_FORMAT_CONTAINER:
                    return listAppendTroubleContainerSortByCritical;

                case TROUBLE_FORMAT_EVENT:
                    return listAppendTroubleEventSortByCritical;

                case TROUBLE_FORMAT_OBJ:
                    return listAppendTroubleObjSortByCritical;
            }
            break;
        }

        case TROUBLE_SORT_BY_INDICATION_GROUP:
        {
            switch(format)
            {
                case TROUBLE_FORMAT_CONTAINER:
                    return listAppendTroubleContainerSortByIndicationGroup;

                case TROUBLE_FORMAT_EVENT:
                    return listAppendTroubleEventSortByIndicationGroup;

                case TROUBLE_FORMAT_OBJ:
                    return listAppendTroubleObjSortByIndicationGroup;
            }
            break;
        }
    }
    return NULL;
}

/**
 * Helper function which takes a callback for mutating the current troubles metadata JSON.
 * NOTE: this makes an IPC call to deviceService
 *
 * @param deviceTroublePayload device trouble payload
 * @param mutatorFunction the callback function which mutates the JSON
 * @param context the context to pass to the mutator function (troubleEvent)
 */
static void mutateTroubleMetadataOnDevice(DeviceTroublePayload *deviceTroublePayload, bool (*mutatorFunction)(cJSON *, void *), void *context)
{
    char *metadataStr = NULL;
    if (deviceHelperReadMetadataByOwner(deviceTroublePayload->ownerUri, DEVICE_TROUBLE_METADATA, &metadataStr))
    {
        cJSON *metadataJSON = NULL;
        if (metadataStr == NULL || strlen(metadataStr) == 0)
        {
            // No existing value, create new
            metadataJSON = cJSON_CreateObject();
        }
        else
        {
            metadataJSON = cJSON_Parse(metadataStr);
        }
        // Clean up
        free(metadataStr);

        // Mutate it
        if (mutatorFunction(metadataJSON, context))
        {
            // Create new metadata and write it
            metadataStr = cJSON_Print(metadataJSON);
            if (!deviceHelperWriteMetadataByOwner(deviceTroublePayload->ownerUri, DEVICE_TROUBLE_METADATA, metadataStr))
            {
                icLogError(SECURITY_LOG, "Failed to write device trouble metadata for uri %s", deviceTroublePayload->ownerUri);
            }
        }
        // Clean up
        cJSON_Delete(metadataJSON);
    }
    else
    {
        icLogError(SECURITY_LOG, "Failed to read device trouble metadata for uri %s", deviceTroublePayload->ownerUri);
    }

    free(metadataStr);
}

/**
 * callback for mutating metadata to add/update a trouble to the trouble metadata
 *
 * @param metadataJSON the JSON metadata of the device/endpoint
 * @param context the trouble event to add
 * @return true if success, false otherwise
 */
static bool addOrUpdateTroubleMetadataOnDevice(cJSON *metadataJSON, void *context)
{
    troubleEvent *event = (troubleEvent *)context;
    // Add it
    char buf[64];
    sprintf(buf, "%"PRIu64, event->trouble->troubleId);
    // Clean out something if it was already there
    cJSON_DeleteItemFromObject(metadataJSON, buf);
    // Now add
    cJSON_AddItemToObject(metadataJSON, buf, encode_troubleEvent_toJSON(event));

    return true;
}

/**
 * callback for mutating metadata to remove a trouble from the trouble metadata
 *
 * @param metadataJSON the JSON metadata of the device/endpoint
 * @param context the troubl event to remove
 * @return true if success, false otherwise
 */
static bool removeTroubleMetadataFromDevice(cJSON *metadataJSON, void *context)
{
    troubleEvent *event = (troubleEvent *)context;
    // Remove it
    char buf[64];
    sprintf(buf, "%"PRIu64, event->trouble->troubleId);
    cJSON_DeleteItemFromObject(metadataJSON, buf);

    return true;
}

/**
 * Add or update a non device trouble event to storage
 * @param troubleEvent the trouble event to add
 * @return true if success, false otherwise
 */
static bool addOrUpdateNonDeviceTrouble(troubleEvent *troubleEvent)
{
    bool retval = false;
    if (troubleEvent != NULL && troubleEvent->trouble != NULL)
    {
        cJSON *troubleJSON = encode_troubleEvent_toJSON(troubleEvent);
        char *trouble = cJSON_Print(troubleJSON);
        char *key = stringBuilder("%s_%s",
                                  troubleTypeLabels[troubleEvent->trouble->type],
                                  troubleReasonLabels[troubleEvent->trouble->reason]);
        retval = storageSave(NON_DEVICE_TROUBLES_NAMESPACE, key, trouble);
        cJSON_Delete(troubleJSON);
        free(trouble);
        free(key);
    }

    return retval;
}

/**
 * Remove a non device trouble event from storage
 * @param troubleEvent the trouble event to remove
 * @return true if success, false otherwise
 */
static bool removeNonDeviceTrouble(troubleEvent *troubleEvent)
{
    bool retval = false;
    if (troubleEvent != NULL && troubleEvent->trouble != NULL)
    {
        char *key = stringBuilder("%s_%s",
                                  troubleTypeLabels[troubleEvent->trouble->type],
                                  troubleReasonLabels[troubleEvent->trouble->reason]);
        retval = storageDelete(NON_DEVICE_TROUBLES_NAMESPACE, key);
        free(key);
    }
    return retval;
}

// struct used for gathering device IDs for each trouble bucket
//
typedef struct{
    int count;
    icStringBuffer *idList;
} telemetryIDList;


/**
 * Allocates memory for a telemetryIDList struct and allocates memory for idList
 * equal to size TELEMETRY_TROUBLE_BUFFER_SIZE
 * User must free memory
 * @return pointer to allocated memory
 */
static telemetryIDList *createTelemetryIDList()
{
    telemetryIDList *retVal = malloc(sizeof(telemetryIDList));
    retVal->idList = stringBufferCreate(TELEMETRY_TROUBLE_BUFFER_SIZE);
    retVal->count = 0;
    return retVal;
}

/**
 * frees allocated memory in telemetryIDList
 * @param list
 */
static void destroyTelemetryIDList(telemetryIDList *list)
{
    stringBufferDestroy(list->idList);
    free(list);
}

/*
 * Used for hashMapDestroy to free a hashmap of string keys and telemetryIDList items
 */
static void mapTelemetryListFreeFunc(void *key, void* item)
{
    free(key);
    telemetryIDList *list = item;
    destroyTelemetryIDList(list);
}

/*
 * increments the counter in the telemetryIDList, and will append the ID to the idList
 * if the ID is supplied
 */
static void addToTelemetryIDList(telemetryIDList* list, const char* id)
{
    if(list != NULL)
    {
        // increment the list
        list->count++;
        // if the ID exists, add it to the list
        if(stringIsEmpty(id) == false)
        {
            // add a comma if there are already IDs in the list
            if(stringBufferLength(list->idList) > 0)
            {
                stringBufferAppend(list->idList, ",");
            }
            stringBufferAppend(list->idList,id);
        }
    }
}

/*
 * this processes troubles and adds them to the runtime stats pojo in a format
 * that is friendly for telemetry.  The key will be a combination of <device type>
 * and <trouble reason>. Example:  ZONE_TROUBLE_REASON_TAMPER.
 * The value will be a comma separated list starting with the count of that trouble,
 * followed by affected device IDs.  Example:  "2,000d6f0004a60810,000d6f00054b07e3"
 *
 * Complete Example:  "ZONE_TROUBLE_REASON_COMM_FAIL":        "2,000d6f0004a60810,000d6f00054b07e3"
 */
void collectTroubleEventStatistics(runtimeStatsPojo *output)
{

    // get the trouble list
    //
    icLinkedList *collectTroubleList = linkedListCreate();
    getTroublesPublic(collectTroubleList, TROUBLE_FORMAT_CONTAINER, true, TROUBLE_SORT_BY_CREATE_DATE);

    // create a hashmap to hold the trouble keys and the ID lists
    icHashMap *telemetryTroubleMap = hashMapCreate();

    // loop through the trouble list
    icLinkedListIterator *listIter = linkedListIteratorCreate(collectTroubleList);
    while(linkedListIteratorHasNext(listIter))
    {
        troubleContainer *currTrouble = (troubleContainer *) linkedListIteratorGetNext(listIter);
        // sanity check
        if (currTrouble == NULL)
        {
            continue;
        }
        // define variables to hold the key and possible ID
        char *key = NULL;
        char *id = NULL;

        // determine the payload type, this will determine how the key and id are found
        switch(currTrouble->payloadType)
        {
            case TROUBLE_DEVICE_TYPE_ZONE:
            {
                key =  stringBuilder("ZONE_%s",
                        troubleReasonLabels[currTrouble->event->trouble->reason]);
                id = stringBuilder("%s",currTrouble->extraPayload.zone->deviceTrouble->rootId);

                break;
            }
            case TROUBLE_DEVICE_TYPE_IOT:
            {
                key = stringBuilder("%s_%s",
                        currTrouble->extraPayload.device->deviceClass,
                        troubleReasonLabels[currTrouble->event->trouble->reason]);
                id = stringBuilder("%s", currTrouble->extraPayload.device->rootId);

                break;
            }
            case TROUBLE_DEVICE_TYPE_CAMERA:
            {
                key = stringBuilder("CAMERA_%s",
                        troubleReasonLabels[currTrouble->event->trouble->reason]);
                id = stringBuilder("%s",currTrouble->extraPayload.camera->deviceTrouble->rootId);

                break;
            }
            case TROUBLE_DEVICE_TYPE_NONE:
            {
                key = stringBuilder("SYSTEM_%s",
                                    troubleReasonLabels[currTrouble->event->trouble->reason]);
                break;
            }
            default:
            {
                //skip this
                key = NULL;
                id = NULL;
                break;
            }
        }

        // find the appropriate list for that trouble based on the key,
        // and add the ID to the list, and increase the counter
        if(stringIsEmpty(key) == false)
        {
            // see if the key already exists in the map
            //
            if (hashMapContains(telemetryTroubleMap, key, strlen(key)) == true)
            {
                // get the list, add the ID and increase the counter
                telemetryIDList *list = hashMapGet(telemetryTroubleMap, key, strlen(key));
                addToTelemetryIDList(list,id);
                
            }
            else
            {
                // create a new list, add the id if necessary, and increase the counter
                telemetryIDList *list = createTelemetryIDList();
                addToTelemetryIDList(list,id);

                //put the list in the hashmap
                hashMapPut(telemetryTroubleMap, strdup(key), strlen(key), list);
            }
        }

        free(key);
        free(id);
    }

    // iterate through the hashmap, and add each list to the pojo
    icHashMapIterator *iter = hashMapIteratorCreate(telemetryTroubleMap);
    while(hashMapIteratorHasNext(iter) == true)
    {
        telemetryIDList *list;
        void *key;
        char *value;
        uint16_t keylen = 0;
        // get the next item in the map
        hashMapIteratorGetNext(iter, &key, &keylen, (void **)&list);

        // sanity check
        //
        if(list != NULL)
        {
            char *idList = stringBufferToString(list->idList);
            // if the list has IDs, then add them after the count
            // some troubles won't generate ids, such as system troubles
            // those will just have the count and it will always be 1
            //
            if (stringIsEmpty(idList) == false)
            {
                value = stringBuilder("%d,%s", list->count, idList);
            }
            else
            {
                value = stringBuilder("%d", list->count);
            }

            // put the key and value strings in the pojo
            //
            put_string_in_runtimeStatsPojo(output, key, value);

            // free value, key will be cleaned up with the hashmap
            //
            free(value);
            free(idList);
        }
    }

    // clean up
    //
    hashMapIteratorDestroy(iter);
    hashMapDestroy(telemetryTroubleMap, mapTelemetryListFreeFunc);

    linkedListIteratorDestroy(listIter);
    linkedListDestroy(collectTroubleList, (linkedListItemFreeFunc) destroyTroubleContainer);
}

/*
 * populate the 'targetList' with trouble clones (container, event or obj).
 * the list contents are dictated by the input parameters.
 * caller is responsible for destroying the returned list and contents.
 *
 * @param outputList - the linked list to place "clones" of troubleEvent/troubleObj
 * @oaram outputFormat - what gets into outputList: troubleContainer, troubleEvent, or troubleObj
 * @oaram includeActTroubles - if true, place acknowledged troubles into the 'outputList'
 * @param sort - sorting mechanism to utilize when adding to 'outputList'
 *
 * @see linkedListDestroy()
 * @see destroy_troubleEvent()
 * @see destroy_troubleObj()
 */
void getTroublesPublic(icLinkedList *outputList, troubleFormat outputFormat, bool includeAckTroubles, troubleSortAlgo sort)
{
    // sanity check
    //
    if (outputList == NULL)
    {
        return;
    }
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return;
    }

    // determine the 'sort' function to use when adding items to 'outputList'.
    // the function is specific to the format we're placing into the outputList.
    //
    linkedListSortCompareFunc sortFunc = getSortFunc(sort, outputFormat);

    // loop through the global list of troubleContainer objects
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(troubleList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // get the next event in the list
        //
        troubleContainer *next = (troubleContainer *)linkedListIteratorGetNext(loop);
        if (includeAckTroubles == false && next->event->trouble->acknowledged == true)
        {
            // skip since this is acknowledge and caller said NOT to include those
            //
            continue;
        }

        // add the object (in the correct format) to the output list
        //
        switch (outputFormat)
        {
            case TROUBLE_FORMAT_CONTAINER:
                sortedLinkedListAdd(outputList, cloneTroubleContainer(next), sortFunc);
                break;

            case TROUBLE_FORMAT_EVENT:
                sortedLinkedListAdd(outputList, clone_troubleEvent(next->event), sortFunc);
                break;

            case TROUBLE_FORMAT_OBJ:
                sortedLinkedListAdd(outputList, clone_troubleObj(next->event->trouble), sortFunc);
                break;
        }
    }

    // cleanup
    //
    linkedListIteratorDestroy(loop);
    unlockSecurityMutex();
}

/*
 * populate the 'targetList' with trouble clones (container, event or obj).
 * with a specific device.  the list contents are dictated by the input parameters.
 * caller is responsible for destroying the returned list and contents.
 *
 * @param outputList - the linked list to place "clones" of troubleEvent/troubleObj
 * @param uri - the uri prefix to look for troubles
 * @oaram outputFormat - what gets into outputList: troubleContainer, troubleEvent, or troubleObj
 * @oaram includeActTroubles - if true, place acknowledged troubles into the 'outputList'
 * @param sort - sorting mechanism to utilize when adding to 'outputList'
 *
 * @see linkedListDestroy()
 * @see destroy_troubleEvent()
 * @see destroy_troubleObj()
 */
void getTroublesForDeviceUriPublic(icLinkedList *outputList, char *uri, troubleFormat outputFormat,
                                   bool includeAckTroubles, troubleSortAlgo sort)
{
    // sanity check
    //
    if (outputList == NULL || uri == NULL)
    {
        return;
    }
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return;
    }

    // now that we have the lock, call the private version
    //
    getTroublesForUriPrivate(outputList, uri, true, outputFormat, includeAckTroubles, sort);
    unlockSecurityMutex();
}

/*
 * populate the 'targetList' with trouble clones (container, event or obj).
 * caller is responsible for destroying the returned list (and contents if makeClone == true).
 *
 * NOTE: internal version that assumes the mutex is held
 *
 * @param outputList - the linked list to place the troubleContainer objects
 * @param uri - the device uri prefix to retrieve troubles about
 * @param makeClone - if true, outputList will containe clones (otherwise return the original objects)
 * @oaram outputFormat - what gets into outputList: troubleContainer, troubleEvent, or troubleObj
 * @oaram includeActTroubles - if true, also include acknowledged troubles
 * @param sort - sorting mechanism to utilize when adding to 'outputList'
 *
 * @see linkedListDestroy()
 * @see destroy_troubleEvent()
 */
void getTroublesForUriPrivate(icLinkedList *outputList, char *uri, bool makeClone,
                              troubleFormat outputFormat, bool includeAckTroubles, troubleSortAlgo sort)
{
    // sanity check
    //
    if (outputList == NULL || uri == NULL)
    {
        return;
    }
    if (didInit == false)
    {
        return;
    }

    // determine the 'sort' function to use when adding items to 'outputList'.
    // the function is specific to the format we're placing into the outputList.
    //
    linkedListSortCompareFunc sortFunc = getSortFunc(sort, outputFormat);

    // loop through our global list of troubleContainer objects
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(troubleList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // get the next trouble
        //
        troubleContainer *next = (troubleContainer *)linkedListIteratorGetNext(loop);
        if (next->event->trouble->type == TROUBLE_TYPE_DEVICE)
        {
            if (includeAckTroubles == false && next->event->trouble->acknowledged == true)
            {
                // skip since this is acknowledge and caller said NOT to include those
                //
                continue;
            }

            // look at the payloadType in the container.  since we're searching for 'device'
            // then only examine this if zone or iot
            //
            DeviceTroublePayload *deviceTroublePayload = NULL;
            switch (next->payloadType)
            {
                case TROUBLE_DEVICE_TYPE_ZONE:
                    deviceTroublePayload = next->extraPayload.zone->deviceTrouble;
                    break;

                case TROUBLE_DEVICE_TYPE_CAMERA:
                    deviceTroublePayload = next->extraPayload.camera->deviceTrouble;
                    break;

                case TROUBLE_DEVICE_TYPE_IOT:
                    deviceTroublePayload = next->extraPayload.device;
                    break;

                case TROUBLE_DEVICE_TYPE_NONE:
                    // not using 'default' on purpose here
                    continue;
            }

            // examine the payload to see if this is the device we're looking for
            //
            if (deviceTroublePayload != NULL)
            {
                if (stringStartsWith(deviceTroublePayload->ownerUri, uri, false) == false)
                {
                    // doesn't match the device we're looking for
                    //
                    continue;
                }

                // found a match
                //
                if (makeClone == true)
                {
                    // add a cloned version in the the requested format
                    //
                    switch (outputFormat)
                    {
                        case TROUBLE_FORMAT_CONTAINER:
                            sortedLinkedListAdd(outputList, cloneTroubleContainer(next), sortFunc);
                            break;

                        case TROUBLE_FORMAT_EVENT:
                            sortedLinkedListAdd(outputList, clone_troubleEvent(next->event), sortFunc);
                            break;

                        case TROUBLE_FORMAT_OBJ:
                            sortedLinkedListAdd(outputList, clone_troubleObj(next->event->trouble), sortFunc);
                            break;
                    }
                }
                else
                {
                    // add the original
                    //
                    switch (outputFormat)
                    {
                        case TROUBLE_FORMAT_CONTAINER:
                            sortedLinkedListAdd(outputList, next, sortFunc);
                            break;

                        case TROUBLE_FORMAT_EVENT:
                            sortedLinkedListAdd(outputList, next->event, sortFunc);
                            break;

                        case TROUBLE_FORMAT_OBJ:
                            sortedLinkedListAdd(outputList, next->event->trouble, sortFunc);
                            break;
                    }
                }
            }
        }
    }

    // Cleanup
    linkedListIteratorDestroy(loop);
}


/*
 * return a list of original troubleContainer objects that match this zone.
 * the elements will be the original, so caller should not free directly
 */
icLinkedList *getTroubleContainersForZonePrivate(uint32_t zoneNumber)
{
    // sanity check
    //
    if (zoneNumber == 0 || didInit == false)
    {
        return NULL;
    }

    // loop through our list
    //
    icLinkedList *retVal = linkedListCreate();
    icLinkedListIterator *loop = linkedListIteratorCreate(troubleList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // get the next trouble in the list
        //
        troubleContainer *next = (troubleContainer *)linkedListIteratorGetNext(loop);

        // look at the payloadType in the container.  since we're searching for 'device'
        // then only examine this if zone or iot
        //
        if (next->payloadType == TROUBLE_DEVICE_TYPE_ZONE)
        {
            // Make sure its a trouble for the same zone
            if (next->extraPayload.zone != NULL && next->extraPayload.zone->zoneNumber == zoneNumber)
            {
                // add the original container
                linkedListAppend(retVal, next);
            }
        }
    }

    // Cleanup
    linkedListIteratorDestroy(loop);
    return retVal;
}

/*
 * count up how many troubles we have for this particular zone.
 * used to clear the 'isTroubled' flag within the device/zone
 */
static uint32_t countTroublesForZonePrivate(securityZone *zone)
{
    uint32_t retVal = 0;

    // sanity check
    //
    if (zone == NULL || didInit == false)
    {
        return retVal;
    }

    // create the URI to search for
    //
    char *deviceUri = createDeviceUri(zone->deviceId);

    // loop through our list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(troubleList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // get the next trouble in the list
        //
        troubleContainer *next = (troubleContainer *)linkedListIteratorGetNext(loop);

        // look at the payloadType in the container.  since we're searching for 'device'
        // then only examine this if zone or iot
        //
        switch (next->payloadType)
        {
            case TROUBLE_DEVICE_TYPE_ZONE:
                // Make sure its a trouble for the same zone
                if (next->extraPayload.zone != NULL && next->extraPayload.zone->zoneNumber == zone->zoneNumber)
                {
                    retVal++;
                }
                break;

            case TROUBLE_DEVICE_TYPE_IOT:
                // For PIM/PRM it could be a trouble related to the device itself(but not some unrelated endpoint)
                if (next->extraPayload.device != NULL &&
                    stringCompare(next->extraPayload.device->ownerUri, deviceUri, false) == 0)
                {
                    retVal++;
                }
                break;

            case TROUBLE_DEVICE_TYPE_CAMERA:
            case TROUBLE_DEVICE_TYPE_NONE:
                // not using 'default' on purpose here
                continue;
        }
    }

    // Cleanup
    linkedListIteratorDestroy(loop);
    free(deviceUri);
    return retVal;
}

/*
 * count the troubles that match the provided 'troubleType' and 'troubleReason'.
 */
uint32_t getTroubleCountForTypePublic(troubleType type, troubleReason reason)
{
    uint32_t retVal = 0;
    
    // sanity check
    //
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return retVal;
    }

    // loop through our list, looking for matches
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(troubleList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // get the next event in the list
        //
        troubleContainer *next = (troubleContainer *) linkedListIteratorGetNext(loop);
        if (next->event->trouble->type == type)
        {
            // match on type. see if we need to also compare the reason
            //
            if (reason == TROUBLE_REASON_NONE || reason == next->event->trouble->reason)
            {
                // found a match
                //
                retVal++;
            }
        }
    }
    linkedListIteratorDestroy(loop);
    unlockSecurityMutex();
    
    return retVal;
}

/*
 * return if there are any unacknowledged 'system' troubles present.
 */
bool areSystemTroublesPresentPrivate()
{
    if (didInit == true)
    {
        // rely on overall flag to be up-to-date
        //
        return haveSystemTroubles;
    }
    return false;
}

/*
 * returns if the system is tampered (as a trouble)
 */
bool hasSystemTamperedTroublePrivate()
{
    // special case for alarm panel since the only system trouble
    // that prevents it from being READY is the system tamper
    //
    if (didInit == true)
    {
        // rely on overall flag to be up-to-date
        //
        return haveSystemTamper;
    }
    return false;
}

/*
 * acknowledge a single trouble event
 */
void acknowledgeTroublePublic(uint64_t troubleId)
{
    lockSecurityMutex();
    if (didInit == true)
    {
        // find the troubleContainer with this troubleId
        //
        troubleContainer *found = linkedListFind(troubleList, &troubleId, findTroubleContainerByTroubleOrEventId);
        if (found != NULL && found->event->trouble->acknowledged == false)
        {
            // set flag, then persist and broadcast the event
            //
            found->event->trouble->acknowledged = true;

            // while we still HAVE THE LOCK, update the panel status
            //
            populateSystemPanelStatusPrivate(found->event->panelStatus);

            // update the replay trackers
            //
            startTroubleReplayTrackers(found->event);

            // perform the persistance to deviceService (in the background) along
            // with the broadcasting of the event
            //
            troubleContainer *copy = cloneTroubleContainer(found);
            int32_t *eventCode = (int32_t *)malloc(sizeof(int32_t));
            *eventCode = TROUBLE_ACKNOWLEDGED_EVENT;

            if (appendSecurityTask(copy, eventCode, updateTroubleTaskRun, updateTroubleTaskFree) == false)
            {
                // executor should have called addTroubleTaskFree to free 'dup'
                icLogWarn(SECURITY_LOG, "Failed queueing trouble update task: executor rejected job");
            }
        }
        else
        {
            if (found == NULL)
            {
                icLogWarn(SECURITY_LOG, "Unable to acknowledge trouble %"PRIu64"; cannot locate trouble with that id", troubleId);
            }
            else
            {
                icLogWarn(SECURITY_LOG, "Unable to acknowledge trouble %"PRIu64"; trouble already acknowledged", troubleId);
            }
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "Unable to acknowledge trouble %"PRIu64"; system is not initialized", troubleId);
    }
    unlockSecurityMutex();
}

/*
 * un-acknowledge a single trouble event
 */
void unacknowledgeTroublePublic(uint64_t troubleId)
{
    lockSecurityMutex();
    if (didInit == true)
    {
        // do the unack and send an event
        //
        unacknowledgeTroublePrivate(troubleId, true);
    }
    unlockSecurityMutex();
}

/*
 * un-acknowledge a single trouble event
 */
void unacknowledgeTroublePrivate(uint64_t troubleId, bool sendEvent)
{
    // find the troubleEvent with this troubleId
    //
    troubleContainer *found = linkedListFind(troubleList, &troubleId, findTroubleContainerByTroubleOrEventId);
    if (found != NULL && found->event->trouble->acknowledged == true)
    {
        // reset flag, then broadcast the event
        //
        found->event->trouble->acknowledged = false;
        if (sendEvent == true)
        {
            // while we still HAVE THE LOCK, update the panel status
            //
            populateSystemPanelStatusPrivate(found->event->panelStatus);

            // perform the persistance to deviceService (in the background) along
            // with the broadcasting of the event
            //
            troubleContainer *copy = cloneTroubleContainer(found);
            int32_t *eventCode = (int32_t *)malloc(sizeof(int32_t));
            *eventCode = TROUBLE_UNACKNOWLEDGED_EVENT;

            if (appendSecurityTask(copy, eventCode, updateTroubleTaskRun, updateTroubleTaskFree) == false)
            {
                // executor should have called addTroubleTaskFree to free 'dup'
                icLogWarn(SECURITY_LOG, "Failed queueing trouble update task: executor rejected job");
            }
        }
    }
}

/*
 * taskExecutor 'run' function for delivering events from addTroubleEventPublic() and updating the device storage
 */
static void addTroubleTaskRun(void *taskObj, void *taskArg)
{
    // sanity check
    if (taskObj == NULL)
    {
        return;
    }
    bool sendEvent = false;
    if (taskArg != NULL)
    {
        bool *tmp = (bool *)taskArg;
        sendEvent = *tmp;
    }
    troubleContainer *container = (troubleContainer *)taskObj;

    // persist if we have metadata
    //
    if(container->persist == true)
    {
        updateTroubleNoSendTaskRun(container, NULL);
    }

    if (sendEvent == true)
    {
        // send the "trouble added" event
        //
        broadcastTroubleEvent(container->event, TROUBLE_OCCURED_EVENT, 0);
    }
    else
    {
        icLogInfo(SECURITY_LOG, "Told to not broadcast trouble %"PRIu64, container->event->trouble->troubleId);
    }

    // destroy the container when our 'addTroubleTaskFree' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * addTroubleTaskRun function
 */
static void addTroubleTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event & arg
    //
    troubleContainer *container = (troubleContainer *)taskObj;
    destroyTroubleContainer(container);
    free(taskArg);
}

/*
 * taskExecutor 'run' function for delivering events from a trouble update and updating the device storage
 */
static void updateTroubleTaskRun(void *taskObj, void *taskArg)
{
    troubleContainer *container = (troubleContainer *)taskObj;
    int32_t *eventCode = (int32_t *)taskArg;

    // persist if we have metadata
    //
    if(container->persist == true)
    {
        updateTroubleNoSendTaskRun(container, NULL);
    }

    // send the "trouble" event, using the code in taskArg
    //
    broadcastTroubleEvent(container->event, *eventCode, 0);

    // destroy the container when our 'updateTroubleTaskFree' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * updateTroubleTaskRun function
 */
static void updateTroubleTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event & arg
    //
    troubleContainer *container = (troubleContainer *)taskObj;
    destroyTroubleContainer(container);
    free(taskArg);
}

/*
 * taskExecutor 'run' function for updating the trouble details in device storage,
 * but NOT broadcast the event
 */
static void updateTroubleNoSendTaskRun(void *taskObj, void *taskArg)
{
    troubleContainer *container = (troubleContainer *)taskObj;

    // persist if we have metadata
    //
    if (container->payloadType == TROUBLE_DEVICE_TYPE_IOT && container->extraPayload.device != NULL)
    {
        mutateTroubleMetadataOnDevice(container->extraPayload.device, addOrUpdateTroubleMetadataOnDevice, container->event);
    }
    else if (container->payloadType == TROUBLE_DEVICE_TYPE_ZONE && container->extraPayload.zone != NULL)
    {
        mutateTroubleMetadataOnDevice(container->extraPayload.zone->deviceTrouble, addOrUpdateTroubleMetadataOnDevice, container->event);
    }
    else if (container->payloadType == TROUBLE_DEVICE_TYPE_CAMERA && container->extraPayload.camera != NULL)
    {
        mutateTroubleMetadataOnDevice(container->extraPayload.camera->deviceTrouble, addOrUpdateTroubleMetadataOnDevice, container->event);
    }
    else if (container->event->trouble->type == TROUBLE_TYPE_SYSTEM ||
             container->event->trouble->type == TROUBLE_TYPE_NETWORK ||
             container->event->trouble->type == TROUBLE_TYPE_POWER)
    {
        addOrUpdateNonDeviceTrouble(container->event);
    }

    // destroy the container when our 'updateTroubleTaskFree' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * updateTroubleNoSendTaskRun function
 */
static void updateTroubleNoSendTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event & arg
    //
    troubleContainer *container = (troubleContainer *)taskObj;
    destroyTroubleContainer(container);
}

/*
 * takes ownership of the 'trouble', and adds to the global list.  if successful, assigns
 * (and returns) a troubleId to the trouble before saving and broadcasting the event.
 *
 * a return of 0 means the trouble was not added (probably due to a duplicate trouble),
 * which the caller should look for so it can free up the event
 *
 * @param container - the trouble to process
 * @param compareFunc - function used for comparison as we search for duplicate troubles
 * @param sendEvent - whether we should broadcast the event or not
 */
uint64_t addTroublePublic(troubleContainer *container, troublePayloadCompareFunc compareFunc, bool sendEvent)
{
    uint64_t retVal = 0;

    // ensure we've initialized
    lockSecurityMutex();
    if (didInit == true)
    {
        // do the add
        retVal = addTroubleContainerPrivate(container, compareFunc, sendEvent);
    }
    unlockSecurityMutex();

    return retVal;
}

/*
 * Helper to check if an existing trouble exists.  Assumes caller holds the security mutex
 * @param container the container representing the trouble to search
 * @param compareFunc the trouble payload compare function to use when checking, can be NULL to ignore payload
 * @return true if a trouble exists, false otherwise
 */
static bool hasExistingTrouble(troubleContainer *container, troublePayloadCompareFunc compareFunc)
{
    // create the search object to see if we have another trouble in our list
    // that matches this deviceId, type, and reason.
    //
    troubleSearchParms search;
    search.type = container->event->trouble->type;
    search.reason = container->event->trouble->reason;
    search.compareFunc = compareFunc;
    if (container->event->trouble->extra != NULL)
    {
        search.payload = container->event->trouble->extra;
    }
    else
    {
        search.payload = NULL;
    }

    // run the search
    //
    troubleContainer *dup = linkedListFind(troubleList, &search, findTroubleContainerBySearchParms);
    return dup != NULL;
}

/*
 * add a trouble to the list and takes ownership of the memory.
 * if a troubleId is not assigned (set to 0), then one will be assigned and
 * placed within the the trouble.
 *
 * a return of 0 means the trouble was not added (probably due to this trouble existing),
 * which the caller should look for so it can free up the container
 */
uint64_t addTroubleContainerPrivate(troubleContainer *container, troublePayloadCompareFunc compareFunc, bool sendEvent)
{
    uint64_t retVal = 0;
    if (container == NULL || container->event == NULL || container->event->trouble == NULL)
    {
        return retVal;
    }

    // Make sure the trouble does not already exist
    if (hasExistingTrouble(container, compareFunc) == true)
    {
        // already have a trouble for this composite key
        //
        icLogDebug(SECURITY_LOG, "not adding trouble eventId %" PRIu64", already have a trouble for type=%s, reason=%s",
                   container->event->baseEvent.eventId,
                   troubleTypeLabels[container->event->trouble->type],
                   troubleReasonLabels[container->event->trouble->reason]);
        return 0;
    }

    // assign a new troubleId
    //
    retVal = getNextEventId();
    container->event->trouble->troubleId = retVal;

    // append to our total list of troubles
    //
    addTroubleToListPrivate(container);

    // while we still HAVE THE LOCK, possibly forward the trouble over to alarmPanel
    // so it can start an alarm, update ready status, etc.  to reduce the overhead,
    // we'll only do this when it's a device or system trouble
    //
    if (container->event->trouble->type == TROUBLE_TYPE_SYSTEM ||
        container->event->trouble->type == TROUBLE_TYPE_POWER ||
        container->event->trouble->type == TROUBLE_TYPE_DEVICE)
    {
        processTroubleContainerForAlarmPanel(container);
    }
    else
    {
        populateSystemPanelStatusPrivate(container->event->panelStatus);
    }

    // stinks, but go ahead and make a copy of the trouble persist/broadcast.
    // needed because we want to drop this into the taskExecutor for processing
    // outside of the mutex (and in FIFO fashion)
    //
    troubleContainer *dup = cloneTroubleContainer(container);
    bool *sendEventArg = (bool *)malloc(sizeof(bool));
    *sendEventArg = sendEvent;
    if (appendSecurityTask(dup, sendEventArg, addTroubleTaskRun, addTroubleTaskFree) == false)
    {
        // executor should have called addTroubleTaskFree to free 'dup'
        icLogWarn(SECURITY_LOG, "Failed queueing trouble add task: executor rejected job");
    }

    return retVal;
}

/*
 * taskExecutor 'run' function for delivering events
 * from clearTroubleEventPublic() and updating the device storage
 */
static void clearTroubleTaskRun(void *taskObj, void *taskArg)
{
    troubleContainer *cont = (troubleContainer *)taskObj;
    bool *sendEvent = (bool *)taskArg;

    if (cont == NULL || cont->event == NULL || cont->event->trouble == NULL)
    {
        return;
    }

    // Get the trouble payload so we can remove it
    DeviceTroublePayload *payload = NULL;
    switch (cont->payloadType)
    {
        case TROUBLE_DEVICE_TYPE_ZONE:
            if (cont->extraPayload.zone != NULL)
            {
                payload = cont->extraPayload.zone->deviceTrouble;
            }
            break;

        case TROUBLE_DEVICE_TYPE_CAMERA:
            if (cont->extraPayload.camera != NULL)
            {
                payload = cont->extraPayload.camera->deviceTrouble;
            }
            break;

        case TROUBLE_DEVICE_TYPE_IOT:
            payload = cont->extraPayload.device;
            break;

        case TROUBLE_DEVICE_TYPE_NONE:
            // not using 'default' on purpose here
            break;
    }
    // persist if we have metadata
    //
    if (payload != NULL && cont->persist == true)
    {
        mutateTroubleMetadataOnDevice(payload, removeTroubleMetadataFromDevice, cont->event);
    }
    // Leaving out network troubles, seems weird to persist them, but could add them easily if necessary
    else if ((cont->event->trouble->type == TROUBLE_TYPE_SYSTEM ||
             cont->event->trouble->type == TROUBLE_TYPE_POWER ||
             cont->event->trouble->type == TROUBLE_TYPE_NETWORK) &&
             cont->persist == true)
    {
        removeNonDeviceTrouble(cont->event);
    }

    if (sendEvent == NULL || *sendEvent == true)
    {
        // send the "trouble cleared" event
        //
        broadcastTroubleEvent(cont->event, TROUBLE_CLEARED_EVENT, 0);
    }

    // destroy the container and arg when our 'free' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * clearTroubleTaskRun function
 */
static void clearTroubleTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event & arg
    //
    troubleContainer *cont = (troubleContainer *)taskObj;
    destroyTroubleContainer(cont);

    free(taskArg);
}

/*
 * find trouble in our list using another trouble as the guide
 */
static troubleContainer *locateTroubleUsingEvent(troubleEvent *searchWith, troublePayloadCompareFunc compareFunc)
{
    // first try with the troubleId of the supplied event
    //
    troubleContainer *found = linkedListFind(troubleList, &(searchWith->trouble->troubleId), findTroubleContainerByTroubleId);
    if (found == NULL)
    {
        // try locating it based of other information
        //
        troubleSearchParms search;
        search.type = searchWith->trouble->type;
        search.reason = searchWith->trouble->reason;
        search.compareFunc = compareFunc;
        if (searchWith->trouble->extra != NULL)
        {
            search.payload = searchWith->trouble->extra;
        }
        else
        {
            search.payload = NULL;
        }

        found = linkedListFind(troubleList, &search, findTroubleContainerBySearchParms);
    }

    return found;
}

/*
 * clear the trouble and potentially delete the original
 */
static bool removeTroubleAndSendEvent(troubleContainer *original, bool sendEvent)
{
    // save off if this was a system event (so we rescan after the delete)
    //
    bool wasSystemEvent = (original->event->trouble->type == TROUBLE_TYPE_SYSTEM ||
                           original->event->trouble->type == TROUBLE_TYPE_POWER) ? true : false;

    // if this trouble is for a zone, see if we need to clear the 'isTroubled' flag
    //
    if (original->payloadType == TROUBLE_DEVICE_TYPE_ZONE && original->extraPayload.zone != NULL)
    {
        securityZone *zone = findSecurityZoneForNumberPrivate(original->extraPayload.zone->zoneNumber);
        if (zone != NULL)
        {
            uint32_t count = countTroublesForZonePrivate(zone);
            if (count <= 1)
            {
                // zone no longer troubled
                //
                zone->isTroubled = false;
            }
        }
    }

    // since we have our match, remove it from the set - but DO NOT FREE
    // it since we'll need it for the event broadcast
    //
    bool removed = false;
    removed = linkedListDelete(troubleList, &(original->event->trouble->troubleId), findTroubleContainerByTroubleId, standardDoNotFreeFunc);
    if (removed == false)
    {
        // no luck removing based on the troubleId.  try to find this via the trouble values
        //
        troubleContainer *found = locateTroubleUsingEvent(original->event, isMatchingDeviceTroublePayload);
        if (found != NULL)
        {
            // found the real one, so delete it
            //
            removed = linkedListDelete(troubleList, &(found->event->trouble->troubleId), findTroubleContainerByTroubleId, standardDoNotFreeFunc);
            if (removed == true)
            {
                // switch the one passed in for the actual one we found, but preserve the contactId if set
                // TODO: this may not be required anymore... and in fact may be dumb since most contactId values
                //       start with 1 on initiation and 3 on clear
                //
                char *origContactId = NULL;
                if (original->event->alarm != NULL)
                {
                    // steal the old contactId
                    origContactId = original->event->alarm->contactId;
                    original->event->alarm->contactId = NULL;
                }
                destroyTroubleContainer(original);
                original = found;
                if (origContactId != NULL)
                {
                    // apply stolen contactId
                    free(original->event->alarm->contactId);
                    original->event->alarm->contactId = origContactId;
                }
            }
        }
    }

    // doing this after we find the original, so that the restore time is now and not the time of the trouble
    // re-purpose the original trouble as a 'clear', but update the 'time' first.
    //
    setEventTimeToNow(&original->event->baseEvent);
    original->event->trouble->eventTime = convertTimespecToUnixTimeMillis(&original->event->baseEvent.eventTime);

    // Give it a new eventId as well
    original->event->baseEvent.eventId = getNextEventId();
    original->event->baseEvent.eventCode = TROUBLE_CLEARED_EVENT;
    original->event->trouble->restored = true;

    if (removed == false)
    {
        icLogWarn(SECURITY_LOG, "Failed to clear trouble %"PRIu64" as we could not locate the corresponding trouble", original->event->trouble->troubleId);
        return false;
    }

    // rescan our list if this was a system trouble (to update internal flags)
    //
    if (wasSystemEvent == true)
    {
        // reset the flag and see if any other troubles are 'system' events
        //
        haveSystemTroubles = false;
        haveSystemTamper = false;
        linkedListIterate(troubleList, updateSystemTroubleFlag, NULL);
    }

    // if we don't have any more troubles in this bucket, we want to reset the timer for it
    // so that the next trouble comes in fresh
    //
    troubleFilterConstraints *constraints = createTroubleFilterConstraints();
    constraints->ackValue = TROUBLE_ACK_EITHER;
    indicationType allowedTypes[] = {INDICATION_VISUAL, INDICATION_BOTH};
    constraints->allowedIndicationTypes->types = allowedTypes;
    constraints->allowedIndicationTypes->size = 2;

    if(getTroubleCategoryCountPrivate(original->event->trouble->indicationGroup, constraints) == 0)
    {
        icLogTrace(SECURITY_LOG,"%s:  no more troubles for category %s, resetting timers", __FUNCTION__,
                indicationCategoryLabels[original->event->trouble->indicationGroup]);
        resetCategoryReplayTracker(original->event->trouble->indicationGroup);
    }
    destroyTroubleFilterConstraints(constraints);

    // if we have no more troubles, then update the replay tracker
    //FIXME: Right now, existing unackable troubles will cause this check to fail even though they aren't included in
    // replay logic. This means the replay tracker will still check at the interval of the first tracked trouble,
    // even if that trouble has been cleared (as long as an unackable trouble exists on the system).
    //
    if (linkedListCount(troubleList) == 0)
    {
        stopTroubleReplayTrackers();
    }

    // while we still HAVE THE LOCK, possibly forward the trouble over to alarmPanel
    // so it can update ready status, etc.  to reduce the overhead, we'll only do this
    // when it's a device or system trouble
    //
    if (supportAlarms() == true && (wasSystemEvent || original->event->trouble->type == TROUBLE_TYPE_DEVICE))
    {
        processTroubleContainerForAlarmPanel(original);
    }
    else
    {
        populateSystemPanelStatusPrivate(original->event->panelStatus);
    }

    bool *sendEventArg = (bool *)malloc(sizeof(bool));
    *sendEventArg = sendEvent;
    if (appendSecurityTask(original, sendEventArg, clearTroubleTaskRun, clearTroubleTaskFree) == false)
    {
        /* executor called clearTroubleTaskFree to free troubleEvent */
        icLogWarn(SECURITY_LOG, "Failed queueing trouble clear task: executor rejected job");
    }

    // The container is freed by 'clearTroubleTaskFree' above

    return true;
}

/*
 * clear a trouble from the list.  uses as much information from the
 * 'clearEvent' to find the corresponding trouble and remove it from the list.
 * returns true if the clear was successful.
 *
 * @param clearEvent - the trouble to process
 * @param searchForExisting - if true, use the compareFunc to find the corresponding trouble
 * @param compareFunc - function used for comparison as we search for corresponding trouble
 * @param sendEvent - whether we should broadcast the event or not
 */
bool clearTroublePublic(troubleEvent *clearEvent, bool searchForExisting, troublePayloadCompareFunc compareFunc,
                        bool sendEvent)
{
    bool retVal = false;

    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return retVal;
    }

    troubleContainer *found = NULL;
    if (searchForExisting == true)
    {
        // find the troubleEvent we want to remove/clear
        //
        found = locateTroubleUsingEvent(clearEvent, compareFunc);
        if (found != NULL)
        {
            icLogInfo(SECURITY_LOG, "Found existing trouble to clear id=%"PRIu64, found->event->trouble->troubleId);

            // destroy what was supplied, and set the return so caller doesn't free again
            //
            destroy_troubleEvent(clearEvent);
            clearEvent = NULL;
            retVal = true;
        }
    }
    else
    {
        // make an empty container, but use the supplied event since we didn't search
        //
        found = createTroubleContainer();
        found->event = clearEvent;
        // TODO: add devicetroubleinfo

        // set return value to true so caller doesn't double-free
        //
        retVal = true;
    }

    if (found != NULL)
    {
        // clear from our list and send the event
        //
        if (removeTroubleAndSendEvent(found, sendEvent) == false)
        {
            // 'found' wasn't removed so do it now
            destroyTroubleContainer(found);
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "unable to find trouble (for clear) with troubleId=%" PRIu64 ", type=%s, reason=%s",
                  clearEvent->trouble->troubleId,
                  troubleTypeLabels[clearEvent->trouble->type],
                  troubleReasonLabels[clearEvent->trouble->reason]);
    }

    unlockSecurityMutex();

    return retVal;
}

/*
 * clear the supplied trouble from the list.  unlike the 'public' variation,
 * this does not perform a "search for something similar", but instead assumes
 * this is a clone of the actual event to remove.
 */
bool clearTroubleContainerPrivate(troubleContainer *container)
{
    if (didInit == true && container != NULL)
    {
        // since 'container' is not a clone, just move forward with it
        //
        return removeTroubleAndSendEvent(container, true);
    }
    else if (container != NULL)
    {
        icLogWarn(SECURITY_LOG, "unable to find trouble (for clear) with troubleId=%"PRIu64", type=%s, reason=%s",
                  container->event->trouble->troubleId,
                  troubleTypeLabels[container->event->trouble->type],
                  troubleReasonLabels[container->event->trouble->reason]);
    }
    return false;
}

/*
 * linkedListIterateFunc for deleting troubles for a device
 */
static bool clearTroubleForDeviceIteratorFunc(void *item, void *arg)
{
    troubleContainer *trouble = (troubleContainer *)item;
    return removeTroubleAndSendEvent(trouble, true);
}

/*
 * clear all troubles for a specific device.  only called
 * when the device is removed from the system, so therefore
 * does NOT mess with clearing metadata from deviceService.
 */
void clearTroublesForDevicePublic(char *deviceId)
{
    if (deviceId == NULL)
    {
        return;
    }
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return;
    }

    // get all original troubles for this deviceId (not a clone)
    //
    icLinkedList *tmp = linkedListCreate();
    char *deviceUri = createDeviceUri(deviceId);
    getTroublesForUriPrivate(tmp, deviceUri, false, TROUBLE_FORMAT_CONTAINER, true, TROUBLE_SORT_BY_CREATE_DATE);
    free(deviceUri);

    // loop through all troubles, deleting each that has this 'deviceId'
    //
    linkedListIterate(tmp, clearTroubleForDeviceIteratorFunc, NULL);
    linkedListDestroy(tmp, standardDoNotFreeFunc);
    unlockSecurityMutex();
}

/*
 * 'linkedListItemFreeFunc' implementation used when deleting troubleEvent
 * objects from the linked list.
 */
void destroyTroubleEventFromList(void *item)
{
    if (item != NULL)
    {
        troubleEvent *event = (troubleEvent *) item;
        destroy_troubleEvent(event);
    }
}

/*
 * helper function to create a base troubleEvent with some basic information.
 * assumes the caller will assign a troubleId since that is not always generated.
 */
troubleEvent *createBasicTroubleEvent(BaseEvent *base,
                                      troubleType type,
                                      troubleCriticalityType criticality,
                                      troubleReason reason)
{
    troubleEvent *retVal = create_troubleEvent();
    if (retVal != NULL)
    {
        if (base != NULL)
        {
            // copy BaseEvent details (eventId, eventTime, eventCode, eventValue)
            //
            baseEvent_copy(&retVal->baseEvent, base);
        }

        // ensure we have an eventId and a timestamp in the Base
        //
        if (retVal->baseEvent.eventId == 0)
        {
            setEventId(&retVal->baseEvent);
        }
        if (retVal->baseEvent.eventTime.tv_sec == 0)
        {
            // set the time.  we'll fill in the eventId below
            //
            setEventTimeToNow(&retVal->baseEvent);
        }

        // troubleObj has some duplicated info from base
        //
        retVal->trouble->eventId = retVal->baseEvent.eventId;
        retVal->trouble->eventTime = convertTimespecToUnixTimeMillis(&retVal->baseEvent.eventTime);

        // assign the type and critical enums
        //
        retVal->trouble->type = type;
        retVal->trouble->critical = criticality;
        retVal->trouble->reason = reason;
    }
    return retVal;
}

static void populateDeviceTroublePayload(const DSResource *resource, const char *deviceId, DeviceTroublePayload *deviceTroublePayload)
{
    deviceTroublePayload->deviceClass = strdup(resource->ownerClass);
    deviceTroublePayload->rootId = strdup(deviceId);
    deviceTroublePayload->resourceUri = strdup(resource->uri);

    if (stringCompare(resource->ownerId, (char *)deviceId, false) != 0)
    {
        deviceTroublePayload->ownerUri = createEndpointUri(deviceId, resource->ownerId);
    }
    else
    {
        deviceTroublePayload->ownerUri = createDeviceUri(deviceId);
    }
}

/**
 * Returns true if the resource indicates a standard trouble clear (i.e. the value is either NULL or false). There are
 * non-standard cases where a trouble clear is not indicated by true/false/NULL. These cases should be handled differently.
 * @param resource
 * @return Returns true if the resource indicates a standard trouble clear
 */
static bool isTroubleStandardClear(const DSResource *resource)
{
    return resource->value == NULL || strcasecmp("false", resource->value) == 0;
}

/**
 * Simple trouble resource handler whose arg contains static values to use to populate the details
 * @param resource the resource to check
 * @param parentDevice the parent device, may be null
 * @param deviceId the device id
 * @param isTrouble output value for whether this resource is troubled
 * @param critical output value with the criticality of the trouble
 * @param reason output value with the trouble reason
 * @param troubleResourceHandlerArg the optional resource handler argument set when the handler was created
 * @return whether the trouble is a clear or not.
 */
static bool simpleTroubleResourceHandler(const DSResource *resource,
                                         const DSDevice *parentDevice,
                                         const char *deviceId,
                                         bool *isTrouble,
                                         troubleCriticalityType *critical,
                                         troubleReason *reason,
                                         void *troubleResourceHandlerArg)
{
    SimpleTroubleResourceHandlerArg *arg = (SimpleTroubleResourceHandlerArg *)troubleResourceHandlerArg;
    *isTrouble = arg->isTrouble;
    *critical = arg->critical;
    *reason = arg->reason;

    return isTroubleStandardClear(resource);
}

/*
 * callback from commFailTimer when a device is considered in communcation failure.
 * used to create and broadcast the trouble
 */
static void deviceCommFailNotify(const DSDevice *device, commFailTimerType type)
{
    if (type == TROUBLE_DELAY_TIMER)
    {
        // device is now considered COMM FAILURE.  extract the communicationFailure resource
        // and push this through again (the first time is what put the device into the
        // commFailCheckList).  note that we'll just grab the resource from the Device object that
        // we just loaded (vs making another IPC call to deviceService)
        //
        char *commFailURI = createResourceUri(device->uri, COMMON_DEVICE_RESOURCE_COMM_FAIL);
        DSResource *commFailResource = get_DSResource_from_DSDevice_resources((DSDevice *) device, commFailURI);
        if (commFailResource != NULL)
        {
            // create the trouble, which should remove the deviceId from our commFailCheckList
            //
            icLogInfo(SECURITY_LOG, "device %s is now considered in COMM FAIL; creating trouble", device->id);
            processTroubleForResource((const DSResource *) commFailResource, device, device->id, NULL, false, true);
        }
        else
        {
            // error getting the comm fail resource
            //
            icLogWarn(SECURITY_LOG, "error retrieving DSResource %s from device %s; unable to determine COMM_FAIL",
                      COMMON_DEVICE_RESOURCE_COMM_FAIL, device->id);
        }
        free(commFailURI);
    }
    else if (type == ALARM_DELAY_TIMER && supportAlarms() == true && getNoAlarmOnCommFailProp() == false)
    {
        // need to inform alarm service that a zone has been in comm fail so long that it can cause an alarm.
        // the approach is to find the existing COMM_FAIL trouble for this device, then escalate it
        // from TROUBLE_CRIT_CRITICAL to TROUBLE_CRIT_ALERT and re-notify alarmPanel
        //
        icLogDebug(SECURITY_LOG, "device %s is now considered in COMM FAIL ALARM; locating existing trouble", device->id);
        lockSecurityMutex();

        troubleSearchDevice search;
        search.deviceId = (const char *)device->id;
        search.type = TROUBLE_TYPE_DEVICE;
        search.reason = TROUBLE_REASON_COMM_FAIL;
        troubleContainer *commFailTrouble = linkedListFind(troubleList, &search, findTroubleContainerBySpecificDevice);
        if (commFailTrouble != NULL)
        {
            icLogInfo(SECURITY_LOG, "device %s is now considered in COMM FAIL ALARM; escalating trouble %"PRIu64,
                                     device->id, commFailTrouble->event->baseEvent.eventId);

            // escalate, forward to alarm, then put back
            //
            commFailTrouble->event->trouble->critical = TROUBLE_CRIT_ALERT;
            processTroubleContainerForAlarmPanel(commFailTrouble);
            commFailTrouble->event->trouble->critical = TROUBLE_CRIT_CRITICAL;
        }

        // remove from the tracker since we got to this point
        stopCommFailTimer(device->id, ALARM_DELAY_TIMER);
        unlockSecurityMutex();
    }
}

/**
 * Comm fail trouble resource, takes care of special checks against when the device was last heard of
 * @param resource the resource to check
 * @param parentDevice the parent device, may be null
 * @param deviceId the device id
 * @param isTrouble output value for whether this resource is troubled
 * @param critical output value with the criticality of the trouble
 * @param reason output value with the trouble reason
 * @param troubleResourceHandlerArg the optional resource handler argument set when the handler was created
 * @return whether the trouble is a clear or not.
 */
static bool commFailTroubleResourceHandler(const DSResource *resource,
                                           const DSDevice *parentDevice,
                                           const char *deviceId,
                                           bool *isTrouble,
                                           troubleCriticalityType *critical,
                                           troubleReason *reason,
                                           void *troubleResourceHandlerArg)
{
    // Presence devices have no comm fail trouble
    if (resource->ownerClass != NULL && strcmp(resource->ownerClass, PRESENCE_DC) == 0)
    {
        *isTrouble = false;
        return false;
    }

    // only look at these if this is a TROUBLE_OCCUR (not TROUBLE_CLEAR).
    //
    if (resource->value != NULL && strcasecmp("true", resource->value) == 0)
    {
        // before we can declare this as a real comm failure, need to see
        // how long this device was offline (in minutes) and compare to the
        // property that dictates this duration.
        //
        bool reallyCommFail = false;
        if (parentDevice == NULL)
        {
            // need to get the device so we can ask for the 'dateLastContacted'
            DSDevice *device = create_DSDevice();
            int rc = deviceService_request_GET_DEVICE_BY_ID((char *) deviceId, device);
            if (rc == IPC_SUCCESS)
            {
                // use the last time we contacted this device
                //
                reallyCommFail = isDeviceConsideredCommFail(device, TROUBLE_DELAY_TIMER);
            }
            else
            {
                // error getting device
                icLogWarn(SECURITY_LOG, "error retrieving DSDevice for id %s; unable to determine COMM_FAIL", (deviceId != NULL) ? deviceId : "NULL");
            }
            destroy_DSDevice(device);
        }
        else
        {
            // use the last time we contacted this device
            //
            reallyCommFail = isDeviceConsideredCommFail(parentDevice, TROUBLE_DELAY_TIMER);
        }

        if (reallyCommFail == false)
        {
            // ignore this, but add it to the list to monitor
            //
            startCommFailTimer(deviceId, TROUBLE_DELAY_TIMER, deviceCommFailNotify);
            icLogDebug(SECURITY_LOG, "ignoring COMM_FAIL notification for %s; has not been in communication failure long enough",  (deviceId != NULL) ? deviceId : "NULL");
            *isTrouble = false;
            return false;
        }
        // potentially look for COMM_FAIL_ALARM
        //
        else if (reallyCommFail == true &&
                 supportAlarms() == true &&
                 getNoAlarmOnCommFailProp() == false &&
                 resource->ownerClass != NULL &&
                 strcmp(resource->ownerClass, SENSOR_DC) == 0)
        {
            // for alarm processing, we need to handle the "COM FAIL ALARM" scenario, so
            // put this device back into the oven so we can be notified when the timeout
            // reaches that secondary threshold.
            // ideally we'd check now to see if this is in COM FAIL ALARM, but we want
            // the comm fail trouble to get processed first so that it can be escalated.
            //
            if (startCommFailTimer(deviceId, ALARM_DELAY_TIMER, deviceCommFailNotify) == true)
            {
                icLogDebug(SECURITY_LOG, "device %s in COMM_FAIL, adding to timer for COMM_FAIL_ALARM",  (deviceId != NULL) ? deviceId : "NULL");
            }
        }
    }

    // if we get here, then process this as a commFail (set or clear)
    //
    *isTrouble = true;
    *reason = TROUBLE_REASON_COMM_FAIL;

    // remove from the timer (requires the lock)
    lockSecurityMutex();
    stopCommFailTimer(deviceId, TROUBLE_DELAY_TIMER);
    unlockSecurityMutex();

    *critical = TROUBLE_CRIT_ERROR;
    if (resource->ownerClass != NULL)
    {
        if (strcmp(resource->ownerClass, LIGHT_DC) == 0)
        {
            // lights don't display troubles, so make criticality INFO
            *critical = TROUBLE_CRIT_INFO;
        }
        else if (strcmp(resource->ownerClass, CAMERA_DC) == 0)
        {
            // cameras don't 'beep' troubles, so make criticality NOTICE
            *critical = TROUBLE_CRIT_NOTICE;
        }
        else if ((strcmp(resource->ownerClass, SENSOR_DC) == 0) ||
                 (strcmp(resource->ownerClass, DOORLOCK_DC) == 0) ||
                 (strcmp(resource->ownerClass, THERMOSTAT_DC) == 0))
        {
            // communication failure is treated as critical.
            *critical = TROUBLE_CRIT_CRITICAL;
        }
    }

    return isTroubleStandardClear(resource);
}

/**
 * Low battery trouble resource handler, takes care of special pre low battery logic
 * @param resource the resource to check
 * @param parentDevice the parent device, may be null
 * @param deviceId the device id
 * @param isTrouble output value for whether this resource is troubled
 * @param critical output value with the criticality of the trouble
 * @param reason output value with the trouble reason
 * @param troubleResourceHandlerArg the optional resource handler argument set when the handler was created
 * @return whether the trouble is a clear or not.
 */
static bool lowBatteryTroubleResourceHandler(const DSResource *resource,
                                             const DSDevice *parentDevice,
                                             const char *deviceId,
                                             bool *isTrouble,
                                             troubleCriticalityType *critical,
                                             troubleReason *reason,
                                             void *troubleResourceHandlerArg)
{
    // Low battery
    *isTrouble = true;
    *reason = TROUBLE_REASON_BATTERY_LOW;
    uint32_t preLowBatDays = 0;
    if (stringCompare(resource->ownerClass, WARNING_DEVICE_DC, false) != 0)
    {
        preLowBatDays = getPropertyAsUInt32(PRELOW_BATTERY_DAYS_PROPERTY, DEFAULT_PRE_LOW_BATTERY_DAYS);
    }
    if (preLowBatDays == 0)
    {
        *critical = TROUBLE_CRIT_WARNING;
    }
    else
    {
        // Pre low battery condition, only a visual indication
        *critical = TROUBLE_CRIT_NOTICE;
    }

    return isTroubleStandardClear(resource);
}

/**
 * End Of Life resource handler, takes care of special PIM/PRM logic
 * @param resource the resource to check
 * @param parentDevice the parent device, may be null
 * @param deviceId the device id
 * @param isTrouble output value for whether this resource is troubled
 * @param critical output value with the criticality of the trouble
 * @param reason output value with the trouble reason
 * @param troubleResourceHandlerArg the optional resource handler argument set when the handler was created
 * @return whether the trouble is a clear or not.
 */
static bool endOfLifeTroubleResourceHandler(const DSResource *resource,
                                            const DSDevice *parentDevice,
                                            const char *deviceId,
                                            bool *isTrouble,
                                            troubleCriticalityType *critical,
                                            troubleReason *reason,
                                            void *troubleResourceHandlerArg)
{
    // FIXME: use isSecurityZoneSimpleDevice() to determine criticality
    *isTrouble = true;
    *reason = TROUBLE_REASON_END_OF_LIFE;
    *critical = TROUBLE_CRIT_CRITICAL;

    return isTroubleStandardClear(resource);
}

/**
 * Firmare Upgrade Status resource handler, takes care of special upgrade failure logic
 * @param resource the resource to check
 * @param parentDevice the parent device, may be null
 * @param deviceId the device id
 * @param isTrouble output value for whether this resource is troubled
 * @param critical output value with the criticality of the trouble
 * @param reason output value with the trouble reason
 * @param troubleResourceHandlerArg the optional resource handler argument set when the handler was created
 * @return whether the trouble is a clear or not.
 */
static bool firmwareUpgradeStatusResourceHandler(const DSResource *resource,
                                                 const DSDevice *parentDevice,
                                                 const char *deviceId,
                                                 bool *isTrouble,
                                                 troubleCriticalityType *critical,
                                                 troubleReason *reason,
                                                 void *troubleResourceHandlerArg)
{
    *isTrouble = false;
    *reason = TROUBLE_REASON_BOOTLOADER;
    *critical = TROUBLE_CRIT_INFO;

    //Note that only zigbee devices set the update status resource. Cameras, for instance, do not use this resource
    // despite the fact that it could. If Non-zigbee devices have a need to notate its update status, we should
    // reconsider the reason/criticality for this trouble for those devices.
    if (stringCompare(FIRMWARE_UPDATE_STATUS_FAILED, resource->value, true) == 0 ||
        stringCompare(FIRMWARE_UPDATE_STATUS_COMPLETED, resource->value, true) == 0)
    {
        *isTrouble = true;
    }

    // Note: firmware upgrades are a special case where instead of true/false, we are looking for failed/completed.
    return resource->value == NULL || strcasecmp(FIRMWARE_UPDATE_STATUS_COMPLETED, resource->value) == 0;
}

/**
 * Create a trouble resource handler with the given details
 * @param func the handler func
 * @param arg the handler arg, can be NULL
 * @param argFreeFunc the free func, can be NULL in which case free() is called on arg
 * @return the trouble resource handler
 */
static TroubleResourceHandler *
createTroubleResourceHandler(TroubleResourceHandlerFunc func, void *arg, TroubleResourceHandlerArgFreeFunc argFreeFunc)
{
    TroubleResourceHandler *handler = (TroubleResourceHandler *) calloc(1, sizeof(TroubleResourceHandler));
    handler->handlerFunc = func;
    handler->handlerArg = arg;
    handler->handlerArgFreeFunc = argFreeFunc;

    return handler;
}

/**
 * Create a simple trouble resource handler with the given static values
 * @param isTrouble if this is a trouble
 * @param critical criticality level
 * @param reason trouble reason
 * @return the trouble resource handler
 */
static TroubleResourceHandler *
createSimpleTroubleResourceHandler(bool isTrouble, troubleCriticalityType critical, troubleReason reason)
{
    SimpleTroubleResourceHandlerArg *arg = (SimpleTroubleResourceHandlerArg *) calloc(1,
                                                                                      sizeof(SimpleTroubleResourceHandlerArg));
    arg->isTrouble = isTrouble;
    arg->critical = critical;
    arg->reason = reason;
    return createTroubleResourceHandler(simpleTroubleResourceHandler, arg, NULL);
}

/**
 * Init all our trouble resource handlers
 */
static void initTroubleResourceHandlers()
{
    pthread_mutex_lock(&troubleResourceHandlersMtx);

    troubleResourceHandlers = hashMapCreate();

    // Comm Fail
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_COMM_FAIL, strlen(COMMON_DEVICE_RESOURCE_COMM_FAIL),
               createTroubleResourceHandler(commFailTroubleResourceHandler, NULL, NULL));

    // Low Battery
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_BATTERY_LOW, strlen(COMMON_DEVICE_RESOURCE_BATTERY_LOW),
               createTroubleResourceHandler(lowBatteryTroubleResourceHandler, NULL, NULL));

    // Tampered
    hashMapPut(troubleResourceHandlers, COMMON_ENDPOINT_RESOURCE_TAMPERED, strlen(COMMON_ENDPOINT_RESOURCE_TAMPERED),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_TAMPER));

    // Jammed
    hashMapPut(troubleResourceHandlers, DOORLOCK_PROFILE_RESOURCE_JAMMED, strlen(DOORLOCK_PROFILE_RESOURCE_JAMMED),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_LOCK_BOLT));

    // Invalid code entry limit
    hashMapPut(troubleResourceHandlers, DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT,
               strlen(DOORLOCK_PROFILE_RESOURCE_INVALID_CODE_ENTRY_LIMIT),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_PIN));

    // AC Power lost
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED,
               strlen(COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_AC_LOSS));

    // Bad Battery
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_BATTERY_BAD, strlen(COMMON_DEVICE_RESOURCE_BATTERY_BAD),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_BATTERY_BAD));

    // Battery missing
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_BATTERY_MISSING,
               strlen(COMMON_DEVICE_RESOURCE_BATTERY_MISSING),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_BATTERY_MISSING));

    // Battery high temperature
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE,
               strlen(COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_BATTERY_HIGH_TEMP));

    // Dirty
    hashMapPut(troubleResourceHandlers, SENSOR_PROFILE_RESOURCE_DIRTY, strlen(SENSOR_PROFILE_RESOURCE_DIRTY),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_DIRTY));

    // End of Life
    hashMapPut(troubleResourceHandlers, SENSOR_PROFILE_RESOURCE_END_OF_LIFE,
               strlen(SENSOR_PROFILE_RESOURCE_END_OF_LIFE),
               createTroubleResourceHandler(endOfLifeTroubleResourceHandler, NULL, NULL));

    // End of Line
    hashMapPut(troubleResourceHandlers, SENSOR_PROFILE_RESOURCE_END_OF_LINE_FAULT,
               strlen(SENSOR_PROFILE_RESOURCE_END_OF_LINE_FAULT),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_END_OF_LINE));

    // Firmware Upgrade Status
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
               strlen(COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS),
               createTroubleResourceHandler(firmwareUpgradeStatusResourceHandler, NULL, NULL));

    // Device high temperature
    hashMapPut(troubleResourceHandlers, COMMON_DEVICE_RESOURCE_HIGH_TEMPERATURE,
               strlen(COMMON_DEVICE_RESOURCE_HIGH_TEMPERATURE),
               createSimpleTroubleResourceHandler(true, TROUBLE_CRIT_ERROR, TROUBLE_REASON_HIGH_TEMP));

    pthread_mutex_unlock(&troubleResourceHandlersMtx);
}

/**
 * Destroy a trouble resource handler hashmap entry
 * @param key the key
 * @param value the value
 */
static void destroyTroubleResourceHandlerEntry(void *key, void *value)
{
    // Key is a string constant
    (void)key;
    // Cleanup the handler
    TroubleResourceHandler *handler = (TroubleResourceHandler *)value;
    if (handler->handlerArg != NULL)
    {
        if (handler->handlerArgFreeFunc != NULL)
        {
            handler->handlerArgFreeFunc(handler->handlerArg);
        }
        else
        {
            free(handler->handlerArg);
        }
    }
    free(handler);
}

/**
 * Cleanup trouble resource handlers
 */
static void destroyTroubleResourceHandlers()
{
    pthread_mutex_lock(&troubleResourceHandlersMtx);
    hashMapDestroy(troubleResourceHandlers, destroyTroubleResourceHandlerEntry);
    troubleResourceHandlers = NULL;
    pthread_mutex_unlock(&troubleResourceHandlersMtx);
}

/*
 * process the resource/device and potentially process as a trouble/clear.
 * this potentially grabs the global security mutex.
 */
void processTroubleForResource(const DSResource *resource, const DSDevice *parentDevice, const char *deviceId,
                               BaseEvent *baseEvent, bool processClear, bool sendEvent)
{
    troubleCriticalityType critical = TROUBLE_CRIT_NOTICE;
    troubleReason reason = TROUBLE_REASON_GENERIC;
    bool isTrouble = false;
    bool isClear = false;

    if (resource == NULL)
    {
        return;
    }

    pthread_mutex_lock(&troubleResourceHandlersMtx);

    // Check for a handler and invoke it
    TroubleResourceHandler *handler = hashMapGet(troubleResourceHandlers, resource->id, strlen(resource->id));
    if (handler != NULL)
    {
        isClear = handler->handlerFunc(resource, parentDevice, deviceId, &isTrouble, &critical, &reason, handler->handlerArg);
    }

    pthread_mutex_unlock(&troubleResourceHandlersMtx);

    // bail if this is not a notification about a "trouble"
    //
    if (isTrouble == false)
    {
        return;
    }

    // bail if its a clear and we don't care about processing clears
    //
    if (isClear == true && processClear == false)
    {
        return;
    }

    // log that we received a trouble event
    //
    icLogDebug(SECURITY_LOG, "received device trouble event; type=%s, clear=%s", resource->id, (isClear == true) ? "true" : "false");

    // create the troubleEvent using the information we have
    //
    troubleEvent *trouble = createBasicTroubleEvent(baseEvent, TROUBLE_TYPE_DEVICE, critical, reason);
    if (trouble == NULL)
    {
        return;
    }

    // place in the troubleContainer (what troubleState wants)
    //
    troubleContainer *container = createTroubleContainer();
    container->event = trouble;
    container->payloadType = TROUBLE_DEVICE_TYPE_NONE;

    // if sensor (zone) or IOT device, get the 'extra' information and place into the container & event.
    // seems funny, but in the container we'll put the unmarshalled object.  and the troubleEvent will
    // hold the marshalled (JSON) representation.  this removes the need to keep parsing the JSON over and over.
    //
    if (stringCompare(resource->ownerClass, SENSOR_DC, false) == 0)
    {
        // sensors need to add additional information into the 'extra' field
        //
        securityZone *zone = NULL;
        SensorTroublePayload *sensorTroublePayload = sensorTroublePayloadCreate();
        if (sensorTroublePayload)
        {
            populateDeviceTroublePayload(resource, deviceId, sensorTroublePayload->deviceTrouble);

            // Add in sensor specific data
            sensorTroublePayload->zoneNumber = getZoneNumberForUriPublic(resource->uri);
            if ((zone = getSecurityZoneForNumberPublic(sensorTroublePayload->zoneNumber)) != NULL)
            {
                sensorTroublePayload->zoneType = zone->zoneType;

                // Stuff it into the extra
                trouble->trouble->extra = encodeSensorTroublePayload(sensorTroublePayload);

                // save the payload into the container
                container->payloadType = TROUBLE_DEVICE_TYPE_ZONE;
                container->extraPayload.zone = sensorTroublePayload;
                sensorTroublePayload = NULL;

                // Cleanup
                destroy_securityZone(zone);
            }

            // Cleanup
            sensorTroublePayloadDestroy(sensorTroublePayload);
        }
    }
    else if (stringCompare(resource->ownerClass, CAMERA_DC, false) == 0)
    {
        CameraTroublePayload *cameraTroublePayload = cameraTroublePayloadCreate();
        populateDeviceTroublePayload(resource, deviceId, cameraTroublePayload->deviceTrouble);

        container->event->trouble->extra = encodeCameraTroublePayload(cameraTroublePayload);
        container->payloadType = TROUBLE_DEVICE_TYPE_CAMERA;
        container->extraPayload.camera = cameraTroublePayload;
        cameraTroublePayload = NULL;
    }
    else if (stringCompare(resource->ownerClass, LIGHT_DC, false) == 0 ||
             stringCompare(resource->ownerClass, THERMOSTAT_DC, false) == 0 ||
             stringCompare(resource->ownerClass, DOORLOCK_DC, false) == 0 ||
             stringCompare(resource->ownerClass, KEYPAD_DC, false) == 0 ||
             // Kinda screwy, but the the ownerClass is either the class or profile depending on whether the resource
             // is on the device or endpoint.  We need to cover all our bases for devices where their profile and
             // device class are different
             stringCompare(resource->ownerClass, SECURITY_CONTROLLER_PROFILE, false) == 0 ||
             stringCompare(resource->ownerClass, KEYPAD_DC, false) == 0 ||
             stringCompare(resource->ownerClass, KEYFOB_DC, false) == 0 ||
             stringCompare(resource->ownerClass, WARNING_DEVICE_PROFILE, false) == 0)
    {
        // All these don't have additional data beyond what's in the device trouble,
        // so just encode as that for now
        DeviceTroublePayload *deviceTroublePayload = deviceTroublePayloadCreate();
        populateDeviceTroublePayload(resource, deviceId, deviceTroublePayload);

        // Store it in the trouble
        container->event->trouble->extra = encodeDeviceTroublePayload(deviceTroublePayload);
        container->payloadType = TROUBLE_DEVICE_TYPE_IOT;
        container->extraPayload.device = deviceTroublePayload;
        deviceTroublePayload = NULL;
    }

    if (isClear == true)
    {
        trouble->trouble->restored = true;
        trouble->baseEvent.eventCode = TROUBLE_CLEARED_EVENT;
    }
    else
    {
        trouble->trouble->restored = false;
        trouble->baseEvent.eventCode = TROUBLE_OCCURED_EVENT;
    }

    // auto assign a description using the reason and possibly the device class
    //
    const char *reasonStr = troubleReasonLabels[reason];
    if (resource->ownerClass != NULL)
    {
        trouble->trouble->description = (char *)malloc(sizeof(char) * (strlen(resource->ownerClass) + strlen(reasonStr) + 2));
        sprintf(trouble->trouble->description, "%s %s", resource->ownerClass, reasonStr);
    }
    else
    {
        trouble->trouble->description = strdup(reasonStr);
    }

    // convert payload to string
    //
    debugPrintTroubleObject(trouble->trouble, SECURITY_LOG, "creating new trouble event:");
    if (isClear == false)
    {
        if (addTroublePublic(container, isMatchingDeviceTroublePayload, sendEvent) == 0)
        {
            // not added, so need to cleanup the object
            destroyTroubleContainer(container);
        }
    }
    else
    {
        // may need revisiting, but use the troubleEvent instead of the container
        //
        if (clearTroublePublic(trouble, true, isMatchingDeviceTroublePayload, sendEvent) == false)
        {
            // not found, so our 'trouble' didn't get removed
            //
            destroy_troubleEvent(trouble);
        }

        // cleanup the container
        //
        container->event = NULL;
        destroyTroubleContainerFromList(container);
    }
}

/*
 * Process a zigbee network interference event
 */
void processZigbeeNetworkInterferenceEvent(DeviceServiceZigbeeNetworkInterferenceChangedEvent *event)
{
    // create the trouble or the clear
    //
    troubleEvent *trouble = createBasicTroubleEvent(&event->baseEvent,
                                                    TROUBLE_TYPE_SYSTEM,
                                                    TROUBLE_CRIT_CRITICAL,
                                                    TROUBLE_REASON_ZIGBEE_INTERFERENCE);
    if (event->interferenceDetected == true)
    {
        // new trouble, add to the tracking hash
        //
        trouble->baseEvent.eventCode = TROUBLE_OCCURED_EVENT;

        // place in a container
        //
        troubleContainer *container = createTroubleContainer();
        container->event = trouble;
        container->payloadType = TROUBLE_DEVICE_TYPE_NONE;
        container->persist = false; // we dont want to persist these across reboot to match legacy behavior
        if (addTroublePublic(container, NULL, true) == 0)
        {
            // not added, so need to cleanup the object
            destroyTroubleContainer(container);
        }
    }
    else
    {
        // clear corresponding trouble, then cleanup
        //
        trouble->baseEvent.eventCode = TROUBLE_CLEARED_EVENT;
        if (clearTroublePublic(trouble, true, NULL, true) == false)
        {
            // not deleted, so cleanup mem
            destroy_troubleEvent(trouble);
        }
    }
}

/*
 * Process a zigbee pan id attack event
 */
void processZigbeePanIdAttackEvent(DeviceServiceZigbeePanIdAttackChangedEvent *event)
{
    // create the trouble or the clear
    //
    troubleEvent *trouble = createBasicTroubleEvent(&event->baseEvent,
                                                    TROUBLE_TYPE_SYSTEM,
                                                    TROUBLE_CRIT_CRITICAL,
                                                    TROUBLE_REASON_ZIGBEE_PAN_ID_ATTACK);
    if (event->attackDetected == true)
    {
        // new trouble, add to the tracking hash
        //
        trouble->baseEvent.eventCode = TROUBLE_OCCURED_EVENT;

        // place in a container
        //
        troubleContainer *container = createTroubleContainer();
        container->event = trouble;
        container->payloadType = TROUBLE_DEVICE_TYPE_NONE;
        container->persist = false; // we dont want to persist these across reboot to match legacy behavior
        if (addTroublePublic(container, NULL, true) == 0)
        {
            // not added, so need to cleanup the object
            destroyTroubleContainer(container);
        }
    }
    else
    {
        // clear corresponding trouble, then cleanup
        //
        trouble->baseEvent.eventCode = TROUBLE_CLEARED_EVENT;
        if (clearTroublePublic(trouble, true, NULL, true) == false)
        {
            // not deleted, so cleanup mem
            destroy_troubleEvent(trouble);
        }
    }
}

/**
 * We require a ResourceUpdatedEvent to check a resource for troubles, so create a fake event for the sources so we
 * do the processing
 * @param device the device that the resource belongs to(the resource may be on an endpoint, but this is still the
 * parent device)
 * @param resource the resource
 * @return the resource updated event
 */
static DeviceServiceResourceUpdatedEvent *createFakeResourceUpdatedEventForInitialTrouble(DSDevice *device, DSResource *resource)
{
    DeviceServiceResourceUpdatedEvent *event = create_DeviceServiceResourceUpdatedEvent();
    destroy_DSResource(event->resource);

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_RESOURCE_UPDATED;
    setEventId(&(event->baseEvent));
    setEventTimeToNow(&(event->baseEvent));

    // Copy over other info
    event->resource = clone_DSResource(resource);
    event->rootDeviceId = strdup(device->id);
    event->rootDeviceClass = strdup(device->deviceClass);

    return event;
}

/*
 * Check a device for initial troubles
 */
void checkDeviceForInitialTroubles(const char *deviceId, bool processClear, bool sendEvent)
{
    DSDevice *device = create_DSDevice();
    int rc = deviceService_request_GET_DEVICE_BY_ID((char *)deviceId, device);
    if (rc == IPC_SUCCESS)
    {
        // Check all device resources
        icHashMapIterator *iter = hashMapIteratorCreate(device->resourcesValuesMap);
        while(hashMapIteratorHasNext(iter))
        {
            char *resourceUri;
            uint16_t keyLen;
            DSResource *resource;
            hashMapIteratorGetNext(iter, (void**)&resourceUri, &keyLen, (void**)&resource);

            pthread_mutex_lock(&troubleResourceHandlersMtx);
            bool checkForTrouble = hashMapContains(troubleResourceHandlers, resource->id, strlen(resource->id));
            pthread_mutex_unlock(&troubleResourceHandlersMtx);

            // If there is no handler this resource is associated with any trouble, so we can skip over it
            if (checkForTrouble == true)
            {
                // Create a fake event to pass along
                DeviceServiceResourceUpdatedEvent *event = createFakeResourceUpdatedEventForInitialTrouble(device,
                                                                                                           resource);

                // This takes care of checking if the resource is one we care about, and creating the trouble
                processTroubleForResource(resource, device, deviceId, &event->baseEvent, processClear, sendEvent);

                // Cleanup the fake event
                destroy_DeviceServiceResourceUpdatedEvent(event);
            }

        }
        hashMapIteratorDestroy(iter);

        // Check all endpoint resources
        icHashMapIterator *endpointsIter = hashMapIteratorCreate(device->endpointsValuesMap);
        while(hashMapIteratorHasNext(endpointsIter))
        {
            char *endpointUri;
            uint16_t keyLen;
            DSEndpoint *endpoint;
            hashMapIteratorGetNext(endpointsIter, (void**)&endpointUri, &keyLen, (void**)&endpoint);

            icHashMapIterator *epResIter = hashMapIteratorCreate(endpoint->resourcesValuesMap);
            while(hashMapIteratorHasNext(epResIter))
            {
                char *resourceUri;
                uint16_t resLen;
                DSResource *resource;
                hashMapIteratorGetNext(epResIter, (void**)&resourceUri, &resLen, (void**)&resource);

                // Create a fake event to pass along
                DeviceServiceResourceUpdatedEvent *event = createFakeResourceUpdatedEventForInitialTrouble(device, resource);

                // This takes care of checking if the resource is one we care about, and creating the trouble
                processTroubleForResource(resource, device, deviceId, &event->baseEvent, processClear, sendEvent);

                // Cleanup the fake event
                destroy_DeviceServiceResourceUpdatedEvent(event);
            }
            hashMapIteratorDestroy(epResIter);
        }
        hashMapIteratorDestroy(endpointsIter);
    }
    else
    {
        icLogWarn(SECURITY_LOG, "Failed to lookup device %s to gather initial troubles: %s", deviceId, IPCCodeLabels[rc]);
    }

    // Cleanup
    destroy_DSDevice(device);
}

/*
 * function used via 'linkedListFind' to find a troubleEvent with a
 * matching troubleId.
 * called internally, should assumes the mutex is held.
 */
static bool findTroubleContainerByTroubleId(void *searchVal, void *item)
{
    // searchVal should be the troubleId
    //
    uint64_t *troubleId = (uint64_t *) searchVal;
    troubleContainer *container = (troubleContainer *) item;

    if (container->event->trouble->troubleId == *troubleId)
    {
        return true;
    }
    return false;
}

/*
 * function used via 'linkedListFind' to find a troubleEvent with a
 * matching troubleId.
 * called internally, should assumes the mutex is held.
 */
static bool findTroubleContainerByTroubleOrEventId(void *searchVal, void *item)
{
    // searchVal should be the troubleId
    //
    uint64_t *searchId = (uint64_t *) searchVal;
    troubleContainer *container = (troubleContainer *) item;

    if (container->event->trouble->troubleId == *searchId ||
        container->event->trouble->eventId == *searchId ||
        container->event->baseEvent.eventId == *searchId)
    {
        return true;
    }
    return false;
}

/*
 * function used via 'linkedListFind' to find a troubleEvent with
 * matching parameters (deviceId, reason, type).
 * called internally, should assumes the mutex is held.
 */
static bool findTroubleContainerBySearchParms(void *searchVal, void *item)
{
    // searchVal should be a troubleSearchParms object
    //
    troubleSearchParms *search = (troubleSearchParms *) searchVal;
    troubleContainer *container = (troubleContainer *) item;

    // only return 'true' if deviceId + type + reason are the same
    //
    if (search->type == container->event->trouble->type &&
        search->reason == container->event->trouble->reason)
    {
        // check the payload if supplied in our search parameters
        //
        if (search->compareFunc != NULL)
        {
            cJSON *payloadJSON = NULL;
            if (container->event->trouble->extra != NULL)
            {
                payloadJSON = container->event->trouble->extra;
            }
            return search->compareFunc(search->payload, payloadJSON);
        }
        else
        {
            // not comparing payloads, so close enough to consider a match
            //
            return true;
        }
    }
    return false;
}

/*
 * function used via 'linkedListFind' to find a troubleEvent with
 * for a specific device, trouble type, and trouble reason (uses troubleSearchDevice)
 * called internally, should assumes the mutex is held.
 */
static bool findTroubleContainerBySpecificDevice(void *searchVal, void *item)
{
    // searchVal should be a troubleSearchParms object
    //
    troubleSearchDevice *search = (troubleSearchDevice *) searchVal;
    troubleContainer *container = (troubleContainer *) item;

    // first check if type & reason match
    //
    if (search->type == container->event->trouble->type &&
        search->reason == container->event->trouble->reason)
    {
        // now compare the deviceId
        //
        switch (container->payloadType)
        {
            case TROUBLE_DEVICE_TYPE_ZONE:
                if (container->extraPayload.zone != NULL &&
                    container->extraPayload.zone->deviceTrouble != NULL &&
                    stringCompare((char *)search->deviceId, container->extraPayload.zone->deviceTrouble->rootId, false) == 0)
                {
                    return true;
                }
                break;

            case TROUBLE_DEVICE_TYPE_CAMERA:
                if (container->extraPayload.camera != NULL &&
                    container->extraPayload.camera->deviceTrouble != NULL &&
                    stringCompare((char *)search->deviceId, container->extraPayload.camera->deviceTrouble->rootId, false) == 0)
                {
                    return true;
                }
                break;

            case TROUBLE_DEVICE_TYPE_IOT:
                if (container->extraPayload.device != NULL &&
                    stringCompare((char *)search->deviceId, container->extraPayload.device->rootId, false) == 0)
                {
                    return true;
                }
                break;

            default:
                // network or system.  call this a match if the 'search' object doesn't define a deviceId
                //
                if (search->deviceId == NULL)
                {
                    return true;
                }
                break;
        }
    }
    return false;
}

/*
 * internal function to count troubles that are un-acknowledged.
 * assumes the elements are 'troubleContainer'.
 * adheres to the 'linkedListIterateFunc' signature.
 * called internally, should assumes the mutex is held.
 */
static bool countUnackedTroubles(void *item, void *arg)
{
    troubleContainer *container = (troubleContainer *)item;
    uint32_t *counter = (uint32_t *)arg;
    if (container->event->trouble->acknowledged == false)
    {
        *counter += 1;
    }
    return true;
}

/*
 * internal function to update our "systemTroubles" flag based on
 * troubleContainer elements in the 'troubleList'.
 * adheres to the 'linkedListIterateFunc' signature.
 * called internally, should assumes the mutex is held.
 */
static bool updateSystemTroubleFlag(void *item, void *arg)
{
    troubleContainer *container = (troubleContainer *)item;
    if (container->event->trouble->type == TROUBLE_TYPE_SYSTEM)
    {
        // set flag
        //
        haveSystemTroubles = true;
        if (container->event->trouble->reason == TROUBLE_REASON_TAMPER)
        {
            // update the tampered flag.  no need to keep searching,
            // so return false to halt the iterator
            //
            haveSystemTamper = true;
            return false;
        }
    }

    // keep going to potentially find any system troubles
    //
    return true;
}

/*
 * Returns the indication category for a zone based on zone type/function
 */
static indicationCategory getIndicationCategoryForZone(securityZone *zone)
{
    indicationCategory cat = INDICATION_CATEGORY_IOT;
    switch (zone->zoneType)
    {
        // smoke & carbon monoxide are "life safety"
        //
        case SECURITY_ZONE_TYPE_SMOKE:
        case SECURITY_ZONE_TYPE_CO:
        case SECURITY_ZONE_TYPE_MEDICAL:
            cat = INDICATION_CATEGORY_SAFETY;
            break;
            // Environmental are IOT
        case SECURITY_ZONE_TYPE_ENVIRONMENTAL:
        case SECURITY_ZONE_TYPE_WATER:
            break;
            // all others are "burg" unless the zone function is MONITOR-24 or it is an environment sensor
        default:
            if (zone->zoneFunction != SECURITY_ZONE_FUNCTION_MONITOR_24HOUR)
            {
                cat = INDICATION_CATEGORY_BURG;
            }
            break;
    }

    return cat;
}

/*
 * assigns the indicationType and indicationCategory for a given trouble
 */
static void assignIndicationType(troubleContainer *container)
{
    // look at the troubleCriticality:
    //   TROUBLE_CRIT_WARNING (or higher)  -- visual & audible
    //   TROUBLE_CRIT_NOTICE               -- visual only
    //   TROUBLE_CRIT_INFO (or lower)      -- none
    //
    // we get away with this because nothing is AUDIBLE only (otherwise user cannot see what's wrong)
    //
    if (container->event->trouble->critical <= TROUBLE_CRIT_WARNING)
    {
        container->event->trouble->indication = INDICATION_BOTH;
    }
    else if (container->event->trouble->critical == TROUBLE_CRIT_NOTICE)
    {
        if (container->event->trouble->reason != TROUBLE_REASON_SWINGER)
        {
            container->event->trouble->indication = INDICATION_VISUAL;
        }
    }
    else
    {
        container->event->trouble->indication = INDICATION_NONE;
    }

    // now the indicationCategory
    //
    switch (container->event->trouble->type)
    {
        case TROUBLE_TYPE_NETWORK:
        case TROUBLE_TYPE_SYSTEM:
        case TROUBLE_TYPE_POWER:
        {
            // according to the 6th edition of the UL 985 standard, a system trouble
            // should be classified as LIFE_SAFETY if we have life-safety zones installed
            container->event->trouble->indicationGroup = INDICATION_CATEGORY_SYSTEM;
            if (haveLifeSafetyZonePrivate() == true)
            {
                container->event->trouble->treatAsLifeSafety = true;
            }
            break;
        }

        case TROUBLE_TYPE_DEVICE:
        {
            // assume IOT (as the catch-all)
            //
            container->event->trouble->indicationGroup = INDICATION_CATEGORY_IOT;

            // possible this is a zone
            //
            if (container->payloadType == TROUBLE_DEVICE_TYPE_ZONE && container->extraPayload.zone != NULL)
            {
                securityZone *zone = findSecurityZoneForNumberPrivate(container->extraPayload.zone->zoneNumber);
                if (zone != NULL)
                {
                    // Set the indication category based on zone type/function
                    container->event->trouble->indicationGroup = getIndicationCategoryForZone(zone);

                    // set the isTroubled flag on the zone while we're here
                    //
                    zone->isTroubled = true;

                    // we didn't get a copy of zone, so DO NOT FREE
                    //
                    zone = NULL;
                }
            }
            else if (container->payloadType == TROUBLE_DEVICE_TYPE_IOT && container->extraPayload.device != NULL &&
                     container->extraPayload.device->deviceClass != NULL)
            {
                // For PIM/PRM we need to determine its indication group based on the highest one for its zones
                icLinkedList *zones = getZonesForDeviceIdPrivate(container->extraPayload.device->rootId);
                icLinkedListIterator *iter = linkedListIteratorCreate(zones);
                // For PIM/PRM we don't go lower than BURG
                indicationCategory maxCat = INDICATION_CATEGORY_BURG;
                while(linkedListIteratorHasNext(iter))
                {
                    securityZone *zone = (securityZone *)linkedListIteratorGetNext(iter);
                    indicationCategory cat = getIndicationCategoryForZone(zone);
                    if (cat > maxCat)
                    {
                        maxCat = cat;
                    }
                }
                linkedListIteratorDestroy(iter);

                container->event->trouble->indicationGroup = maxCat;

                // Cleanup
                linkedListDestroy(zones, standardDoNotFreeFunc);
            }

            // if this is LIFE_SAFETY or SYSTEM + have life-safety zones, then set
            // the treatAsLifeSafety flag on the trouble.  this is for the UI to interpret
            //
            if (container->event->trouble->indicationGroup == INDICATION_CATEGORY_SAFETY)
            {
                container->event->trouble->treatAsLifeSafety = true;
            }
            break;
        }

        default:
        {
            // catch-all group of IOT
            //
            container->event->trouble->indicationGroup = INDICATION_CATEGORY_IOT;
            break;
        }
    }
}

/*
 * Schedule the pre low battery cron job
 */
static void schedulePreLowBatteryCron(bool devMode)
{
    char buf[24];
    if (devMode == false)
    {
        // Generate a random minute within the hour to run so that every device is not hitting the server at once
        srand(time(NULL));
        int r = rand() % 60;
        snprintf(buf, sizeof(buf), LOW_BATTERY_ELEVATION_CRON_SCHEDULE_FORMAT, r);
    }
    else
    {
        // fire off every minute
        strncpy(buf, LOW_BATTERY_ELEVATION_CRON_SCHEDULE_DEV, sizeof(buf));
    }
    // Setup the cron
    if (registerForCronEvent(LOW_BATTERY_ELEVATION_CRON_NAME, buf, lowBatElevateCallback) == false)
    {
        icLogError(SECURITY_LOG, "Failed to register for low battery elevation cron event");
    }
}

/*
 * Internal function to look through existing troubles and see if any
 * device low battery troubles need to be elevated in criticality
 */
static bool lowBatElevateCallback(const char *name)
{
    // We just leave the cron schedule setup and then do this check when we are called since its just once a day
    uint32_t preLowBatDays = getPropertyAsUInt32(PRELOW_BATTERY_DAYS_PROPERTY, DEFAULT_PRE_LOW_BATTERY_DAYS);
    if (preLowBatDays > 0)
    {
        bool devMode = getPropertyAsBool(PRELOW_BATTERY_DAYS_DEV_PROPERTY, false);
        int divisor = SECONDS_IN_A_DAY;
        if (devMode)
        {
            divisor = SECONDS_IN_A_MINUTE;
        }
        time_t now = getCurrentTime_t(false);

        // Lock so we can iterate
        lockSecurityMutex();
        icLogDebug(SECURITY_LOG, "Looking for notice level low battery troubles more than %" PRIu32 " days old", preLowBatDays);

        // Loop through troubles to see if any preLowBattery troubles should be elevated
        icLinkedListIterator *iter = linkedListIteratorCreate(troubleList);
        while(linkedListIteratorHasNext(iter) == true)
        {
            // Only check to elevate if its in pre-low condition
            troubleContainer *next = (troubleContainer *)linkedListIteratorGetNext(iter);
            if (next->event->trouble->type == TROUBLE_TYPE_DEVICE &&
                next->event->trouble->reason == TROUBLE_REASON_BATTERY_LOW &&
                next->event->trouble->critical == TROUBLE_CRIT_NOTICE)
            {
                // Compute days in trouble
                int daysInTrouble = floor(difftime(now,next->event->baseEvent.eventTime.tv_sec)/divisor);
                icLogDebug(SECURITY_LOG, "Low battery trouble %"PRIu64" is %d days old", next->event->trouble->troubleId, daysInTrouble);
                if (daysInTrouble >= preLowBatDays)
                {
                    // Elevate level to warning to signal its now in real "lowBat"
                    next->event->trouble->critical = TROUBLE_CRIT_WARNING;
                    // Set acknowledged back to false
                    next->event->trouble->acknowledged = false;
                    // Update indication
                    assignIndicationType(next);
                    // Update the date everywhere
                    setEventTimeToNow(&next->event->baseEvent);
                    next->event->trouble->eventTime = convertTimespecToUnixTimeMillis(&next->event->baseEvent.eventTime);
                    // Give it a new event id
                    next->event->baseEvent.eventId = getNextEventId();

                    if (supportAlarms() == true)
                    {
                        // Rerun it through alarm just to get it all updated
                        processTroubleContainerForAlarmPanel(next);
                    }
                    else
                    {
                        populateSystemPanelStatusPrivate(next->event->panelStatus);
                    }

                    // clone our container so we can drop this into the taskExecutor for processing
                    // outside of the mutex (and in FIFO fashion)
                    //
                    troubleContainer *dup = cloneTroubleContainer(next);
                    bool *sendEventArg = (bool *)malloc(sizeof(bool));
                    *sendEventArg = true;
                    if (appendSecurityTask(dup, sendEventArg, addTroubleTaskRun, addTroubleTaskFree) == false)
                    {
                        /* executor called addTroubleTaskFree to free dup */
                        icLogWarn(SECURITY_LOG, "Failed queueing trouble add task: executor rejected job");
                    }
                }
            }
        }
        linkedListIteratorDestroy(iter);
        unlockSecurityMutex();
    }
    return false; //do not unregister and remove the cron
}

/*
 * Listener for property event
 */
static void cpePropListener(cpePropertyEvent *event)
{
    if (event == NULL || event->propKey == NULL)
    {
        return;
    }

    // see if 'prelow days' changed
    if (strcmp(event->propKey,PRELOW_BATTERY_DAYS_DEV_PROPERTY) == 0)
    {
        bool devMode = false;
        if (event->baseEvent.eventValue != GENERIC_PROP_DELETED)
        {
            devMode = getPropertyEventAsBool(event, false);
        }

        // Lock and do the schedule if we are initialized
        lockSecurityMutex();
        if (didInit == true)
        {
            schedulePreLowBatteryCron(devMode);
        }
        unlockSecurityMutex();
    }
}

bool restoreTroubleConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    bool didSomething = false;
    // Restore the configuration. The current namespace will be deleted automatically.
    if (storageRestoreNamespace(NON_DEVICE_TROUBLES_NAMESPACE, tempRestoreDir) == true)
    {
        didSomething = true;
    }

    return didSomething;
}

