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
// Created by gfaulk200 on 3/9/20.
//
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>

#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icLog/logging.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <icUtil/stringUtils.h>

#include "propertyTypeDefs_internal.h"
#include "propertyTypeDefsParser.h"
#include "propertyTypeDefinitions.h"


static icHashMap *propertyTypesMap = NULL;


/**
 * initialize the property type definitions from the supplied config file
 */
void initPropertyTypeDefs()
{
    propertyTypesMap = parsePropertyTypesDefinition();
}

/**
 * get the count of the number of property type defs found
 */
uint16_t getPropertyTypeDefsCount()
{
    if (propertyTypesMap != NULL)
    {
        return hashMapCount(propertyTypesMap);
    }
    return 0;
}

/**
 * callback to free up a propertyTypesMap entry
 */
void freePropTypesEntry(void *key, void *value)
{
    // first, just free the key
    free(key);
    // now, figure out how to free the type definition
    propertyTypeDef *typeDef = (propertyTypeDef *)value;
    switch (typeDef->propertyType)
    {
        case PROPERTY_TYPE_UINT64:
        case PROPERTY_TYPE_UINT32:
        case PROPERTY_TYPE_UINT16:
        case PROPERTY_TYPE_UINT8:
        case PROPERTY_TYPE_INT64:
        case PROPERTY_TYPE_INT32:
        case PROPERTY_TYPE_INT16:
        case PROPERTY_TYPE_INT8:
        {
            // these will all have simple restrictions
            //
            free(typeDef->restrictions);
            break;
        }

        case PROPERTY_TYPE_ENUM:
        {
            // first, destroy the linked lis
            linkedListDestroy(typeDef->restrictions->enumValues,free);
            free(typeDef->restrictions);
            break;
        }

        case PROPERTY_TYPE_STRING:
        case PROPERTY_TYPE_URL:
        case PROPERTY_TYPE_URLSET:
        case PROPERTY_TYPE_BOOLEAN:
        {
            // nothing to do here
            break;
        }

        default:
        {
            icLogWarn(LOG_TAG,"%s: found unexpected property type %d when cleaning up",__FUNCTION__,typeDef->propertyType);
            break;
        }

    }
    free(typeDef->propertyName);
    free(value);
}

/**
 * destroy the in-memory property type definitions
 */
void destroyPropertyTypeDefs()
{
    hashMapDestroy(propertyTypesMap,freePropTypesEntry);
    propertyTypesMap = NULL;
}

/**
 * determines if the suggested value is a valid boolean (true/false) string representation
 */
static bool isValueValidBoolean(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    bool boolVal;
    bool retval = stringToBoolStrict(suggestedValue,&boolVal);
    if (retval == false)
    {
        if (errorMessage != NULL)
        {
            *errorMessage = stringBuilder("Value %s is not a valid boolean value", suggestedValue);
        }
        icLogWarn(LOG_TAG, "Value %s is not a valid boolean value for %s", suggestedValue, typeDef->propertyName);
    }
    return retval;
}

