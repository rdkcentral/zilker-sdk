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
// Created by Boyd, Weston on 5/3/18.
//

#include <icrule/icrule.h>
#include <cslt/cslt.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <jsonrpc/jsonrpc.h>
#include <inttypes.h>
#include <commonDeviceDefs.h>
#include <icUtil/stringUtils.h>

#include "../cslt_internal.h"
#include "sheens_javascript.h"
#include "sheens_transcoders.h"
#include "sheens_json.h"
#include "sheens_request.h"

typedef int (*action_handler_t)(const uint64_t ruleId,
                                const icrule_action_t* action,
                                cJSON* output_objects,
                                cJSON* nodes_object,
                                cJSON* start_branches);

struct icrule_target {
    const char* name;
    action_handler_t handler;
};

static const char* duration_key = "duration";
static const int PICTURE_COUNT_DEFAULT = 5;

// Creates a token using the ruleId
// We may want to change this in the future to generate a random UUID that we store for verification
// Used in conjunction with IPC call IS_VALID_TOKEN to validate that command came from rules
//
static char* create_token(uint64_t ruleId)
{
    char *token = calloc(21,1);  // max number of digits in UINT64_MAX + 1
    snprintf(token,21,"%"PRIu64,ruleId);
    return token;
}

static int action_light_handler(const uint64_t ruleId,
                                const icrule_action_t* action,
                                cJSON* output_objects,
                                cJSON* nodes_object,
                                cJSON* start_branches)
{
    cJSON* json;
    bool enabled;
    char* lightId;
    icrule_action_parameter_t* parameter;

    unused(ruleId);

    enabled = (action->target[strlen(action->target) - 1] == 'n'); // Test if this is light o'n' or off

    parameter = hashMapGet(action->parameters,
                           "lightID",
                           FACTORY_HASHMAP_KEYLEN("lightID"));
    if ((parameter == NULL) ||
        (parameter->value == NULL) ||
        (strlen(parameter->value) == 0)) {
        errno = EINVAL;
        return -1;
    }

    lightId = parameter->value;

    /* If this is a dimmer then we want to send the level as well.
     * However, changing the level above zero will force the light
     * on. Changing the level to zero will force the light off.
     * Thus we must change the level before "isOn" so that the
     * user does not see the light come on at one level then
     * suddenly dim down to another.
     *
     * Note: In this scenario it is completely possible to
     * create a rule that has a level of zero, but then
     * set "isOn" to "true". This will force the light
     * off then back on to 100%.
     */
    parameter = hashMapGet(action->parameters,
                           (void*) "level",
                           FACTORY_HASHMAP_KEYLEN("level"));
    if (parameter != NULL) {
        unsigned long level;
        char level_str[8];

        errno = 0;
        level = strtoul(parameter->value, NULL, 10);
        if (errno == ERANGE) {
            errno = EINVAL;
            return -1;
        }

        if (level > 100) level = 100;

        sprintf(level_str, "%lu", level);

        json = sheens_create_write_device_request(lightId,
                                                  LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL,
                                                  NULL,
                                                  level_str);
        if (json == NULL) return -1;

        cJSON_AddItemToArray(output_objects, json);
    }

    json = sheens_create_write_device_request(lightId,
                                              LIGHT_PROFILE_RESOURCE_IS_ON,
                                              NULL,
                                              bool2str(enabled));
    if (json == NULL) return -1;

    cJSON_AddItemToArray(output_objects, json);

    /* If the duration is defined then we need to setup a timer
     * so that we can handle the light on/off.
     */
    parameter = hashMapGet(action->parameters,
                           (void*) duration_key,
                           FACTORY_HASHMAP_KEYLEN(duration_key));
    if (parameter != NULL) {
        /* We need to setup a timer and toggle the light.
         * This is going to require a new node, and a check
         * in the start branches for our special condition.
         * We will also need to add in another output command
         * from the machine to setup the timer.
         */
        static const char* emit_js = "_.out([%s]);\nreturn _.bindings;\n";

        uint32_t seconds;

        char *js, *request_js;

        errno = 0;
        seconds = (uint32_t) strtoul(parameter->value, NULL, 10);
        if (errno == ERANGE) {
            errno = EINVAL;
            return -1;
        }

        json = sheens_create_write_device_request(lightId,
                                                  LIGHT_PROFILE_RESOURCE_IS_ON,
                                                  NULL,
                                                  bool2str(!enabled));
        if (json == NULL) return -1;

        request_js = cJSON_PrintBuffered(json, 512, false);

        js = malloc(strlen(request_js) + strlen(emit_js) + 1);
        if (js == NULL) {
            cJSON_Delete(json);

            errno = ENOMEM;
            return -1;
        }

        sprintf(js, emit_js, request_js);
        free(request_js);
        cJSON_Delete(json);

        /* Generate the JavaScript for the light toggle after duration */
        json = sheens_create_timer_oneshot_request(seconds, js, nodes_object, start_branches);
        free(js);

        cJSON_AddItemToArray(output_objects, json);
    }

    return 0;
}

