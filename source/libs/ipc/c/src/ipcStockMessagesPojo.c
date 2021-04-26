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
 * ipcStockMessagesPojo.c
 *
 *
 * Define the standard/stock set of IPC messages that
 * ALL services should handle.  Over time this may grow,
 * but serves the purpose of creating well-known IPC
 * messages without linking in superfluous API libraries
 * or relying on convention.
 *
 * Author: jelderton - 3/2/16
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "icIpc/ipcStockMessagesPojo.h"

/*
 * 'create' function for serviceStatusPojo
 */
serviceStatusPojo *create_serviceStatusPojo()
{
    // allocate wrapper
    serviceStatusPojo *retVal = (serviceStatusPojo *)malloc(sizeof(serviceStatusPojo));
    memset(retVal, 0, sizeof(serviceStatusPojo));

    // create dynamic data-types (arrays & maps)
    retVal->statusMap = hashMapCreate();

    return retVal;
}

/*
 * 'destroy' function for serviceStatusPojo
 */
void destroy_serviceStatusPojo(serviceStatusPojo *pojo)
{
    if (pojo == NULL)
    {
        return;
    }

    // destroy dynamic data-structures (arrays & maps)
    hashMapDestroy(pojo->statusMap, NULL);

    // now the wrapper
    free(pojo);
}

/*
 * extract value for 'key' of type 'char' from the hash map 'map' contained in 'serviceStatusPojo'
 */
char *get_string_from_serviceStatusPojo(serviceStatusPojo *pojo, const char *key)
{
    if (pojo != NULL && key != NULL)
    {
        char *val = hashMapGet(pojo->statusMap, (void *)key, (uint16_t)strlen(key));
        if (val != NULL)
        {
            return val;
        }
    }
    return NULL;
}

/*
 * set/add value for 'key' of type 'char' into the hash map 'map' contained in 'serviceStatusPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
void put_string_in_serviceStatusPojo(serviceStatusPojo *pojo, const char *key, char *value)
{
    if (pojo != NULL && key != NULL && value != NULL)
    {
        // create copy of 'key' and the supplied 'value' into the map.
        // they will be released when our 'destroy_serviceStatusPojo' is called
        //
        uint16_t keyLen = (uint16_t)strlen(key);
        hashMapPut(pojo->statusMap, (void *)strdup(key), keyLen, strdup(value));
    }
}

/*
 * set/add value for 'key' of type 'int32_t' into the hash map 'stats' contained in 'serviceStatusPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
void put_int_in_serviceStatusPojo(serviceStatusPojo *pojo, const char *key, int32_t value)
{
    if (pojo != NULL && key != NULL)
    {
        // create copy of 'key' and 'value', then place into the map.
        // they will be released when our 'destroy_serviceStatusPojo' is called
        //
        uint16_t keyLen = (uint16_t)strlen(key);
        char buffer[128];
        sprintf(buffer, "%"PRIi32, value);

        hashMapPut(pojo->statusMap, (void *)strdup(key), keyLen, strdup(buffer));
    }
}

/*
 * 'encode to JSON' function for serviceStatusPojo
 */
cJSON *encode_serviceStatusPojo_toJSON(serviceStatusPojo *pojo)
{
    // create new JSON container object
    cJSON *root = cJSON_CreateObject();

    // create JSON object for serviceStatusPojo
    cJSON *serviceStatusPojo_json = NULL;
    serviceStatusPojo_json = cJSON_CreateObject();
    if (pojo->statusMap != NULL)
    {
        // need 2 structures to allow proper decoding later on
        //  1 - values
        //  2 - keys
        cJSON *mapVals_json = cJSON_CreateObject();
        cJSON *mapKeys_json = cJSON_CreateArray();

        // add each value & key
        icHashMapIterator *map_loop = hashMapIteratorCreate(pojo->statusMap);
        while (hashMapIteratorHasNext(map_loop) == true)
        {
            void *mapKey;
            void *mapVal;
            uint16_t mapKeyLen = 0;

            // get the key & value
            //
            hashMapIteratorGetNext(map_loop, &mapKey, &mapKeyLen, &mapVal);
            if (mapKey == NULL || mapVal == NULL)
            {
                continue;
            }

            // assume string, since thats all we have get/set functions for
            //
            char *c = (char *)mapVal;
            cJSON_AddStringToObject(mapVals_json, mapKey, c);
            cJSON_AddItemToArray(mapKeys_json, cJSON_CreateString(mapKey));
        }
        hashMapIteratorDestroy(map_loop);

        // add both containers to the parent
        //
        cJSON_AddItemToObject(serviceStatusPojo_json, "mapVals", mapVals_json);
        cJSON_AddItemToObject(serviceStatusPojo_json, "mapKeys", mapKeys_json);
    }
    cJSON_AddItemToObject(root, "serviceStatusPojo", serviceStatusPojo_json);

    return root;
}