static bool isValueValidUInt64(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    uint64_t uint64Value;
    bool validCvt = stringToUint64(suggestedValue,&uint64Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid uint64 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid uint64 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            uint64_t minValue = typeDef->restrictions->uintLimits.minValue;
            uint64_t maxValue = typeDef->restrictions->uintLimits.maxValue;
            if ((uint64Value >= minValue) && (uint64Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL) {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIu64" to %"PRIu64, suggestedValue,
                                                  minValue, maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIu64" to %"PRIu64,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidInt64(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    int64_t int64Value;
    bool validCvt = stringToInt64(suggestedValue, &int64Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid int64 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid int64 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            int64_t minValue = typeDef->restrictions->intLimits.minValue;
            int64_t maxValue = typeDef->restrictions->intLimits.maxValue;
            if ((int64Value >= minValue) && (int64Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIi64" to %"PRIi64,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIi64" to %"PRIi64,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidUint32(const char *suggestedValue, propertyTypeDef *typeDef,char **errorMessage)
{
    uint32_t uint32Value;
    bool validCvt = stringToUint32(suggestedValue,&uint32Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid uint32 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid uint32 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            uint32_t minValue = (uint32_t)typeDef->restrictions->uintLimits.minValue;
            uint32_t maxValue = (uint32_t)typeDef->restrictions->uintLimits.maxValue;
            if ((uint32Value >= minValue) && (uint32Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIu32" to %"PRIu32,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIu32" to %"PRIu32,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidInt32(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    int32_t int32Value;
    bool validCvt = stringToInt32(suggestedValue,&int32Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid int32 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s for property %s is not a valid int32 value",suggestedValue,typeDef->propertyName);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            int32_t minValue = (int32_t)typeDef->restrictions->intLimits.minValue;
            int32_t maxValue = (int32_t)typeDef->restrictions->intLimits.maxValue;
            if ((int32Value >= minValue) && (int32Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIi32" to %"PRIi32,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIi32" to %"PRIi32,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidUint16(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    uint16_t uint16Value;
    bool validCvt = stringToUint16(suggestedValue,&uint16Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid uint16 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid uint16 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            uint16_t minValue = (uint16_t)typeDef->restrictions->uintLimits.minValue;
            uint16_t maxValue = (uint16_t)typeDef->restrictions->uintLimits.maxValue;
            if ((uint16Value >= minValue) && (uint16Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIu16" to %"PRIu16,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIu16" to %"PRIu16,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidInt16(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    int16_t int16Value;
    bool validCvt = stringToInt16(suggestedValue,&int16Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid int16 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid int16 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            int16_t minValue = (int16_t)typeDef->restrictions->intLimits.minValue;
            int16_t maxValue = (int16_t)typeDef->restrictions->intLimits.maxValue;
            if ((int16Value >= minValue) && (int16Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIi16" to %"PRIi16,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIi16" to %"PRIi16,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidUint8(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    uint8_t uint8Value;
    bool validCvt = stringToUint8(suggestedValue,&uint8Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid uint8 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid uint8 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            uint8_t minValue = (uint8_t)typeDef->restrictions->uintLimits.minValue;
            uint8_t maxValue = (uint8_t)typeDef->restrictions->uintLimits.maxValue;
            if ((uint8Value >= minValue) && (uint8Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage != NULL)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIu8" to %"PRIu8,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside the range %"PRIu8" to %"PRIu8,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidInt8(const char *suggestedValue, propertyTypeDef *typeDef,char **errorMessage)
{
    int8_t int8Value;
    bool validCvt = stringToInt8(suggestedValue,&int8Value);
    if (validCvt == false) {
        if (errorMessage != NULL) {
            *errorMessage = stringBuilder("Value %s is not a valid int8 value",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid int8 value",suggestedValue);
        return false;
    } else {
        if (typeDef->restrictions != NULL) {
            int8_t minValue = (int8_t)typeDef->restrictions->intLimits.minValue;
            int8_t maxValue = (int8_t)typeDef->restrictions->intLimits.maxValue;
            if ((int8Value >= minValue) && (int8Value <= maxValue)) {
                return true;
            } else {
                if (errorMessage)
                {
                    *errorMessage = stringBuilder("Value %s is outside the range %"PRIi8" to %"PRIi8,suggestedValue,minValue,maxValue);
                }
                icLogWarn(LOG_TAG,"Value %s for property %s is outside of the range %"PRIi8" to %"PRIi8,suggestedValue,typeDef->propertyName,minValue,maxValue);
                return false;
            }
        } else {
            return true;
        }
    }
}

static bool isValueValidForEnum(const char *suggestedValue, propertyTypeDef *typeDef, char **errorMessage)
{
    if (typeDef->restrictions == NULL) {
        return true;
    }
    if (typeDef->restrictions->enumValues == NULL) {
        return true;
    }
    bool retval = false;
    AUTO_CLEAN(linkedListIteratorDestroy__auto) icLinkedListIterator *iterator = linkedListIteratorCreate(typeDef->restrictions->enumValues);
    if (iterator != NULL) {
        while (linkedListIteratorHasNext(iterator) == true) {
            char *allowed = (char *)linkedListIteratorGetNext(iterator);
            if (stringCompare(suggestedValue,allowed,false) == 0) {
                retval = true;
                break;
            }
        }
    }
    if (retval == false)
    {
        if (errorMessage != NULL)
        {
            *errorMessage = stringBuilder("Value %s is not a value member of the defined ENUM",suggestedValue);
        }
        icLogWarn(LOG_TAG,"Value %s is not a valid member of the defined ENUM for %s",suggestedValue,typeDef->propertyName);
    }
    return retval;
}

/**
 * checks to make sure that the suggested value for the property with the given name is allowed
 */
bool isValueValid(const char *propertyName, const char *suggestedValue, char **errorMessage)
{
    bool retval = true;
    uint16_t keyLen = strlen(propertyName);
    // not sure why the hashMapGet() function doesn't take a const void *.
    propertyTypeDef *typeDef = (propertyTypeDef *)hashMapGet(propertyTypesMap,(void *)propertyName,keyLen);
    if (typeDef != NULL)
    {
        if (suggestedValue != NULL)
        {
            switch (typeDef->propertyType)
            {
                case PROPERTY_TYPE_STRING:
                case PROPERTY_TYPE_URL:
                case PROPERTY_TYPE_URLSET:
                {
                    break;
                }

                case PROPERTY_TYPE_BOOLEAN:
                {
                    retval = isValueValidBoolean(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_UINT64:
                {
                    retval = isValueValidUInt64(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_INT64:
                {
                    retval = isValueValidInt64(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_UINT32:
                {
                    retval = isValueValidUint32(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_INT32:
                {
                    retval = isValueValidInt32(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_UINT16:
                {
                    retval = isValueValidUint16(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_INT16:
                {
                    retval = isValueValidInt16(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_UINT8:
                {
                    retval = isValueValidUint8(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_INT8:
                {
                    retval = isValueValidInt8(suggestedValue, typeDef, errorMessage);
                    break;
                }

                case PROPERTY_TYPE_ENUM:
                {
                    retval = isValueValidForEnum(suggestedValue, typeDef, errorMessage);
                    break;
                }

                default:
                {
                    icLogWarn(LOG_TAG,"%s: found unexpected property type %d for property %s",__FUNCTION__,typeDef->propertyType,stringCoalesce(propertyName));
                    break;
                }
            }
        }
    }
    return retval;
}