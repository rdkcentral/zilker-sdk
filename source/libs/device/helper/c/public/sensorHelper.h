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
 * sensorHelper.h
 *
 * set of helper functions to aid with the "business logic" needs
 * that many services require when interacting with sensor endpoints.
 * allows the lower layers of deviceService to remain generic and
 * not have to be concerned with specifics of sensors.
 *
 * Author: jelderton - 7/19/16
 *-----------------------------------------------*/

#ifndef ZILKER_SENSORHELPER_H
#define ZILKER_SENSORHELPER_H

#include <stdbool.h>
#include <deviceService/deviceService_pojo.h>
#include <deviceService/deviceService_event.h>

/*
 * examine the DSEndpoint 'status' attribute to see if faulted or restored.
 */
bool isEndpointFaulted(DSEndpoint *endpoint);

/*
 * similar to 'isEndpointFaulted', however uses the information
 * from a DeviceServiceResourceUpdatedEvent.
 * note that this can only extract the faulted state when the
 * event->resource->id == SENSOR_PROFILE_RESOURCE_STATUS
 */
bool isEndpointFaultedViaEvent(DeviceServiceResourceUpdatedEvent *event);

/*
 * extract the DSEndpoint 'bypassed' attribute.
 */
bool isEndpointBypassed(DSEndpoint *endpoint);

/*
 * similar to 'isEndpointBypassed', however uses the information
 * from a DeviceServiceResourceUpdatedEvent.
 * note that this can only extract the bypassed state when the
 * event->resource->id == SENSOR_PROFILE_RESOURCE_BYPASSED
 */
bool isEndpointBypassedViaEvent(DeviceServiceResourceUpdatedEvent *event);

#endif // ZILKER_SENSORHELPER_H
