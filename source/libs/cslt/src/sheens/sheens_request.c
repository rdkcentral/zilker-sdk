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

//
// Created by Boyd, Weston on 5/8/18.
//

#include <uuid/uuid.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <jsonrpc/jsonrpc.h>
#include <actions/deviceActions.h>

#include "sheens_request.h"
#include "sheens_json.h"
#include "sheens_transcoders.h"

#define DEVICE_URI "/%s/ep/%s"
#define DEVICE_RESOURCE_URI DEVICE_URI "/r/%s"

#define JSON_TIMERID_KEY "timerId"

/** Parse a device ID from the legacy rule into
 * the device URI so we can manipulate the device.
 *
 * Note:
 * There are three versions of this device ID format.
 * Yes, three.
 *
 * First, (pre-Hawaii) has the format:
 * <MAC as decimal>
 * Example: 3781220541460145
 *
 * Second, (post-Hawaii a.k.a. 'alias') has the format:
 * <EUI64>.<endpoint>
 * Example: 000d6f0002a67cbe.1
 *
 * Third, (post-Zilker) has the format:
 * <Premise ID>.<EUI64>
 * Example: 6051.000d6f0003c1b733
 *
 * In order to construct the URI currently we *must* have
 * the devices _endpoint_. However, in the first and third
 * formats we do not have the endpoint. Right now the
 * endpoint will just be faked, and we will _hope_ that
 * the device service knows how to figure it out.
 *
 * @param src The device ID as reported by icrule.
 * @param resource The device Resource name so build into the URI.
 * @return A full URI with the format: <EUI64>/ep/<endpoint>/r/<resource>
 */
char* sheens_get_device_uri(const char* src, const char* resource)
{
    char *dst, *ep, *id;

    if ((src == NULL) || (strlen(src) == 0)) {
        errno = EINVAL;
        return NULL;
    }

    dst = NULL;

    if (sheens_transcoder_map_device_id(src, &id, &ep) == true) {
        dst = malloc(strlen(DEVICE_RESOURCE_URI) + strlen(id) + strlen(ep) + strlen(resource) + 1);
        if (dst == NULL) {
            errno = ENOMEM;
            return NULL;
        }

        /* Convert to the URI! */
        sprintf(dst, DEVICE_RESOURCE_URI, id, ep, resource);

        free(id);
        free(ep);
    }

    return dst;
}

cJSON* sheens_create_timer_fired_object(const char* uuid)
{
    cJSON* params = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(params, JSON_TIMERID_KEY, cJSON_CreateString(uuid));

    return jsonrpc_create_notification("timerFired", params);
}

cJSON* sheens_create_timer_emit_object(uint32_t seconds,
                                       const char* uuid,
                                       cJSON* message)
{
    cJSON* params;

    params = cJSON_CreateObject();

    /* The previous UUID JSON object is owned by
     * a different JSON object. Each JSON object
     * needs at least one of its own copies of
     * the object in order to function with
     * references.
     */
    cJSON_AddItemToObjectCS(params, JSON_TIMERID_KEY, cJSON_CreateString(uuid));
    cJSON_AddItemToObjectCS(params, "in", cJSON_CreateNumber(seconds));

    if (message) {
        cJSON_AddItemToObjectCS(params, "private", message);
    }

    return jsonrpc_create_notification("makeTimerAction", params);
}

cJSON* sheens_create_timer_oneshot_request(uint32_t seconds,
                                           const char* action_js,
                                           cJSON* nodes_object,
                                           cJSON* start_branches)
{
    cJSON *timer_fired_json;

    uuid_t uuid = "";
    char uuid_str[37]; // len(UUID) + '\0'

    // Add in our new Node to the Sheens spec.
    uuid_generate(uuid);
    uuid_unparse_upper(uuid, uuid_str);

    /* Create the branch for the start node pattern. */
    timer_fired_json = sheens_create_timer_fired_object(uuid_str);

    cJSON_AddItemToArray(start_branches, sheens_create_branch(timer_fired_json, uuid_str, false));

    /* Create the new node with UUID to handle the custom action. */
    cJSON_AddItemToObject(nodes_object,
                          uuid_str,
                          sheens_create_state_node(cJSON_CreateString(action_js), NULL, false));

    /* Now create the timer object for the
     * emit message.
     */
    return sheens_create_timer_emit_object(seconds, uuid_str, NULL);
}

/** Create a 'device' resource URI from the passed
 * in icrule ID given a resource name and mode
 * of operation.
 *
 * @param deviceId The ID passed in from the legacy rule.
 * @param resource The resource to interact with in the device.
 * @param actionSuppressResourceURI An optional resource ID that will be read before writing it. If the resource is "true"
 *                                  when the jsonrpc action executes, the write is not performed.
 * @return A JSON object that contains all the necessary
 * information to emit from the machine.
 */
cJSON* sheens_create_write_device_request(const char* deviceId,
                                          const char* resource,
                                          const char* actionSuppressResourceURI,
                                          const char* value)
{
    cJSON *params;
    char* writeURI = NULL;
    char* suppressURI = NULL;

    if ((writeURI = sheens_get_device_uri(deviceId, resource)) == NULL) {
        errno = EBADMSG;
        return NULL;
    }

    if (actionSuppressResourceURI != NULL && (suppressURI = sheens_get_device_uri(deviceId, actionSuppressResourceURI)) == NULL)
    {
        free(writeURI);
        errno = EBADMSG;
        return NULL;
    }

    params = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(params, AUTOMATION_DEV_RESOURCE_PARAM_URI, cJSON_CreateString(writeURI));
    cJSON_AddItemToObjectCS(params, AUTOMATION_DEV_WRITE_RESOURCE_PARAM_VALUE, cJSON_CreateString(value));
    cJSON_AddItemToObjectCS(params,
                            AUTOMATION_DEV_RESOURCE_PARAM_ACTION_SUPPRESS_RESOURCE_URI,
                            suppressURI != NULL ? cJSON_CreateString(suppressURI) : cJSON_CreateNull());

    free(writeURI);
    free(suppressURI);

    return jsonrpc_create_notification(AUTOMATION_DEV_WRITE_RESOURCE_METHOD, params);
}

