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
 * lightMessage.h
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a Light to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_LIGHTMESSAGE_H
#define ZILKER_SDK_LIGHTMESSAGE_H

#include <light.h>
#include "message.h"

/*
 * create a message to inform the server that
 * a new light has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createLightAddedMessage(Light *light);

/*
 * create a message to inform the server that
 * a light was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createLightRemovedMessage(char *lightEndpointId);

/*
 * create a message to inform the server that
 * a light was updated (label, on/off, level, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createLightUpdatedMessage(Light *light);

#endif //ZILKER_SDK_LIGHTMESSAGE_H
