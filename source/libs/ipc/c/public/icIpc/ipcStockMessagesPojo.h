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
 * ipcStockMessagesPojo.h
 *
 * Define the standard/stock set of IPC messages that
 * ALL services should handle.  Over time this may grow,
 * but serves the purpose of creating well-known IPC
 * messages without linking in superfluous API libraries
 * or relying on convention.
 *
 * Author: jelderton - 3/2/16
 *-----------------------------------------------*/

#ifndef ZILKER_IPCSTOCKMESSAGES_POJO_H
#define ZILKER_IPCSTOCKMESSAGES_POJO_H

#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <cjson/cJSON.h>

#include <icTypes/icHashMap.h>
#include "icIpc/ipcMessage.h"
#include "icIpc/pojo.h"

/*
 * define the 'serviceStatusPojo', used to contain a group of 'status'
 * strings from a service.  internally stored as string/string key/value pairs.
 */
typedef struct _serviceStatusPojo
{
    icHashMap * statusMap;  // map of string/string key/value pairs
} serviceStatusPojo;

/*
 * 'create' function for serviceStatusPojo
 */
extern serviceStatusPojo *create_serviceStatusPojo();

/*
 * 'destroy' function for serviceStatusPojo
 */
extern void destroy_serviceStatusPojo(serviceStatusPojo *pojo);

/*
 * extract value for 'key' of type 'char' from the hash map 'map' contained in 'serviceStatusPojo'
 */
extern char *get_string_from_serviceStatusPojo(serviceStatusPojo *pojo, const char *key);

/*
 * set/add value for 'key' of type 'char' into the hash map 'map' contained in 'serviceStatusPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
extern void put_string_in_serviceStatusPojo(serviceStatusPojo *pojo, const char *key, char *value);

/*
 * set/add value for 'key' of type 'int32_t' into the hash map 'stats' contained in 'serviceStatusPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
extern void put_int_in_serviceStatusPojo(serviceStatusPojo *pojo, const char *key, int32_t value);

/*
 * 'encode to JSON' function for serviceStatusPojo
 */
extern cJSON *encode_serviceStatusPojo_toJSON(serviceStatusPojo *pojo);

/*
 * 'decode from JSON' function for serviceStatusPojo
 */
extern int decode_serviceStatusPojo_fromJSON(serviceStatusPojo *pojo, cJSON *buffer);



/*
 * define the 'runtimeStatsPojo', used to contain a collection of 'statistics'
 * from a service.  stats are internally stored in a variety of key/value pairs,
 * allowing for number, date, or string value types.
 */
typedef struct _runtimeStatsPojo
{
    char      *serviceName;
    uint64_t  collectionTime;
    icHashMap *valuesMap;  // two internal variables to represent map
    icHashMap *typesMap;
} runtimeStatsPojo;


/*
 * 'create' function for runtimeStatsPojo
 */
extern runtimeStatsPojo *create_runtimeStatsPojo();

/*
 * 'destroy' function for runtimeStatsPojo
 */
extern void destroy_runtimeStatsPojo(runtimeStatsPojo *pojo);

/*
 * return number of items in the hash map 'map' contained in 'runtimeStatsPojo'
 */
extern uint16_t get_count_of_runtimeStatsPojo(runtimeStatsPojo *pojo);

/*
 * return the 'type' of data contained for a key.  needed in situations where
 * the data is unknown, and need way to determine which "get" function to use.
 *
 * returns one of: 'string', 'date', 'int', 'long', NULL
 * caller should NOT remove the memory returned.
 */
extern char *get_valueType_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key);

/*
 * extract value for 'key' of type 'string' from the hash map contained in 'runtimeStatsPojo'
 */
extern char *get_string_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key);

/*
 * extract value for 'key' of type 'uint64_t' from the hash map contained in 'runtimeStatsPojo'
 */
extern uint64_t get_date_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key);

/*
 * extract value for 'key' of type 'int32_t' from the hash map contained in 'runtimeStatsPojo'
 */
extern int32_t get_int_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key);

/*
 * extract value for 'key' of type 'int64_t' from the hash map contained in 'runtimeStatsPojo'
 */
extern int64_t get_long_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key);

