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
// Created by gfaulk200 on 3/12/20.
//
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icLog/logging.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/paths.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>
#include <jsonHelper/jsonHelper.h>

#include "propertyTypeDefs_internal.h"
#include "propertyTypeDefsParser.h"
#include "propertyTypeDefinitions.h"

#define TYPE_DEFS_FILE "propertyTypeDefs.json"
#define PROPERTY_NAME_KEY "propertyName"
#define PROPERTY_TYPE_KEY "propertyType"
#define MIN_VALUE_KEY     "minValue"
#define MAX_VALUE_KEY     "maxValue"
#define ENUM_VALUES_KEY   "enumValues"

// hate hard-coding this here, but we do it elsewhere, so...
#define MAX_PATHNAME_LEN 1024

/**
 * setup the restrictions for an unsigned int data type
 */
static bool setupUnSignedMinMax(propertyTypeDef *propTypeDef, uint64_t min, uint64_t max)
{
    if (min > max)
    {
        icLogWarn(LOG_TAG,"%s: minimum value %"PRIu64" is greater than the maximum value %"PRIu64"; for %s; ignoring",
                __FUNCTION__,min,max,propTypeDef->propertyName);
        return false;
    }
    propTypeDef->restrictions = (typeRestrictions *)calloc(1,sizeof(typeRestrictions));
    propTypeDef->restrictions->uintLimits.minValue = min;
    propTypeDef->restrictions->uintLimits.maxValue = max;
    return true;
}

/**
 * setup the restrictions for a signed int property type
 */
static bool setupSignedMinMax(propertyTypeDef *propTypeDef, int64_t min, int64_t max)
{
    if (min > max)
    {
        icLogWarn(LOG_TAG,"%s: minimum value %"PRIi64" is greater than the maximum value %"PRIi64"; for %s; ignoring",
                  __FUNCTION__,min,max,propTypeDef->propertyName);
        return false;
    }
    propTypeDef->restrictions = (typeRestrictions *)calloc(1,sizeof(typeRestrictions));
    propTypeDef->restrictions->intLimits.minValue = min;
    propTypeDef->restrictions->intLimits.maxValue = max;
    return true;
}

static void setupEnumRestrictions(propertyTypeDef *propTypeDef, cJSON *enumArray)
{
    if (enumArray->type == cJSON_Array)
    {
        propTypeDef->restrictions = (typeRestrictions *)calloc(1,sizeof(typeRestrictions));
        propTypeDef->restrictions->enumValues = linkedListCreate();
        int numValues = cJSON_GetArraySize(enumArray);
        for (int cnt = 0; cnt < numValues; cnt++)
        {
            cJSON *valueJson = cJSON_GetArrayItem(enumArray,cnt);
            if (valueJson->type == cJSON_String)
            {
                char *value = valueJson->valuestring;
                if (value != NULL)
                {
                    linkedListAppend(propTypeDef->restrictions->enumValues,strdup(value));
                }
            }
            else
            {
                icLogWarn(LOG_TAG,"%s: found enum value of type %d which is not supported",__FUNCTION__,valueJson->type);
            }
        }
    }
    else
    {
        icLogWarn(LOG_TAG,"%s: enum values list was not an array",__FUNCTION__);
    }
}

/**
 * add in a new property type from the definitions file
 */
