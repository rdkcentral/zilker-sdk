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
 * securityZone.c
 *
 * collection of 'securityZone' objects, kept in memory.
 * during init, a call will be made to deviceService to get all
 * of the known sensors.  each sensor device that has a 'secZone'
 * attribute will be wrapped into a securityZone object and kept
 * in an ordered list.
 *
 * the 'secZone' attribute is actually a JSON object containing
 * the zone details for the sensor.  it's done this way so that
 * securityService can properly provide a layer above the 'sensor'
 * without easily exposing each of the attributes that make up a
 * logical 'security zone'.  for example, saving the 'displayIndex'
 * in a public attribute of a sensor could not prevent a service or
 * application from altering that value - which would have dire
 * consequences on the zone objects (dup numbers or out of sync with
 * the central station).
 *
 * several of the functions below will utilize the sharedTaskExecutor,
 * which is a queue of taks to be executed in FIFO order.  we added this
 * complexity because there are several situations where saving metadata
 * or sending events needs to occur outside of the SECURITY_MTX; and we
 * need to serialize those operations to handle quick succession of events
 * or requests.  For example, when a zone is added, it most likely will have
 * several update events immediately following the add.  we need the events
 * from those modifications to be sent in the order they were processed.
 *
 * Author: jelderton - 7/12/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>

#include <icLog/logging.h>
#include <icTypes/icSortedLinkedList.h>
#include <icUtil/stringUtils.h>
#include <icSystem/softwareCapabilities.h>
#include <deviceService/deviceService_ipc.h>
#include <deviceService/deviceService_eventAdapter.h>
#include <securityService/sensorTroubleEventHelper.h>
#include <commonDeviceDefs.h>
#include <deviceHelper.h>
#include <resourceTypes.h>
#include <sensorHelper.h>
#include <deviceService/resourceModes.h>
#include <icTime/timeUtils.h>
#include <securityService/securityZoneHelper.h>
#include "alarm/alarmPanel.h"
#include "zone/securityZone.h"
#include "zone/securityZonePrivate.h"
#include "trouble/troubleStatePrivate.h"
#include "trouble/troubleContainer.h"
#include "common.h"
#include "broadcastEvent.h"
#include "internal.h"

#define SEC_ZONE_ATTRIBUTE      "secZone"

#define NO_UPDATE                           0x00U
#define UPDATE_ZONE_METADATA_FLAG           0x01U
#define UPDATE_LABEL_FLAG                   0x02U
#define UPDATE_SENSOR_TYPE_FLAG             0x04U
#define UPDATE_LPM_FLAG                     0x08U
#define UPDATE_BYPASS_FLAG                  0x10U


/*
 * private structures
 */
typedef struct
{
    char *deviceUUID;
    char *endpointId;
} ZoneSearchInfo;

/*
 * private functions
 */
static bool reloadFromDeviceService();
static int8_t zoneIndexCompareFunc(void *newItem, void *element);
static bool findZoneByDeviceId(void *search, void *item);
static bool findZoneByDeviceIdAndEndpointId(void *search, void *item);
static bool findZoneByZoneNumber(void *search, void *item);
static bool findZoneByDisplayIndex(void *search, void *item);
static void endpointAddedNotify(DeviceServiceEndpointAddedEvent *event);
static void endpointRemovedNotify(DeviceServiceEndpointRemovedEvent *event);
static void deviceResourceUpdatedNotify(DeviceServiceResourceUpdatedEvent *event);
static void deviceDiscovered(DeviceServiceDeviceDiscoveredEvent *event);
static securityZone *createZoneFromEndpoint(DSEndpoint *endpoint);
static lpmPolicyPriority determineLPMPolicy(securityZone *zone);
static bool persistZoneMetadata(securityZone *zone, uint8_t value);
static cJSON *encodeSecZone_toJSON(securityZone *zone);
static bool decodeSecZone_fromJSON(securityZone *zone, cJSON *buffer);
static void printZone(securityZone *zone);
static uint32_t allocateZoneNumber(const char *deviceId, const char *endpointId);
static char *getAllocatedZoneKey(const char *deviceUuid, const char *endpointId);
static bool passesSwingerRestoreCheck(securityZoneEvent *zoneEvent);
static uint32_t allocateSensorId();
static void recalculateHasLifeSafetyFlag();
static bool isZoneBypassable(securityZoneType zoneType);
static icLinkedList *reorderZonesIfNecessary();
static void updateZoneViaEventTaskRun(void *taskObj, void *taskArg);
static void updateZoneViaEventTaskFree(void *taskObj, void *taskArg);
static void updateReorderedZoneTaskRun(void *taskObj, void *taskArg);
static void updateReorderedZoneTaskFree(void *taskObj, void *taskArg);
static void setZoneIsSimpleDevice(securityZone *zone);

/*
 * private variables
 */
static icSortedLinkedList *zoneList = NULL;
// Use a key generated by getAllocatedZoneKey
static icHashMap *allocatedZoneNumbersByKey = NULL;
static bool didInit = false;
static bool hasLifeSafetyZone = false;

/*
 * one-time init to load the zones via deviceService.
 * since this makes requests to deviceService, should
 * not be called until all of the services are available
 */
void initSecurityZonesPublic()
{
    lockSecurityMutex();
    if (didInit == false)
    {
        // allocate our zoneList
        //
        zoneList = sortedLinkedListCreate();
        allocatedZoneNumbersByKey = hashMapCreate();
        hasLifeSafetyZone = false;

        // ask device service for all sensor endpoints
        //
        reloadFromDeviceService();

        // Fixup any display order gaps, etc.
        icLinkedList *reorderList = reorderZonesIfNecessary();
        if (reorderList != NULL)
        {
            icLogWarn(SECURITY_LOG, "Detected and corrected gaps/duplicate zone display indices on init");
            linkedListDestroy(reorderList, (linkedListItemFreeFunc)destroy_securityZone);
        }

        // register for endpoint add, update, delete events
        //
        register_DeviceServiceEndpointAddedEvent_eventListener(endpointAddedNotify);
        register_DeviceServiceEndpointRemovedEvent_eventListener(endpointRemovedNotify);
        register_DeviceServiceResourceUpdatedEvent_eventListener(deviceResourceUpdatedNotify);
        register_DeviceServiceDeviceDiscoveredEvent_eventListener(deviceDiscovered);

        // update the 'init' flag
        //
        didInit = true;
    }
    unlockSecurityMutex();
}

void destroySecurityZonesPublic()
{
    linkedListDestroy(zoneList, zoneFreeFunc);
    zoneList = NULL;

    hashMapDestroy(allocatedZoneNumbersByKey, NULL);
    allocatedZoneNumbersByKey = NULL;

    unregister_DeviceServiceEndpointAddedEvent_eventListener(endpointAddedNotify);
    unregister_DeviceServiceEndpointRemovedEvent_eventListener(endpointRemovedNotify);
    unregister_DeviceServiceResourceUpdatedEvent_eventListener(deviceResourceUpdatedNotify);
    unregister_DeviceServiceDeviceDiscoveredEvent_eventListener(deviceDiscovered);

    didInit = false;
}

/*
 * used to get runtime status (via IPC)
 */
void getSecurityZoneStatusDetailsPublic(serviceStatusPojo *output)
{
    // get the number of zones
    //
    lockSecurityMutex();
    uint32_t zoneCount = linkedListCount(zoneList);
    // TODO: get faulted or troubled zone count?
    unlockSecurityMutex();
    put_int_in_serviceStatusPojo(output, "SECURITY_ZONE_COUNT", zoneCount);

    // TODO: is there more interesting data to add here?

}

/*
 * return a linked list of known securityZone objects (sorted by displayIndex).
 * caller is responsible for freeing the list and the elements
 *
 * @see zoneFreeFunc
 */
icLinkedList *getAllSecurityZonesPublic()
{
    // make a list, then populate it
    //
    icLinkedList *retVal = linkedListCreate();
    extractAllSecurityZonesPublic(retVal);

    return retVal;
}

/*
 * populate a linked list with known securityZone objects
 * caller is responsible for freeing the elements
 *
 * @see zoneFreeFunc
 */
void extractAllSecurityZonesPublic(icLinkedList *targetList)
{
    lockSecurityMutex();
    if (didInit == true)
    {
        // loop through all zones. clone each and add to the 'targetList'
        //
        icLinkedListIterator *loop = linkedListIteratorCreate(zoneList);
        while (linkedListIteratorHasNext(loop) == true)
        {
            securityZone *next = (securityZone *)linkedListIteratorGetNext(loop);
            linkedListAppend(targetList, clone_securityZone(next));
        }
        linkedListIteratorDestroy(loop);
    }
    unlockSecurityMutex();
}

/*
 * return the zone with the supplied zoneNumber.
 * caller is responsible for freeing the returned object
 */
securityZone *getSecurityZoneForNumberPublic(uint32_t zoneNumber)
{
    // create the wrapper, then find the match for this 'zone'
    //
    securityZone *zone = create_securityZone();
    if (extractSecurityZoneForNumberPublic(zoneNumber, zone) == false)
    {
        // not found, so cleanup and return NULL
        //
        destroy_securityZone(zone);
        zone = NULL;
    }
    return zone;
}

/*
 * locate the zone with the supplied zoneNumber, and copy
 * the information into the provided object.
 */
bool extractSecurityZoneForNumberPublic(uint32_t zoneNumber, securityZone *targetZone)
{
    bool worked = false;

    lockSecurityMutex();
    if (didInit == true)
    {
        // find the securityZone with the matching zoneNum
        //
        securityZone *match = (securityZone *)linkedListFind(zoneList, &zoneNumber, findZoneByZoneNumber);
        if (match != NULL)
        {
            // found the match, so clone information from 'match' to 'targetZone'
            // TODO: make this more efficient
            //
            cJSON *json = encode_securityZone_toJSON(match);
            decode_securityZone_fromJSON(targetZone, json);
            cJSON_Delete(json);
            worked = true;
        }
    }
    unlockSecurityMutex();

    return worked;
}

/*
 * return a pointer to the securityZone with this zoneNumber
 * this is internal and should therefore be treated as READ-ONLY
 */
securityZone *findSecurityZoneForNumberPrivate(uint32_t zoneNumber)
{
    return (securityZone *)linkedListFind(zoneList, &zoneNumber, findZoneByZoneNumber);
}

securityZone *findSecurityZoneForDisplayIndexPrivate(uint32_t displayIndex)
{
    return (securityZone *)linkedListFind(zoneList, &displayIndex, findZoneByDisplayIndex);
}

/*
 * returns if there are any life-safety zones in the list.
 * primarily used by trouble 'replay' when determining if a system
 * trouble needs to be treated as life-safety because we have one
 */
bool haveLifeSafetyZonePrivate()
{
    // already have the lock, so just return our flag
    //
    return hasLifeSafetyZone;
}

/*
 * reset or set the 'hasLifeSafetyZone' flag based on the known zones
 * assumes global security mutex is held.
 */
static void recalculateHasLifeSafetyFlag()
{
    // reset to false and loop to see if any others exist
    //
    hasLifeSafetyZone = false;
    icLinkedListIterator *loop = linkedListIteratorCreate(zoneList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        securityZone *next = (securityZone *)linkedListIteratorGetNext(loop);
        if (isSecurityZoneLifeSafety(next) == true)
        {
            hasLifeSafetyZone = true;
            break;
        }
    }
    linkedListIteratorDestroy(loop);
}

/*
 * taskExecutor 'run' function for delivering events
 * from updateSecurityZonePublic() and updating the
 * device storage
 */
static void updateZoneTaskRun(void *taskObj, void *taskArg)
{
    securityZoneEvent *event = (securityZoneEvent *)taskObj;
    uint8_t *updateDevice = (uint8_t *)taskArg;

    // use the zone in the event to persist the changes in deviceService
    // (note this is a copy of the original zone)
    //
    persistZoneMetadata(event->zone, *updateDevice);

    // safely broadcast the event
    //
    broadcastZoneEvent(event, event->baseEvent.eventCode, event->baseEvent.eventValue, event->requestId);

    // destroy the event, zone, and arg when our 'free' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * updateZoneTaskRun function
 */
static void updateZoneTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event, zone & the boolean arg
    //
    securityZoneEvent *event = (securityZoneEvent *)taskObj;
    destroy_securityZoneEvent(event);

    uint8_t *updateDevice = (uint8_t *)taskArg;
    free(updateDevice);
}


/*
 * emmit the event and cache an update the the metadata via deviceService
 */
static void updateZoneAfterUpdating(securityZone *zone, int32_t eventCode, int32_t eventValue,
                                    indicationType indication, uint64_t requestId, uint8_t changedMask)
{
    // create the base event (processZoneEventForAlarmPanel will finish it)
    // note that we'll use the supplied 'zone' for the first part (updating
    // state within security), then use a clone for the background portion
    // (notify deviceService and send the event).
    // into our sharedTaskExecutor so we don't hold the SECURITY_MTX too long
    //
    securityZoneEvent *event = create_securityZoneEvent();
    event->baseEvent.eventCode = eventCode;
    event->indication = indication;
    destroy_securityZone(event->zone);
    event->requestId = requestId;
    event->zone = zone;           // pointer to original zone object

    // while we still HAVE THE LOCK, forward the event over to alarmPanel
    // so it can update ready status and popualate panelStatus of the event
    //
    if (supportAlarms() == true)
    {
        processZoneEventForAlarmPanel(event);
    }
    else
    {
        populateSystemPanelStatusPrivate(event->panelStatus);
    }

    // now replace the zone pointer with a clone for the bagrounding
    //
    event->zone = clone_securityZone(zone);

    // add the event and the 'labelChanged' to the taskExecutor
    //
    uint8_t *arg = (uint8_t *)malloc(sizeof(uint8_t));
    *arg = changedMask;
    appendSecurityTask(event, arg, updateZoneTaskRun, updateZoneTaskFree);
}

/*
 * update the zone within the list using the information provided.  if
 * the 'requestId' is greater then 0, then it will be included with
 * the securityZoneEvent
 */