static int action_doorlock_handler(const uint64_t ruleId,
                                   const icrule_action_t* action,
                                   cJSON* output_objects,
                                   cJSON* nodes_object,
                                   cJSON* start_branches)
{
    cJSON* json;
    icrule_action_parameter_t* parameter;

    unused(ruleId);
    unused(nodes_object);
    unused(start_branches);

    bool enabled = (action->target[11] != 'u'); // Test if this is 'u'nlock or lock.

    parameter = hashMapGet(action->parameters,
                           "doorLockID",
                           FACTORY_HASHMAP_KEYLEN("doorLockID"));
    if ((parameter == NULL) ||
        (parameter->value == NULL) ||
        (strlen(parameter->value) == 0)) {
        errno = EINVAL;
        return -1;
    }

    json = sheens_create_write_device_request(parameter->value,
                                              DOORLOCK_PROFILE_RESOURCE_LOCKED,
                                              NULL,
                                              bool2str(enabled));
    if (json == NULL) return -1;

    cJSON_AddItemToArray(output_objects, json);

    return 0;
}

static int action_thermostat_handler(const uint64_t ruleId,
                                     const icrule_action_t* action,
                                     cJSON* output_objects,
                                     cJSON* nodes_object,
                                     cJSON* start_branches)
{
    cJSON* json;
    icrule_action_parameter_t* parameter;
    const char* tstatid;
    const char* tstat_mode;

    unused(ruleId);
    unused(nodes_object);
    unused(start_branches);

    parameter = hashMapGet(action->parameters,
                           "thermostatID",
                           FACTORY_HASHMAP_KEYLEN("thermostatID"));
    if ((parameter == NULL) ||
        (parameter->value == NULL) ||
        (strlen(parameter->value) == 0)) {
        errno = EINVAL;
        return -1;
    }

    /* The device request requires the thermostat
     * ID for each request. Thus save off the ID.
     */
    tstatid = parameter->value;

    switch (action->target[strlen(action->target) - 1]) {
        case 'f':
            tstat_mode = THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_OFF;
            break;
        case 'l':
            tstat_mode = THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_COOL;
            break;
        case 't':
            tstat_mode = THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE_HEAT;
            break;
        default:
            errno = EBADMSG;
            return -1;
    }

    json = sheens_create_write_device_request(tstatid,
                                              THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE,
                                              THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                              tstat_mode);
    if (json == NULL) return -1;

    cJSON_AddItemToArray(output_objects, json);

    parameter = hashMapGet(action->parameters,
                           "setpoint",
                           FACTORY_HASHMAP_KEYLEN("setpoint"));
    if ((parameter != NULL) &&
        (parameter->value != NULL) &&
        (strlen(parameter->value) > 0)) {
        const char* tstat_setpoint;

        switch (action->target[strlen(action->target) - 1]) {
            case 'l':
                tstat_setpoint = THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT;
                break;
            case 't':
                tstat_setpoint = THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT;
                break;
            default:
                errno = EBADMSG;
                return -1;
        }

        json = sheens_create_write_device_request(tstatid,
                                                  tstat_setpoint,
                                                  THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                                  parameter->value);
        if (json == NULL) return -1;

        cJSON_AddItemToArray(output_objects, json);
    }

    parameter = hashMapGet(action->parameters,
                           "hold",
                           FACTORY_HASHMAP_KEYLEN("hold"));
    if ((parameter != NULL) &&
        (parameter->value != NULL) &&
        (strlen(parameter->value) > 0)) {
        json = sheens_create_write_device_request(tstatid,
                                                  THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                                  NULL,
                                                  parameter->value);
        if (json == NULL) return -1;

        cJSON_AddItemToArray(output_objects, json);
    }

    return 0;
}

