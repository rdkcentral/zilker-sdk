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
 * propsHelper.c
 *
 * Helper functions to make getting properties (and
 * typecasting them) less painful.
 *
 * Author: jelderton - 6/17/16
 *-----------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_ipc.h>
#include <propsMgr/commonProperties.h>

/*
 * queries propsService to see if a property is set.
 *
 * @param propName - the property name to query from propsService
 */
bool hasProperty(const char *propName)
{
    bool retVal = false;

    // ask props service for this property
    //
    char *val = getPropertyAsString(propName, NULL);
    if (val != NULL)
    {
        // got something back, so return true
        //
        retVal = true;
        free(val);
    }

    return retVal;
}

/*
 * Checks to see if this particular property key and/or value is
 * changable by any remote request (Server or XConf).  Will return
 * false if changing this property is not allowed.
 */
bool canServerSetProperty(const char *propName, const char *propsValue)
{
    if (propName == NULL)
    {
        return false;
    }

    // we want to prevent external influences from changing the "ssl verify" options
    // to "none", which would allow for a "man in the middle" style attack
    //
    if (stringCompare(SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER, propName, true) == 0 ||
        stringCompare(SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE, propName, true) == 0)
    {
        // found a 'ssl verify' key, now make sure the value isn't being set to 'none'
        //
        if (stringCompare(propsValue, SSL_VERIFICATION_TYPE_NONE, true) == 0)
        {
            // not allowed to set to None from a remote source
            //
            return false;
        }
    }

    // passed all checks above, must be ok
    //
    return true;
}

/*
 * queries propsService for a property that has a string value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * NOTE: the caller must free the return string via free() - if not NULL
 *
 * @param propName - the property name to get from propsService
 * @param defValue - if not NULL, the default value to return if getting 'propName' fails
 */
char *getPropertyAsString(const char *propName, const char *defValue)
{
    char *retVal = NULL;

    // ask propsService for 'propName' and don't worry
    // if the request fails as we'll just look at the length
    // of the returned property
    //
    property *p = create_property();
    if (propsService_request_GET_CPE_PROPERTY((char *)propName, p) == IPC_SUCCESS)
    {
        // see if we got a value back
        //
        if (p->value == NULL || strlen(p->value) == 0)
        {
            // nothing defined, so use default
            //
            if (defValue != NULL)
            {
                retVal = strdup(defValue);
            }
        }
        else
        {
            // return a copy of what we got back
            //
            retVal = strdup(p->value);
        }
    }
    else if (defValue != NULL)
    {
        // unable to ask propService; return default value
        //
        retVal = strdup(defValue);
    }

    // cleanup
    //
    destroy_property(p);

    return retVal;
}

const char *getPropertyEventAsString(const cpePropertyEvent *event, const char *defValue)
{
    if (event == NULL || event->propValue == NULL || event->baseEvent.eventValue == GENERIC_PROP_DELETED)
    {
        return defValue;
    }
    return event->propValue;
}

static int32_t getInternalAsInt32(const char* value, int32_t defValue)
{
    long ret;

    if ((value == NULL) || (value[0] == '\0')) {
        ret = defValue;
    } else {
        errno = 0;
        ret = strtol(value, NULL, 0);

        if (errno != 0) {
            ret = (long) defValue;
        }
    }

    // Don't allow truncation
    return (ret > INT32_MAX) ? defValue : (int32_t) ret;
}

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
int32_t getPropertyAsInt32(const char *propName, int32_t defValue)
{
    // get the property (no default)
    //
    char *value = getPropertyAsString(propName, NULL);
    int32_t retVal = getInternalAsInt32(value, defValue);

    free(value);

    return retVal;
}

int32_t getPropertyEventAsInt32(const cpePropertyEvent *event, int32_t defValue)
{
    if (event == NULL || event->baseEvent.eventValue == GENERIC_PROP_DELETED)
    {
        return defValue;
    }

    return getInternalAsInt32(event->propValue, defValue);
}

static uint32_t getInternalAsUInt32(const char *value, uint32_t defValue)
{
    unsigned long ret;
    AUTO_CLEAN(free_generic__auto) char *buffer = trimString(value);

    if ((value == NULL) || (value[0] == '\0') || (buffer[0] == '-')) {
        ret = defValue;
    } else {
        errno = 0;
        ret = strtoul(value, NULL, 10);

        if (errno != 0) {
            ret = (unsigned long) defValue;
        }
    }

    // Don't allow truncation
    return (ret > UINT32_MAX) ? defValue : (uint32_t) ret;
}

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
uint32_t getPropertyAsUInt32(const char *propName, uint32_t defValue)
{
    // get the property (no default)
    //
    char *value = getPropertyAsString(propName, NULL);
    uint32_t retVal = getInternalAsUInt32(value, defValue);

    free(value);

    return retVal;
}

uint32_t getPropertyEventAsUInt32(const cpePropertyEvent *event, uint32_t defValue)
{
    if (event == NULL || event->baseEvent.eventValue == GENERIC_PROP_DELETED)
    {
        return defValue;
    }

    return getInternalAsUInt32(event->propValue, defValue);
}

