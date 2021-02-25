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
 * troubleStatePrivate.h
 *
 * set of 'internal' private functions for use where the
 * SECURITY_MTX is already held.  placed into a separate header
 * to help reduce confusion.
 *
 * Author: jelderton -  11/13/18.
 *-----------------------------------------------*/


#ifndef ZILKER_TROUBLESTATEPRIVATE_H
#define ZILKER_TROUBLESTATEPRIVATE_H

#include "trouble/troubleState.h"

// used when counting the troubles in a category
typedef enum {
    TROUBLE_ACK_YES,
    TROUBLE_ACK_NO,
    TROUBLE_ACK_EITHER,
} troubleAckValue;

typedef struct {
    indicationType *types;      //Is not freed
    uint8_t size;
} troubleIndicationTypes;

typedef struct {
    troubleAckValue ackValue;
    troubleIndicationTypes *allowedIndicationTypes;
} troubleFilterConstraints;

troubleFilterConstraints *createTroubleFilterConstraints(void);
void destroyTroubleFilterConstraints(troubleFilterConstraints *constraints);

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
void getTroublesForUriPrivate(icLinkedList *outputList, char *deviceId, bool makeClone,
                              troubleFormat outputFormat, bool includeAckTroubles, troubleSortAlgo sort);

/*
 * return a list of original troubleContainer objects that match this zone.
 * the elements will be the original, so caller should not free directly
 */
icLinkedList *getTroubleContainersForZonePrivate(uint32_t zoneNumber);

/*
 * add a trouble to the list and takes ownership of the memory.
 * if a troubleId is not assigned (set to 0), then one will be assigned and
 * placed within the the trouble.
 *
 * a return of 0 means the trouble was not added (probably due to this trouble existing),
 * which the caller should look for so it can free up the container
 */
uint64_t addTroubleContainerPrivate(troubleContainer *container, troublePayloadCompareFunc compareFunc, bool sendEvent);

/*
 * clear the supplied trouble from the list.  unlike the 'public' variation,
 * this does not perform a "search for something similar", but instead assumes
 * this is a clone of the actual event to remove.
 */
bool clearTroubleContainerPrivate(troubleContainer *container);

/*
 * un-acknowledge a single trouble event
 */
void unacknowledgeTroublePrivate(uint64_t troubleId, bool sendEvent);

/*
 * return if there are any un-acknowledged 'system' troubles present.
 */
bool areSystemTroublesPresentPrivate();

/*
 * returns if the system is tampered (as a trouble)
 */
bool hasSystemTamperedTroublePrivate();

/*
 * returns the number of troubles
 */
uint32_t getTroubleCountPrivate(bool includeAckTroubles);

/*
 * returns the number of troubles for a given indication 'category'
 * and matches the 'constraints' provided
 */
uint32_t getTroubleCategoryCountPrivate(indicationCategory category, troubleFilterConstraints *constraints);


#endif // ZILKER_TROUBLESTATEPRIVATE_H
