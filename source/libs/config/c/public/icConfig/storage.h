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
// Created by tlea on 4/4/18.
//

/**
 * This is a simple storage library for key value string pairs organized into a 'namespace'.
 * An example namespace might be 'deviceService'.  A key might be the EUI64 of a ZigBee device
 * and its value might be the JSON representation of the device and all of its settings.
 */

#ifndef ZILKER_STORAGE_H
#define ZILKER_STORAGE_H

#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include <libxml/tree.h>
#include <cjson/cJSON.h>
#include <sys/time.h>

/**
 * Save a value for a key under a namespace.
 *
 * @param namespace
 * @param key
 * @param value
 * @return true on success
 */
bool storageSave(const char* namespace, const char* key, const char* value);

/**
 * Load a value for a key under a namespace.
 *
 * @param namespace
 * @param key
 * @param value the retrieved value, if any.  Caller frees.
 * @return true on success
 */
bool storageLoad(const char* namespace, const char* key, char** value);

/**
 * Try to load valid XML from storage.
 * @param namespace
 * @param key
 * @param encoding the document encoding or NULL
 * @param xmlParserOptions A set of logically OR'd xmlParserOptions or 0 for defaults
 * @return a valid xmlDoc, or NULL on failure. Free with xmlFreeDoc when finished.
 */
xmlDoc *storageLoadXML(const char *namespace, const char *key, const char *encoding, int xmlParserOptions);

/**
 * Try to load valid JSON from storage.
 * @param namespace
 * @param key
 * @return a valid cJSON document, or NULL on failure. Free with cJSON_Delete when finished.
 */
cJSON *storageLoadJSON(const char *namespace, const char *key);


typedef struct StorageCallbacks
{
    /**
     * Any context data needed by parse()
     */
    void *parserCtx;

    /**
     * A custom parser for reading and validating storage data.
     * @param fileContents
     * @param parserCtx
     * @return true if fileContents represents valid data.
     */
    bool (*parse)(const char *fileContents, void *parserCtx);
} StorageCallbacks;

/**
 * Load valid data from storage with a custom parser/validator.
 * @param namespace
 * @param key
 * @param cb A non-null StorageCallbacks that describes your parser.
 * @return true if any valid data was loaded.
 * @see storageLoadJSON and storageLoadXML if you only want basic document validation.
 */
bool storageParse(const char *namespace, const char *key, const StorageCallbacks *cb);

/**
 * Delete a key from a namespace.
 *
 * @param namespace
 * @param key
 * @return true on success
 */
bool storageDelete(const char* namespace, const char* key);

/**
 * Delete a namespace and all of the keys stored within it.
 *
 * NOTE: this will remove other content added under the namespace out-of-band from this library
 *
 * @param namespace
 * @return true on success
 */
bool storageDeleteNamespace(const char* namespace);

/**
 * Restore a storage namespace from a backed up
 * storage location.
 *
 * All content within the existing namespace (if any)
 * will be destroyed.
 *
 * @param namespace The namespace to restore.
 * @param basePath The base configuration path without any storage modifiers.
 * @return True on success or restore namespace not found.
 *         Failure conditions:
 *           - Existing namespace could not be removed.
 *           - Namespace could not be restored.
 */
bool storageRestoreNamespace(const char* namespace, const char* basePath);

/**
 * Retrieve a list of all the keys under a namespace.
 *
 * @param namespace
 * @return the list of keys or NULL on failure
 */
icLinkedList* storageGetKeys(const char* namespace);

/**
 * Retrieve the name of the storage directory within the dynamic config directory
 * @return the storage dir name, caller should NOT free this string
 */
const char *getStorageDir();

/**
 * Retrieve the last modification date for the key.
 * @note only the key's main entry is loaded. Invoke @refitem storageLoad first
 *       for best results.
 * @param namespace
 * @param key
 * @return true when the item's modification date is avaialable.
 */
bool storageGetMtime(const char *namespace, const char *key, struct timespec *mtime);

#endif //ZILKER_STORAGE_H