updateZoneResultCode updateSecurityZonePublic(securityZone *copy, uint64_t requestId)
{
    // get the global lock
    //
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return UPDATE_ZONE_FAIL_MISSING_ZONE;
    }

    // first find the securityZone in our list with the matching deviceId and endpointId
    //
    ZoneSearchInfo zoneSearchInfo = { .deviceUUID = copy->deviceId, .endpointId = copy->endpointId };
    securityZone *target = linkedListFind(zoneList, &zoneSearchInfo, findZoneByDeviceIdAndEndpointId);
    if (target == NULL)
    {
        icLogWarn(SECURITY_LOG, "unable to fund zone matching deviceId=%s endpointId=%s",
                  (copy->deviceId != NULL) ? copy->deviceId : "NULL",
                  (copy->endpointId != NULL) ? copy->endpointId : "NULL");
        unlockSecurityMutex();
        return UPDATE_ZONE_FAIL_MISSING_ZONE;
    }

    // first, see if we're altering the type and/or function.  if so we need to double check
    // that the caller isn't doing something stupid (ex: smoke sensor set to interior follower)
    //
    if (target->zoneType != copy->zoneType || target->zoneFunction != copy->zoneFunction)
    {
        icLogDebug(SECURITY_LOG, "update zone, checking that type %s and function %s are compatible",
                   securityZoneTypeLabels[copy->zoneType],
                   securityZoneFunctionTypeLabels[copy->zoneFunction]);
        if (validateSecurityZoneTypeAndFunction(copy->zoneType, copy->zoneFunction) == false)
        {
            icLogWarn(SECURITY_LOG, "unable to modify zone %" PRIu32 "; incompatible type/function combo %s/%s",
                      target->zoneNumber,
                      securityZoneTypeLabels[copy->zoneType],
                      securityZoneFunctionTypeLabels[copy->zoneFunction]);
            unlockSecurityMutex();
            return UPDATE_ZONE_FAIL_MISMATCH;
        }
    }

    // since we found a match, init to a success return so that caller doesn't
    // think this failed if nothing really needed to change
    //
    updateZoneResultCode worked = UPDATE_ZONE_SUCCESS;
    int32_t eventCode = ZONE_EVENT_UPDATED_CODE;
    indicationType indication = INDICATION_VISUAL;

    // apply anything in 'copy' that is different (that can be changed externally)
    //
    bool anyChanges = false;
    uint8_t changedMask = UPDATE_ZONE_METADATA_FLAG;
    bool wasConfigured = target->isConfigured;
    if (copy->label != NULL && stringCompare(target->label, copy->label, false) != 0)
    {
        // update the zone label
        //
        free(target->label);
        target->label = copy->label;
        copy->label = NULL;

        // set flags
        //
        icLogDebug(SECURITY_LOG, "update zone, asked to change the label");
        target->isConfigured = true;
        anyChanges = true;
        changedMask |= UPDATE_LABEL_FLAG;
    }
    if (target->displayIndex != copy->displayIndex)
    {
        target->displayIndex = copy->displayIndex;
        target->isConfigured = true;
        icLogDebug(SECURITY_LOG, "update zone, asked to change the display index");
        anyChanges = true;
    }
    if (target->isBypassed != copy->isBypassed && isZoneBypassable(target->zoneType) == true)
    {
        // apply bypass change, then switch to a different event code
        // and set the VISUAL flag so the UI knows to show the state change
        //
        target->isBypassed = copy->isBypassed;
        anyChanges = true;
        changedMask |= UPDATE_LPM_FLAG | UPDATE_BYPASS_FLAG;
        indication = INDICATION_VISUAL;
        if (target->isBypassed == true)
        {
            eventCode = ZONE_EVENT_BYPASSED_CODE;
        }
        else
        {
            eventCode = ZONE_EVENT_UNBYPASSED_CODE;
        }
        icLogDebug(SECURITY_LOG, "update zone, asked to change the bypassed state");
    }
    if (target->zoneFunction != copy->zoneFunction)
    {
        target->zoneFunction = copy->zoneFunction;
        target->isConfigured = true;
        changedMask |= UPDATE_LPM_FLAG;
        anyChanges = true;
        icLogDebug(SECURITY_LOG, "update zone, asked to change the function");
    }
    if (target->zoneType != copy->zoneType)
    {
        target->zoneType = copy->zoneType;
        target->isConfigured = true;
        changedMask |= UPDATE_SENSOR_TYPE_FLAG;
        anyChanges = true;
        icLogDebug(SECURITY_LOG, "update zone, asked to change the type");
    }
    if (target->zoneMute != copy->zoneMute)
    {
        target->zoneMute = copy->zoneMute;
        target->isConfigured = true;
        icLogDebug(SECURITY_LOG, "update zone, asked to change the mute type");
        anyChanges = true;
    }

    // Even if nothing got changed, if we weren't "configured"
    // before, we are now, so make it so
    if (!wasConfigured)
    {
        target->isConfigured = true;
        anyChanges = true;
    }

    icLogDebug(SECURITY_LOG, "update zone, anychanges = %s", (anyChanges == true) ? "true" : "false");
    if (anyChanges == true)
    {
        // sync the zone with deviceService and send the event in the background
        //
        updateZoneAfterUpdating(target, eventCode, 0, indication, requestId, changedMask);
    }
    unlockSecurityMutex();

    icLogDebug(SECURITY_LOG, "zone: update zone %"PRIu32" returning %s", copy->zoneNumber, updateZoneResultCodeLabels[worked]);
    return worked;
}

#if 0    // DISABLED for now.  These were "nice to have" that are not getting utilized
/*
 * update the zone muted flag.
 */
bool updateSecurityZoneMutedPublic(uint32_t zoneNum, zoneMutedType muteState, uint64_t requestId)
{
    bool worked = false;

    // get the global lock
    //
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return worked;
    }

    // find the zone to update
    //
    securityZone *target = findSecurityZoneForNumberPrivate(zoneNum);
    if (target != NULL)
    {
        // since we found a match, set 'worked' to true so that caller doesn't
        // think this failed if nothing really needed to change
        //
        worked = true;
        bool doApply = false;
        uint8_t changedMask = 0;

        // apply the mute change (if different)
        //
        if (target->zoneMute != muteState)
        {
            target->zoneMute = muteState;
            target->isConfigured = true;
            changedMask |= UPDATE_ZONE_METADATA_FLAG;
            doApply = true;
        }

        if (doApply == true)
        {
            // sync the zone with deviceService and send the event in the background
            //
            updateZoneAfterUpdating(target, ZONE_EVENT_UPDATED_CODE, 0, INDICATION_VISUAL, requestId, changedMask);
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "zone: unable to locate zone %"PRIu32, zoneNum);
    }

    unlockSecurityMutex();
    return worked;
}

/*
 * update the zone bypassed flag.
 */
bool updateSecurityZoneBypassedPublic(uint32_t zoneNum, bool bypassed, uint64_t requestId)
{
    bool worked = false;

    // get the global lock
    //
    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return worked;
    }

    // find the zone to update
    //
    securityZone *target = findSecurityZoneForNumberPrivate(zoneNum);
    if (target != NULL && isZoneBypassable(target->zoneType) == true)
    {
        // since we found a match, set 'worked' to true so that caller doesn't
        // think this failed if nothing really needed to change
        //
        worked = true;
        bool doApply = false;

        // even if we don't actually change the label, update the configured flag
        //
        uint8_t changedMask = UPDATE_ZONE_METADATA_FLAG;
        if (target->isConfigured == false)
        {
            target->isConfigured = true;
            doApply = true;
        }

        int32_t eventCode = 0;
        if (target->isBypassed != bypassed)
        {
            // apply bypass change, then switch to a different event code
            // and set the VISUAL flag so the UI knows to show the state change
            //
            target->isBypassed = bypassed;
            doApply = true;
            changedMask |= UPDATE_LPM_FLAG;
            if (target->isBypassed == true)
            {
                eventCode = ZONE_EVENT_BYPASSED_CODE;
            }
            else
            {
                eventCode = ZONE_EVENT_UNBYPASSED_CODE;
            }
        }

        if (doApply == true)
        {
            // sync the zone with deviceService and send the event in the background
            //
            updateZoneAfterUpdating(target, eventCode, 0, INDICATION_VISUAL, requestId, changedMask);
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "zone: unable to locate zone %"PRIu32", or it is unbypassable", zoneNum);
    }

    unlockSecurityMutex();
    return worked;
}
#endif  // end if 0

/*
 * removes the zone with this zoneNumber.  if the 'requestId' is
 * greater then 0, then it will be included with the securityZoneEvent
 */
bool removeSecurityZonePublic(uint32_t zoneNumber, uint64_t requestId)
{
    // seems odd, but to keep in sync with device service we're not going to
    // mess with our list here.  instead, we'll locate the zone, then ask
    // deviceService to delete the sensor & metadata.  if that is successful,
    // we'll get the 'endpointRemove' event.  at that point we'll remove the
    // zone and send the security-zone-deleted event.
    //
    bool worked = false;
    securityZone *copy = getSecurityZoneForNumberPublic(zoneNumber);
    if (copy != NULL)
    {
        icLogDebug(SECURITY_LOG, "zone: deleting endpoint %s", copy->endpointId);
        DSEndpointRequest *epRequest = create_DSEndpointRequest();
        epRequest->deviceUuid = strdup(copy->deviceId);
        epRequest->endpointId = strdup(copy->endpointId);
        if (deviceService_request_REMOVE_ENDPOINT(epRequest, &worked) == IPC_SUCCESS)
        {
            // simply wait for the event via deviceService
            //
            icLogDebug(SECURITY_LOG, "zone: successfully deleted endpoint %s", copy->endpointId);
        }
        destroy_DSEndpointRequest(epRequest);
    }
    destroy_securityZone(copy);
    return worked;
}

/*
 * attempts a zone bypass toggle with a provided code.  The code must be valid
 * and authorized for bypassing.
 */
bool bypassToggleSecurityZonePublic(uint32_t displayIndex,
                                    const char *code,
                                    const armSourceType source,
                                    uint64_t requestId)
{
    bool worked = false;

    lockSecurityMutex();
    if (didInit == false)
    {
        unlockSecurityMutex();
        return worked;
    }

    /*
     * DSC PIM will not supply a code. Check the code for panels that supply it (Vista)
     */
    userAuthLevelType authType;
    if (code == NULL && source == ARM_SOURCE_TAKEOVER_KEYPAD)
    {
        authType = KEYPAD_USER_LEVEL_MASTER;
    }
    else
    {
        // TODO: Needs implementation
        authType = KEYPAD_USER_LEVEL_INVALID;
    }

    if(authType == KEYPAD_USER_LEVEL_MASTER || authType == KEYPAD_USER_LEVEL_STANDARD)
    {
        securityZone *target = findSecurityZoneForDisplayIndexPrivate(displayIndex);
        if (target != NULL && isZoneBypassable(target->zoneType) == true)
        {
            target->isBypassed = !target->isBypassed; //toggle the current isBypassed setting

            // create the base event (processZoneEventForAlarmPanel will finish it)
            // note that we'll clone 'zone' since sending the event will be dropped
            // into our sharedTaskExecutor so we don't hold the SECURITY_MTX too long
            //
            securityZoneEvent *event = create_securityZoneEvent();
            event->baseEvent.eventCode = target->isBypassed ? ZONE_EVENT_BYPASSED_CODE : ZONE_EVENT_UNBYPASSED_CODE;
            event->indication = INDICATION_VISUAL;
            destroy_securityZone(event->zone);
            event->zone = target;
            event->requestId = requestId;

            // while we still HAVE THE LOCK, forward the event over to alarmPanel
            // so it can update ready status and popualate panelStatus of the event
            //
            if (supportAlarms() == true)
            {
                processZoneEventForAlarmPanel(event);
            }
            else
            {
                populateSystemPanelStatusPrivate(event->panelStatus);
            }

            // now the clone
            //
            event->zone = clone_securityZone(target);

            // add the event and the 'lpm changed' to the taskExecutor
            //
            uint8_t *arg = (uint8_t *)malloc(sizeof(uint8_t));
            *arg = UPDATE_LPM_FLAG | UPDATE_BYPASS_FLAG;
            appendSecurityTask(event, arg, updateZoneTaskRun, updateZoneTaskFree);

            worked = true;
        }
        else
        {
            icLogWarn(SECURITY_LOG, "unable to toggle bypass flag on zone %"PRIu32", either not located or unbypassable", displayIndex);
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "zone: unable to bypass zone %"PRIu32" : code invalid or not allowed", displayIndex);
    }

    unlockSecurityMutex();

    return worked;
}

/**
 * Get the zone number based on a URI(could be resouce, endpoint, or device)
 *
 * @param uri the uri to use when searching
 * @return the zone number or 0 if not found
 */
uint32_t getZoneNumberForUriPublic(char *uri)
{
    uint32_t zoneNumber = 0;

    // First try by endpoint
    DSEndpoint *endpoint = create_DSEndpoint();
    if (deviceService_request_GET_ENDPOINT_BY_URI(uri, endpoint) == IPC_SUCCESS)
    {
        lockSecurityMutex();
        if (didInit == true)
        {
            ZoneSearchInfo zoneSearchInfo = { .deviceUUID = endpoint->ownerId, .endpointId = endpoint->id };
            securityZone *zone = linkedListFind(zoneList, &zoneSearchInfo, findZoneByDeviceIdAndEndpointId);
            if (zone != NULL)
            {
                zoneNumber = zone->zoneNumber;
            }
        }
        unlockSecurityMutex();
    }
    else
    {
        icLogError(SECURITY_LOG, "Failed to get endpoint for uri %s", uri);
    }
    // Cleanup
    destroy_DSEndpoint(endpoint);

    if (zoneNumber == 0)
    {
        DSDevice *device = create_DSDevice();
        if (deviceService_request_GET_DEVICE_BY_URI(uri, device) == IPC_SUCCESS)
        {
            lockSecurityMutex();
            if (didInit == true)
            {
                securityZone *zone = linkedListFind(zoneList, device->id, findZoneByDeviceId);
                if (zone != NULL)
                {
                    zoneNumber = zone->zoneNumber;
                }
            }
            unlockSecurityMutex();
        }
        else
        {
            icLogError(SECURITY_LOG, "Failed to get device for uri %s", uri);
        }
        // Cleanup
        destroy_DSDevice(device);
    }

    return zoneNumber;
}

/*
 * returns true is security zone is a panic zone
 */
bool isSecurityZonePanic(securityZone *zone)
{
    if (zone == NULL)
    {
        icLogWarn(SECURITY_LOG, "unable to use zone for %s", __FUNCTION__);
        return false;
    }

    // all silents are "panics"
    //
    if (isSecurityZoneSilent(zone) == true)
    {
        return true;
    }

    // now we're down to a small list
    //
    if (zone->zoneType == SECURITY_ZONE_TYPE_PANIC ||
        zone->zoneFunction == SECURITY_ZONE_FUNCTION_FIRE_24HOUR)
    {
        return true;
    }

    return false;
}

/*
 * returns true is security zone is a silent zone
 */
bool isSecurityZoneSilent(securityZone *zone)
{
    // first look at the zone type
    //
    if (zone->zoneType == SECURITY_ZONE_TYPE_DURESS)
    {
        // duress is always silent
        return true;
    }
    // then look at the zone function
    //
    switch (zone->zoneFunction)
    {
        case SECURITY_ZONE_FUNCTION_SILENT_BURGLARY:
        case SECURITY_ZONE_FUNCTION_SILENT_24HOUR:
            return true;

        default:
            // all other functions
            return false;
    }
}

/*
 * returns true is security zone is a life safety device
 */
