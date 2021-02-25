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
 * troubleEventHelper.c
 *
 * Set of helper functions to aid in the construction and
 * analysis of trouble objects.  Also has static strings
 * and debug functions to aid logging of trouble enumerations.
 *
 * Author: jelderton -  4/24/18.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <securityService/troubleEventHelper.h>
#include <stdio.h>

/*
 * print a trouble to the log.  if 'message' is non-NULL,
 * will be added to the debug message before the trouble
 * details (ex:  'added new trouble: [id=x time=y...]'
 */
void debugPrintTroubleObject(troubleObj *trouble, const char *logCategory, const char *message)
{
    char *payload = NULL;
    if (trouble->extra != NULL)
    {
        payload = cJSON_Print(trouble->extra);
    }
    icLogDebug(logCategory, "%s [trouble eventId=%"PRIu64"; troubleId=%"PRIu64", type=%s, reason=%s, critical=%s, restore=%s, payload=%s]",
               (message != NULL) ? message : "",
               trouble->eventId,
               trouble->troubleId,
               troubleTypeLabels[trouble->type],
               troubleReasonLabels[trouble->reason],
               troubleCriticalityTypeLabels[trouble->critical],
               (trouble->restored == true) ? "true" : "false",
               (payload != NULL) ? payload : "N/A");
    // Cleanup
    free(payload);
}


