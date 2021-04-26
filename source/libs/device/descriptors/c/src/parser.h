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

//
// Created by Thomas Lea on 7/31/15.
//

#ifndef ZILKER_PARSER_H
#define ZILKER_PARSER_H

#include <icTypes/icLinkedList.h>

/*
 * Parse the device descriptor list (aka whitelist) and any optional blacklist at the provided paths
 * and return a list of device descriptors that are not explicitly blacklisted.
 */
icLinkedList* parseDeviceDescriptors(const char *whitelistPath, const char *blacklistPath);

/*
 * Allows just parsing a blacklist, can be used for validating a blacklist file is valid
 * @param blacklistPath the path to the blacklist
 * @return the blacklisted uuids
 */
icStringHashMap *getBlacklistedUuids(const char *blacklistPath);

#endif //ZILKER_PARSER_H