/*
 * set/add value for 'key' of type 'char' into the hash map contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
extern void put_string_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, char *value);

/*
 * set/add value for 'key' of type 'time_t' into the hash map contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
extern void put_date_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, uint64_t value);

/*
 * set/add value for 'key' of type 'int32_t' into the hash map contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
extern void put_int_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, int32_t value);

/*
 * set/add value for 'key' of type 'int64_t' into the hash map contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
extern void put_long_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, int64_t value);

/*
 * 'encode to JSON' function for runtimeStatsPojo
 */
extern cJSON *encode_runtimeStatsPojo_toJSON(runtimeStatsPojo *pojo);

/*
 * 'decode from JSON' function for runtimeStatsPojo
 */
extern int decode_runtimeStatsPojo_fromJSON(runtimeStatsPojo *pojo, cJSON *buffer);

/*
 * enum definitions
 */

typedef enum {
    /**
     * The service will perform the config restore
     * asynchronously. The service is required to
     * call back to the restore orchestrator to
     * inform it that the service is finished.
     */
    CONFIG_RESTORED_CALLBACK  = 0,

    /**
     * The service has successfully finished
     * restoring all configuration and is available
     * for use with the new configuration in place.
     */
    CONFIG_RESTORED_COMPLETE  = 1,

    /**
     * The service has successfully finished
     * restoring all configuration, however
     * the service requires a restart in order
     * to be available for use with the new
     * configuration in place.
     */
    CONFIG_RESTORED_RESTART   = 2,

    /**
     * The service failed to restore the
     * configuration files.
     */
    CONFIG_RESTORED_FAILED    = 3
} configRestoredAction;

// NULL terminated array that should correlate to the configRestoredAction enum
static const char *configRestoredActionLabels[] = {
    "CONFIG_RESTORED_CALLBACK", // CONFIG_RESTORED_CALLBACK
    "CONFIG_RESTORED_COMPLETE", // CONFIG_RESTORED_COMPLETE
    "CONFIG_RESTORED_RESTART",  // CONFIG_RESTORED_RESTART
    "CONFIG_RESTORED_FAILED",   // CONFIG_RESTORED_FAILED
    NULL
};

/*
 * input object to the 'configRestored' call (and handle_CONFIG_RESTORED implementation).
 * represents the temporary directory that the old backup was unpacked in, as well as the
 * location of our config path - so it doesn't need to be queried during the restore process.
 */
typedef struct _configRestoredInput {
    char *tempRestoreDir;     // temp directory containing unpacked restore information.  service should use this to locate and parse the old files.
    char *dynamicConfigPath;  // same as getDynamicConfigPath(), but provided here to remove need for asking for the storage location.
} configRestoredInput;

// Inform the configuration system what to do on config restoration completion.
typedef struct _configRestoredOutput {
    Pojo                 base;    // Base object; use pojo.h interface
    configRestoredAction action;
} configRestoredOutput;

/*
 * 'create' function for configRestoredInput
 */
extern configRestoredInput *create_configRestoredInput();

/*
 * 'destroy' function for configRestoredInput
 */
extern void destroy_configRestoredInput(configRestoredInput *pojo);

/*
 * 'encode to JSON' function for configRestoredInput
 */
extern cJSON *encode_configRestoredInput_toJSON(configRestoredInput *pojo);

/*
 * 'decode from JSON' function for configRestoredInput
 */
extern int decode_configRestoredInput_fromJSON(configRestoredInput *pojo, cJSON *buffer);

/*
 * 'create' function for configRestoredOutput
 */
extern configRestoredOutput *create_configRestoredOutput();

/*
 * 'destroy' function for configRestoredOutput
 */
extern void destroy_configRestoredOutput(configRestoredOutput *pojo);

/*
 * create a deep clone of a configRestoredOutput object.
 * should be release via destroy_configRestoredOutput()
 */
extern configRestoredOutput *clone_configRestoredOutput(configRestoredOutput *src);

/*
 * 'encode to JSON' function for configRestoredOutput
 */
extern cJSON *encode_configRestoredOutput_toJSON(configRestoredOutput *pojo);

/*
 * 'decode from JSON' function for configRestoredOutput
 */
extern int decode_configRestoredOutput_fromJSON(configRestoredOutput *pojo, cJSON *buffer);

#endif //ZILKER_IPCSTOCKMESSAGES_POJO_H