bool isSecurityZoneLifeSafety(securityZone *zone)
{
    if (zone == NULL)
    {
        icLogWarn(SECURITY_LOG, "unable to use zone for %s", __FUNCTION__);
        return false;
    }

    // first look at the zone type
    //
    switch (zone->zoneType)
    {
        case SECURITY_ZONE_TYPE_SMOKE:
        case SECURITY_ZONE_TYPE_CO:
            return true;

        default:
            // all other functions
            return false;
    }
}

/*
 * returns true is security zone is a panic zone or life safety device
 */
bool isSecurityZonePanicOrLifeSafety(securityZone *zone)
{
    if (zone == NULL)
    {
        icLogWarn(SECURITY_LOG, "unable to use zone for %s", __FUNCTION__);
        return false;
    }

    if (isSecurityZonePanic(zone) == true ||
        isSecurityZoneLifeSafety(zone) == true)
    {
        return true;
    }

    return false;
}

/*
 * returns true is security zone is a non-silent burglary zone
 */
bool isSecurityZoneBurglary(securityZone *zone)
{
    // do the easy ones first.  technically a silent zone is burg, but that
    // is not the intent here
    //
    if (zone == NULL || isSecurityZonePanicOrLifeSafety(zone) == true || isSecurityZoneSilent(zone) == true)
    {
        return false;
    }

    // first the zone type
    switch (zone->zoneType)
    {
        case SECURITY_ZONE_TYPE_DOOR:
        case SECURITY_ZONE_TYPE_WINDOW:
        case SECURITY_ZONE_TYPE_MOTION:
        case SECURITY_ZONE_TYPE_GLASS_BREAK:

            // now the zone function
            //
            switch (zone->zoneFunction)
            {
                case SECURITY_ZONE_FUNCTION_ENTRY_EXIT:
                case SECURITY_ZONE_FUNCTION_PERIMETER:
                case SECURITY_ZONE_FUNCTION_TROUBLE_DAY_ALARM_NIGHT:
                case SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR:
                case SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER:
                case SECURITY_ZONE_FUNCTION_INTERIOR_WITH_DELAY:
                case SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT:
                case SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT_DELAY:
                    return true;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    return false;
}

/**
 * Get the zones based on the deviceId
 * this is internal and the zones in the list should therefore be treated as READ-ONLY
 * Caller must free the returned list but should not free the zones contained within
 */
icLinkedList *getZonesForDeviceIdPrivate(char *deviceId)
{
    icLinkedList *zones = linkedListCreate();

    if (deviceId != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(zoneList);
        while(linkedListIteratorHasNext(iter))
        {
            securityZone *zone = (securityZone *)linkedListIteratorGetNext(iter);
            if (zone->deviceId != NULL && strcmp(zone->deviceId, deviceId) == 0)
            {
                linkedListAppend(zones, zone);
            }
        }
        linkedListIteratorDestroy(iter);
    }

    return zones;
}

/**
 * Convert a pre zilker security zone to a security zone if possible
 * @param sensor the endpoint to convert
 * @return true if success, false otherwise
 */
static bool migratePreZilkerSecurityZone(DSEndpoint *sensor)
{
    bool success = false;

    char *jsonStr = NULL;
    // Look for the migrated user properties that hold the zone information
    if (deviceHelperReadMetadataByOwner(sensor->uri, MIGRATED_USER_PROPERTIES_METADATA, &jsonStr) == true)
    {
        icLogDebug(SECURITY_LOG, "PreZilkerMigration: Migrating zone for %s", sensor->uri);

        cJSON *jsonObj = cJSON_Parse(jsonStr);
        if (jsonObj != NULL)
        {
            securityZone *zone = createZoneFromEndpoint(sensor);

            bool zoneProcessFailure = false;

            // Loop through the zone related user props and populate the security zone
            cJSON *elem;
            cJSON_ArrayForEach(elem, jsonObj)
            {
                if (elem->string != NULL)
                {
                    if (strcmp(elem->string, "Zone.ZoneNumber") == 0)
                    {
                        if (stringToUint32(elem->valuestring, &zone->zoneNumber) == false)
                        {
                            icLogError(SECURITY_LOG, "PreZilkerMigration: Failed to read pre-zilker zone number %s",
                                       elem->valuestring);
                            zoneProcessFailure = true;
                            break;
                        }
                    }
                    else if (strcmp(elem->string, "Zone.ZoneType") == 0)
                    {
                        // 1 to 1 mapping for pre zilker zone type to our zone type
                        uint64_t zoneType;
                        if (stringToUnsignedNumberWithinRange(elem->valuestring, &zoneType, 10,
                                                              SECURITY_ZONE_TYPE_UNKNOWN, SECURITY_ZONE_TYPE_MEDICAL) ==
                            true)
                        {
                            zone->zoneType = zoneType;
                        }
                        else
                        {
                            icLogError(SECURITY_LOG, "PreZilkerMigration: Failed to read pre-zilker zone type %s",
                                       elem->valuestring);
                            zoneProcessFailure = true;
                            break;
                        }
                    }
                    else if (strcmp(elem->string, "Zone.FunctionType") == 0)
                    {
                        // 1 to 1 mapping for pre zilker function type to our function type
                        uint64_t zoneFunction;
                        if (stringToUnsignedNumberWithinRange(elem->valuestring, &zoneFunction, 10,
                                                              SECURITY_ZONE_FUNCTION_UNKNOWN,
                                                              SECURITY_ZONE_FUNCTION_DISARM) == true)
                        {
                            zone->zoneFunction = zoneFunction;
                        }
                        else
                        {
                            icLogError(SECURITY_LOG, "PreZilkerMigration: Failed to read pre-zilker zone function %s",
                                       elem->valuestring);
                            zoneProcessFailure = true;
                            break;
                        }
                    }
                    else if (strcmp(elem->string, "Zone.Edited") == 0)
                    {
                        zone->isConfigured = stringCompare(elem->valuestring, "true", true) == 0;
                    }
                    else if (strcmp(elem->string, "Zone.TestMode") == 0)
                    {
                        zone->isInTestMode = stringCompare(elem->valuestring, "true", true) == 0;
                    }
                    else if (strcmp(elem->string, "orderIndex") == 0)
                    {
                        if (stringToUint32(elem->valuestring, &zone->displayIndex) == true)
                        {
                            // Order index is 0 based, zone display index is 1 based, so adjust accordingly
                            ++zone->displayIndex;
                        }
                        else
                        {
                            icLogError(SECURITY_LOG,
                                       "PreZilkerMigration: Failed to read pre-zilker zone display index %s",
                                       elem->valuestring);
                            zoneProcessFailure = false;
                            break;
                        }

                    }
                    else if (strcmp(elem->string, "Zone.SwingShutdown") == 0)
                    {
                        zone->inSwingerShutdown = stringCompare(elem->valuestring, "true", true) == 0;
                        // We aren't going to worry about trying to create a trouble if this is in swinger shutdown
                        // We should be disarmed when we upgrade so really there should be no zones remaining in
                        // swinger shutdown...
                    }
                    else if (strcmp(elem->string, "Zone.SensorId") == 0)
                    {
                        if (stringToUint32(elem->valuestring, &zone->sensorId) == false)
                        {
                            icLogError(SECURITY_LOG,
                                       "PreZilkerMigration: Failed to read pre-zilker zone sensor id %s",
                                       elem->valuestring);
                            zoneProcessFailure = true;
                            break;
                        }
                    }
                    else if (strcmp(elem->string, "Zone.Silenced") == 0)
                    {
                        // Oddly enough we stored these as strings, but fortunately the strings are the same as we use
                        bool foundValue = false;
                        for(int i = ZONE_NO_EVENT_MUTED; i <= ZONE_ALL_EVENT_MUTED; ++i)
                        {
                            if (stringCompare(elem->valuestring, (char *)zoneMutedTypeLabels[i], false) == 0)
                            {
                                zone->zoneMute = i;
                                foundValue = true;
                                break;
                            }
                        }

                        if (foundValue == false)
                        {
                            icLogError(SECURITY_LOG,
                                       "PreZilkerMigration: Failed to process pre-zilker zone muted type %s",
                                       elem->valuestring);
                            zoneProcessFailure = true;
                            break;
                        }
                    }
                }
            }

            if (zoneProcessFailure == false)
            {
                // persist metadata
                uint8_t update = UPDATE_LPM_FLAG | UPDATE_ZONE_METADATA_FLAG;
                success = persistZoneMetadata(zone, update);

                icLogDebug(SECURITY_LOG, "PreZilkerMigration: Successfully stored migrated metadata for zone for %s",
                           sensor->uri);
            }
            else
            {
                icLogError(SECURITY_LOG, "PreZilkerMigration: Failed migrating zone for %s", sensor->uri);
            }

            // Cleanup
            destroy_securityZone(zone);
            cJSON_Delete(jsonObj);
        }
    }

    // Cleanup
    free(jsonStr);

    return success;
}

/**
 * Read a security zone from its metadata and add it to our internal list of zones
 * @param sensor the endpoint to read from
 * @return true if successful, false otherwise
 */
static bool readSecurityZoneFromMetadata(DSEndpoint *sensor)
{
    bool success = false;

    char *jsonStr = NULL;

    if (deviceHelperReadMetadataByOwner(sensor->uri, SEC_ZONE_ATTRIBUTE, &jsonStr) == true)
    {
        if (jsonStr == NULL)
        {
            // no meta-data on this sensor
            //
            icLogDebug(SECURITY_LOG, "zone: unable to find metadata on sensor %s", sensor->uri);
            return false;
        }

        // device was setup as a 'security zone', start out with the basic info
        //
        securityZone *zone = createZoneFromEndpoint(sensor);

        // now parse the metadata to get "zone" details
        //
        cJSON *jsonObj = cJSON_Parse(jsonStr);
        if (decodeSecZone_fromJSON(zone, jsonObj) == true)
        {
            // TODO: do we need to check zone validity???

            // potentially update our 'hasLifeSafetyZone'
            //
            if (isSecurityZoneLifeSafety(zone) == true)
            {
                hasLifeSafetyZone = true;
            }

            setZoneIsSimpleDevice(zone);

            // add to our zone list, keeping them sorted by displayIndex
            //
            printZone(zone);
            sortedLinkedListAdd(zoneList, zone, zoneIndexCompareFunc);
            uint32_t *allocatedZoneNumber = malloc(sizeof(uint32_t));
            *allocatedZoneNumber = zone->zoneNumber;

            char *key = getAllocatedZoneKey(zone->deviceId, sensor->id);
            if (hashMapPut(allocatedZoneNumbersByKey, key, (uint16_t)(strlen(key) + 1), allocatedZoneNumber) == false)
            {
                icLogError(SECURITY_LOG, "zone: Duplicate zone number found, unable to add zone %" PRIu32 " for device %s", *allocatedZoneNumber, key);
                free(key);
                // TODO Can we automatically fix this, e.g. assign a new zone number?
            }
            else
            {
                success = true;
            }
        }
        else
        {
            // invalid zone definition
            //
            icLogWarn(SECURITY_LOG, "zone: unable to extract 'securityZone' information from endpoint %s", sensor->id);
            destroy_securityZone(zone);
        }

        // cleanup
        //
        if (jsonObj != NULL)
        {
            cJSON_Delete(jsonObj);
        }
        free(jsonStr);
    }

    return success;
}

/*
 * reload all zone information from deviceService, converting each
 * 'sensor' into a 'securityZone', saving them into our 'zoneList'
 *
 * internal funcation that assumes the mutex is held
 */
static bool reloadFromDeviceService()
{
    // clear exiting list
    //
    bool retVal = false;
    linkedListClear(zoneList, zoneFreeFunc);
    hashMapClear(allocatedZoneNumbersByKey, NULL);
    hasLifeSafetyZone = false;

    // wait for deviceService to be available
    //
    waitForServiceAvailable(DEVICESERVICE_IPC_PORT_NUM, ONE_MINUTE_SECS);

    // ask deviceService for all 'endpoints' with a 'sensor' profile
    //
    icLogInfo(SECURITY_LOG, "zone: loading sensor endpoints from deviceService...");
    IPCCode ipcRc;
    DSEndpointList *tmpList = create_DSEndpointList();
    if ((ipcRc = deviceService_request_GET_ENDPOINTS_BY_PROFILE(SENSOR_PROFILE, tmpList)) == IPC_SUCCESS)
    {
        // loop through all of them, creating a securityZone for the ones that make sense
        //
        icLinkedListIterator *loop = linkedListIteratorCreate(tmpList->endpointList);
        while (linkedListIteratorHasNext(loop) == true)
        {
            // examine the device by extracting the 'secZone' attribute.
            //
            DSEndpoint *sensor = (DSEndpoint *) linkedListIteratorGetNext(loop);
            char *qualifiedUri = createResourceUri(sensor->uri, SENSOR_PROFILE_RESOURCE_QUALIFIED);
            DSResource *qualifiedResource = get_DSResource_from_DSEndpoint_resources(sensor,
                                                                                     qualifiedUri);
            free(qualifiedUri);
            // Skip over any unqualified sensors
            if (qualifiedResource != NULL && stringCompare(qualifiedResource->value, "true", true) == 0)
            {
                if (readSecurityZoneFromMetadata(sensor) == false)
                {
                    icLogWarn(SECURITY_LOG, "Could not read zone metadata for %s", sensor->uri);
                }
            }
        }

        // cleanup iterator and set return value
        //
        linkedListIteratorDestroy(loop);
        icLogInfo(SECURITY_LOG, "zone: done loading sensor endpoints from deviceService, count=%d", linkedListCount(zoneList));
        retVal = true;
    }
    else
    {
        icLogError(SECURITY_LOG, "zone: unable to obtain sensor endpoints from deviceService; rc=%d %s", ipcRc, IPCCodeLabels[ipcRc]);
    }

    // cleanup & return
    //
    destroy_DSEndpointList(tmpList);
    icLogDebug(SECURITY_LOG, "zone: done loading zones, count=%d", linkedListCount(zoneList));
    return retVal;
}

/*
 * implementation of 'linkedListItemFreeFunc' to release
 * a securityZone object.
 */
void zoneFreeFunc(void *item)
{
    if (item != NULL)
    {
        securityZone *zone = (securityZone *)item;
        destroy_securityZone(zone);
    }
}

/*
 * implementation of 'linkedListSortCompareFunc' to keep the
 * securityZone objects sorted by 'displayIndex'.  called when
 * adding new objects to our 'zoneList'
 */
static int8_t zoneIndexCompareFunc(void *newItem, void *element)
{
    securityZone *newZone = (securityZone *)newItem;
    securityZone *currZone = (securityZone *)element;

    // see if the displayIndex of the new object is less, greater, or equal
    //
    if (newZone->displayIndex < currZone->displayIndex)
    {
        return -1;
    }
    else if (newZone->displayIndex > currZone->displayIndex)
    {
        return 1;
    }

    // NOTE: should not happen!!!
    //
    return 0;
}

/*
 * implementation of 'linkedListSortCompareFunc' to keep sort
 * out items by zone number
 */
static int8_t zoneNumberCompareFunc(void *newItem, void *element)
{
    uint32_t *newZoneNumber = (uint32_t *)newItem;
    uint32_t *currZoneNumber = (uint32_t *)element;

    // see if the zone number of the new object is less, greater, or equal
    //
    if (*newZoneNumber < *currZoneNumber)
    {
        return -1;
    }
    else if (*newZoneNumber > *currZoneNumber)
    {
        return 1;
    }

    // NOTE: should not happen!!!
    //
    return 0;
}

/*
 * implementation of 'linkedListCompareFunc' to find a securityZone
 * in our list that matches the supplied information
 */
static bool findZoneByDeviceIdAndEndpointId(void *search, void *item)
{
    ZoneSearchInfo *zoneSearchInfo = (ZoneSearchInfo *)search;
    securityZone *zone = (securityZone *)item;

    if (stringCompare(zoneSearchInfo->deviceUUID, zone->deviceId, false) == 0 &&
        stringCompare(zoneSearchInfo->endpointId, zone->endpointId, false) == 0)
    {
        // found our match
        //
        return true;
    }

    return false;
}

/*
 * implementation of 'linkedListCompareFunc' to find a securityZone
 * in our list that matches the supplied 'deviceId'
 */
static bool findZoneByDeviceId(void *search, void *item)
{
    char *deviceId = (char *)search;
    securityZone *zone = (securityZone *)item;

    if (stringCompare(deviceId, zone->deviceId, false) == 0)
    {
        // found our match
        //
        return true;
    }

    return false;
}

/*
 * implementation of 'linkedListCompareFunc' to find a securityZone
 * in our list that matches the supplied 'zoneNumber'
 */
static bool findZoneByZoneNumber(void *search, void *item)
{
    uint32_t *zoneNum = (uint32_t *) search;
    securityZone *zone = (securityZone *) item;

    if (zone->zoneNumber == *zoneNum)
    {
        // found our match
        //
        return true;
    }

    return false;
}

static bool findZoneByDisplayIndex(void *search, void *item)
{
    uint32_t *displayIndex = (uint32_t *) search;
    securityZone *zone = (securityZone *) item;

    return zone->displayIndex == *displayIndex;
}

/*
 * extract a resource from the values map.  the map-key will be in a
 * uri format (ex: /000d6f000c25c74c/ep/1/r/qualified)
 * caller should NOT free the returned string
 */
static char *extractResource(icHashMap *map, const char *ownerUri, const char *resourceName)
{
    // build the URI using the device/endpoint id and the resource name
    //
    char *retVal = NULL;
    char *uri = createResourceUri(ownerUri, resourceName);

    // find in the hash
    //
    DSResource *resource = (DSResource *)hashMapGet(map, (void *)uri, (uint16_t)strlen(uri));
    if (resource != NULL)
    {
        retVal = resource->value;
    }

    // cleanup and return
    //
    free(uri);
    return retVal;
}

static bool getZoneTypeAndFunctionForSensorType(const char *sensorType, securityZoneType *zoneType,
                                                securityZoneFunctionType *zoneFunction)
{
    bool success = false;
    if (sensorType != NULL)
    {
        if (strcmp(sensorType, SENSOR_PROFILE_CONTACT_SWITCH_TYPE) == 0)
        {
            // door, window, ???
            //
            *zoneType = SECURITY_ZONE_TYPE_DOOR;
            *zoneFunction = SECURITY_ZONE_FUNCTION_ENTRY_EXIT;
            success = true;
        }
        else if (strcmp(sensorType, SENSOR_PROFILE_MOTION_TYPE) == 0)
        {
            // motion
            //
            *zoneType = SECURITY_ZONE_TYPE_MOTION;
            *zoneFunction = SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER;
            success = true;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_CO) == 0)
        {
            *zoneType = SECURITY_ZONE_TYPE_CO;
            *zoneFunction = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            success = true;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_WATER) == 0)
        {
            *zoneType = SECURITY_ZONE_TYPE_WATER;
            *zoneFunction = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            success = true;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_SMOKE) == 0)
        {
            *zoneType = SECURITY_ZONE_TYPE_SMOKE;
            *zoneFunction = SECURITY_ZONE_FUNCTION_FIRE_24HOUR;
            success = true;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_GLASS_BREAK) == 0)
        {
            *zoneType = SECURITY_ZONE_TYPE_GLASS_BREAK;
            *zoneFunction = SECURITY_ZONE_FUNCTION_PERIMETER;
            success = true;
        }
        else
        {
            // TODO: more sensor type to zone type mappings?
            // assume door/window for now
            //
            *zoneType = SECURITY_ZONE_TYPE_DOOR;
            *zoneFunction = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            success = true;
        }
    }

    return success;
}