/*
 * 'decode from JSON' function for serviceStatusPojo
 */
int decode_serviceStatusPojo_fromJSON(serviceStatusPojo *pojo, cJSON *buffer)
{
    int rc = -1;
    cJSON *item = NULL;
    if (buffer != NULL)
    {
        // extract JSON object for serviceStatusPojo
        cJSON *serviceStatusPojo_json = NULL;
        serviceStatusPojo_json = cJSON_GetObjectItem(buffer, (const char *)"serviceStatusPojo");
        if (serviceStatusPojo_json != NULL)
        {
            // get the 2 structures added during encoding:
            //  1 - values
            //  2 - keys
            cJSON *mapVals_json = cJSON_GetObjectItem(serviceStatusPojo_json, "mapVals");
            cJSON *mapKeys_json = cJSON_GetObjectItem(serviceStatusPojo_json, "mapKeys");
            if (mapVals_json != NULL && mapKeys_json != NULL)
            {
                // loop through keys to extract values and types
                //
                int i;
                int size = cJSON_GetArraySize(mapKeys_json);
                for (i = 0 ; i < size ; i++)
                {
                    // get the next key
                    //
                    cJSON *key = cJSON_GetArrayItem(mapKeys_json, i);
                    if (key == NULL || key->valuestring == NULL)
                    {
                        continue;
                    }

                    // pull it's value
                    //
                    item = cJSON_GetObjectItem(mapVals_json, key->valuestring);
                    if (item == NULL)
                    {
                        continue;
                    }

                    // assume string, since that's all we have get/set functions for
                    //
                    put_string_in_serviceStatusPojo(pojo, key->valuestring, item->valuestring);
                }
            }
        }
    }

    return rc;
}

/*
 * 'create' function for runtimeStatsPojo
 */
runtimeStatsPojo *create_runtimeStatsPojo()
{
    // allocate wrapper
    runtimeStatsPojo *retVal = (runtimeStatsPojo *)malloc(sizeof(runtimeStatsPojo));
    memset(retVal, 0, sizeof(runtimeStatsPojo));

    // create dynamic data-types (arrays & maps)
    retVal->valuesMap = hashMapCreate();
    retVal->typesMap = hashMapCreate();

    return retVal;
}

/*
 * 'destroy' function for runtimeStatsPojo
 */
void destroy_runtimeStatsPojo(runtimeStatsPojo *pojo)
{
    if (pojo == NULL)
    {
        return;
    }

    // destroy dynamic data-structures (arrays & maps)
    // destroy both hash-maps along with all keys/values
    hashMapDestroy(pojo->valuesMap, NULL);
    hashMapDestroy(pojo->typesMap, NULL);

    // now the wrapper
    free(pojo);
}

/*
 * return number of items in the hash map 'map' contained in 'runtimeStatsPojo'
 */
uint16_t get_count_of_runtimeStatsPojo(runtimeStatsPojo *pojo)
{
    return hashMapCount(pojo->valuesMap);
}

/*
 * return the 'type' of data contained for a key.  needed in situations where
 * the data is unknown, and need way to determine which "get" function to use.
 *
 * returns one of: 'string', 'date', 'int', 'long', NULL
 * caller should NOT remove the memory returned.
 */
char *get_valueType_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key)
{
    // find corresponding 'type'
    //
    return (char *)hashMapGet(pojo->typesMap, (void *)key, (uint16_t)strlen(key));
}

