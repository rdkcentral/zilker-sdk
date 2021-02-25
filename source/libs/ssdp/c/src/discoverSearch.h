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

#ifndef IC_DISCOVERSEARCH_H
#define IC_DISCOVERSEARCH_H

#include <stdint.h>
#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include "ssdp/ssdp.h"              // objects and functions publicly defined

#define SSDP_LOG_TAG    "ssdp"

typedef enum
{
    SSDP_SEARCH_TYPE_STANDARD = 0,
    SSDP_SEARCH_TYPE_MARVELL
} SsdpSearchCategory;

/*
 * container of all input parms when somoen requests a discovery
 */
typedef struct _discoverSearch
{
    icLinkedList         *stList;           // list of strings, defining the ST to search for
    SsdpDiscoverCallback callback;          // function to notify when a potential match is found
    SearchType           searchType;
    SsdpSearchCategory   searchCategory;
    uint32_t             handle;
    icLinkedList         *processedList;    // list of IPs (string) that were processed by this entity
} discoverSearch;


/*
 * helper function to allocate and clear a discoverSearch object
 *
 * @see destroyDiscoverSearch
 */
discoverSearch *createDiscoverSearch();

/*
 * helper to safely release memory associated with the discoverSearch object
 */
void destroyDiscoverSearch(discoverSearch *obj);

/*
 * helper function to see if an IP address is stored in the 'processedList'.
 */
bool discoverSearchDidProcessedIp(discoverSearch *obj, char *ipAddress);

/*
 * add an IP address to the 'processedList' so that the device is not examined
 * again by this entity (until the next search attempt).
 */
void discoverSearchAddProcessedIp(discoverSearch *obj, char *ipAddress);


#endif // IC_DISCOVERSEARCH_H
