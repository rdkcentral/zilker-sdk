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
 * simpleProtectConfig.h
 *
 * simplified variation of functions within the protectConfig.h
 * headerfile.  that mechanism assumes the caller will store the
 * random key somewhere in the system.  although flexible, it allows
 * for non-standard procedures for safeguarding and saving the
 * generated keys.
 *
 * Author: jelderton -  6/7/19.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <cjson/cJSON.h>

#include <icUtil/base64.h>
#include <icConfig/simpleProtectConfig.h>
#include <icConfig/storage.h>
#include <icConfig/protectedConfig.h>
#include <icConfig/obfuscation.h>

#define DEFAULT_KEY_IDENTIFIER      "default"
#define OBFUSCATE_KEY               "config"        // simple yet not out of place.  need to make this better

struct ProtectSecret
{
    pcData *key;
};

// private functions
static pcData *readNamespaceKey(const char *namespace, const char *identifier);
static bool writeNamespaceKey(const char *namespace, const char *identifier, pcData *key);

/**
 * encrypt 'dataToProtect' and return a BASE64 encoded string that is
 * NULL terminated, which allows the caller to safely save to storage.
 *
 * requires a "namespace" for storage and retrieval of the generated
 * keys (which happen under the hood).
 *
 * caller is responsible for releasing the returned string
 */
char *simpleProtectConfigData(const char *namespace, const char *dataToProtect)
{
    char *retVal = NULL;
    if (namespace == NULL || dataToProtect == NULL || openProtectConfigSession() == false)
    {
        return NULL;
    }

    ProtectSecret *encrKey = simpleProtectGetSecret(namespace, true);

    retVal = simpleProtectEncrypt(encrKey, dataToProtect);

    simpleProtectDestroySecret(encrKey);

    return retVal;
}

/**
 * decrypt 'protectedData' and return a NULL terminated string.
 *
 * requires a "namespace" for storage and retrieval of the generated
 * keys (which happen under the hood).
 *
 * caller is responsible for releasing the returned string.
 */
char *simpleUnprotectConfigData(const char *namespace, const char *protectedData)
{
    char *retVal = NULL;
    if (namespace == NULL || protectedData == NULL)
    {
        return NULL;
    }

    // see if we have an encryption key already associated with this namespace
    //
    ProtectSecret *encrKey = simpleProtectGetSecret(namespace, false);

    retVal = simpleProtectDecrypt(encrKey, protectedData);

    simpleProtectDestroySecret(encrKey);

    return retVal;
}

char *simpleProtectDecrypt(const ProtectSecret *secret, const char *protectedData)
{
    if (secret == NULL || protectedData == NULL || openProtectConfigSession() == false)
    {
        return NULL;
    }

    // use the encryption key to decode the protected data
    //
    char *retVal = NULL;
    pcData input;
    input.data = (unsigned char *)protectedData;
    input.length = (uint32_t)strlen(protectedData);
    pcData *data = unprotectConfigData(&input, secret->key);
    if (data != NULL)
    {
        // move from 'data' to 'retVal'
        retVal = (char *)data->data;
        data->data = NULL;
    }

    // cleanup and return
    //
    destroyProtectConfigData(data, false);
    closeProtectConfigSession();

    return retVal;
}

char *simpleProtectEncrypt(const ProtectSecret *secret, const char *dataToProtect)
{
    if (secret == NULL || dataToProtect == NULL || openProtectConfigSession() == false)
    {
        return NULL;
    }

    // use the encryption key to protect the data
    //
    char *retVal = NULL;
    pcData in;
    in.data = (unsigned char *)dataToProtect;
    in.length = (uint32_t)strlen(dataToProtect);
    pcData *encr = protectConfigData(&in, secret->key);

    if (encr != NULL)
    {
        // move from 'encr' to 'retVal'
        retVal = (char *)encr->data;
        encr->data = NULL;
    }

    // cleanup and return
    //
    destroyProtectConfigData(encr, false);
    closeProtectConfigSession();

    return retVal;
}

