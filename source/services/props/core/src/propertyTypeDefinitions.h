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

#ifndef ZILKER_PROPERTYTYPEDEFINITIONS_H
#define ZILKER_PROPERTYTYPEDEFINITIONS_H
#include <stdbool.h>

/**
 * initialize the property type definitions from the supplied config file
 */
void initPropertyTypeDefs();

/**
 * get the count of the number of property type defs found
 */
uint16_t getPropertyTypeDefsCount();

/**
 * destroy the in-memory property type definitions
 */
void destroyPropertyTypeDefs();

/**
 * callback to free up a propertyTypesMap entry
 */
void freePropTypesEntry(void *key, void *value);

/**
 * checks to make sure that the suggested value for the property with the given name is allowed
 */
bool isValueValid(const char *propertyName, const char *suggestedValue, char **errorMessage);
#endif //ZILKER_PROPERTYTYPEDEFINITIONS_H
