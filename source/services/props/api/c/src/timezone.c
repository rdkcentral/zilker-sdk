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
 * timezone.c
 *
 * Helper to register for a "property changed event"
 * and adjust the internal clock when the CPE_TZ
 * property changes.
 *
 * Author: jelderton - 2/4/16
 *-----------------------------------------------*/

#include <time.h>
#include <string.h>

#include <icLog/logging.h>
#include <propsMgr/timezone.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsService_event.h>
#include <propsMgr/propsService_eventAdapter.h>

static void propChangeEventHandler(cpePropertyEvent *event)
{
    if (event != NULL && event->propKey != NULL && event->propValue != NULL &&
        strcmp(event->propKey, POSIX_TIME_ZONE_PROP) == 0)
    {
        // the timezone property changed, so apply the POSIX value
        // to our environment (as the TZ variable), and reload the
        // timezone in our process
        //
        icLogDebug("timezone", "timezone changed, applying new timezone '%s' to local process", event->propValue);
        setenv("TZ", (const char *)event->propValue, 1);
        tzset();
    }
}

/*
 * add this process as a property change listener, specificly
 * looking for a change in the CPE_TZ property.  when that occurs,
 * reset the timezone for this process via tzset()
 */
void autoAdjustTimezone()
{
    register_cpePropertyEvent_eventListener(propChangeEventHandler);
}

void disableAutoAdjustTimezone()
{
    unregister_cpePropertyEvent_eventListener(propChangeEventHandler);
}