/*
 * extract value for 'key' of type 'char' from the hash map 'map' contained in 'runtimeStatsPojo'
 */
char *get_string_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key)
{
    if (pojo != NULL && key != NULL)
    {
        char *val = hashMapGet(pojo->valuesMap, (void *)key, (uint16_t)strlen(key));
        if (val != NULL)
        {
            return val;
        }
    }
    return NULL;
}

/*
 * extract value for 'key' of type 'uint64_t' from the hash map 'stats' contained in 'runtimeStatsPojo'
 */
uint64_t get_date_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key)
{
    if (pojo != NULL && key != NULL)
    {
        uint64_t *val = hashMapGet(pojo->valuesMap, (void *)key, (uint16_t)strlen(key));
        if (val != NULL)
        {
            return *val;
        }
    }
    return (uint64_t)0;
}


/*
 * extract value for 'key' of type 'int32_t' from the hash map 'stats' contained in 'runtimeStatsPojo'
 */
int32_t get_int_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key)
{
    if (pojo != NULL && key != NULL)
    {
        int32_t *val = hashMapGet(pojo->valuesMap, (void *)key, (uint16_t)strlen(key));
        if (val != NULL)
        {
            return *val;
        }
    }
    return (int32_t)-1;
}


/*
 * extract value for 'key' of type 'int64_t' from the hash map 'stats' contained in 'runtimeStatsPojo'
 */
int64_t get_long_from_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key)
{
    if (pojo != NULL && key != NULL)
    {
        int64_t *val = hashMapGet(pojo->valuesMap, (void *)key, (uint16_t)strlen(key));
        if (val != NULL)
        {
            return *val;
        }
    }
    return (int64_t)-1;
}

/*
 * set/add value for 'key' of type 'char' into the hash map 'map' contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
void put_string_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, char *value)
{
    if (pojo != NULL && key != NULL && value != NULL)
    {
        // create copy of 'key' and the supplied 'value' into the map.
        // they will be released when our 'destroy_runtimeStatsPojo' is called
        //
        uint16_t keyLen = (uint16_t)strlen(key);

        hashMapPut(pojo->valuesMap, (void *)strdup(key), keyLen, strdup(value));
        hashMapPut(pojo->typesMap, (void *)strdup(key), keyLen, strdup("string"));
    }
}

/*
 * set/add value for 'key' of type 'uint64_t' into the hash map 'stats' contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
void put_date_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, uint64_t value)
{
    if (pojo != NULL && key != NULL)
    {
        // create copy of 'key' and 'value', then place into the map.
        // they will be released when our 'destroy_runtimeStatsPojo' is called
        //
        uint16_t keyLen = (uint16_t)strlen(key);
        uint64_t *valCopy = (uint64_t *)malloc(sizeof(uint64_t));
        *valCopy = value;

        hashMapPut(pojo->valuesMap, (void *)strdup(key), keyLen, valCopy);
        hashMapPut(pojo->typesMap, (void *)strdup(key), keyLen, strdup("date"));
    }
}

/*
 * set/add value for 'key' of type 'int32_t' into the hash map 'stats' contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
void put_int_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, int32_t value)
{
    if (pojo != NULL && key != NULL)
    {
        // create copy of 'key' and 'value', then place into the map.
        // they will be released when our 'destroy_runtimeStatsPojo' is called
        //
        uint16_t keyLen = (uint16_t)strlen(key);
        int32_t *valCopy = (int32_t *)malloc(sizeof(int32_t));
        *valCopy = value;

        hashMapPut(pojo->valuesMap, (void *)strdup(key), keyLen, valCopy);
        hashMapPut(pojo->typesMap, (void *)strdup(key), keyLen, strdup("int"));
    }
}

/*
 * set/add value for 'key' of type 'int64_t' into the hash map 'stats' contained in 'runtimeStatsPojo
 * NOTE: will clone key & value to keep memory management of internal icHashMap simple
 */