/*
 * create a securityZone using the 'endpoint resources'
 * caller must assign some variables such as id, displayIndex, zoneNum, etc.
 * caller should check if 'label' is empty, and assign one as needed.
 */
static securityZone *createZoneFromEndpoint(DSEndpoint *endpoint)
{
    // create the zone object
    //
    securityZone *zone = create_securityZone();

    // assign info from the endpoint itself
    //
    zone->deviceId = strdup(endpoint->ownerId);
    zone->endpointId = strdup(endpoint->id);
    zone->isFaulted = isEndpointFaulted(endpoint);
    zone->isBypassed = isEndpointBypassed(endpoint);

    // extract the resources so it can be used to obtain details
    //
    icHashMap *map = endpoint->resourcesValuesMap;

    // serial number
    char *serial = extractResource(map, endpoint->uri, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER);
    if (serial != NULL)
    {
        zone->sensorSerialNum = strdup(serial);
    }

    // label
    char *label = extractResource(map, endpoint->uri, COMMON_ENDPOINT_RESOURCE_LABEL);
    if (label != NULL)
    {
        // use the label assigned to the device/endpoint
        //
        zone->label = strdup(label);
    }

    // force to 'monitor 24' if we don't support alarms
    //
    if (supportAlarms() == false)
    {
        zone->zoneFunction = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
    }

    // TODO: extract/save more zone details
    // has battery, has temperature, is wireless,

    return zone;
}

static const char *getSensorTypeForZoneType(securityZoneType zoneType)
{
    const char *sensorType = NULL;
    switch(zoneType)
    {
        case SECURITY_ZONE_TYPE_UNKNOWN:
            sensorType = SENSOR_PROFILE_UNKNOWN_TYPE;
            break;
        case SECURITY_ZONE_TYPE_DOOR:
        case SECURITY_ZONE_TYPE_WINDOW:
            sensorType = SENSOR_PROFILE_CONTACT_SWITCH_TYPE;
            break;
        case SECURITY_ZONE_TYPE_MOTION:
            sensorType = SENSOR_PROFILE_MOTION;
            break;
        case SECURITY_ZONE_TYPE_GLASS_BREAK:
            sensorType = SENSOR_PROFILE_GLASS_BREAK;
            break;
        case SECURITY_ZONE_TYPE_SMOKE:
            sensorType = SENSOR_PROFILE_SMOKE;
            break;
        case SECURITY_ZONE_TYPE_CO:
            sensorType = SENSOR_PROFILE_CO;
            break;
        case SECURITY_ZONE_TYPE_ENVIRONMENTAL:
        case SECURITY_ZONE_TYPE_WATER:
            sensorType = SENSOR_PROFILE_WATER;
            break;
        case SECURITY_ZONE_TYPE_PANIC:
        case SECURITY_ZONE_TYPE_MEDICAL:
        case SECURITY_ZONE_TYPE_DURESS:
            sensorType = SENSOR_PROFILE_PERSONAL_EMERGENCY;
            break;
        default:
            icLogWarn(SECURITY_LOG, "Unable to map zone type %d to a sensor type", zoneType);
            break;
    }

    return sensorType;
}

/*
 * Helper function for determining the LPM Policy
 * for a security zone
 */
static lpmPolicyPriority determineLPMPolicy(securityZone *zone)
{
    lpmPolicyPriority lpmPolicy = LPM_POLICY_NONE;

    // motion sensors type
    //
    if (zone->zoneType == SECURITY_ZONE_TYPE_MOTION)
    {
        // interior follower zone functions
        //
        if ((zone->zoneFunction == SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER ||
            zone->zoneFunction == SECURITY_ZONE_FUNCTION_INTERIOR_WITH_DELAY))
        {
            lpmPolicy = LPM_POLICY_ARMED_AWAY;
        }

        // interior follower armed night zone functions
        //
        else if ((zone->zoneFunction == SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT ||
                zone->zoneFunction == SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT_DELAY))
        {
            lpmPolicy = LPM_POLICY_ARMED_NIGHT;
        }
    }

    // all other zone types
    //
    else
    {
        // make sure the zone is not a 24 inform
        //
        if (zone->zoneFunction != SECURITY_ZONE_FUNCTION_MONITOR_24HOUR)
        {
            // if the zone is not bypassed
            //
            if (zone->isBypassed == false)
            {
                // no reason to look the other zones, at highest priority
                //
                lpmPolicy = LPM_POLICY_ALWAYS;
            }
        }
    }

    return lpmPolicy;
}

/*
 * generate our metadata and persist it in deviceService
 */
static bool persistZoneMetadata(securityZone *zone, uint8_t value)
{
    bool updateLabel = (value & UPDATE_LABEL_FLAG) > 0;
    bool updateLPM = (value & UPDATE_LPM_FLAG) > 0;
    bool updateSensorType = (value & UPDATE_SENSOR_TYPE_FLAG) > 0;
    bool updateBypass = (value & UPDATE_BYPASS_FLAG) > 0;

    // encode the zone details into JSON, and set the 'secZone' attribute
    //
    bool updatedSecZoneAttribute = false;
    cJSON *json = encodeSecZone_toJSON(zone);
    if (json != NULL)
    {
        // save the encoded string as the 'secZone' attribute (in the metadata section of the endpoint)
        //
        char *uri = createEndpointUri(zone->deviceId, zone->endpointId);
        char *jsonStr = cJSON_Print(json);
        if (deviceHelperWriteMetadataByOwner(uri, SEC_ZONE_ATTRIBUTE, jsonStr) == true)
        {
            updatedSecZoneAttribute = true;
            icLogDebug(SECURITY_LOG, "zone: stored securityZone metadata for endpoint=%s", zone->endpointId);
        }
        else
        {
            icLogWarn(SECURITY_LOG, "zone: unable to store securityZone metadata for endpoint=%s", zone->endpointId);
        }
        printZone(zone);

        // cleanup
        //
        cJSON_Delete(json);
        free(jsonStr);
        free(uri);
    }

    // potentially update the label
    //
    bool labelUpdated = false;
    if (updateLabel && zone->label != NULL)
    {
        // pass along to deviceService
        //
        DSWriteResourceRequest *request = create_DSWriteResourceRequest();
        request->uri = createEndpointResourceUri(zone->deviceId, zone->endpointId, COMMON_ENDPOINT_RESOURCE_LABEL);
        request->value = strdup(zone->label);
        bool success = false;
        if (deviceService_request_WRITE_RESOURCE(request, &success) == IPC_SUCCESS)
        {
            labelUpdated = success;
            icLogDebug(SECURITY_LOG, "zone: stored securityZone label for %s", zone->endpointId);
        }
        else
        {
            icLogWarn(SECURITY_LOG, "zone: unable to store securityZone label for %s", zone->endpointId);
        }
        destroy_DSWriteResourceRequest(request);
    }

    // potentially update low power policy metadata
    //
    bool updatedLPMPolicy = false;
    if (updateLPM)
    {
        lpmPolicyPriority lpmPolicy = LPM_POLICY_NONE;

        // get all zones on device
        //
        icLinkedList *tmpZones = getZonesForDeviceIdPrivate(zone->deviceId);

        // see how many zones were found
        //
        if (linkedListCount(tmpZones) != 0)
        {
            // determine the LPM Policy Priority based on:
            // zone type, zone function, and if bypassed
            // will set device to the highest priority needed
            //
            icLinkedListIterator *iter = linkedListIteratorCreate(tmpZones);
            while (linkedListIteratorHasNext(iter))
            {
                securityZone *tmpZone = (securityZone *) linkedListIteratorGetNext(iter);
                if (tmpZone == NULL)
                {
                    continue;
                }

                // determine the LPM policy priority and see
                // if the one we found is higher priority than before
                //
                lpmPolicyPriority tmpLPMPriority = determineLPMPolicy(tmpZone);
                if (lpmPolicy < tmpLPMPriority)
                {
                    lpmPolicy = tmpLPMPriority;
                }
            }

            // cleanup
            linkedListIteratorDestroy(iter);
        }
        else
        {
            // since we could not find the list of zones on device
            // just determine the Policy for the original zone
            //
            lpmPolicy = determineLPMPolicy(zone);
            icLogWarn(SECURITY_LOG, "zone: unable to find zones for device %s when setting LPM Policy, defaulting to %s",
                    zone->deviceId, lpmPolicyPriorityLabels[lpmPolicy]);
        }

        // cleanup, but DO NOT delete the contents of the linked list
        // that WILL REMOVE the zones from the system...
        //
        linkedListDestroy(tmpZones, standardDoNotFreeFunc);

        // now update the LPM metadata policy
        //
        char *uri = createDeviceMetadataUri(zone->deviceId, LPM_POLICY_METADATA);
        if (deviceHelperWriteMetadataByUri(uri, lpmPolicyPriorityLabels[lpmPolicy]))
        {
            updatedLPMPolicy = true;
            icLogDebug(SECURITY_LOG, "zone: stored the LPM policy to be %s for device %s",
                       lpmPolicyPriorityLabels[lpmPolicy], zone->deviceId);
        }
        else
        {
            icLogDebug(SECURITY_LOG, "zone: was unable to to store LPM policy to be %s for device %s",
                       lpmPolicyPriorityLabels[lpmPolicy], zone->deviceId);
        }

        // cleanup
        free(uri);
    }

    // potentially update the zone type
    //
    bool updatedSensorTypeValue = false;
    if (updateSensorType)
    {
        char *endpointUri = createEndpointUri(zone->deviceId, zone->endpointId);
        DSResource *resource = create_DSResource();
        char * resourceUri = createResourceUri(endpointUri, SENSOR_PROFILE_RESOURCE_TYPE);
        if (deviceHelperGetResourceByUri(resourceUri, resource))
        {
            // Only update if its writeable
            if (resource != NULL && ((resource->mode & RESOURCE_MODE_WRITEABLE) != 0))
            {
                const char *sensorType = getSensorTypeForZoneType(zone->zoneType);
                if (sensorType != NULL)
                {
                    if (deviceHelperWriteEndpointResource(zone->deviceId, zone->endpointId,
                                                          SENSOR_PROFILE_RESOURCE_TYPE,
                                                          (char *) sensorType))
                    {
                        updatedSensorTypeValue = true;
                        icLogDebug(SECURITY_LOG, "zone: updated sensor type to %s for zone %s", sensorType,
                                   zone->label);
                    }
                    else
                    {
                        icLogError(SECURITY_LOG, "zone: failed to update sensor type to %s for zone %s", sensorType,
                                   zone->label);
                    }
                }
                else
                {
                    icLogError(SECURITY_LOG, "zone: failed to determine sensor type for zone type %s for zone %s",
                               securityZoneTypeLabels[zone->zoneType], zone->label);
                }
            }
        }
        else
        {
            icLogError(SECURITY_LOG, "zone: failed to get sensor type resource for zone %s", zone->label);
        }

        // Cleanup
        destroy_DSResource(resource);
        free(endpointUri);
        free(resourceUri);
    }

    // potentially update the bypass resource
    bool didUpdateBypass = false;
    if (updateBypass == true)
    {
        char *bypassStr = zone->isBypassed ? "true" : "false";
        if (deviceHelperWriteEndpointResource(zone->deviceId, zone->endpointId, SENSOR_PROFILE_RESOURCE_BYPASSED,
                                              bypassStr) == true)
        {
            didUpdateBypass = true;
            icLogDebug(SECURITY_LOG, "zone: updated bypass to %s for zone %s", bypassStr, zone->label);
        }
        else
        {
            icLogError(SECURITY_LOG, "zone, failed to update bypassed resource for zone %s", zone->label);
        }
    }

    return updatedSecZoneAttribute || labelUpdated || updatedLPMPolicy || updatedSensorTypeValue || didUpdateBypass;
}

