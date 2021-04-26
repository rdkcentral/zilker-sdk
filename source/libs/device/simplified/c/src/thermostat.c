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
 * thermostat.c
 *
 * simplified object to represent a thermostat.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include <deviceHelper.h>
#include <commonDeviceDefs.h>
#include <thermostat.h>
#include "helper.h"


/*
 * Create a basic Thermostat object
 */
Thermostat *createThermostat(void)
{
    return (Thermostat *)calloc(1, sizeof(Thermostat));
}

/*
 * Create a Thermostat using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
Thermostat *createThermostatFromEndpoint(DSEndpoint *endpoint)
{
    // assign info from the endpoint itself
    Thermostat *retVal = createThermostat();
    retVal->deviceId = strdup(endpoint->ownerId);
    retVal->endpointId = strdup(endpoint->id);
    retVal->label = extractEndpointResource(endpoint, COMMON_ENDPOINT_RESOURCE_LABEL, NULL);

    // see if the system is on (asking if it's off)
    retVal->systemOn = true;
    char *systemState = extractEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_OFF);
    if (stringCompare(systemState, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_STATE_OFF, false) == 0)
    {
        retVal->systemOn = false;
    }
    free(systemState);

    // termostat state (uses the 'mode')
    char *systemMode = extractEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_OFF);
    if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_COOL, false) == 0)
    {
        retVal->state = THERMOSTAT_COOL;
    }
    else if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_COOL, false) == 0)
    {
        retVal->state = THERMOSTAT_HEAT;
    }
    else if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_AUTO, false) == 0)
    {
        retVal->state = THERMOSTAT_AUTO;
    }
    else if (stringCompare(systemMode, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_FAN_ONLY, false) == 0)
    {
        retVal->state = THERMOSTAT_FAN_ONLY;
    }
    else // THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_OFF
    {
        retVal->state = THERMOSTAT_OFF;
    }
    free(systemMode);

    retVal->fanOn = extractEndpointResourceAsBool(endpoint, THERMOSTAT_PROFILE_RESOURCE_FAN_ON, false);
    retVal->currentTemperature = extractEndpointResourceAsFloat(endpoint, THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP, 0.0F);
    retVal->coolSetpoint = extractEndpointResourceAsFloat(endpoint, THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT, 0.0F);
    retVal->heatSetpoint = extractEndpointResourceAsFloat(endpoint, THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT, 0.0F);

    // get info saved in the device (parent owner)
    char *tmp = NULL;
    if (deviceHelperReadDeviceResource(retVal->deviceId, COMMON_DEVICE_RESOURCE_MANUFACTURER, &tmp) == true)
    {
        retVal->manufacturer = tmp;
        tmp = NULL;
    }
    if (deviceHelperReadDeviceResource(retVal->deviceId, COMMON_DEVICE_RESOURCE_MODEL, &tmp) == true)
    {
        retVal->model = tmp;
        tmp = NULL;
    }

    return retVal;
}

/*
 * Destroy a Thermostat object
 */
void destroyThermostat(Thermostat *thermostat)
{
    if (thermostat != NULL)
    {
        free(thermostat->label);
        thermostat->label = NULL;
        free(thermostat->deviceId);
        thermostat->deviceId = NULL;
        free(thermostat->endpointId);
        thermostat->endpointId = NULL;
        free(thermostat->manufacturer);
        thermostat->manufacturer = NULL;
        free(thermostat->model);
        thermostat->model = NULL;
        free(thermostat);
        thermostat = NULL;
    }
}
