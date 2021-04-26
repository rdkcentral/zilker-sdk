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
// Created by Boyd, Weston on 4/28/18.
//

#ifndef ZILKER_ICRULE_ACTION_H
#define ZILKER_ICRULE_ACTION_H

#include <libxml2/libxml/tree.h>
#include <icTypes/icLinkedList.h>

int icrule_action_list_load(const char* filename, icHashMap* map);
void icrule_action_list_release(icHashMap* map);

void icrule_free_action(void* alloc);
void icrule_free_parameter(void* key, void* value);

int icrule_parse_action(xmlNodePtr parent, icLinkedList* actions, icHashMap* actionMap);

#endif //ZILKER_ICRULE_ACTION_H