/*
 * event listeners
 */

/*
 * taskExecutor 'run' function for delivering events
 * from endpointAddedNotify() and updating the device storage
 */
static void addZoneViaEventTaskRun(void *taskObj, void *taskArg)
{
    securityZoneEvent *event = (securityZoneEvent *)taskObj;
    securityZone *zone = event->zone;
    uint8_t *updateDevice = (uint8_t *)taskArg;

    // encode the zone details into JSON, and set the 'secZone' attribute
    //
    bool saved = persistZoneMetadata(zone, *updateDevice);
    if (saved == true)
    {
        // send the event
        //
        icLogInfo(SECURITY_LOG, "zone: successfully saved securityZone for endpoint %s", zone->endpointId);
        broadcastZoneEvent(event, ZONE_EVENT_ADDED_CODE, 0, 0);

        // See if there are any initial troubles
        checkDeviceForInitialTroubles(zone->deviceId, false, true);
    }
    else
    {
        // oops.  failed to add to device, so need to delete the zone internally
        //
        icLogWarn(SECURITY_LOG, "zone: unable to encode securityZone for endpoint %s", zone->endpointId);
        lockSecurityMutex();
        ZoneSearchInfo zoneSearchInfo = { .deviceUUID = zone->deviceId, .endpointId = event->zone->endpointId };
        linkedListDelete(zoneList, &zoneSearchInfo, findZoneByDeviceIdAndEndpointId, destroy_securityZone_fromList);

        char *key = getAllocatedZoneKey(zone->deviceId, zone->endpointId);
        hashMapDelete(allocatedZoneNumbersByKey, key, (uint16_t)(strlen(key) + 1), NULL);
        unlockSecurityMutex();
        free(key);
    }

    // destroy the event, zone, and arg when our 'free' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * addZoneViaEventTaskRun function
 */
static void addZoneViaEventTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event, zone & the boolean arg
    //
    securityZoneEvent *event = (securityZoneEvent *)taskObj;
    destroy_securityZoneEvent(event);

    uint8_t *updateDevice = (uint8_t *)taskArg;
    free(updateDevice);
}

/*
 * callback from deviceService when a new endpoint (logical device) is added
 */
static void endpointAddedNotify(DeviceServiceEndpointAddedEvent *event)
{
    if (event == NULL || event->details == NULL || event->details->uri == NULL || event->details->profile == NULL)
    {
        return;
    }

    // only care about 'sensor' endpoints
    //
    if (strcmp(SENSOR_PROFILE, event->details->profile) != 0)
    {
        return;
    }

    // need to get the full endpoint so we can look at the details
    //
    DSEndpoint *endpoint = create_DSEndpoint();
    if (deviceService_request_GET_ENDPOINT_BY_URI(event->details->uri, endpoint) == IPC_SUCCESS &&
        endpoint->profile != NULL &&
        endpoint->id != NULL)
    {
        // see if this is a sensor that is "qualified" to be a securityZone.  this
        // flag is set as a resource within the endpoint object
        //
        char *isQualified = extractResource(endpoint->resourcesValuesMap, endpoint->uri, SENSOR_PROFILE_RESOURCE_QUALIFIED);
        if (isQualified != NULL && strcmp(isQualified, "true") == 0)
        {
            // create the secureZone and apply initial values
            //
            icLogDebug(SECURITY_LOG, "zone: received 'qualified endpoint' added notification; endpoint=%s", endpoint->id);
            securityZone *zone = createZoneFromEndpoint(endpoint);
            zone->isConfigured = false;
            uint8_t changedMask = UPDATE_LPM_FLAG;

            setZoneIsSimpleDevice(zone);

            // before accessing the zoneList, need to grab the mutex
            //
            lockSecurityMutex();
            if (zone->label == NULL)
            {
                // assign a label, and set the flag so we save this with the meta-data
                //
                char temp[64];
                sprintf(temp, "Zone %"PRIu32, linkedListCount(zoneList) + 1);
                zone->label = strdup(temp);
                changedMask |= UPDATE_LABEL_FLAG;
            }

            // Fetch the zone number(or allocate one if it doesn't exist)
            uint32_t zoneNum = allocateZoneNumber(endpoint->ownerId, endpoint->id);
            zone->zoneNumber = zoneNum;
            zone->sensorId = allocateSensorId();
            zone->displayIndex = linkedListCount(zoneList) + 1;
            sortedLinkedListAdd(zoneList, zone, zoneIndexCompareFunc);

            // potentially update our 'hasLifeSafetyZone'
            //
            if (isSecurityZoneLifeSafety(zone) == true)
            {
                hasLifeSafetyZone = true;
            }

            // create the event while we still have the mutex.  note that we will
            // clone 'zone' since sending the event will be after we release
            // the lock (and the zone object won't be thread safe) - however we
            // will wait until the end to allow other subsystems a chance to alter the zone
            //
            securityZoneEvent *zoneEvent = create_securityZoneEvent();
            zoneEvent->baseEvent.eventCode = ZONE_EVENT_ADDED_CODE;
            zoneEvent->indication = INDICATION_VISUAL;
            destroy_securityZone(zoneEvent->zone);
            zoneEvent->zone = zone;

            // while we still HAVE THE LOCK, forward the event over to alarmPanel
            // so it can start an alarm, update ready status, etc.
            //
            if (supportAlarms() == true)
            {
                processZoneEventForAlarmPanel(zoneEvent);
            }
            else
            {
                populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
            }

            // now the clone
            //
            zoneEvent->zone = clone_securityZone(zone);

            // add our event & save-metadata to the our taskExecutor
            //
            uint8_t *arg = (uint8_t *)malloc(sizeof(uint8_t));
            *arg = changedMask;
            appendSecurityTask(zoneEvent, arg, addZoneViaEventTaskRun, addZoneViaEventTaskFree);

            unlockSecurityMutex();
        }
    }

    // cleanup
    //
    destroy_DSEndpoint(endpoint);
}

/**
 * Update the current zones in the zoneList to ensure there are no gaps and zones are ordered sequentially starting
 * from 1.  Assumes the security mutex is held by the caller
 * @return the list of zones that were required to be reordered, or NULL if no reorder was required
 */
static icLinkedList *reorderZonesIfNecessary()
{
    icLinkedList *reorderList = NULL;
    // re-order zones, if necessary while we still have the lock
    //
    bool reorderRequired = false;
    icLinkedListIterator *iter = linkedListIteratorCreate(zoneList);
    uint32_t i = 1;
    while (linkedListIteratorHasNext(iter))
    {
        // Check if its needed and fix
        securityZone *item = (securityZone *) linkedListIteratorGetNext(iter);
        if (item->displayIndex != i)
        {
            reorderRequired = true;
            icLogInfo(SECURITY_LOG,
                      "reorderZonesIfNecessary: Changing zone for %s.%s from displayIndex %"PRIu32" to %"PRIu32,
                      item->deviceId,
                      item->endpointId,
                      item->displayIndex,
                      i);

            item->displayIndex = i;
            // Persist the metadata change, but via our security task so that we aren't holding the mutex
            appendSecurityTask(clone_securityZone(item), NULL, updateReorderedZoneTaskRun, updateReorderedZoneTaskFree);

        }
        // Build up the list of changes for the event.  notice
        // that we use a clone of the zone object to maintain
        // thread safety
        if (reorderRequired == true)
        {
            if (reorderList == NULL)
            {
                reorderList = linkedListCreate();
            }
            linkedListAppend(reorderList, clone_securityZone(item));
        }
        ++i;
    }
    linkedListIteratorDestroy(iter);

    return reorderList;
}

/**
 * Remove any ancillary references to a zone which is being removed.  Assumes the caller holds the security mutex.
 * @param zone the zone that is being removed
 */
static void cleanupRelatedZoneInfoForRemove(securityZone *zone)
{
    // we have this zone, so go ahead and remove from our master list (no destroy)
    //
    char *key = getAllocatedZoneKey(zone->deviceId, zone->endpointId);
    hashMapDelete(allocatedZoneNumbersByKey, key, (uint16_t) (strlen(key) + 1), NULL);
    free(key);
}

/**
 * Create a securityZoneEvent for a zone remove and populate it appropriately
 * @param zone the zone to remove
 * @return the populated event
 */
static securityZoneEvent *createAndPopulateSecurityZoneRemovedEvent(securityZone *zone)
{
    // create the base event (processZoneEventForAlarmPanel will finish it)
    //
    securityZoneEvent *zoneEvent = create_securityZoneEvent();
    zoneEvent->baseEvent.eventCode = ZONE_EVENT_REMOVED_CODE;
    zoneEvent->indication = INDICATION_VISUAL;
    destroy_securityZone(zoneEvent->zone);
    zoneEvent->zone = zone; // no clone, it's gone from the list

    // forward the event over to alarmPanel so it can update ready status
    //
    if (supportAlarms() == true)
    {
        processZoneEventForAlarmPanel(zoneEvent);
    }
    else
    {
        populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
    }

    return zoneEvent;
}

/*
 * linkedListIterateFunc to clear troubles associated with a zone
 * item should be a troubleContainer
 */
static bool iterateAndClearZoneTroublesFunc(void *item, void *arg)
{
    // clear the trouble which will destroy the container.
    // the event created should end up in the securityTask lisst
    //
    troubleContainer *container = (troubleContainer *)item;
    if (clearTroubleContainerPrivate(container) == false)
    {
        // error clearing the trouble, so destroy it here
        //
        destroyTroubleContainer(container);
    }
    return true;
}

/**
 * Remove a single zone from the system, including sending out the zone removed event.  Assumes caller does NOT hold
 * the security mutex
 * @param deviceId the deviceId of the zone to remove
 * @param endpointId the endpointId of the zone to remove
 * @return the list of zones required to be reordered as part of the remove, or NULL if no reorder required
 */
static icLinkedList *removeSingleZone(const char *deviceId, const char *endpointId)
{
    icLinkedList *reorderList = NULL;

    icLogDebug(SECURITY_LOG, "zone: Got endpoint removed for sensor endpoint deviceId=%s, endpointId=%s",
               deviceId, endpointId);

    lockSecurityMutex();

    // the "event->endpointId" should match a UID of a sensorId in our zoneList
    //
    ZoneSearchInfo zoneSearchInfo = {.deviceUUID = (char *)deviceId, .endpointId = (char *)endpointId};
    securityZone *zone = linkedListFind(zoneList, &zoneSearchInfo, findZoneByDeviceIdAndEndpointId);
    if (zone != NULL)
    {
        // Cleanup other places where this zone is referenced
        cleanupRelatedZoneInfoForRemove(zone);

        // we have this zone, so go ahead and remove from our master list (no destroy)
        //
        linkedListDelete(zoneList, &zoneSearchInfo, findZoneByDeviceIdAndEndpointId, standardDoNotFreeFunc);

        // Create the zone removed event
        securityZoneEvent *zoneEvent = createAndPopulateSecurityZoneRemovedEvent(zone);

        // clear any troubles for this zone.  the list of original troubleContainers
        // will be released as they are processed via iterateAndClearZoneTroublesFunc
        //
        icLogDebug(SECURITY_LOG, "zone: clearing troubles for zone %"PRIu32" before deletion", zone->zoneNumber);
        icLinkedList *troubles = getTroubleContainersForZonePrivate(zone->zoneNumber);
        linkedListIterate(troubles, iterateAndClearZoneTroublesFunc, NULL);
        linkedListDestroy(troubles, standardDoNotFreeFunc);

        // broadcast the delete event.  since trouble clear could have generated
        // events, go ahead and send this via the securityTask mechanism so that the "remove"
        // is the last event sent regarding this zone.
        //
        icLogDebug(SECURITY_LOG, "zone: received 'qualified endpoint' deleted notification; endpoint=%s", endpointId);
        uint8_t *arg = (uint8_t *)malloc(sizeof(uint8_t));
        *arg = NO_UPDATE;
        if (appendSecurityTask(zoneEvent, arg, updateZoneViaEventTaskRun, updateZoneViaEventTaskFree) == false)
        {
            // executor called updateZoneViaEventTaskFree to free the event
            icLogError(SECURITY_LOG, "Failed queueing zone delete event: executor rejected job");
        }

        // now that the zone is gone, ensure no gaps in display order after the remove
        reorderList = reorderZonesIfNecessary();

        // potentially update our 'hasLifeSafetyZone'
        //
        if (hasLifeSafetyZone == true)
        {
            recalculateHasLifeSafetyFlag();
        }
    }
    unlockSecurityMutex();

    return reorderList;
}

/**
 * Remove all zones that are bridged through the given device and endpoint.  Sends out zone removed events for each
 * zone removed as well as a zonesRemoved bulk event.  Assumes the caller does NOT hold the security mutex
 * @param bridgeDeviceId the bridged device that was removed
 * @param bridgeEndpointId the bridged endpoint that was removed
 * @return the list of zones required to be reordered as part of the remove, or NULL if no reorder required
 */
