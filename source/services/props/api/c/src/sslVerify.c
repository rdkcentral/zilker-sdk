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
 * sslVerify.c
 *
 * Define enumerations for and helper functions for getting the
 * 'ssl verify' setting (SSL_CERT_VALIDATE) as defined within properties.
 *
 * Author: jelderton - 6/16/18
 *-----------------------------------------------*/

#include <string.h>
#include <propsMgr/sslVerify.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <icLog/logging.h>

#define LOG_TAG "ssl"

/* Keep synchronized with sslVerifyConvertCPEPropEvent documentation */
#define DEFAULT_SSL_VERIFY_MODE SSL_VERIFY_BOTH

static const char *sslVerifyCategoryToProp[] =
{
        SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER,
        SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE
};

static sslVerify convertVerifyPropValToMode(const char *strVal);

/*
 * Return the sslVerify value of the SSL_CERT_VALIDATE property
 * for the given category (as each can be different)
 */
sslVerify getSslVerifyProperty(sslVerifyCategory category)
{
    sslVerify retVal = DEFAULT_SSL_VERIFY_MODE;
    const char *key = sslVerifyPropKeyForCategory(category);

    if (key == NULL)
    {
        icLogWarn(LOG_TAG, "No property key for TLS verify category [%d]", category);
        return retVal;
    }

    // get the value of the property, then convert to sslVerify enumeration
    //
    char *strVal = getPropertyAsString(key, "");
    retVal = convertVerifyPropValToMode(strVal);

    // cleanup & return
    //
    free(strVal);
    return retVal;
}

static sslVerify convertVerifyPropValToMode(const char *strVal)
{
    sslVerify retVal = DEFAULT_SSL_VERIFY_MODE;
    if (strVal == NULL || strlen(strVal) == 0 || strcasecmp(strVal, SSL_VERIFICATION_TYPE_NONE) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_NONE option");
        retVal = SSL_VERIFY_NONE;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_HOST) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_HOST option");
        retVal = SSL_VERIFY_HOST;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_PEER) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_PEER option");
        retVal = SSL_VERIFY_PEER;
    }
    else if (strcasecmp(strVal, SSL_VERIFICATION_TYPE_BOTH) == 0)
    {
        icLogDebug(LOG_TAG, "using VERIFY_BOTH option");
        retVal = SSL_VERIFY_BOTH;
    }
    else
    {
        icLogDebug(LOG_TAG, "using default option [%d]", retVal);
    }

    return retVal;
}

const char *sslVerifyPropKeyForCategory(sslVerifyCategory cat)
{
    const char *propKey = NULL;

    if (cat >= SSL_VERIFY_CATEGORY_FIRST && cat <= SSL_VERIFY_CATEGORY_LAST)
    {
        propKey = sslVerifyCategoryToProp[cat];
    }

    return propKey;
}


sslVerify sslVerifyConvertCPEPropEvent(cpePropertyEvent *event, sslVerifyCategory cat)
{
    const char *propKey = sslVerifyPropKeyForCategory(cat);
    sslVerify retVal = SSL_VERIFY_INVALID;

    if (propKey == NULL || event->propKey == NULL)
    {
        return retVal;
    }

    if (strcmp(propKey, event->propKey) == 0)
    {
        if (event->baseEvent.eventCode != GENERIC_PROP_DELETED)
        {
            retVal = convertVerifyPropValToMode(event->propValue);
        }
        else
        {
            retVal = DEFAULT_SSL_VERIFY_MODE;
        }
    }

    return retVal;
}