void put_long_in_runtimeStatsPojo(runtimeStatsPojo *pojo, const char *key, int64_t value)
{
    if (pojo != NULL && key != NULL)
    {
        // create copy of 'key' and 'value', then place into the map.
        // they will be released when our 'destroy_runtimeStatsPojo' is called
        //
        uint16_t keyLen = (uint16_t)strlen(key);
        int64_t *valCopy = (int64_t *)malloc(sizeof(int64_t));
        *valCopy = value;

        hashMapPut(pojo->valuesMap, (void *)strdup(key), keyLen, valCopy);
        hashMapPut(pojo->typesMap, (void *)strdup(key), keyLen, strdup("long"));
    }
}

/*
 * 'encode to JSON' function for runtimeStatsPojo
 */
cJSON *encode_runtimeStatsPojo_toJSON(runtimeStatsPojo *pojo)
{
    // create new JSON container object
    cJSON *root = cJSON_CreateObject();

    // create JSON object for runtimeStatsPojo
    cJSON *runtimeStatsPojo_json = NULL;
    runtimeStatsPojo_json = cJSON_CreateObject();
    if (pojo->serviceName != NULL)
    {
        cJSON_AddStringToObject(runtimeStatsPojo_json, "serviceName", pojo->serviceName);
    }
    cJSON_AddNumberToObject(runtimeStatsPojo_json, "collectionTime", (double)pojo->collectionTime);
    if (pojo->valuesMap != NULL && pojo->typesMap != NULL)
    {
        // need 3 structures to allow proper decoding later on
        //  1 - values
        //  2 - types
        //  3 - keys
        cJSON *mapVals_json = cJSON_CreateObject();
        cJSON *mapTypes_json = cJSON_CreateObject();
        cJSON *mapKeys_json = cJSON_CreateArray();

        // add each value & type
        icHashMapIterator *map_loop = hashMapIteratorCreate(pojo->valuesMap);
        while (hashMapIteratorHasNext(map_loop) == true)
        {
            void *mapKey;
            void *mapVal;
            uint16_t mapKeyLen = 0;

            // get the key & value
            //
            hashMapIteratorGetNext(map_loop, &mapKey, &mapKeyLen, &mapVal);
            if (mapKey == NULL || mapVal == NULL)
            {
                continue;
            }

            // find corresponding 'type'
            //
            char *kind = (char *)hashMapGet(pojo->typesMap, mapKey, mapKeyLen);
            if (kind == NULL)
            {
                continue;
            }

            // convert for each possible 'type'
            if (strcmp(kind, "string") == 0)
            {
                char *c = (char *)mapVal;
                cJSON_AddStringToObject(mapVals_json, mapKey, c);
            }
            else if (strcmp(kind, "date") == 0)
            {
                uint64_t *t = (uint64_t *)mapVal;
                cJSON_AddNumberToObject(mapVals_json, mapKey, *t);
            }
            else if (strcmp(kind, "int") == 0)
            {
                int32_t *n = (int32_t *)mapVal;
                cJSON_AddNumberToObject(mapVals_json, mapKey, *n);
            }
            else if (strcmp(kind, "long") == 0)
            {
                int64_t *n = (int64_t *)mapVal;
                cJSON_AddNumberToObject(mapVals_json, mapKey, *n);
            }

            // save the key & type for decode
            //
            cJSON_AddStringToObject(mapTypes_json, mapKey, kind);
            cJSON_AddItemToArray(mapKeys_json, cJSON_CreateString(mapKey));
        }
        hashMapIteratorDestroy(map_loop);

        // add all 3 containers to the parent
        //
        cJSON_AddItemToObject(runtimeStatsPojo_json, "mapVals", mapVals_json);
        cJSON_AddItemToObject(runtimeStatsPojo_json, "mapTypes", mapTypes_json);
        cJSON_AddItemToObject(runtimeStatsPojo_json, "mapKeys", mapKeys_json);
    }
    cJSON_AddItemToObject(root, "runtimeStatsPojo", runtimeStatsPojo_json);

    return root;
}

/*
 * 'decode from JSON' function for runtimeStatsPojo
 */