static icLinkedList *removeBridgedZones(const char *bridgeDeviceId, const char *bridgeEndpointId)
{
    icLinkedList *reorderList = NULL;

    icLogDebug(SECURITY_LOG, "zone: Got endpoint removed for security bridge endpoint deviceId=%s, endpointId=%s",
               bridgeDeviceId, bridgeEndpointId);

    lockSecurityMutex();

    bool zonesRemoved = false;

    icLinkedList *zoneEvents = linkedListCreate();
    securityZonesRemovedEvent *zonesRemovedEvent = create_securityZonesRemovedEvent();
    systemPanelStatus *lastPanelStatus = NULL;
    zonesRemovedEvent->baseEvent.eventCode = ZONE_EVENT_BULK_REMOVE_CODE;
    icLinkedListIterator *iter = linkedListIteratorCreate(zoneList);
    while(linkedListIteratorHasNext(iter))
    {
        securityZone *zone = (securityZone *)linkedListIteratorGetNext(iter);
        if (strcmp(zone->deviceId, bridgeDeviceId) == 0)
        {
            linkedListAppend(zonesRemovedEvent->zoneList->zoneArray, zone);

            // Cleanup other places where this zone is referenced
            cleanupRelatedZoneInfoForRemove(zone);

            // we have this zone, so go ahead and remove from our master list (no destroy)
            //
            linkedListIteratorDeleteCurrent(iter, standardDoNotFreeFunc);

            // Create the single zone removed event
            securityZoneEvent *zoneEvent = createAndPopulateSecurityZoneRemovedEvent(zone);
            // Save off the zone event so we can send it out later
            linkedListAppend(zoneEvents, zoneEvent);
            // Save off the last panel status to use for the bulk event
            lastPanelStatus = zoneEvent->panelStatus;
            zonesRemoved = true;
        }
    }
    linkedListIteratorDestroy(iter);

    if (zonesRemoved)
    {
        // Ensure no gaps in display order after the removes
        reorderList = reorderZonesIfNecessary();
    }

    unlockSecurityMutex();

    if (zonesRemoved)
    {
        // Update our bulk event with the last panel status
        destroy_systemPanelStatus(zonesRemovedEvent->panelStatus);
        // Have to clone because we are going to destroy the event it belongs to
        zonesRemovedEvent->panelStatus = clone_systemPanelStatus(lastPanelStatus);

        // We send out individual zone removed events so that existing API consumers don't have to change
        // We add a special event value to indicate that it is part of a bulk remove.  That will allow comm
        // to filter out the individual events and send just use the bulk to do a bulk remove
        icLinkedListIterator *zoneEventIter = linkedListIteratorCreate(zoneEvents);
        while(linkedListIteratorHasNext(zoneEventIter))
        {
            securityZoneEvent *zoneEvent = (securityZoneEvent *)linkedListIteratorGetNext(zoneEventIter);
            icLogDebug(SECURITY_LOG, "zone: removing zone as part of security bridge; endpoint=%s", zoneEvent->zone->endpointId);
            broadcastZoneEvent(zoneEvent, zoneEvent->baseEvent.eventCode, ZONE_EVENT_BULK_VALUE, 0);
            // We are going to let the bulk zone removed event own the zones in it list, so clear it out from here
            zoneEvent->zone = NULL;
        }
        linkedListIteratorDestroy(zoneEventIter);

        // broadcast the removed event.  since this is a "remove", we shouldn't
        // have subsequent events about this device - so no need for the taskExecutor
        //
        icLogDebug(SECURITY_LOG, "zone: removed all zones from security bridge; deviceId=%s", bridgeDeviceId);
        broadcastZonesRemovedEvent(zonesRemovedEvent);
    }

    // Cleanup
    linkedListDestroy(zoneEvents, (linkedListItemFreeFunc)destroy_securityZoneEvent);
    destroy_securityZonesRemovedEvent(zonesRemovedEvent);

    return reorderList;
}

/*
 * taskExecutor 'run' function for delivering zone-reorder events
 */
static void sendZoneReorderEventTaskRun(void *taskObj, void *taskArg)
{
    icLinkedList *reorderList = (icLinkedList *)taskObj;

    // send the events
    //
    broadcastZoneReorderedEvent(reorderList);
}

/*
 * taskExecutor 'free' function that goes along with the
 * sendZoneReorderEventTaskRun function
 */
static void sendZoneReorderEventTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event, zone & the boolean arg
    //
    icLinkedList *reorderList = (icLinkedList *)taskObj;
    linkedListDestroy(reorderList, (linkedListItemFreeFunc) destroy_securityZone);
}

/*
 * callback from deviceService when a new endpoint (logical device) is removed
 */
static void endpointRemovedNotify(DeviceServiceEndpointRemovedEvent *event)
{
    if (event == NULL || event->endpoint == NULL || event->endpoint->profile == NULL)
    {
        return;
    }

    icLinkedList *reorderList = NULL;

    // handle the sensor endpoint case
    //
    if (strcmp(SENSOR_PROFILE, event->endpoint->profile) == 0)
    {
        reorderList = removeSingleZone(event->endpoint->ownerId, event->endpoint->id);
    }

    // Check if this remove triggered a zone re-order
    if (reorderList != NULL)
    {
        // Send the event (via securityTask) to ensure these events follow ones created from above
        //
        if (appendSecurityTask(reorderList, NULL, sendZoneReorderEventTaskRun, sendZoneReorderEventTaskFree) == false)
        {
            // executor called updateZoneViaEventTaskFree to free the event
            icLogError(SECURITY_LOG, "Failed queueing zone reorder events: executor rejected job");
        }
    }
}

/*
 * taskExecutor 'run' function for delivering events
 * from deviceResourceUpdatedNotify() and updating the device storage
 */
static void updateZoneViaEventTaskRun(void *taskObj, void *taskArg)
{
    securityZoneEvent *event = (securityZoneEvent *)taskObj;
    uint8_t *changedMask = (uint8_t *)taskArg;

    // if need to persist some metadata, use the cloned copy of the zone
    // that we shoved into the zoneEvent
    //
    if (*changedMask != NO_UPDATE)
    {
        persistZoneMetadata(event->zone, *changedMask);
    }

    // send the event
    //
    broadcastZoneEvent(event, event->baseEvent.eventCode, event->baseEvent.eventValue, event->requestId);

    // destroy the event, zone, and arg when our 'free' function is called
}

/*
 * taskExecutor 'free' function that goes along with the
 * updateZoneViaEventTaskRun function
 */
static void updateZoneViaEventTaskFree(void *taskObj, void *taskArg)
{
    // destroy the event, zone & the boolean arg
    //
    securityZoneEvent *event = (securityZoneEvent *)taskObj;
    destroy_securityZoneEvent(event);

    uint8_t *changedMask = (uint8_t *)taskArg;
    free(changedMask);
}

/*
 * taskExecutor 'run' function that persists zone metadata update
 * on a reorder
 */
static void updateReorderedZoneTaskRun(void *taskObj, void *taskArg)
{
    securityZone *zone = taskObj;
    // Unused
    (void)taskArg;
    persistZoneMetadata(zone, UPDATE_ZONE_METADATA_FLAG);
}

/*
 * taskExecutor 'free' function that goes along with the
 * updateReorderedZoneTaskRun function
 */
static void updateReorderedZoneTaskFree(void *taskObj, void *taskArg)
{
    securityZone *zone = taskObj;
    // Unused
    (void)taskArg;
    destroy_securityZone(zone);
}

/**
 * Set the zone's simple device flag when adding or loading a zone.
 * @param zone
 */
static void setZoneIsSimpleDevice(securityZone *zone)
{
    DSDevice *device = create_DSDevice();

    if (deviceService_request_GET_DEVICE_BY_ID(zone->deviceId, device) == IPC_SUCCESS)
    {
        zone->isSimpleDevice = deviceHelperIsMultiEndpointCapable(device) == false;
    }
    else
    {
        icLogWarn(SECURITY_LOG, "%s could not find device for %s", __func__, zone->deviceId);
    }

    destroy_DSDevice(device);
}

/*
 * callback from deviceService when a device has a change to one of its resources
 */