static int64_t getInternalAsInt64(const char *value, int64_t defValue)
{
    long long int ret;

    if ((value == NULL) || (value[0] == '\0')) {
        ret = defValue;
    } else {
        errno = 0;
        ret = strtoll(value, NULL, 0);

        if (errno != 0) {
            ret = (long long int) defValue;
        }
    }

    // Don't allow truncation
    return (ret > INT64_MAX) ? defValue : (int64_t) ret;
}

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
int64_t getPropertyAsInt64(const char *propName, int64_t defValue)
{
    // get the property (no default)
    //
    char *value = getPropertyAsString(propName, NULL);
    int64_t retVal = getInternalAsInt64(propName, defValue);

    free(value);

    return retVal;
}

int64_t getPropertyEventAsInt64(const cpePropertyEvent *event, int64_t defValue)
{
    if (event == NULL || event->baseEvent.eventValue == GENERIC_PROP_DELETED)
    {
        return defValue;
    }

    return getInternalAsInt64(event->propValue, defValue);
}

static uint64_t getInternalAsUInt64(const char *value, uint64_t defValue)
{
    unsigned long long int ret;
    AUTO_CLEAN(free_generic__auto) char *buffer = trimString(value);

    if ((value == NULL) || (value[0] == '\0') || (buffer[0] == '-')) {
        ret = defValue;
    } else {
        errno = 0;
        ret = strtoull(value, NULL, 0);

        if (errno != 0) {
            ret = (unsigned long long int) defValue;
        }
    }

    // Don't allow truncation
    return (ret > UINT64_MAX) ? defValue : (uint64_t) ret;
}

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
uint64_t getPropertyAsUInt64(const char *propName, uint64_t defValue)
{
    // get the property (no default)
    //
    char *value = getPropertyAsString(propName, NULL);
    uint64_t retVal = getInternalAsUInt64(value, defValue);

    free(value);

    return retVal;
}

uint64_t getPropertyEventAsUInt64(const cpePropertyEvent *event, uint64_t defValue)
{
    if (event == NULL || event->baseEvent.eventValue == GENERIC_PROP_DELETED)
    {
        return defValue;
    }

    return getInternalAsUInt64(event->propValue, defValue);
}

static bool getInternalAsBool(const char *value, bool defValue)
{
    bool ret;

    if ((value == NULL) || (value[0] == '\0'))
    {
        ret = defValue;
    }
    else
    {
        ret = stringToBool(value);
    }

    return ret;
}

/*
 * queries propsService for a property that has a boolean value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
bool getPropertyAsBool(const char *propName, bool defValue)
{
    // get the property (no default)
    //
    char *value = getPropertyAsString(propName, NULL);
    bool retVal = getInternalAsBool(value, defValue);

    free(value);

    return retVal;
}

bool getPropertyEventAsBool(const cpePropertyEvent *event, bool defValue)
{
    if (event == NULL || event->baseEvent.eventValue == GENERIC_PROP_DELETED)
    {
        return defValue;
    }

    return getInternalAsBool(event->propValue, defValue);
}

/*
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param propValue - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 */
propSetResult setPropertyValue(const char *propName, const char *propValue, bool overwrite, propSource source)
{
    // sanity check
    //
    if (propName == NULL || propValue == NULL)
    {
        return PROPERTY_SET_INVALID_REQUEST;
    }

    // first see if we need to check for this property's existance
    //
    if (overwrite == false)
    {
        if (hasProperty(propName) == true)
        {
            // already got a value for this prop
            //
            return PROPERTY_SET_ALREADY_EXISTS;
        }
    }

    // create the container to send to propsService
    //
    propSetResult retVal = PROPERTY_SET_GENERAL_ERROR;
    property *prop = create_property();
    prop->key = strdup(propName);
    prop->value = strdup(propValue);
    prop->source = source;
    propertySetResult *result = create_propertySetResult();
    IPCCode ipcResult = propsService_request_SET_CPE_PROPERTY(prop,result);
    if ((ipcResult == IPC_SUCCESS) || (ipcResult == IPC_GENERAL_ERROR) || (ipcResult == IPC_INVALID_ERROR))
    {
        retVal = result->result;
    }
    else
    {
        retVal = PROPERTY_SET_IPC_ERROR;
    }
    destroy_propertySetResult(result);
    destroy_property(prop);

    return retVal;
}

propSetResult setPropertyUInt32(const char *propName, uint32_t value, bool overwrite, propSource source)
{
    char buffer[16];

    snprintf(buffer, 16, "%" PRIu32, value);

    return setPropertyValue(propName, buffer, overwrite, source);
}

propSetResult setPropertyInt32(const char *propName, int32_t value, bool overwrite, propSource source)
{
    char buffer[16];

    snprintf(buffer, 16, "%" PRId32, value);

    return setPropertyValue(propName, buffer, overwrite, source);
}

propSetResult setPropertyUInt64(const char *propName, uint64_t value, bool overwrite, propSource source)
{
    char buffer[32];

    snprintf(buffer, 32, "%" PRIu64, value);

    return setPropertyValue(propName, buffer, overwrite, source);
}

propSetResult setPropertyInt64(const char *propName, int64_t value, bool overwrite, propSource source)
{
    char buffer[32];

    snprintf(buffer, 32, "%" PRId64, value);

    return setPropertyValue(propName, buffer, overwrite, source);
}

propSetResult setPropertyBool(const char *propName, bool value, bool overwrite, propSource source)
{
    return setPropertyValue(propName, (false == value) ? "false" : "true", overwrite, source);
}