static int action_notification_handler(const uint64_t ruleId,
                                       const icrule_action_t* action,
                                       cJSON* output_objects,
                                       cJSON* nodes_object,
                                       cJSON* start_branches)
{
    cJSON* params;
    icrule_action_parameter_t* parameter;

    unused(nodes_object);
    unused(start_branches);

    params = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(params, "ruleId", cJSON_CreateNumber(ruleId));
    cJSON_AddItemToObjectCS(params, "eventId", cJSON_CreateRaw("_.bindings['" sheens_event_id_bound_key "']"));
    cJSON_AddItemToObjectCS(params, "time", cJSON_CreateRaw("_.bindings['" sheens_event_time_bound_key "']"));

    parameter = hashMapGet(action->parameters,
                           "attachment",
                           FACTORY_HASHMAP_KEYLEN("attachment"));
    if ((parameter != NULL) &&
        (parameter->value != NULL) &&
        (strlen(parameter->value) > 0)) {
        cJSON_AddItemToObjectCS(params, "attachment", cJSON_CreateString(parameter->value));
    } else {
        cJSON_AddItemToObjectCS(params, "attachment", cJSON_CreateNull());
    }

    cJSON_AddItemToArray(output_objects, jsonrpc_create_notification("sendEmailAction", params));

    return 0;
}

static cJSON *create_picture_handler_base_params(const uint64_t ruleId, const icrule_action_t* action)
{
    cJSON* params;
    icrule_action_parameter_t* parameter;
    uint32_t pictureCount = PICTURE_COUNT_DEFAULT;

    char *quality;


    parameter = hashMapGet(action->parameters,
                           "count",
                           FACTORY_HASHMAP_KEYLEN("count"));

    if ((parameter != NULL) && (parameter->value != NULL)) {
        if (stringToUint32(parameter->value, &pictureCount) == false) {
            pictureCount = PICTURE_COUNT_DEFAULT;
        }
    }

    parameter = hashMapGet(action->parameters,
                           "size",
                           FACTORY_HASHMAP_KEYLEN("size"));

    if ((parameter != NULL) && (parameter->value != NULL)) {
        if (strcmp(parameter->value, "small") == 0) {
            quality = "low";
        } else if (strcmp(parameter->value, "large") == 0) {
            quality = "high";
        } else {
            quality = "medium";
        }
    } else {
        quality = "medium";
    }

    params = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(params, "ruleId", cJSON_CreateNumber(ruleId));
    cJSON_AddItemToObjectCS(params, "eventCode", cJSON_CreateRaw("_.bindings['" sheens_event_code_key "']"));
    cJSON_AddItemToObjectCS(params, "eventId", cJSON_CreateRaw("_.bindings['" sheens_event_id_bound_key "']"));
    cJSON_AddItemToObjectCS(params, "time", cJSON_CreateRaw("_.bindings['" sheens_event_time_bound_key "']"));
    cJSON_AddItemToObjectCS(params, "count", cJSON_CreateNumber(pictureCount));
    cJSON_AddItemToObjectCS(params, "quality", cJSON_CreateString(quality));

    return params;
}