static void deviceResourceUpdatedNotify(DeviceServiceResourceUpdatedEvent *event)
{
    // got a sensor updated event.  see if fault, restore, label change, bypass, etc
    //
    if (event->resource == NULL || event->resource->id == NULL || event->resource->value == NULL)
    {
        return;
    }

    // TODO: this function needs to be broken up and optimize event checks before grabbing the lock


    // the "event->resource->ownerId" should match a UID of a sensorId in our zoneList
    //
    lockSecurityMutex();
    ZoneSearchInfo zoneSearchInfo = { .deviceUUID = event->rootDeviceId, .endpointId = event->resource->ownerId };
    securityZone *zone = linkedListFind(zoneList, &zoneSearchInfo, findZoneByDeviceIdAndEndpointId);
    if (zone == NULL)
    {
        // For some events, like firmware updates, the ownerId is the device, not the endpoint
        zone = linkedListFind(zoneList, event->resource->ownerId, findZoneByDeviceId);
        if (zone == NULL)
        {
            // not found
            unlockSecurityMutex();
            return;
        }
    }

    // show that we got some form of update event for a zone
    //
    icLogDebug(SECURITY_LOG, "received device resource updated event of a securityZone; id=%s device id=%s uri=%s value=%s type=%s",
              (event->resource->id != NULL) ? event->resource->id : "unknown",
              (event->resource->ownerId != NULL) ? event->resource->ownerId : "unknown",
              (event->resource->uri != NULL) ? event->resource->uri : "unknown",
              (event->resource->value != NULL) ? event->resource->value : "unknown",
              (event->resource->type != NULL) ? event->resource->type : "unknown");

    // to reduce chances of NOT unlocking the mutex, set aside placeholders
    // for the zone event and metadata changes so they can be done outside
    // of the mutex lock (supplied to the taskExecutor)
    //
    securityZoneEvent *zoneEvent = NULL;
    uint8_t changedMask = NO_UPDATE;

    // now that we know this is an event about a zone we are tracking, look at the
    // type of event we just received so we know how to react to it if its not test.
    // if event details json have metadata related to fault then it treated as sensor test fault
    // button press event
    bool isTestFault = false; // fault or restore
    bool isTestTypeEvent = false;

    if (event->details != NULL)
    {
        cJSON *element = cJSON_GetObjectItem(event->details, SENSOR_PROFILE_METADATA_TEST);
        if (cJSON_IsNull(element) == false && cJSON_IsBool(element) == true)
        {
            isTestTypeEvent = true;
            isTestFault = cJSON_IsTrue(element);
        }
    }

    if (stringCompare(event->resource->id, SENSOR_PROFILE_RESOURCE_FAULTED, false) == 0 && isTestTypeEvent == false)
    {
        // got a sensor fault/restore, so update the
        // zone if different then what we have cached
        //
        bool faulted = isEndpointFaultedViaEvent(event);
        if (zone->isFaulted != faulted)
        {
            // save the faulted state in mem
            //
            zone->isFaulted = faulted;

            // for the event code, use FAULT or RESTORE.  however need to
            // look at other attributes of the zone for the event value
            //
            indicationType indication = INDICATION_NONE;
            int eventVal = 0;
            if (zone->isBypassed == true)
            {
                // bypass fault/restore can be visually shown.  no audible beep
                indication = INDICATION_VISUAL;
                if (faulted == true)
                {
                    eventVal = ZONE_EVENT_FAULT_BYPASSED_VALUE;
                }
                else
                {
                    eventVal = ZONE_EVENT_RESTORE_BYPASSED_VALUE;
                }
            }
            else
            {
                // determine the visual/audible indication based on zone type & function.  most should
                // be shown and play a beep, so easier to just look for the exceptions
                //
                bool isAudible = true;
                bool isVisual = true;

                // mute setting
                if (zone->zoneMute == ZONE_ALL_EVENT_MUTED)
                {
                    isAudible = false;
                }
                else if (zone->zoneMute == ZONE_FAULT_EVENT_MUTED && faulted == true)
                {
                    isAudible = false;
                }
                else if (zone->zoneMute == ZONE_RESTORE_EVENT_MUTED && faulted == false)
                {
                    isAudible = false;
                }

                // specific zone types
                if (zone->zoneType == SECURITY_ZONE_TYPE_DURESS)
                {
                    isAudible = false;
                    isVisual = false;
                }
                else if (zone->zoneType == SECURITY_ZONE_TYPE_MOTION)
                {
                    // motion zones should send OCCUPANCY instead of FAULT based
                    // on the alarm service state (ignored if monitor24)
                    //
                    // make this silent since it's an OCC fault/restore
                    //
                    isAudible = false;
                    if (faulted == true)
                    {
                        eventVal = ZONE_EVENT_OCC_FAULT_VALUE;
                    }
                    else
                    {
                        eventVal = ZONE_EVENT_OCC_RESTORE_VALUE;
                    }
                }

                // specific zone functions
                switch (zone->zoneFunction)
                {
                    case SECURITY_ZONE_FUNCTION_SILENT_24HOUR:
                    case SECURITY_ZONE_FUNCTION_SILENT_BURGLARY:
                        isAudible = false;
                        isVisual = false;
                        break;

                    default:
                        break;
                }

                if (isAudible == true && isVisual == true)
                {
                    indication = INDICATION_BOTH;
                }
                else if (isAudible == false && isVisual == false)
                {
                    indication = INDICATION_NONE;
                }

                //Incomplete switch eval during analysis marks this as dead
                //coverity[dead_error_condition]
                else if (isAudible == true)
                {
                    indication = INDICATION_AUDIBLE;
                }
                else // (isVisual == true)
                {
                    indication = INDICATION_VISUAL;
                }
            }

            // create the zone event and fill in what we can.
            //
            changedMask &= UPDATE_ZONE_METADATA_FLAG;
            zoneEvent = create_securityZoneEvent();
            zoneEvent->baseEvent.eventCode = (faulted == true) ? ZONE_EVENT_FAULT_CODE : ZONE_EVENT_RESTORE_CODE;
            zoneEvent->baseEvent.eventValue = eventVal;
            zoneEvent->indication = indication;
            destroy_securityZone(zoneEvent->zone);
            zoneEvent->zone = zone;
            icLogDebug(SECURITY_LOG, "creating zone fault/restore event; device=%s zone=%"PRIu32" eventCode=%"PRIi32" eventValue=%"PRIi32" ind=%s",
                       (event->resource->id != NULL) ? event->resource->id : "unknown", zone->zoneNumber,
                       zoneEvent->baseEvent.eventCode,
                       zoneEvent->baseEvent.eventValue,
                       indicationTypeLabels[zoneEvent->indication]);

            // forward the event over to alarmPanel so it can start an alarm, update ready status, etc.
            //
            if (supportAlarms() == true)
            {
                processZoneEventForAlarmPanel(zoneEvent);
            }
            else
            {
                populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
            }
        }
    }
    else if (stringCompare(event->resource->id, SENSOR_PROFILE_RESOURCE_FAULTED, false) == 0 && isTestTypeEvent == true)
    {
        icLogDebug(SECURITY_LOG, "Received test faulted event, isRestore = %s, nothing to do.", (isTestFault == true) ? "false": "true");
    }
    else if (strcmp(event->resource->id, SENSOR_PROFILE_RESOURCE_BYPASSED) == 0)
    {
        // got a sensor bypass/not-bypassed, so update
        // the zone with the value (if different).
        // note that it could be already reflected in the zone
        // if this is the event caused by 'updateSecurityZone',
        // which should have already broadcasted the zone event
        //
        bool bypass = isEndpointBypassedViaEvent(event);
        if (zone->isBypassed != bypass)
        {
            // save bypass state
            //
            zone->isBypassed = bypass;

            // create the zone event and fill in what we can.
            //
            changedMask &= UPDATE_ZONE_METADATA_FLAG;
            zoneEvent = create_securityZoneEvent();
            zoneEvent->baseEvent.eventCode = (bypass == true) ? ZONE_EVENT_BYPASSED_CODE : ZONE_EVENT_UNBYPASSED_CODE;
            zoneEvent->indication = INDICATION_VISUAL;
            destroy_securityZone(zoneEvent->zone);
            zoneEvent->zone = zone;
            icLogDebug(SECURITY_LOG, "creating zone bypass/unbypass event; device=%s zone=%"PRIu32" eventCode=%"PRIi32" eventValue=%"PRIi32" ind=%s",
                       (event->resource->id != NULL) ? event->resource->id : "unknown", zone->zoneNumber,
                       zoneEvent->baseEvent.eventCode,
                       zoneEvent->baseEvent.eventValue,
                       indicationTypeLabels[zoneEvent->indication]);

            // forward the event over to alarmPanel so it can start an alarm, update ready status, etc.
            //
            if (supportAlarms() == true)
            {
                processZoneEventForAlarmPanel(zoneEvent);
            }
            else
            {
                populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
            }
        }
    }
    else if (strcmp(event->resource->type, RESOURCE_TYPE_LABEL) == 0)
    {
        // got a label change, so update if different
        // note that it could be already reflected in the zone
        // if this is the event caused by 'updateSecurityZone',
        // which should have already broadcasted the zone event
        //
        if (stringCompare(zone->label, event->resource->value, false) != 0)
        {
            // update the zone label
            //
            if (zone->label != NULL)
            {
                free(zone->label);
            }
            zone->label = strdup(event->resource->value);

            // create the zone event and fill in what we can.
            // note that we'll clone 'zone' since sending the event will be after the lock is released
            //
            // No further updates here to metadata since we are reacting to a change from device service
            zoneEvent = create_securityZoneEvent();
            zoneEvent->baseEvent.eventCode = ZONE_EVENT_UPDATED_CODE;
            zoneEvent->indication = INDICATION_VISUAL;
            destroy_securityZone(zoneEvent->zone);
            zoneEvent->zone = zone;
            icLogDebug(SECURITY_LOG, "creating zone update-label event; device=%s zone=%"PRIu32" eventCode=%"PRIi32" eventValue=%"PRIi32" ind=%s",
                       (event->resource->id != NULL) ? event->resource->id : "unknown", zone->zoneNumber,
                       zoneEvent->baseEvent.eventCode,
                       zoneEvent->baseEvent.eventValue,
                       indicationTypeLabels[zoneEvent->indication]);

            // update the status in the event
            //
            populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
        }
    }
    else if (strcmp(event->resource->id, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION) == 0)
    {
        // nothing to cache, just create the event to send
        // note that we'll clone 'zone' since sending the event will be after the lock is released
        //
        // No further updates here to metadata since we are reacting to a change from device service
        zoneEvent = create_securityZoneEvent();
        zoneEvent->baseEvent.eventCode = ZONE_EVENT_UPDATED_CODE;
        zoneEvent->indication = INDICATION_NONE;
        destroy_securityZone(zoneEvent->zone);
        zoneEvent->zone = zone;
        icLogDebug(SECURITY_LOG, "creating zone update-firmware event; device=%s zone=%"PRIu32" eventCode=%"PRIi32" eventValue=%"PRIi32" ind=%s",
                   (event->resource->id != NULL) ? event->resource->id : "unknown", zone->zoneNumber,
                   zoneEvent->baseEvent.eventCode,
                   zoneEvent->baseEvent.eventValue,
                   indicationTypeLabels[zoneEvent->indication]);
    }
    else if (strcmp(event->resource->id, SENSOR_PROFILE_RESOURCE_TYPE) == 0)
    {
        // changed the 'type'
        //
        securityZoneType zoneType = SECURITY_ZONE_TYPE_UNKNOWN;
        securityZoneFunctionType zoneFunction = SECURITY_ZONE_FUNCTION_UNKNOWN;
        if (getZoneTypeAndFunctionForSensorType(event->resource->value, &zoneType, &zoneFunction))
        {
            bool typeChanged = (zoneType != zone->zoneType);
            bool funcChanged = (zoneFunction != zone->zoneFunction);
            // Check if its changed and something valid
            //
            if (zoneType != SECURITY_ZONE_TYPE_UNKNOWN &&
                zoneFunction != SECURITY_ZONE_FUNCTION_UNKNOWN &&
                (typeChanged == true || funcChanged == true))
            {
                bool validateFunc = funcChanged;
                if (typeChanged == true)
                {
                    // Update the zone
                    //
                    zone->zoneType = zoneType;
                    // need to validate zoneFunction
                    //
                    validateFunc = true;
                }

                // we want to default zoneFunction only if previously stored zone function is not valid for new zone type.
                //
                if (validateFunc == true)
                {
                    if (validateSecurityZoneTypeAndFunction(zoneType, zone->zoneFunction) == false)
                    {
                        zone->zoneFunction = zoneFunction;
                        funcChanged = true;
                    }
                    else
                    {
                        funcChanged = false;
                    }
                }

                if (typeChanged == true || funcChanged == true)
                {
                    changedMask &= UPDATE_ZONE_METADATA_FLAG;
                    // Potentially affects LPM functionality, go ahead and update that
                    //
                    changedMask &= UPDATE_LPM_FLAG;

                    // create the zone event and fill in what we can.
                    //
                    zoneEvent = create_securityZoneEvent();
                    zoneEvent->baseEvent.eventCode = ZONE_EVENT_UPDATED_CODE;
                    zoneEvent->indication = INDICATION_NONE;
                    destroy_securityZone(zoneEvent->zone);
                    zoneEvent->zone = zone;
                    icLogDebug(SECURITY_LOG, "creating zone update event; device=%s zone=%"PRIu32" eventCode=%"PRIi32" eventValue=%"PRIi32" ind=%s",
                               (event->resource->id != NULL) ? event->resource->id : "unknown", zone->zoneNumber,
                               zoneEvent->baseEvent.eventCode,
                               zoneEvent->baseEvent.eventValue,
                               indicationTypeLabels[zoneEvent->indication]);

                    // forward the event over to alarmPanel so it can start an alarm, update ready status, etc.
                    //
                    if (supportAlarms() == true)
                    {
                        processZoneEventForAlarmPanel(zoneEvent);
                    }
                    else
                    {
                        populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
                    }

                    // recalculate the 'hasLifeSafetyZone' flag
                    //
                    recalculateHasLifeSafetyFlag();
                }
            }
        }
    }

    if (zoneEvent != NULL)
    {
        // now the clone of the zone object so that event doesn't
        // destroy it (and to keep thread safety)
        //
        zoneEvent->zone = clone_securityZone(zone);

        // place into the taskExecutor to serialize the notification and
        // metadata updates for this zone change.
        //
        uint8_t *arg = (uint8_t *)malloc(sizeof(uint8_t));
        *arg = changedMask;
        appendSecurityTask(zoneEvent, arg, updateZoneViaEventTaskRun, updateZoneViaEventTaskFree);
    }
    unlockSecurityMutex();
}

/**
 * Helper function that finds the first unused number in a list, or if no gaps exist will return the next highest
 * number.  Assumes the starting number is 1.
 * @param sortedList the sorted list to check
 * @return the next number to use
 */
static uint32_t getUnusedNumberInList(icSortedLinkedList *sortedList)
{
    // Search for gaps to fill in
    uint32_t prev = 0;
    uint32_t unusedNumber = -1;
    icLinkedListIterator *zoneIdsIter = linkedListIteratorCreate(sortedList);
    while(linkedListIteratorHasNext(zoneIdsIter))
    {
        uint32_t *item = (uint32_t *)linkedListIteratorGetNext(zoneIdsIter);
        if (*item - prev > 1)
        {
            // found a gap
            icLogDebug(SECURITY_LOG, "zone: Found gap between numbers %" PRIu32 " and %" PRIu32, prev, *item);
            unusedNumber = prev+1;
        }
        prev = *item;

    }
    linkedListIteratorDestroy(zoneIdsIter);

    if (unusedNumber == -1)
    {
        icLogDebug(SECURITY_LOG, "zone: No gap found, using next available number");
        unusedNumber = linkedListCount(sortedList)+1;
    }

    return unusedNumber;
}

/**
 * Allocates a zone number for a new device.  Internal function that assumes the
 * mutex is being held.
 *
 * @param deviceId the device id of the new device
 * @return the allocated zone number.
 */
static uint32_t allocateZoneNumber(const char *deviceId, const char *endpointId)
{
    icLogDebug(SECURITY_LOG, "zone: Searching for available zone number for %s", deviceId);

    // First check if already allocated for this key
    char *zoneKey = getAllocatedZoneKey(deviceId, endpointId);
    uint32_t *allocatedNewZoneNumber = hashMapGet(allocatedZoneNumbersByKey, zoneKey, (uint16_t)(strlen(zoneKey) + 1));
    if (allocatedNewZoneNumber != NULL)
    {
        icLogInfo(SECURITY_LOG, "zone: Existing zone number %" PRIu32 " for key %s", *allocatedNewZoneNumber, zoneKey);
        free(zoneKey);
        zoneKey = NULL;
        return *allocatedNewZoneNumber;
    }

    icSortedLinkedList *sortedZoneNumbers = sortedLinkedListCreate();
    icHashMapIterator *iter = hashMapIteratorCreate(allocatedZoneNumbersByKey);
    while(hashMapIteratorHasNext(iter))
    {
        char *key;
        uint16_t keyLen;
        uint32_t *value;
        hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&value);
        icLogDebug(SECURITY_LOG, "zone: Found already allocated zone number %" PRIu32 " for key %s", *value, key);
        sortedLinkedListAdd(sortedZoneNumbers, value, zoneNumberCompareFunc);
    }
    hashMapIteratorDestroy(iter);

    uint32_t newZoneNumber = getUnusedNumberInList(sortedZoneNumbers);
    icLogDebug(SECURITY_LOG, "zone: Using zone number %" PRIu32 " for key %s", newZoneNumber, zoneKey);
    allocatedNewZoneNumber = malloc(sizeof(uint32_t));
    *allocatedNewZoneNumber = newZoneNumber;

    hashMapPut(allocatedZoneNumbersByKey, zoneKey, (uint16_t)(strlen(zoneKey) + 1), allocatedNewZoneNumber);

    // Cleanup
    linkedListDestroy(sortedZoneNumbers, standardDoNotFreeFunc);

    return newZoneNumber;
}

/**
 * Allocates a sensor id for a new device.  Unlike zone numbers, we don't pre-allocate sensor ids, so this just looks at
 * the existing sensorIds in our zones and picks an available one.  Internal function that assumes the mutex is being
 * held.
 * @return the allocated sensor id
 */
static uint32_t allocateSensorId()
{
    // Build a sorted list of our existing values
    icSortedLinkedList *sortedSensorIds = sortedLinkedListCreate();

    icLinkedListIterator *iter = linkedListIteratorCreate(zoneList);
    while(linkedListIteratorHasNext(iter))
    {
        securityZone *zone = (securityZone *)linkedListIteratorGetNext(iter);
        sortedLinkedListAdd(sortedSensorIds, &zone->sensorId, zoneNumberCompareFunc);
    }
    linkedListIteratorDestroy(iter);

    // Get the next unused number
    uint32_t newSensorId = getUnusedNumberInList(sortedSensorIds);

    // Cleanup the list
    linkedListDestroy(sortedSensorIds, standardDoNotFreeFunc);

    return newSensorId;
}

/*
 * Called when a device is discoverd.  For qualified sensors we need to send out our own
 * zone version of this so that outer layers can understand which zone this is for
 */
static void deviceDiscovered(DeviceServiceDeviceDiscoveredEvent *event)
{

    if (event == NULL || event->details == NULL || event->details->metadataValuesMap == NULL)
    {
        return;
    }

    // Only care about qualified sensors
    char *isQualified = get_metadataValue_from_DSEarlyDeviceDiscoveryDetails_metadata(event->details, SENSOR_PROFILE_RESOURCE_QUALIFIED);
    if (isQualified != NULL && strcasecmp(isQualified, "true") == 0 && strcmp(SENSOR_DC, event->details->deviceClass) == 0)
    {
        //sensor device class has a single endpoint.  Just get that endpoint id to use for the allocation
        char *endpointId = NULL;
        char *epListStr = get_metadataValue_from_DSEarlyDeviceDiscoveryDetails_metadata(event->details, SENSOR_PROFILE_ENDPOINT_ID_LIST);

        if(epListStr != NULL)
        {
            cJSON *epList = cJSON_Parse(epListStr);
            if(epList != NULL && cJSON_GetArraySize(epList) > 0)
            {
                cJSON *firstEp = cJSON_GetArrayItem(epList, 0);
                endpointId = stringBuilder("%d", firstEp->valueint);
            }
            cJSON_Delete(epList);
        }

        if(endpointId != NULL)
        {
            lockSecurityMutex();
            uint32_t zoneNumber = allocateZoneNumber(event->details->id, endpointId);
            unlockSecurityMutex();

            free(endpointId);

            // Now that we have allocated the new number, go ahead and broadcast our zoneDiscovered event
            broadcastZoneDiscoveredEvent(zoneNumber, event->details);
        }
        else
        {
           icLogError(SECURITY_LOG, "%s: failed to find first endpoint id", __FUNCTION__);
        }
    }
}

/*
 * linkedListIterateFunc to unbypass any zones that are flagged as bypassed. Called via linkedListIterate via the
 * unbypassAllZones function. Unbypasses bypassed zones, creates zone events for it, and places those zone events in
 * supplied queue for phase two processing.
 */
static bool iterateAndUnbypassZonesFunc(void *item, void *arg)
{
    // first see if this zone is bypassed
    //
    securityZone *zone = (securityZone *)item;
    if (zone->isBypassed == true)
    {
        // set to false then create the event & persistance information
        //
        zone->isBypassed = false;

        // create the zone event and fill in what we can.
        // note that we'll clone 'zone' since sending the event will be backgrounded
        //
        securityZoneEvent *zoneEvent = create_securityZoneEvent();
        zoneEvent->baseEvent.eventCode = ZONE_EVENT_UNBYPASSED_CODE;
        zoneEvent->indication = INDICATION_VISUAL;
        destroy_securityZone(zoneEvent->zone);
        zoneEvent->zone = clone_securityZone(zone);

        icLinkedList *unbypassedZoneEventList = (icLinkedList *) arg;
        linkedListAppend(unbypassedZoneEventList, zoneEvent);
    }

    // tell linked list to keep going
    //
    return true;
}

