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
 * properties.h
 *
 * Database of properties stored locally and kept
 * in memory via a HashMap
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_PROPERTIES_H
#define IC_PROPERTIES_H

#include <stdbool.h>
#include <propsMgr/propsService_pojo.h>

typedef enum {
    SET_PROP_NEW,       // saved as new property
    SET_PROP_OVERWRITE, // overwrote existing
    SET_PROP_FAILED,    // failed to apply
    SET_PROP_DROPPED    // dropped in favor of an existing higher priority property
} setPropRc;

/**
 * initializes the properties settings
 */
bool initProperties(char *configDir, char *homeDir);

/**
 * Cleans up the properties
 */
void destroyProperties();

/*
 * called during RMA/Restore
 */
bool restorePropConfig(char *tempDir, char *destDir);

/*
 * extract the global default values from assman (branding)
 * and apply each that is not defined.  cannot be called until
 * AFTER IPC is functional since this depends on assman, which
 * asks this service for the IC_HOME path.
 */
void loadGlobalDefaults();

/*
 * helper function to allocate and clear a new property object
 * if 'key' and/or 'value' are not NULL, they will be copied
 * into the new object
 */
property *createProperty(const char *key, const char *val, propSource source);

/**
 * retrieve the property for the given key, or NULL if not found.
 * caller MUST NOT free a non-NULL result.
 */
property *getProperty(char *key);

/**
 * creates or updates a cpe property value.
 *
 * the caller MUST examine the return value so that it knows if it's save to free "prop" or not:
 *   SET_PROP_NEW        - prop was taken as-is; do NOT free 'prop'
 *   SET_PROP_OVERWRITE, - prop was duplicated; DO FREE 'prop'
 *   SET_PROP_FAILED     - failure; DO FREE 'prop'
 */
setPropRc setProperty(property *prop);

/**
 * creates or updates a cpe property value, overwriting even if the values match;
 * forcing the GENERIC_PROP_UPDATED to be broadcast
 *
 * the caller MUST examine the return value so that it knows if it's save to free "prop" or not:
 *   SET_PROP_NEW        - prop was taken as-is; do NOT free 'prop'
 *   SET_PROP_OVERWRITE, - prop was duplicated; DO FREE 'prop'
 *   SET_PROP_FAILED     - failure; DO FREE 'prop'
 */
setPropRc setPropertyOverwrite(property *prop);

/*
 * create/update a set of properties, but only applies to ones that are new or different.
 * this is more efficient when applying several because it does just a single write
 */
bool setPropertiesBulk(propertyValues *group);

/**
 * deletes a cpe property value
 */
bool deleteProperty(char *key); // need the source?, propSource source);

/*
 * return the set of all known property keys as well as how many
 * are in the return array.  caller must free the returned array
 * as well as each string in the array.
 */
char **getAllPropertyKeys(int *countOut);

/**
 * gets the version of the storage file
 */
uint64_t getConfigFileVersion();

/**
 * gets the number of cpe properties that are set
 */
int getPropertyCount();

#endif // IC_PROPERTIES_H
