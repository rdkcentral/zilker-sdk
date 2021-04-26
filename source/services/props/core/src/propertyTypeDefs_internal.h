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

#ifndef ZILKER_PROPERTYTYPEDEFS_INTERNAL_H
#define ZILKER_PROPERTYTYPEDEFS_INTERNAL_H
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#include <icTypes/icHashMap.h>
#include <icTypes/icLinkedList.h>
#include <icLog/logging.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/paths.h>
#include <propsMgr/timezone.h>
#include <icUtil/stringUtils.h>
#include <libgen.h>

#define LOG_TAG "propTypeDefs"

typedef enum
{
    PROPERTY_TYPE_STRING=0,
    PROPERTY_TYPE_BOOLEAN,
    PROPERTY_TYPE_UINT64,
    PROPERTY_TYPE_UINT32,
    PROPERTY_TYPE_UINT16,
    PROPERTY_TYPE_UINT8,
    PROPERTY_TYPE_INT64,
    PROPERTY_TYPE_INT32,
    PROPERTY_TYPE_INT16,
    PROPERTY_TYPE_INT8,
    PROPERTY_TYPE_ENUM,
    PROPERTY_TYPE_URL,
    PROPERTY_TYPE_URLSET,
    PROPERTY_TYPE_UNKNOWN
} propertyDataType;

#define TYPEDEF_TYPE_NAME_STRING     "string"
#define TYPEDEF_TYPE_NAME_BOOLEAN    "boolean"
#define TYPEDEF_TYPE_NAME_UINT64     "uint64"
#define TYPEDEF_TYPE_NAME_UINT32     "uint32"
#define TYPEDEF_TYPE_NAME_UINT16     "uint16"
#define TYPEDEF_TYPE_NAME_UINT8      "uint8"
#define TYPEDEF_TYPE_NAME_INT64      "int64"
#define TYPEDEF_TYPE_NAME_INT32      "int32"
#define TYPEDEF_TYPE_NAME_INT16      "int16"
#define TYPEDEF_TYPE_NAME_INT8       "int8"
#define TYPEDEF_TYPE_NAME_ENUM       "enum"
#define TYPEDEF_TYPE_NAME_URL        "url"
#define TYPEDEF_TYPE_NAME_URLSET     "urlSet"

typedef struct _unsignedIntLimits
{
    uint64_t minValue;
    uint64_t maxValue;
} unsignedIntLimits;

typedef struct _signedIntLimits
{
    int64_t minValue;
    int64_t maxValue;
} signedIntLimits;

typedef union _typeRestrictions
{
    unsignedIntLimits uintLimits;
    signedIntLimits   intLimits;
    icLinkedList      *enumValues;
} typeRestrictions;

typedef struct _propertyTypeDef
{
    char *propertyName;
    propertyDataType propertyType;
    union _typeRestrictions *restrictions;
} propertyTypeDef;


#endif //ZILKER_PROPERTYTYPEDEFS_INTERNAL_H