static bool addPropertyType(char *propertyName, propertyDataType dataType, cJSON *defObject, icHashMap *propertyTypesMap)
{
    bool retval = true;
    propertyTypeDef *propTypeDef = (propertyTypeDef *)calloc(1,sizeof(propertyTypeDef));
    // we don't own this memory, make a copy
    propTypeDef->propertyName = strdup(propertyName);
    propTypeDef->propertyType = dataType;
    AUTO_CLEAN(free_generic__auto) char *minValueStr = getCJSONString(defObject,MIN_VALUE_KEY);
    AUTO_CLEAN(free_generic__auto) char *maxValueStr = getCJSONString(defObject,MAX_VALUE_KEY);
    switch (dataType)
    {
        case PROPERTY_TYPE_UINT64:
        {
            uint64_t minValue = 0;
            uint64_t maxValue = UINT64_MAX;
            if (minValueStr != NULL)
            {
                if (stringToUint64(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to a uint64",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToUint64(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to a uint64",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupUnSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_UINT32:
        {
            uint32_t minValue = 0;
            uint32_t maxValue = UINT32_MAX;
            if (minValueStr != NULL)
            {
                if (stringToUint32(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to a uint32",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToUint32(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to a uint32",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupUnSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_UINT16:
        {
            uint16_t minValue = 0;
            uint16_t maxValue = UINT16_MAX;
            if (minValueStr != NULL)
            {
                if (stringToUint16(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to a uint16",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToUint16(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to a uint16",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupUnSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_UINT8:
        {
            uint8_t minValue = 0;
            uint8_t maxValue = UINT8_MAX;
            if (minValueStr != NULL)
            {
                if (stringToUint8(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to a uint8",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToUint8(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to a uint8",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupUnSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_INT64:
        {
            int64_t minValue = INT64_MIN;
            int64_t maxValue = INT64_MAX;
            if (minValueStr != NULL)
            {
                if (stringToInt64(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to an int64",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToInt64(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to an int64",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_INT32:
        {
            int32_t minValue = INT32_MIN;
            int32_t maxValue = INT32_MAX;
            if (minValueStr != NULL)
            {
                if (stringToInt32(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to an int32 value",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToInt32(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to an int32 value",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_INT16:
        {
            int16_t minValue = INT16_MIN;
            int16_t maxValue = INT16_MAX;
            if (minValueStr != NULL)
            {
                if (stringToInt16(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to an int16 value",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToInt16(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to an int16 value",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_INT8:
        {
            int8_t minValue = INT8_MIN;
            int8_t maxValue = INT8_MAX;
            if (minValueStr != NULL)
            {
                if (stringToInt8(minValueStr,&minValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert minValue %s to an int8 value",__FUNCTION__,minValueStr);
                    retval = false;
                }
            }
            if (maxValueStr != NULL)
            {
                if (stringToInt8(maxValueStr,&maxValue) == false)
                {
                    icLogWarn(LOG_TAG,"%s: unable to convert maxValue %s to an int8 value",__FUNCTION__,maxValueStr);
                    retval = false;
                }
            }
            if (retval == true)
            {
                retval = setupSignedMinMax(propTypeDef, minValue, maxValue);
            }
            break;
        }

        case PROPERTY_TYPE_ENUM:
        {
            cJSON *valuesArray = cJSON_GetObjectItem(defObject,ENUM_VALUES_KEY);
            setupEnumRestrictions(propTypeDef,valuesArray);
            break;
        }

        case PROPERTY_TYPE_STRING:
        case PROPERTY_TYPE_BOOLEAN:
        case PROPERTY_TYPE_URL:
        case PROPERTY_TYPE_URLSET:
        {
            break;
        }

        default:
        {
            icLogWarn(LOG_TAG,"Found unexpected property type %d",dataType);
            free(propTypeDef);
            return false;
        }
    }
    if (retval == true)
    {
        // we don't own propertyName, make a copy
        hashMapPut(propertyTypesMap, strdup(propertyName), (uint16_t) strlen(propertyName), propTypeDef);
    }
    else
    {
        free(propTypeDef);
    }
    return retval;
}

/**
 * convert the supplied property type to our internal representation
 */
static propertyDataType convertTypeNameToType(char *typeName)
{
    if (stringCompare(typeName,TYPEDEF_TYPE_NAME_STRING,false) == 0)
    {
        return PROPERTY_TYPE_STRING;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_BOOLEAN, false) == 0)
    {
        return PROPERTY_TYPE_BOOLEAN;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_UINT64, false) == 0)
    {
        return PROPERTY_TYPE_UINT64;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_UINT32, false) == 0)
    {
        return PROPERTY_TYPE_UINT32;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_UINT16, false) == 0)
    {
        return PROPERTY_TYPE_UINT16;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_UINT8, false) == 0)
    {
        return PROPERTY_TYPE_UINT8;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_INT64, false) == 0)
    {
        return PROPERTY_TYPE_INT64;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_INT32, false)  == 0)
    {
        return PROPERTY_TYPE_INT32;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_INT16, false) == 0)
    {
        return PROPERTY_TYPE_INT16;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_INT8, false) == 0)
    {
        return PROPERTY_TYPE_INT8;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_ENUM, false) == 0)
    {
        return PROPERTY_TYPE_ENUM;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_URL, false) == 0)
    {
        return PROPERTY_TYPE_URL;
    }
    else if (stringCompare(typeName,TYPEDEF_TYPE_NAME_URLSET,false) == 0)
    {
        return PROPERTY_TYPE_URLSET;
    }
    else
    {
        icLogWarn(LOG_TAG,"Unable to convert %s to a known property type",typeName);
        return PROPERTY_TYPE_UNKNOWN;
    }
}

/**
 * process an individual typedef from JSON
 */
static bool processTypeDef(cJSON *typeDefJson, int innerArrayNumber, int itemNumber, icHashMap *propertyTypesDefinition)
{
    AUTO_CLEAN(free_generic__auto) char *propertyName = getCJSONString(typeDefJson,PROPERTY_NAME_KEY);
    AUTO_CLEAN(free_generic__auto) char *propertyType = getCJSONString(typeDefJson,PROPERTY_TYPE_KEY);
    if ((propertyName == NULL) || (propertyType == NULL))
    {
        icLogError(LOG_TAG,"item %d from inner array %d is not a properly formed property type definition",itemNumber,innerArrayNumber);
        return false;
    }
    propertyDataType givenType = convertTypeNameToType(propertyType);
    if (givenType != PROPERTY_TYPE_UNKNOWN)
    {
        return addPropertyType(propertyName,givenType,typeDefJson,propertyTypesDefinition);
    }
    return false;
}

/**
 * process the inner JSON array from the config
 * @param innerArray
 */
static bool processInnerJSONArray(cJSON *innerArray, int innerArrayNumber, icHashMap *propertyTypesDefinition)
{
    // first, make sure it's an array
    if (innerArray->type != cJSON_Array)
    {
        icLogWarn(LOG_TAG,"Inner object that should be a JSON array is not");
        return false;
    }
    int cnt = 0;
    cJSON *jsonTypeDef = NULL;
    cJSON_ArrayForEach(jsonTypeDef,innerArray)
    {

        if (jsonTypeDef->type != cJSON_Object)
        {
            icLogError(LOG_TAG,"Item %d of inner array %d is not a JSON object",cnt,innerArrayNumber);
            return false;
        }
        else
        {
            if (processTypeDef(jsonTypeDef,innerArrayNumber,cnt,propertyTypesDefinition) == false)
            {
                return false;
            }
        }
        cnt++;
    }
    return true;
}

/**
 * parse the propertyTypeDefs.json file
 */
icHashMap *parsePropertyTypesDefinition()
{
    int rc;
    AUTO_CLEAN(free_generic__auto) char *staticCfgDir = getStaticConfigPath();
    struct stat sb;
    char typeDefsPath[MAX_PATHNAME_LEN];
    icHashMap *retval = hashMapCreate();

    // make sure the file exists
    sprintf(typeDefsPath,"%s/%s",staticCfgDir,TYPE_DEFS_FILE);
    rc = stat(typeDefsPath,&sb);
    if (rc != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG,"Unable to stat %s: %s",typeDefsPath,errStr);
        return retval;
    }

    // read the file
    AUTO_CLEAN(free_generic__auto) char *contentsBuf = readFileContents(typeDefsPath);
    if (contentsBuf == NULL)
    {
        icLogError(LOG_TAG,"Unable to read property types definitions from %s",typeDefsPath);
        return retval;
    }

    // parse it as JSON
    AUTO_CLEAN(cJSON_Delete__auto) cJSON *typeDefJson = cJSON_Parse(contentsBuf);
    if (typeDefJson == NULL)
    {
        const char *jsonError = cJSON_GetErrorPtr();
        if (jsonError != NULL)
        {
            icLogError(LOG_TAG, "Unable to parse %s as JSON: %s", typeDefsPath, jsonError);
        }
        else
        {
            icLogError(LOG_TAG, "Unable to parse %s as JSON", typeDefsPath);
        }
        return retval;
    }

    // should be one massive JSON array
    if (typeDefJson->type != cJSON_Array)
    {
        icLogError(LOG_TAG,"Wrong type for topmost JSON object in %s; found %d, should be %d",typeDefsPath,typeDefJson->type,cJSON_Array);
        return retval;
    }

    // now make sure it was sane; both in terms of the definitions provided, as well
    // as consistent limits provided
    //
    bool worked = true;
    int cnt = 0;
    cJSON *innerArray = NULL;
    cJSON_ArrayForEach(innerArray,typeDefJson)
    {
        worked = processInnerJSONArray(innerArray,cnt++,retval);
        if (worked == false)
        {
            icLogError(LOG_TAG,"Errors found in property type definitions");
            break;
        }
    }
    // if this couldn't be parsed, then throw it all away; this lets the unit tests
    // discover type definition problems instead of runtime.
    if (worked == false)
    {
        hashMapDestroy(retval,freePropTypesEntry);
        retval = hashMapCreate();
    }
    uint16_t mapSize = hashMapCount(retval);
    icLogInfo(LOG_TAG,"Found %d properties with predefined types",mapSize);
    return retval;
}