int decode_runtimeStatsPojo_fromJSON(runtimeStatsPojo *pojo, cJSON *buffer)
{
    int rc = -1;
    cJSON *item = NULL;
    if (buffer != NULL)
    {
        // extract JSON object for runtimeStatsPojo
        cJSON *runtimeStatsPojo_json = NULL;
        runtimeStatsPojo_json = cJSON_GetObjectItem(buffer, (const char *)"runtimeStatsPojo");
        if (runtimeStatsPojo_json != NULL)
        {
            item = cJSON_GetObjectItem(runtimeStatsPojo_json, "serviceName");
            if (item != NULL && item->valuestring != NULL)
            {
                pojo->serviceName = strdup(item->valuestring);
                rc = 0;
            }
            item = cJSON_GetObjectItem(runtimeStatsPojo_json, "collectionTime");
            if (item != NULL)
            {
                pojo->collectionTime = (uint64_t)item->valuedouble;
                rc = 0;
            }

            // get the 3 structures added during encoding:
            //  1 - values
            //  2 - types
            //  3 - keys
            cJSON *mapVals_json = cJSON_GetObjectItem(runtimeStatsPojo_json, "mapVals");
            cJSON *mapTypes_json = cJSON_GetObjectItem(runtimeStatsPojo_json, "mapTypes");
            cJSON *mapKeys_json = cJSON_GetObjectItem(runtimeStatsPojo_json, "mapKeys");
            if (mapVals_json != NULL && mapTypes_json != NULL && mapKeys_json != NULL)
            {
                // loop through keys to extract values and types
                //
                int i;
                int size = cJSON_GetArraySize(mapKeys_json);
                for (i = 0 ; i < size ; i++)
                {
                    // get the next key
                    //
                    cJSON *key = cJSON_GetArrayItem(mapKeys_json, i);
                    if (key == NULL || key->valuestring == NULL)
                    {
                        continue;
                    }

                    // pull it's type & value
                    //
                    cJSON *type = cJSON_GetObjectItem(mapTypes_json, key->valuestring);
                    if (type == NULL || type->valuestring == NULL)
                    {
                        continue;
                    }
                    item = cJSON_GetObjectItem(mapVals_json, key->valuestring);
                    if (item == NULL)
                    {
                        continue;
                    }


                    // convert for each possible 'type'
                    if (strcmp(type->valuestring, "string") == 0)
                    {
                        put_string_in_runtimeStatsPojo(pojo, key->valuestring, item->valuestring);
                    }
                    else if (strcmp(type->valuestring, "date") == 0)
                    {
                        put_date_in_runtimeStatsPojo(pojo, key->valuestring, (uint64_t)item->valuedouble);
                    }
                    else if (strcmp(type->valuestring, "int") == 0)
                    {
                        put_int_in_runtimeStatsPojo(pojo, key->valuestring, (int32_t)item->valueint);
                    }
                    else if (strcmp(type->valuestring, "long") == 0)
                    {
                        put_long_in_runtimeStatsPojo(pojo, key->valuestring, (int64_t)item->valuedouble);
                    }
                }
            }
        }
    }

    return rc;
}

/*
 * 'create' function for configRestoredInput
 */
configRestoredInput *create_configRestoredInput()
{
    // allocate wrapper
    //
    configRestoredInput *retVal = (configRestoredInput *)malloc(sizeof(configRestoredInput));
    memset(retVal, 0, sizeof(configRestoredInput));

    return retVal;
}

/*
 * 'destroy' function for configRestoredInput
 */
void destroy_configRestoredInput(configRestoredInput *pojo)
{
    if (pojo == NULL)
    {
        return;
    }

    // destroy dynamic data-structures within the object
    //
    if (pojo->tempRestoreDir != NULL)
    {
        free(pojo->tempRestoreDir);
    }
    if (pojo->dynamicConfigPath != NULL)
    {
        free(pojo->dynamicConfigPath);
    }

    // now the wrapper
    //
    free(pojo);
}

/*
 * 'encode to JSON' function for configRestoredInput
 */
cJSON *encode_configRestoredInput_toJSON(configRestoredInput *pojo)
{
    // create new JSON container object
    //
    cJSON *root = cJSON_CreateObject();

    // create JSON object for configRestoredInput
    //
    cJSON *configRestoredInput_json = NULL;
    configRestoredInput_json = cJSON_CreateObject();
    if (pojo->tempRestoreDir != NULL)
    {
        cJSON_AddStringToObject(configRestoredInput_json, "tempRestoreDir", pojo->tempRestoreDir);
    }
    if (pojo->dynamicConfigPath != NULL)
    {
        cJSON_AddStringToObject(configRestoredInput_json, "dynamicConfigPath", pojo->dynamicConfigPath);
    }
    cJSON_AddItemToObject(root, "configRestoredInput", configRestoredInput_json);

    return root;
}

