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
// Created by Boyd, Weston on 5/8/18.
//

#ifndef ZILKER_ICRULE_SCHEDULE_H
#define ZILKER_ICRULE_SCHEDULE_H

#include <libxml2/libxml/tree.h>

void icrule_free_schedule(void* alloc);
int icrule_parse_schedule(xmlNodePtr parent, icLinkedList* schedule_list);

#endif //ZILKER_ICRULE_SCHEDULE_H
