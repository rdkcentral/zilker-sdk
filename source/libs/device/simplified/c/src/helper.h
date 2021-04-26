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
 * helper.h
 *
 * helper functions used by the simplified object creation functions
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_HELPER_H
#define ZILKER_SDK_HELPER_H

#include <stdlib.h>
#include <stdbool.h>
#include <deviceService/deviceService_pojo.h>

/*
 * extract the desired resource from the device object.
 * returns a copy of the resource value, or a copy of the defaultValue
 * caller is responsible for releasing the returned string
 */
char *extractDeviceResource(DSDevice *device, const char *attribName, const char *defaultValue);

/*
 * extract the desired resource from the endpoint object.
 * returns a copy of the resource value, or a copy of the defaultValue
 * caller is responsible for releasing the returned string
 */
char *extractEndpointResource(DSEndpoint *endpoint, const char *attribName, const char *defaultValue);

/*
 * extract the desired resource from the endpoint object.
 */
bool extractEndpointResourceAsBool(DSEndpoint *endpoint, const char *attribName, bool defaultValue);

/*
 * extract the desired resource from the endpoint object.
 */
float extractEndpointResourceAsFloat(DSEndpoint *endpoint, const char *attribName, float defaultValue);

#endif //ZILKER_SDK_HELPER_H