ProtectSecret *simpleProtectGetSecret(const char *ns, bool autoCreate)
{
    // see if we have an encryption key already associated with this namespace
    //
    pcData *encrKey = NULL;
    ProtectSecret *secret = NULL;
    if (openProtectConfigSession() == false)
    {
        return NULL;
    }

    encrKey = readNamespaceKey(ns, DEFAULT_KEY_IDENTIFIER);
    if (encrKey == NULL && autoCreate == true)
    {
        // create one, then persist it
        //
        encrKey = generateProtectPassword();
        if (encrKey != NULL)
        {
            writeNamespaceKey(ns, DEFAULT_KEY_IDENTIFIER, encrKey);
        }
    }

    closeProtectConfigSession();

    if (encrKey != NULL)
    {
        secret = calloc(1, sizeof(ProtectSecret));
        secret->key = encrKey;
    }

    return secret;
}

void simpleProtectDestroySecret(ProtectSecret *secret)
{
    if (secret != NULL)
    {
        destroyProtectConfigData(secret->key, true);
    }
    free(secret);
}

void simpleProtectDestroySecret__auto(ProtectSecret **secret)
{
    simpleProtectDestroySecret(*secret);
}

/*
 * extract the 'key' from this namespace
 */
static pcData *readNamespaceKey(const char *namespace, const char *identifier)
{
    // first extract the obfuscated 'identifier' from the 'key file' in this 'namespace'
    //
    pcData *retVal = NULL;
    char *value = NULL;
    char *encoded = NULL;
    if (storageLoad(namespace, STORAGE_KEY_FILE_NAME, &value) == true)
    {
        // file is in JSON format, so parse it and extract the 'identifier'
        //
        cJSON *body = cJSON_Parse(value);
        if (body != NULL)
        {
            cJSON *item = NULL;
            if (((item = cJSON_GetObjectItem(body, identifier)) != NULL) && (item->valuestring != NULL))
            {
                encoded = strdup(item->valuestring);
            }
            cJSON_Delete(body);
        }
    }
    free(value);

    if (encoded != NULL && encoded[0] != '\0')
    {
        // base64 decode first
        //
        uint8_t *decoded = NULL;
        uint16_t decodeLen = 0;
        if (icDecodeBase64((const char *)encoded, &decoded, &decodeLen) == true)
        {
            // successfully decoded, so un-obfuscate.  for simplicity, we use
            // a hard-coded obfuscation seed (requested by developers).
            //
            uint32_t keyLen = 0;
            char *key = unobfuscate(OBFUSCATE_KEY, (uint32_t) strlen(OBFUSCATE_KEY), (const char *) decoded, decodeLen, &keyLen);
            if (key != NULL)
            {
                // save as our return object
                //
                retVal = (pcData *)malloc(sizeof(pcData));
                retVal->data = (unsigned char *)key;
                retVal->length = keyLen;
            }
            free(decoded);
        }
    }
    free(encoded);

    return retVal;
}

/*
 * store 'key' in the namespace
 * NOTE: at this time we are only saving a single key, even
 *       though we allow the caller to name is via 'identifier'.
 *       when we want to store more then 1 key, the file will need
 *       to be read, massaged, then saved.  currently this will just
 *       overwrite what is there.
 */
static bool writeNamespaceKey(const char *namespace, const char *identifier, pcData *key)
{
    bool retVal = false;
    char *encodedKey = NULL;
    if (key != NULL)
    {
        // obfuscate our key.  for simplicity, we use a hard-coded obfuscation seed
        //
        uint32_t obLen = 0;
        char *ob1 = obfuscate(OBFUSCATE_KEY, (uint32_t)strlen(OBFUSCATE_KEY), (const char *)key->data, key->length, &obLen);
        if (ob1 != NULL)
        {
            // base64 encode the key so we can save it in JSON
            //
            encodedKey = icEncodeBase64((uint8_t *)ob1, (uint16_t)obLen);
            free(ob1);
        }
    }

    if (encodedKey != NULL)
    {
        // create the JSON
        //
        cJSON *body = cJSON_CreateObject();
        cJSON_AddStringToObject(body, identifier, encodedKey);

        // convert to string
        //
        char *contents = cJSON_Print(body);
        cJSON_Delete(body);
        body = NULL;

        // save to the namespace file
        //
        retVal = storageSave(namespace, STORAGE_KEY_FILE_NAME, contents);
        free(contents);
        free(encodedKey);
        contents = NULL;
    }

    return retVal;
}