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
// Created by Boyd, Weston on 4/27/18.
//

#ifndef ZILKER_ICRULE_INTERNAL_H
#define ZILKER_ICRULE_INTERNAL_H

#include <libxml2/libxml/tree.h>

#include <icrule/icrule.h>

#ifndef unused
#define unused(v) ((void) (v))
#endif

#ifndef sizeof_array
#define sizeof_array(a) (sizeof(a) / sizeof(a[0]))
#endif

#define hashmap_string_sizeof(v) ((uint16_t) strlen((v)))

/** Retrieve the configured action definition
 * XML directory.
 *
 * @return A valid string pointing to the action
 * definition XML directory. Do NOT attempt to
 * release this memory. The string will never
 * be invalid!
 */
const char* icrule_get_action_list_dir(void);

/** Helper routine to parse the XML "time slot"
 * type.
 *
 * @param parent The parent node that holds the
 * time slot XML.
 * @param time THe output memory where the time
 * slot will be decoded into.
 * @return Zero on success, otherwise -1 and errno
 * will be set.
 */
int icrule_parse_time_slot(xmlNodePtr parent, icrule_time_t* time);

/** We need to update sms/email actions just in case there are video/picture
 * attachments that are required. This is necessary
 * because the legacy rules implicitly implied ordering
 * between video/pictures and sms/email attachments. That
 * does not mean however that other rule engines will do this.
 * Thus let's add in a new parameter key "attachment" with
 * value "video" | "picture".
 *
 * @param actions The list of actions to see if video/picture and sms/email are in.
 */
void icrule_update_message_attachment(icLinkedList* actions);

icLinkedList* strtok2list(const char* value, const char* delim);

#endif //ZILKER_ICRULE_INTERNAL_H
