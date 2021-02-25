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
 * securityZoneHelper.c
 *
 * Set of helper functions for securityZone validation
 * and manipulation.  Available for client-side processing
 * to reduce the need for un-necessary IPC calls.
 *
 * Author: jelderton -  9/27/16.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <icSystem/softwareCapabilities.h>
#include <securityService/securityZoneHelper.h>

/*
 * validates that the security zone 'type' and 'function' are compatible.
 * for example, using a 'smoke' as an 'entry/exit' is invalid.
 */
bool validateSecurityZoneTypeAndFunction(securityZoneType type, securityZoneFunctionType func)
{
    // if this platform does not support alarms, then only allow function type of "monitor 24 hour"
    //
    if (supportAlarms() == false)
    {
        if (func != SECURITY_ZONE_FUNCTION_MONITOR_24HOUR)
        {
            return false;
        }
        return true;
    }

    // could get the valid set of functions via 'getSecurityZoneFunctionsForType', but
    // that is a lot of copy and search.  more efficient to just run through the
    // switch statements again
    //
    switch(type)
    {
        case SECURITY_ZONE_TYPE_DOOR:
        case SECURITY_ZONE_TYPE_WINDOW:
        {
            switch(func)
            {
                case SECURITY_ZONE_FUNCTION_ENTRY_EXIT:
                case SECURITY_ZONE_FUNCTION_PERIMETER:
                case SECURITY_ZONE_FUNCTION_MONITOR_24HOUR:
                case SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR:
                case SECURITY_ZONE_FUNCTION_SILENT_24HOUR:
                case SECURITY_ZONE_FUNCTION_SILENT_BURGLARY:
                case SECURITY_ZONE_FUNCTION_TROUBLE_DAY_ALARM_NIGHT:
                case SECURITY_ZONE_FUNCTION_NO_ALARM_RESPONSE:
                    return true;

                default:
                    return false;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_MOTION:
        {
            switch (func)
            {
                case SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER:
                case SECURITY_ZONE_FUNCTION_INTERIOR_WITH_DELAY:
                case SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT:
                case SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT_DELAY:
                case SECURITY_ZONE_FUNCTION_MONITOR_24HOUR:
                    return true;

                default:
                    return false;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_ENVIRONMENTAL:
        case SECURITY_ZONE_TYPE_WATER:
        {
            if (func == SECURITY_ZONE_FUNCTION_MONITOR_24HOUR ||
                func == SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR)
            {
                return true;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_GLASS_BREAK:
        {
            if (func == SECURITY_ZONE_FUNCTION_PERIMETER ||
                func == SECURITY_ZONE_FUNCTION_MONITOR_24HOUR)
            {
                return true;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_CO:
        {
            if (func == SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR)
            {
                return true;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_SMOKE:
        {
            if (func == SECURITY_ZONE_FUNCTION_FIRE_24HOUR)
            {
                return true;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_DURESS:
        {
            switch (func)
            {
                // duress is always silent
                case SECURITY_ZONE_FUNCTION_MONITOR_24HOUR:
                case SECURITY_ZONE_FUNCTION_SILENT_24HOUR:
                    return true;

                default:
                    return false;
            }
            break;
        }

        case SECURITY_ZONE_TYPE_MEDICAL:
        case SECURITY_ZONE_TYPE_PANIC:
        {
            switch (func)
            {
                case SECURITY_ZONE_FUNCTION_MONITOR_24HOUR:
                case SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR:
                case SECURITY_ZONE_FUNCTION_SILENT_24HOUR:
                    return true;

                default:
                    return false;
            }
            break;
        }

        default:    // unknown type, so allow everything
            return true;
    }

    return false;
}

/*
 * returns the set of zone functions for a particular zone type as a
 * NULL-terminated array.  caller must release the returned array.
 */
securityZoneFunctionType *getSecurityZoneFunctionsForType(securityZoneType type)
{
    // if this platform does not support alarms, then only return
    // the "monitor 24 hour" + NULL
    //
    if (supportAlarms() == false)
    {
        securityZoneFunctionType *retVal = (securityZoneFunctionType *)calloc(sizeof(securityZoneFunctionType), 2);
        retVal[0] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
        return retVal;
    }

    // allocate an array to handle our worst case scenario (11 items + NULL)
    //
    securityZoneFunctionType *retVal = (securityZoneFunctionType *)calloc(sizeof(securityZoneFunctionType), 12);
    int i = 0;

    switch(type)
    {
        case SECURITY_ZONE_TYPE_DOOR:
        case SECURITY_ZONE_TYPE_WINDOW:
            retVal[i++] = SECURITY_ZONE_FUNCTION_ENTRY_EXIT;
            retVal[i++] = SECURITY_ZONE_FUNCTION_PERIMETER;
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_SILENT_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_SILENT_BURGLARY;
            retVal[i++] = SECURITY_ZONE_FUNCTION_TROUBLE_DAY_ALARM_NIGHT;
            retVal[i++] = SECURITY_ZONE_FUNCTION_NO_ALARM_RESPONSE;
            break;

        case SECURITY_ZONE_TYPE_MOTION:
            retVal[i++] = SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER;
            retVal[i++] = SECURITY_ZONE_FUNCTION_INTERIOR_WITH_DELAY;
            retVal[i++] = SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT;
            retVal[i++] = SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT_DELAY;
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            break;

        case SECURITY_ZONE_TYPE_ENVIRONMENTAL:
        case SECURITY_ZONE_TYPE_WATER:
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            break;


        case SECURITY_ZONE_TYPE_GLASS_BREAK:
            retVal[i++] = SECURITY_ZONE_FUNCTION_PERIMETER;
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            break;

        case SECURITY_ZONE_TYPE_CO:
            retVal[i++] = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            break;

        case SECURITY_ZONE_TYPE_SMOKE:
            retVal[i++] = SECURITY_ZONE_FUNCTION_FIRE_24HOUR;
            break;

        case SECURITY_ZONE_TYPE_DURESS:
            // duress is always silent
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_SILENT_24HOUR;
            break;

        case SECURITY_ZONE_TYPE_MEDICAL:
        case SECURITY_ZONE_TYPE_PANIC:
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_SILENT_24HOUR;
            break;

        default:
            retVal[i++] = SECURITY_ZONE_FUNCTION_ENTRY_EXIT;
            retVal[i++] = SECURITY_ZONE_FUNCTION_PERIMETER;
            retVal[i++] = SECURITY_ZONE_FUNCTION_MONITOR_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_AUDIBLE_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_SILENT_24HOUR;
            retVal[i++] = SECURITY_ZONE_FUNCTION_SILENT_BURGLARY;
            retVal[i++] = SECURITY_ZONE_FUNCTION_TROUBLE_DAY_ALARM_NIGHT;
            retVal[i++] = SECURITY_ZONE_FUNCTION_NO_ALARM_RESPONSE;
            retVal[i++] = SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER;
            retVal[i++] = SECURITY_ZONE_FUNCTION_INTERIOR_WITH_DELAY;
            retVal[i++] = SECURITY_ZONE_FUNCTION_FIRE_24HOUR;
            break;
    }

    return retVal;
}

bool securityZoneFaultPreventsArming(const securityZone *zone)
{
    bool preventsArming = false;

    if (zone != NULL)
    {
        switch (zone->zoneFunction)
        {
            case SECURITY_ZONE_FUNCTION_MONITOR_24HOUR:
            case SECURITY_ZONE_FUNCTION_INTERIOR_FOLLOWER:
            case SECURITY_ZONE_FUNCTION_INTERIOR_WITH_DELAY:
            case SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT:
            case SECURITY_ZONE_FUNCTION_INTERIOR_ARM_NIGHT_DELAY:
            case SECURITY_ZONE_FUNCTION_SILENT_24HOUR:
            case SECURITY_ZONE_FUNCTION_SILENT_BURGLARY:
                preventsArming = false;
                break;

            default:
                preventsArming = true;
                break;
        }
    }

    return preventsArming;
}