static int action_picture_handler(const uint64_t ruleId,
                                  const icrule_action_t* action,
                                  cJSON* output_objects,
                                  cJSON* nodes_object,
                                  cJSON* start_branches)
{
    cJSON* params;
    icrule_action_parameter_t* parameter;

    char *cameraId;

    unused(nodes_object);
    unused(start_branches);

    parameter = hashMapGet(action->parameters,
                           "cameraID",
                           FACTORY_HASHMAP_KEYLEN("cameraID"));
    if ((parameter == NULL) ||
        (parameter->value == NULL) ||
        (strlen(parameter->value) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if (sheens_transcoder_map_device_id(parameter->value, &cameraId, NULL) == false) {
        return -1;
    }

    params = create_picture_handler_base_params(ruleId, action);
    cJSON_AddItemToObjectCS(params, "cameraId", cJSON_CreateString(cameraId));

    cJSON_AddItemToArray(output_objects, jsonrpc_create_notification("takePictureAction", params));

    free(cameraId);

    return 0;
}

static int action_video_handler(const uint64_t ruleId,
                                const icrule_action_t* action,
                                cJSON* output_objects,
                                cJSON* nodes_object,
                                cJSON* start_branches)
{
    cJSON* params;
    icrule_action_parameter_t* parameter;

    int preroll = 5;
    int duration = 10;

    char *cameraId;

    unused(nodes_object);
    unused(start_branches);

    parameter = hashMapGet(action->parameters,
                           "cameraID",
                           FACTORY_HASHMAP_KEYLEN("cameraID"));
    if ((parameter == NULL) ||
        (parameter->value == NULL) ||
        (strlen(parameter->value) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if (sheens_transcoder_map_device_id(parameter->value, &cameraId, NULL)  == false) {
        return -1;
    }

    parameter = hashMapGet(action->parameters,
                           "duration",
                           FACTORY_HASHMAP_KEYLEN("duration"));
    if ((parameter != NULL) &&
        (parameter->value != NULL) &&
        (strlen(parameter->value) > 0)) {
        errno = 0;
        duration = (int) strtol(parameter->value, NULL, 10);
        if (errno == ERANGE) {
            duration = 10;
            errno = 0;
        }
    }

    params = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(params, "ruleId", cJSON_CreateNumber(ruleId));
    cJSON_AddItemToObjectCS(params, "eventId", cJSON_CreateRaw("_.bindings['" sheens_event_id_bound_key "']"));
    cJSON_AddItemToObjectCS(params, "eventCode", cJSON_CreateRaw("_.bindings['" sheens_event_code_key "']"));
    cJSON_AddItemToObjectCS(params, "time", cJSON_CreateRaw("_.bindings['" sheens_event_time_bound_key "']"));
    cJSON_AddItemToObjectCS(params, "cameraId", cJSON_CreateString(cameraId));
    cJSON_AddItemToObjectCS(params, "pre-roll", cJSON_CreateNumber(preroll));
    cJSON_AddItemToObjectCS(params, "duration", cJSON_CreateNumber(duration));

    cJSON_AddItemToArray(output_objects, jsonrpc_create_notification("takeVideoAction", params));

    free(cameraId);

    return 0;
}

static int action_playsound_handler(const uint64_t ruleId,
                                    const icrule_action_t* action,
                                    cJSON* output_objects,
                                    cJSON* nodes_object,
                                    cJSON* start_branches)
{
    static const char* js = "_.bindings['%s']";

    cJSON* params;
    icrule_action_parameter_t* parameter;

    char* name;

    char buffer[strlen(js) + strlen(sheens_event_id_bound_key) + strlen(sheens_event_time_bound_key) + 1];

    unused(ruleId);
    unused(nodes_object);
    unused(start_branches);

    parameter = hashMapGet(action->parameters,
                           "sound",
                           FACTORY_HASHMAP_KEYLEN("sound"));
    if ((parameter != NULL) &&
        (parameter->value != NULL) &&
        (strlen(parameter->value) > 0)) {
        name = parameter->value;
    } else {
        name = "default";
    }

    params = cJSON_CreateObject();

    sprintf(buffer, js, sheens_event_id_bound_key);
    cJSON_AddItemToObjectCS(params, "eventId", cJSON_CreateRaw(buffer));

    sprintf(buffer, js, sheens_event_time_bound_key);
    cJSON_AddItemToObjectCS(params, "time", cJSON_CreateRaw(buffer));

    cJSON_AddItemToObjectCS(params, "name", cJSON_CreateString(name));

    cJSON_AddItemToArray(output_objects, jsonrpc_create_notification("playAudioAction", params));

    return 0;
}

static const struct icrule_target icrule2sheen[] = {
    { "ruleAction_turnLightOn",  action_light_handler },
    { "ruleAction_turnLightOff", action_light_handler },
    { "ruleAction_lockDoorLock", action_doorlock_handler },
    { "ruleAction_unlockDoorLock", action_doorlock_handler },
    { "ruleAction_setTemperatureCool", action_thermostat_handler },
    { "ruleAction_setTemperatureHeat", action_thermostat_handler },
    { "ruleAction_setTemperatureOff", action_thermostat_handler },
    { "ruleAction_sendEmail", action_notification_handler },
    { "ruleAction_sendSms", action_notification_handler },
    { "ruleAction_sendPushNotif", action_notification_handler},
    { "ruleAction_takePicture", action_picture_handler },
    { "ruleAction_recordVideo", action_video_handler },
    { "ruleAction_playSound", action_playsound_handler },
//    { "ruleAction_", action__handler },
};

static int action2javascript(const uint64_t ruleId,
                             const icrule_action_t* action,
                             cJSON* output_objects,
                             cJSON* nodes_object,
                             cJSON* start_branches)
{
    int i;
    int len = sizeof_array(icrule2sheen);

    for (i = 0; i < len; i++) {
        if (strcmp(action->target, icrule2sheen[i].name) == 0) {
            return icrule2sheen[i].handler(ruleId,
                                           action,
                                           output_objects,
                                           nodes_object,
                                           start_branches);
        }
    }
    errno = ENOTSUP;
    return -1;
}

cJSON* sheens_actions2javascript(const uint64_t ruleId,
                                 icLinkedListIterator* iterator,
                                 cJSON* nodes_object,
                                 cJSON* start_branches)
{
    static const char* emit_js = "if (!('" sheens_event_id_bound_key "' in _.bindings)) {\n"
                                 "  _.bindings['" sheens_event_id_bound_key "'] = null;\n"
                                 "} else if ('" sheens_event_on_demand_required_key "' in _.bindings) {\n"
                                 "  _.bindings['" sheens_event_orig_id_bound_key "'] = _.bindings['" sheens_event_id_bound_key "'];\n"
                                 "  _.bindings['" sheens_event_id_bound_key "'] = 0;\n"
                                 "}\n"
                                 "_.out(%s);\n"
                                 "return _.bindings;\n";

    cJSON* json = NULL;
    cJSON* output_objects = cJSON_CreateArray();

    char *js, *action_js;

    /* Build a list of all actions into JavaScript Objects */
    while (linkedListIteratorHasNext(iterator)) {
        icrule_action_t* action = linkedListIteratorGetNext(iterator);

        if (action) {
            int ret;

            /* The action2javascript routine will set errno for us. */
            ret = action2javascript(ruleId, action, output_objects, nodes_object, start_branches);
            if (ret < 0) {
                linkedListIteratorDestroy(iterator);
                cJSON_Delete(output_objects);

                return NULL;
            }
        }
    }
    linkedListIteratorDestroy(iterator);

    action_js = cJSON_PrintBuffered(output_objects, 2148, false);
    cJSON_Delete(output_objects);

    if (action_js == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    js = malloc(strlen(action_js) + strlen(emit_js) + 1);
    if (js == NULL) {
        free(action_js);

        errno = ENOMEM;
        return NULL;
    }

    sprintf(js, emit_js, action_js);

    json = cJSON_CreateString(js);

    free(action_js);
    free(js);

    return json;
}
