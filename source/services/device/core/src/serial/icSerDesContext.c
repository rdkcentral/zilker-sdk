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
// Created by mdeleo739 on 6/10/19.
//

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <icLog/logging.h>
#include <serial/icSerDesContext.h>

#define LOG_TAG "serDesContext"

icSerDesContext *serDesCreateContext(void)
{
    icSerDesContext *context = (icSerDesContext *)calloc(1, sizeof(icSerDesContext));
    context->props = stringHashMapCreate();
    return context;
}

void serDesDestroyContext(icSerDesContext *context)
{
    if (context != NULL)
    {
        stringHashMapDestroy(context->props, NULL);
        free(context);
    }
}

bool serDesSetContextValue(icSerDesContext *context, const char *key, const char *value)
{
    if (context == NULL)
    {
        icLogWarn(LOG_TAG, "Attempting to set value on null context: \"%s\" -> \"%s\"", key, value);
        return false;
    }
    return stringHashMapPut(context->props, strdup(key), strdup(value));
}

bool serDesHasContextValue(const icSerDesContext *context, const char *key)
{
    if (context == NULL)
    {
        icLogWarn(LOG_TAG, "Attempting to verify value on null context: \"%s\"", key);
        return false;
    }
    return stringHashMapContains(context->props, key);
}

const char *serDesGetContextValue(const icSerDesContext *context, const char *key)
{
    if (context == NULL)
    {
        icLogWarn(LOG_TAG, "Attempting to access value on null context: \"%s\"", key);
        return false;
    }
    return stringHashMapGet(context->props, key);
}
