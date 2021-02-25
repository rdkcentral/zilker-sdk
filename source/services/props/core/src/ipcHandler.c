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
 * ipcHandler.c
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <icIpc/ipcMessage.h>
#include <icIpc/eventConsumer.h>
#include <icIpc/ipcReceiver.h>
#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include "propsService_ipc_handler.h"            // local IPC handler
#include "properties.h"
#include "common.h"
#include "propertyTypeDefinitions.h"
#include <propsMgr/paths.h>
#include <propsMgr/commonProperties.h>
#include <watchdog/serviceStatsHelper.h>
#include <icTime/timeUtils.h>

/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_propsService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    //
    collectEventStatistics(output, input);
    collectIpcStatistics(get_propsService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

    output->serviceName = strdup(PROPS_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_propsService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
{
    // TODO: return status
    return IPC_SUCCESS;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   details - both the 'temp dir' the config was extracted to, and the 'target dir' of where to store
 */
IPCCode handle_propsService_CONFIG_RESTORED_request(configRestoredInput *input, configRestoredOutput* output)
{
    // ask our config file to load the old information
    //
    if (restorePropConfig(input->tempRestoreDir, input->dynamicConfigPath) == true)
    {
        output->action = CONFIG_RESTORED_COMPLETE;
    }
    else
    {
        output->action = CONFIG_RESTORED_FAILED;
    }

    return IPC_SUCCESS;
}

/**
 * Get the 'property' with the given 'key'
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_CPE_PROPERTY_request(char *input, property *output)
{
    if (input == NULL || strlen(input) == 0)
    {
        // bad input
        //
        return IPC_INVALID_ERROR;
    }

    output->key = strdup(input);

    // ask our properties container for this one
    //
    property *defined = getProperty(output->key);
    if (defined != NULL)
    {
        // transfer content to the output variable
        //
        output->value = strdup(defined->value);
        output->source = defined->source;
    }
    else
    {
        output->value = NULL;
        output->source = PROPERTY_SRC_DEVICE;
    }

    return IPC_SUCCESS;
}

/**
 * Set (add or replace) a property using 'key' and 'value'
 *   input - value to send to the service
 **/
IPCCode handle_SET_CPE_PROPERTY_request(property *input, propertySetResult *output)
{
    IPCCode ipcCode = IPC_SUCCESS;

    if (input == NULL || input->key == NULL || input->value == NULL || output == NULL)
    {
        // bad input
        //
        return IPC_INVALID_ERROR;
    }

    char *errorMessage = NULL;

    if (isValueValid(input->key, input->value, &errorMessage) == false)
    {
        output->result = PROPERTY_SET_VALUE_NOT_ALLOWED;
        // will have been malloc'ed
        output->errorMessage = errorMessage;
        return ipcCode;
    }

    // create new property object since our container
    // will want to keep the memory we provide
    //
    property *copy = createProperty(input->key, input->value, input->source);

    // apply this copy to our container
    //
    setPropRc rc = setProperty(copy);
    if (rc != SET_PROP_FAILED && rc != SET_PROP_DROPPED)
    {
        // Note: This log line is used for telemetry, please DO NOT modify or remove it
        icLogDebug(PROP_LOG, "done setting property k=%s v=%s s=%s", input->key, input->value, propSourceLabels[input->source]);
        output->result = PROPERTY_SET_OK;
    }
    else
    {
        icLogDebug(PROP_LOG, "did not set property k=%s v=%s s=%s; already existed", input->key, input->value, propSourceLabels[input->source]);
        ipcCode = IPC_GENERAL_ERROR;
        output->result = PROPERTY_SET_GENERAL_ERROR;
    }

    // cleanup
    //
    if (rc != SET_PROP_NEW)
    {
        destroy_property(copy);
    }

    return ipcCode;
}

/**
 * Set (add or replace) a property using 'key' and 'value', however overwrite even if the value is the same to force the 'GENERIC_PROP_UPDATED' event.
 *   input - single property definition
 **/
IPCCode handle_SET_CPE_PROPERTY_OVERWRITE_request(property *input, propertySetResult *output)
{
    if (input == NULL || input->key == NULL || input->value == NULL || output == NULL)
    {
        // bad input
        //
        return IPC_INVALID_ERROR;
    }

    char *errorMessage = NULL;

    if (isValueValid(input->key, input->value, &errorMessage) == false)
    {
        output->result = PROPERTY_SET_VALUE_NOT_ALLOWED;
        output->errorMessage = errorMessage;
        return IPC_SUCCESS;
    }

    // create new property object since our container
    // will want to keep the memory we provide
    //
    property *copy = createProperty(input->key, input->value, input->source);

    // apply this copy to our container
    //
    setPropRc rc = setPropertyOverwrite(copy);
    if (rc != SET_PROP_FAILED)
    {
        icLogDebug(PROP_LOG, "done setting (overwrite) property k=%s v=%s s=%s", input->key, input->value, propSourceLabels[input->source]);
        output->result = PROPERTY_SET_OK;
    }
    else
    {
        icLogDebug(PROP_LOG, "did not set (overwrite) property k=%s v=%s s=%s; already existed", input->key, input->value, propSourceLabels[input->source]);
        output->result = PROPERTY_SET_GENERAL_ERROR;
    }

    // cleanup
    //
    if (rc != SET_PROP_NEW)
    {
        destroy_property(copy);
    }

    return IPC_SUCCESS;
}

/**
 * Set (add/replace) a group of proerties
 *   input - container of known property key/values
 *   output - response from service (boolean)
 **/
IPCCode handle_SET_CPE_PROPERTIES_BULK_request(propertyValues *input, bool *output)
{
    // perform the bulk operation
    //
    *output = setPropertiesBulk(input);
    return IPC_SUCCESS;
}

/**
 * Delete the 'property' with the given 'key'
 *   input - value to send to the service
 **/
IPCCode handle_DEL_CPE_PROPERTY_request(char *input)
{
    if (input == NULL || strlen(input) == 0)
    {
        // bad input
        //
        return IPC_INVALID_ERROR;
    }

    // pass along to our container
    //
    deleteProperty(input);
    icLogDebug(PROP_LOG, "done removing property k=%s", input);
    return IPC_SUCCESS;
}

/**
 * Return number of properties known to the service
 *   output - response from service
 **/
IPCCode handle_COUNT_PROPERTIES_request(int *output)
{
    *output = getPropertyCount();
    return IPC_SUCCESS;
}

/*
 * called by qsort to get the keys in the correct order
 */
static int sortKeys(const void *left, const void *right)
{
    // looks strange, but this is correct based
    // on the way our array of strings was put together
    //
    const char **leftStr = (const char **)left;
    const char **rightStr = (const char **)right;

    return strcmp(*leftStr, *rightStr);
}

/**
 * Return up-to 256 property keys known to the service
 *   output - container of known property keys
 **/
IPCCode handle_GET_ALL_KEYS_request(propertyKeys *output)
{
    // get all of the keys
    //
    int i;
    int count = 0;
    char **array = NULL;
    array = getAllPropertyKeys(&count);

    if (count > 1)
    {
        // sort the keys alphabetically
        //
        qsort(array, (size_t)count, sizeof(char *), sortKeys);
    }

    // transfer each to the output object
    //
    for (i = 0 ; i < count ; i++)
    {
        // put in output (so no need to free it)
        //
        put_key_in_propertyKeys_list(output, array[i]);
        array[i] = NULL;
    }

    // mem cleanup the outer array since each item has
    // moved into the IPC object.
    //
    free(array);

    return IPC_SUCCESS;
}

/**
 * Return all properties known to the service.  This is expensive, so use sparingly
 *   output - container of known property key/values
 **/
IPCCode handle_GET_ALL_KEY_VALUES_request(propertyValues *output)
{
    //
    // NOTE: just a test, not intended to be used in the product
    //

    // get all of the keys
    //
    int i;
    int count = 0;
    char **array = NULL;
    array = getAllPropertyKeys(&count);

    if (count > 1)
    {
        // sort the keys alphabetically
        //
        qsort(array, (size_t)count, sizeof(char *), sortKeys);
    }

    // transfer each to the output object
    //
    for (i = 0 ; i < count ; i++)
    {
        // get the property for this key
        //
        property *prop = getProperty(array[i]);
        if (prop != NULL)
        {
            // set the key from the 'get all', but need to clone the
            // property object so that the destruction of 'output'
            // can perform normal cleanup
            //
            property *copy = createProperty(prop->key, prop->value, prop->source);
            put_property_in_propertyValues_set(output, array[i], copy);
        }

        // cleanup the 'key'
        //
        free(array[i]);
        array[i] = NULL;
    }

    // mem cleanup the outer array
    //
    free(array);

    return IPC_SUCCESS;
}

/**
 * Return version number used during initialInform
 *   output - response from service
 **/
IPCCode handle_GET_FILE_VERSION_request(uint64_t *output)
{
    // get the XML file revision
    //
    *output = getConfigFileVersion();
    return IPC_SUCCESS;
}

/**
 * Get a path based on properties, etc.
 * @param propPath the path type
 * @param output the path, or NULL if unknown/undefined
 * @return SUCCESS/FAILURE code
 */
IPCCode handle_GET_PATH_request(propertyPath *propPath, char **output)
{
    IPCCode retCode = IPC_SUCCESS;
    *output = NULL;
    if (propPath != NULL)
    {
        switch (propPath->pathType)
        {

            case DYNAMIC_PATH:
                *output = getDynamicPath();
                break;
            case DYNAMIC_CONFIG_PATH:
                *output = getDynamicConfigPath();
                break;
            case STATIC_PATH:
                *output = getStaticPath();
                break;
            case STATIC_CONFIG_PATH:
                *output = getStaticConfigPath();
                break;
            default:
                retCode = IPC_INVALID_ERROR;
                break;
        }
    }
    else
    {
        retCode = IPC_INVALID_ERROR;
    }
    return retCode;
}

