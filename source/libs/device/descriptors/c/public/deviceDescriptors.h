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
// Created by Thomas Lea on 7/29/15.
//

#ifndef ZILKER_DEVICEDESCRIPTORS_H
#define ZILKER_DEVICEDESCRIPTORS_H

#include <libxml/parser.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <deviceDescriptor.h>

/*
 * Initialize the library and provide the path to where the device descriptor files will be located.
 */
void deviceDescriptorsInit(const char *whiteListPath, const char *blackListPath);

/*
 * Release all resources used by the device descriptors library.
 */
void deviceDescriptorsCleanup();

/*
 * Retrieve the matching DeviceDescriptor for the provided input or NULL if a matching one doesnt exist.
 */
DeviceDescriptor* deviceDescriptorsGet(const char *manufacturer,
                                      const char *model,
                                      const char *hardwareVersion,
                                      const char *firmwareVersion);

/*
 * Retrieve the currently configured whitelist path.
 *
 * Caller frees
 */
char *getWhiteListPath();

/*
 * Retrieve the currently configured blacklist path.
 *
 * Caller frees
 */
char *getBlackListPath();

/*
 * Check whether a given white list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param whiteListPath the white list file to check
 * @return true if valid, false otherwise
 */
bool checkWhiteListValid(const char *whiteListPath);

/*
 * Check whether a given black list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param blackListPath the black list file to check
 * @return true if valid, false otherwise
 */
bool checkBlackListValid(const char *blackListPath);

#endif //ZILKER_DEVICEDESCRIPTORS_H