/*
 * 'decode from JSON' function for configRestoredInput
 */
int decode_configRestoredInput_fromJSON(configRestoredInput *pojo, cJSON *buffer)
{
    int rc = -1;
    cJSON *item = NULL;
    if (buffer != NULL)
    {
        // extract JSON object for configRestoredInput
        //
        cJSON *configRestoredInput_json = NULL;
        configRestoredInput_json = cJSON_GetObjectItem(buffer, (const char *)"configRestoredInput");
        if (configRestoredInput_json != NULL)
        {
            item = cJSON_GetObjectItem(configRestoredInput_json, "tempRestoreDir");
            if (item != NULL && item->valuestring != NULL)
            {
                pojo->tempRestoreDir = strdup(item->valuestring);
                rc = 0;
            }
            item = cJSON_GetObjectItem(configRestoredInput_json, "dynamicConfigPath");
            if (item != NULL && item->valuestring != NULL)
            {
                pojo->dynamicConfigPath = strdup(item->valuestring);
                rc = 0;
            }
        }
    }

    return rc;
}

/*
 * 'create' function for configRestoredOutput
 */
configRestoredOutput *create_configRestoredOutput()
{
    // allocate wrapper
    configRestoredOutput *retVal = calloc(1, sizeof(configRestoredOutput));
    pojo_init((Pojo *) retVal,
              sizeof(configRestoredOutput),
              (pojoDestructor) destroy_configRestoredOutput,
              (pojoCloneFunc) clone_configRestoredOutput);

    // create dynamic data-types (arrays & maps)


    return retVal;
}


/*
 * 'destroy' function for configRestoredOutput
 */
void destroy_configRestoredOutput(configRestoredOutput *pojo)
{
    if (pojo == NULL)
    {
        return;
    }

    // destroy dynamic data-structures (arrays & maps)


    // now the wrapper
    pojo_free((Pojo *) pojo);
    free(pojo);
}

/*
 * create a deep clone of a configRestoredOutput object.
 * should be release via destroy_configRestoredOutput()
 */
configRestoredOutput *clone_configRestoredOutput(configRestoredOutput *src)
{
    if (src == NULL)
    {
        return NULL;
    }

    configRestoredOutput *copy = create_configRestoredOutput();
    cJSON *json = encode_configRestoredOutput_toJSON(src);
    decode_configRestoredOutput_fromJSON(copy, json);
    cJSON_Delete(json);
    return copy;
}

/*
 * 'encode to JSON' function for configRestoredOutput
 */
cJSON *encode_configRestoredOutput_toJSON(configRestoredOutput *pojo)
{
    // create new JSON container object
    cJSON *root = cJSON_CreateObject();


    // create JSON object for configRestoredOutput
    cJSON *configRestoredOutput_json = NULL;
    configRestoredOutput_json = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(configRestoredOutput_json, "action", cJSON_CreateNumber(pojo->action));
    cJSON_AddItemToObjectCS(root, "configRestoredOutput", configRestoredOutput_json);


    return root;
}


/*
 * 'decode from JSON' function for configRestoredOutput
 */
int decode_configRestoredOutput_fromJSON(configRestoredOutput *pojo, cJSON *buffer)
{
    int rc = -1;
    cJSON *item = NULL;
    if (buffer != NULL)
    {

        // extract JSON object for configRestoredOutput
        cJSON *configRestoredOutput_json = NULL;
        configRestoredOutput_json = cJSON_GetObjectItem(buffer, (const char *)"configRestoredOutput");
        if (configRestoredOutput_json != NULL)
        {
            item = cJSON_GetObjectItem(configRestoredOutput_json, "action");
            if (item != NULL)
            {
                pojo->action = (configRestoredAction)item->valueint;
                rc = 0;
            }
        }

    }

    return rc;
}
