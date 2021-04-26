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
 * sensorMessage.h
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a Sensor to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_SENSORMESSAGE_H
#define ZILKER_SDK_SENSORMESSAGE_H

#include <sensor.h>
#include "message.h"

/*
 * create a message to inform the server that
 * a new sensor has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorAddedMessage(Sensor *sensor);

/*
 * create a message to inform the server that
 * a sensor was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorRemovedMessage(char *sensorEndpointId);

/*
 * create a message to inform the server that
 * a sensor was updated (label, type/function, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorUpdatedMessage(Sensor *sensor);

/*
 * create a message to inform the server that
 * a sensor reported a fault/restore.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorFaultRestoreMessage(Sensor *sensor);

#endif //ZILKER_SDK_SENSORMESSAGE_H