// Update isZonesFaulted for each unbypassed zone event, populate the system panel status, then pass it along to executor.
static bool iterateAndBroadcastUnbypassZoneEventsFunc(void *item, void *arg)
{
    securityZoneEvent *zoneEvent = (securityZoneEvent *) item;
    bool *anyZonesFaulted = (bool *) arg;
    zoneEvent->alarm->isZonesFaulted = *anyZonesFaulted;

    // forward the event over to alarmPanel so it can update the panel status
    // NOTE that we're not asking it to process this event, only update the status.
    //
    populateSystemPanelStatusPrivate(zoneEvent->panelStatus);

    // place into the taskExecutor to serialize the notification and
    // metadata/resource updates for this zone change.  we will re-use the task functions
    // for updating zone state via device events because it does what we need.
    //
    uint8_t *arg2 = (uint8_t *)malloc(sizeof(uint8_t));
    *arg2 = UPDATE_BYPASS_FLAG | UPDATE_LPM_FLAG;
    appendSecurityTask(zoneEvent, arg2, updateZoneViaEventTaskRun, updateZoneViaEventTaskFree);
    return true;
}

/**
 * Linked list iterate func to check if a zone is bypassed
 * @param item the zone
 * @param arg the bypass flag
 * @return true to continue iterating, false otherwise
 */
static bool checkZoneBypassed(void *item, void *arg)
{
    bool continueChecking = true;
    securityZone *zone = (securityZone *)item;
    if (zone->isBypassed == true)
    {
        bool *bypassActive = (bool *)arg;
        *bypassActive = true;
        continueChecking = false;
    }

    return continueChecking;
}

/**
 * Get the whether any zones are bypassed
 * securityMutex must be locked when calling this.
 * @return
 */
bool isZoneBypassActivePrivate()
{
    bool bypassActive = false;

    // Check for bypass
    linkedListIterate(zoneList, checkZoneBypassed, &bypassActive);

    return bypassActive;
}

/*
 * take a specific zone in/out of "swinger shutdown" state.
 * done as a specific modification because this is tied directly
 * to the TROUBLE_REASON_SWINGER trouble reason.
 */
void setZoneSwingerShutdownStatePrivate(securityZone *zone, bool isSwingerFlag, uint64_t alarmSessionId)
{
    // private function, so assume this is the real zone (not a copy)
    // first need to see if the flag needs to change
    //
    if (isSwingerFlag == zone->inSwingerShutdown)
    {
        // nothing to change
        return;
    }

    // update the zone, then create or clear the trouble that is associated with the flag
    //
    icLogDebug(SECURITY_LOG, "zone: setting zone %"PRIu32" swinger=%s", zone->zoneNumber, (isSwingerFlag == true) ? "true" : "false");
    zone->inSwingerShutdown = isSwingerFlag;

    // create the zone event and fill in what we can.
    // note that we'll clone 'zone' since sending the event will be backgrounded
    //
    securityZoneEvent *zoneEvent = create_securityZoneEvent();
    zoneEvent->baseEvent.eventCode = ZONE_EVENT_UPDATED_CODE;
    zoneEvent->indication = INDICATION_NONE;    // not visible
    destroy_securityZone(zoneEvent->zone);
    zoneEvent->zone = clone_securityZone(zone);

    // forward the event over to alarmPanel so it can update the panel status
    // NOTE that we're not asking it to process this event, only update the status.
    //
    populateSystemPanelStatusPrivate(zoneEvent->panelStatus);
    populateSystemCurrentAlarmStatusPrivate(zoneEvent->alarm);

    // create or clear the trouble.  first the basic trouble information
    //
    troubleEvent *trbEvent = create_troubleEvent();
    if (isSwingerFlag == true)
    {
        trbEvent->baseEvent.eventCode = TROUBLE_OCCURED_EVENT;
        trbEvent->trouble->restored = false;
    }
    else
    {
        trbEvent->baseEvent.eventCode = TROUBLE_CLEARED_EVENT;
        trbEvent->trouble->restored = true;
    }
    setEventId(&trbEvent->baseEvent);
    setEventTimeToNow(&trbEvent->baseEvent);
    trbEvent->trouble->troubleId = trbEvent->baseEvent.eventId;
    trbEvent->trouble->eventId = trbEvent->baseEvent.eventId;
    trbEvent->trouble->eventTime = convertTimespecToUnixTimeMillis(&trbEvent->baseEvent.eventTime);
    trbEvent->trouble->type = TROUBLE_TYPE_DEVICE;
    trbEvent->trouble->critical = TROUBLE_CRIT_NOTICE;
    trbEvent->trouble->reason = TROUBLE_REASON_SWINGER;
    trbEvent->trouble->indication = INDICATION_NONE;    // only sent to the server
    free(trbEvent->alarm->contactId);
    trbEvent->alarm->alarmSessionId = alarmSessionId;
    trbEvent->alarm->sendImmediately = true;

    // now the device specific metadata
    //
    SensorTroublePayload *payload = sensorTroublePayloadCreate();
    payload->deviceTrouble->deviceClass = strdup(SENSOR_DC);
    payload->deviceTrouble->rootId = strdup(zone->deviceId);
    payload->deviceTrouble->ownerUri = createDeviceUri(zone->deviceId);
    payload->zoneNumber = zone->zoneNumber;
    payload->zoneType = zone->zoneType;

    // add panel/alarm status to the trouble
    //
    payload->alarmStatus = zoneEvent->panelStatus->alarmStatus;
    payload->armMode = zoneEvent->panelStatus->armMode;
    trbEvent->trouble->extra = encodeSensorTroublePayload(payload);

    // place into a container
    //
    troubleContainer *container = createTroubleContainer();
    container->event = trbEvent;
    container->payloadType = TROUBLE_DEVICE_TYPE_ZONE;
    container->extraPayload.zone = payload;
    payload = NULL;

    // now the SET or CLEAR of the swinger trouble for this zone
    //
    if (isSwingerFlag == true)
    {
        // add the SWINGER trouble for this zone.  if successful, this will
        // keep our container object (stuffing it into the master set of troubles)
        //
        icLogInfo(SECURITY_LOG, "zone: adding SWINGER trouble for zone %"PRIu32, zoneEvent->zone->zoneNumber);
        if (addTroubleContainerPrivate(container, NULL, true) == 0)
        {
            icLogWarn(SECURITY_LOG, "zone: error adding SWINGER trouble for zone %"PRIu32, zoneEvent->zone->zoneNumber);
            destroyTroubleContainer(container);
        }
    }
    else
    {
        // clear the SWINGER trouble for this zone.  if successful, this
        // will destroy our container object
        //
        icLogInfo(SECURITY_LOG, "zone: clearing adding SWINGER trouble for zone %"PRIu32, zoneEvent->zone->zoneNumber);
        if (clearTroubleContainerPrivate(container) == false)
        {
            icLogWarn(SECURITY_LOG, "zone: error clearing SWINGER trouble for zone %"PRIu32, zoneEvent->zone->zoneNumber);
            destroyTroubleContainer(container);
        }
    }

    // place into the taskExecutor to serialize the notification and
    // metadata updates for this zone change.  we will re-use the task functions
    // for updating zone state via device events because it does what we need.
    //
    uint8_t *arg = (uint8_t *)malloc(sizeof(uint8_t));
    *arg = UPDATE_ZONE_METADATA_FLAG;
    appendSecurityTask(zoneEvent, arg, updateZoneViaEventTaskRun, updateZoneViaEventTaskFree);
}

bool isSecurityZoneSimpleDevice(const securityZone *zone)
{
    return zone->isSimpleDevice;
}

/*
 * encode/decode
 */

/*
 * take the attributes from 'zone', and encode into JSON so
 * we can ask deviceService to store this info as part of the
 * device-endpoint.  allows us to keep persistance within the
 * deviceService, yet obscure zone attributes so other apps or
 * services won't be fiddling with the zone details
 */
static cJSON *encodeSecZone_toJSON(securityZone *zone)
{
    // create new JSON container object
    //
    cJSON *root = cJSON_CreateObject();

    // add each zone attribute into the JSON structure
    // NOTE: copied from securityService_pojo
    //
    cJSON_AddNumberToObject(root, "zoneNumber", zone->zoneNumber);
    cJSON_AddNumberToObject(root, "sensorId", zone->sensorId);
    cJSON_AddNumberToObject(root, "displayIndex", zone->displayIndex);
    if (zone->isTroubled == true)
        cJSON_AddTrueToObject(root, "isTroubled");
    else
        cJSON_AddFalseToObject(root, "isTroubled");
    if (zone->isConfigured == true)
        cJSON_AddTrueToObject(root, "isConfigured");
    else
        cJSON_AddFalseToObject(root, "isConfigured");
    if (zone->inSwingerShutdown == true)
        cJSON_AddTrueToObject(root, "inSwingerShutdown");
    else
        cJSON_AddFalseToObject(root, "inSwingerShutdown");
    if (zone->isInTestMode == true)
        cJSON_AddTrueToObject(root, "isInTestMode");
    else
        cJSON_AddFalseToObject(root, "isInTestMode");
    if (zone->isWirelessDevice == true)
        cJSON_AddTrueToObject(root, "isWirelessDevice");
    else
        cJSON_AddFalseToObject(root, "isWirelessDevice");
    if (zone->isBatteryDevice == true)
        cJSON_AddTrueToObject(root, "isBatteryDevice");
    else
        cJSON_AddFalseToObject(root, "isBatteryDevice");
    if (zone->hasTemperature == true)
        cJSON_AddTrueToObject(root, "hasTemperature");
    else
        cJSON_AddFalseToObject(root, "hasTemperature");

    cJSON_AddNumberToObject(root, "zoneType", zone->zoneType);
    cJSON_AddNumberToObject(root, "zoneFunction", zone->zoneFunction);
    cJSON_AddNumberToObject(root, "zoneMute", zone->zoneMute);

    return root;
}


/*
 * decode the JSON string saved in the 'secZone' attribute of the 'sensor'
 * (stored in deviceService).  most was copied/pasted from securityService_pojo.c
 */
static bool decodeSecZone_fromJSON(securityZone *zone, cJSON *buffer)
{
    bool rc = false;
    if (buffer == NULL)
    {
        return rc;
    }

    // extract the JSON object for each possible 'securityZone' attribute
    //
    cJSON *item = NULL;

    item = cJSON_GetObjectItem(buffer, "zoneNumber");
    if (item != NULL)
    {
        zone->zoneNumber = (uint32_t)item->valueint;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "sensorId");
    if (item != NULL)
    {
        zone->sensorId = (uint32_t)item->valueint;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "displayIndex");
    if (item != NULL)
    {
        zone->displayIndex = (uint32_t)item->valueint;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "isTroubled");
    if (item != NULL)
    {
        zone->isTroubled = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "isConfigured");
    if (item != NULL)
    {
        zone->isConfigured = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "inSwingerShutdown");
    if (item != NULL)
    {
        zone->inSwingerShutdown = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "isInTestMode");
    if (item != NULL)
    {
        zone->isInTestMode = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "isWirelessDevice");
    if (item != NULL)
    {
        zone->isWirelessDevice = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "isBatteryDevice");
    if (item != NULL)
    {
        zone->isBatteryDevice = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    item = cJSON_GetObjectItem(buffer, "hasTemperature");
    if (item != NULL)
    {
        zone->hasTemperature = (item->type == cJSON_True) ? true : false;
        rc = 0;
    }
    if ((item = cJSON_GetObjectItem(buffer, "zoneType")) != NULL)
    {
        zone->zoneType = (securityZoneType) item->valueint;
        rc = true;
    }
    if ((item = cJSON_GetObjectItem(buffer, "zoneFunction")) != NULL)
    {
        zone->zoneFunction = (securityZoneFunctionType) item->valueint;
        rc = true;
    }
    if ((item = cJSON_GetObjectItem(buffer, "zoneMute")) != NULL)
    {
        zone->zoneMute = (zoneMutedType) item->valueint;
        rc = true;
    }

    return rc;
}

/*
 * debugging
 */
static void printZone(securityZone *zone)
{
    if (isIcLogPriorityDebug() == true)
    {
        icLogDebug(SECURITY_LOG, "zone: zoneNum=%d displayOrder=%d sensorId=%s endpointId=%s label=%s faulted=%s bypassed=%s, simpleDevice=%s",
                   zone->zoneNumber,
                   zone->displayIndex,
                   (zone->deviceId != NULL) ? zone->deviceId : "NULL",
                   (zone->endpointId != NULL) ? zone->endpointId : "NULL",
                   (zone->label != NULL) ? zone->label : "NULL",
                   (zone->isFaulted == true) ? "true" : "false",
                   (zone->isBypassed == true) ? "true" : "false",
                   stringValueOfBool(zone->isSimpleDevice));
    }
}

/*
 * unused function to allow injecting bogus zone objects during unitTest.
 * purposefully not declared in our header and not static so the unitTest can link this in
 */
void addSecurityZoneForUnitTest(securityZone *zone)
{
    // before accessing the zoneList, need to grab the mutex
    //
    lockSecurityMutex();

    // since for unit tests...we may have not been initialized (without talking to device service)
    //
    if (didInit == false)
    {
        // allocate our zoneList
        //
        zoneList = sortedLinkedListCreate();
        allocatedZoneNumbersByKey = hashMapCreate();
        didInit = true;
    }

    // assign a label if necessary
    //
    if (zone->label == NULL)
    {
        char temp[64];
        sprintf(temp, "Zone %"PRIu32, linkedListCount(zoneList) + 1);
        zone->label = strdup(temp);
    }

    // assign a display index
    //
    zone->displayIndex = linkedListCount(zoneList) + 1;

    // potentially update our 'hasLifeSafetyZone'
    //
    if (isSecurityZoneLifeSafety(zone) == true)
    {
        hasLifeSafetyZone = true;
    }

    // add to our master list of zones
    //
    sortedLinkedListAdd(zoneList, zone, zoneIndexCompareFunc);

    unlockSecurityMutex();
}

//caller frees result
static char *getAllocatedZoneKey(const char *deviceUuid, const char *endpointId)
{
    size_t keyLen = strlen(deviceUuid) + strlen(endpointId) + 2; //. and \0
    char *key = (char*)malloc(keyLen);
    snprintf(key, keyLen, "%s.%s", deviceUuid, endpointId);

    return key;
}

/*
 * return false if this zone type is NOT bypassable
 */
static bool isZoneBypassable(securityZoneType zoneType)
{
    switch (zoneType)
    {
        case SECURITY_ZONE_TYPE_SMOKE:
        case SECURITY_ZONE_TYPE_CO:
            return false;

        default:
            return true;
    }
}

