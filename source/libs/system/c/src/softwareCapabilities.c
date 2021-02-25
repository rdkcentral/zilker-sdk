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
 * softwareCapabilities.h
 *
 * Single point of reference to query the system for
 * any software abilities that are present.  This used
 * to reside in the Java utilsLib, but is a better fit
 * in it's own library as not all services need to
 * know which functionality is present.
 *
 * Author: jelderton - 7/13/15
 *-----------------------------------------------*/

// look at buildtime settings to direct many 'ifdef' expressions below
#include <icBuildtime.h>
#include <icSystem/softwareCapabilities.h>

/*
 * return true if "systemMode" (ie: scenes) are supported
 * at this time systems either support "alarms" or
 * "systemMode", but not both
 */
bool supportSystemMode()
{
    // touchstone should utilize systemMode vs alarm
    //
#ifdef CONFIG_SERVICE_SECURITY_SCENE
    return true;
#else
    return false;
#endif
}

/*
 * return true if "alarms" are supported.  at this
 * time systems either support "alarm" or "systemMode",
 * but not both
 */
bool supportAlarms()
{
#ifdef CONFIG_SERVICE_SECURITY_ALARM
    return true;
#else
    return false;
#endif
}

/*
 * If this environment supports the full breadth
 * of zone functions and sensors (i.e. motion will
 * allow '24 Monitor Alarm Night' mode)
 */
bool supportSecurityZones()
{
#ifdef CONFIG_SERVICE_SECURITY_ALARM
    return true;
#else
    return false;
#endif
}

/*
 * return true if this environment supports PIMs
 * and Takeover Zones
 */
bool supportTakeoverZones()
{
#ifdef CONFIG_SERVICE_SECURITY_ALARM
    return true;
#else
    return false;
#endif
}

/*
 * return if this platform supports add-on local
 * applications to be installed and ran.
 */
bool supportLocalApplications()
{
    return false;
}

/*
 * return if this platform supports automations,
 * e.g. rules
 */
bool supportAutomations()
{
#ifndef CONFIG_SERVICE_AUTOMATIONS
    return false;
#else
    return true;
#endif
}

