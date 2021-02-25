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
 * troubleState.h
 *
 * Track the set of troubles throughout the system.
 * Note that this is an in-memory store, and not persisted
 * in any way.
 *
 * Authors: jelderton, mkoch - 7/9/15
 *-----------------------------------------------*/

#ifndef IC_TROUBLESTATE_H
#define IC_TROUBLESTATE_H

#include <stdint.h>
#include <stdbool.h>
#include <icIpc/ipcStockMessagesPojo.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <deviceService/deviceService_pojo.h>
#include <deviceService/deviceService_event.h>
#include <securityService/securityService_event.h>
#include <securityService/securityService_pojo.h>
#include "trouble/troubleContainer.h"


typedef enum {
    TROUBLE_FORMAT_CONTAINER,
    TROUBLE_FORMAT_EVENT,
    TROUBLE_FORMAT_OBJ,
} troubleFormat;

/*
 * function prototype for searching troubles during add/clear
 */
typedef bool (*troublePayloadCompareFunc)(cJSON *searchVal, cJSON *item);


/*
 * one-time init to setup for troubles
 */
void initTroubleStatePublic();

/*
 * called during shutdown
 */
void destroyTroubleStatePublic();

/*
 * should be called once all of the services are online
 * so that each can be probed to gather initial troubles.
 */
void loadInitialTroublesPublic();

/*
 * return the number of troubles that are known
 */
uint32_t getTroubleCountPublic(bool includeAckTroubles);

/*
 * used to get runtime stats (via IPC)
 */
void collectTroubleEventStatistics(runtimeStatsPojo *output);

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
void getTroublesPublic(icLinkedList *outputList, troubleFormat outputFormat, bool includeAckTroubles, troubleSortAlgo sort);

/*
 * populate the 'targetList' with trouble clones (container, event or obj).
 * with a specific device.  the list contents are dictated by the input parameters.
 * caller is responsible for destroying the returned list and contents.
 *
 * @param outputList - the linked list to place "clones" of troubleEvent/troubleObj
 * @param uri - the uri prefix to retrieve troubles about
 * @oaram outputFormat - what gets into outputList: troubleContainer, troubleEvent, or troubleObj
 * @oaram includeActTroubles - if true, place acknowledged troubles into the 'outputList'
 * @param sort - sorting mechanism to utilize when adding to 'outputList'
 *
 * @see linkedListDestroy()
 * @see destroy_troubleEvent()
 * @see destroy_troubleObj()
 */
void getTroublesForDeviceUriPublic(icLinkedList *outputList, char *uri, troubleFormat outputFormat,
                                   bool includeAckTroubles, troubleSortAlgo sort);

/*
 * count the troubles that match the provided 'troubleType' and 'troubleReason'.
 */
uint32_t getTroubleCountForTypePublic(troubleType type, troubleReason reason);

/*
 * acknowledge a single trouble
 */
void acknowledgeTroublePublic(uint64_t troubleId);

/*
 * un-acknowledge a single trouble
 */
void unacknowledgeTroublePublic(uint64_t troubleId);

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
uint64_t addTroublePublic(troubleContainer *container, troublePayloadCompareFunc compareFunc, bool sendEvent);

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
                        bool sendEvent);

/*
 * clear all troubles for a specific device.  only called
 * when the device is removed from the system, so therefore
 * does NOT mess with clearing metadata from deviceService.
 *
 * @param deviceId - the device identifier to use for the search
 */
void clearTroublesForDevicePublic(char *deviceId);

/*
 * 'linkedListItemFreeFunc' implementation used when deleting troubleEvent
 * objects from the linked list.
 */
void destroyTroubleEventFromList(void *item);

/*
 * Check a device for initial troubles
 */
void checkDeviceForInitialTroubles(const char *deviceId, bool processClear, bool sendEvent);

/*
 * helper function to create a base troubleEvent with some basic information.
 * assumes the caller will assign a troubleId since that is not always generated.
 */
troubleEvent *createBasicTroubleEvent(BaseEvent *base,
                                      troubleType type,
                                      troubleCriticalityType criticality,
                                      troubleReason reason);

/*
 * Process a resource for troubles
 * this potentially grabs the global security mutex.
 */
void processTroubleForResource(const DSResource *resource, const DSDevice *parentDevice, const char *deviceId, BaseEvent *baseEvent, bool processClear, bool sendEvent);

/*
 * Process a zigbee network interference event
 */
void processZigbeeNetworkInterferenceEvent(DeviceServiceZigbeeNetworkInterferenceChangedEvent *event);

/*
 * Process a zigbee pan id attack event
 */
void processZigbeePanIdAttackEvent(DeviceServiceZigbeePanIdAttackChangedEvent *event);

/*
 * Restore trouble configuration
 */
bool restoreTroubleConfig(const char *tempRestoreDir, const char *dynamicConfigPath);

#endif // IC_TROUBLESTATE_H
