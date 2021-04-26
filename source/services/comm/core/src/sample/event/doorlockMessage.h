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
 * doorlockMessage.h
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a DoorLock to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_DOORLOCKMESSAGE_H
#define ZILKER_SDK_DOORLOCKMESSAGE_H

#include <doorlock.h>
#include "message.h"

/*
 * create a message to inform the server that
 * a new door-lock has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createDoorLockAddedMessage(DoorLock *doorLock);

/*
 * create a message to inform the server that
 * a door-lock was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createDoorLockRemovedMessage(char *doorLockEndpointId);

/*
 * create a message to inform the server that
 * a door-lock was updated (label, locked/unlocked, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createDoorLockUpdatedMessage(DoorLock *doorLock);

#endif //ZILKER_SDK_DOORLOCKMESSAGE_H
