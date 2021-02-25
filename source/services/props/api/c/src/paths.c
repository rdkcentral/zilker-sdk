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
 * paths.c
 *
 * Similar to the PathsUtil Java object, this returns
 * the set of path prefixes to use for the various storage
 * locations on an iControl CPE.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>

#include <propsMgr/commonProperties.h>
#include <propsMgr/paths.h>
#include <propsMgr/propsHelper.h>
#include <icUtil/parsePropFile.h>
#include <icUtil/stringUtils.h>
#include <icTypes/sbrm.h>
#include <icBuildtime.h>

#define CERT_TRUST_LOC "security/certificates"
#define MASTER_CA_CERT_FILE_NAME "ca-certs.pem"

//Locally cache static and dynamic paths so we dont have to reach out to props service each time
// statics are initialized to all zeros by default
static char staticPath[PATH_MAX+1];
static char dynamicPath[PATH_MAX+1];
static pthread_mutex_t pathMtx = PTHREAD_MUTEX_INITIALIZER;

/*
 * internal, caller must free return string
 */
static char *findPathFromScript(const char *script, char *propName)
{
    char *retVal = NULL;

    if (access(script, R_OK) == 0)
    {
        char *exportLine = (char *)malloc(sizeof(char) * (strlen(propName) + 8));
        sprintf(exportLine, "export %s", propName);

        // able to read the script
        //
        icPropertyIterator *loop = propIteratorCreate(script);
        while (propIteratorHasNext(loop) == true)
        {
            // see if this prop key matches what we're looking for
            // keep in mind the line could be one of:
            //   abc=123
            //   export abc=123
            //
            icProperty *prop = propIteratorGetNext(loop);
            if (prop->key == NULL || prop->value == NULL)
            {
                // bad property
            }
            else if (stringCompare(prop->key, propName, false) == 0)
            {
                // exact match
                //
                retVal = strdup(prop->value);
            }
            else if (stringCompare(prop->key, exportLine, false) == 0)
            {
                // "export var"
                //
                retVal = strdup(prop->value);
            }

            // cleanup
            //
            propertyDestroy(prop);

            // break if we found a match
            //
            if (retVal != NULL)
            {
                break;
            }
        }

        // cleanup
        //
        propIteratorDestroy(loop);
        free(exportLine);
    }

    return retVal;
}

/*
 * Return the path to where the 'dynamic files' are stored.
 * This is a convenience function to obtain the IC_DYNAMIC_DIR_PROP
 * property (which defaults to /opt)
 *
 * NOTE: the caller must free this memory
 */
char *getDynamicPath()
{
    char *result = NULL;
    bool firstRead = false;

    pthread_mutex_lock(&pathMtx);

    if(strlen(dynamicPath) == 0) // this is our first time retrieving it
    {
        firstRead = true;

        // ask propsService for IC_DYNAMIC_DIR_PROP
        //
        result = getPropertyAsPath(IC_DYNAMIC_DIR_PROP, NULL);
        if (result != NULL)
        {
            // successfully extracted the property
            //
            goto exit;
        }

        // see if the IC_CONF environment variable is set.
        // attempting to handle the situation where services are
        // not running (yet), but need to get to the config dir.
        //
        char *possible = getenv("IC_CONF");
        if (possible != NULL && strlen(possible) > 0)
        {
            // use environment variable
            //
            result = strdup(possible);
            goto exit;
        }

        // propsService is not running AND no IC_CONF environment variable.
        // try to parse the /tmp/xh_env.sh script (should be
        // a link to the real env.sh) to extract the IC_CONF value
        //
        result = findPathFromScript("/tmp/xh_env.sh", "IC_CONF");
        if (result != NULL)
        {
            // return what was pulled from the environment script ( no need to dup )
            //
            goto exit;
        }

        // none of the above are possible, so return the default
        // NOTE: use platform specific default if available
        //
#ifdef CONFIG_DYNAMIC_PATH
        result = strdup(CONFIG_DYNAMIC_PATH);
#else
        result = strdup(DEFAULT_DYNAMIC_PATH);
#endif
    }
    else
    {
        // we already had a cached path
        //
        result = strdup(dynamicPath);
    }

exit:

    if(firstRead)
    {
        strncpy(dynamicPath, result, PATH_MAX);
    }

    pthread_mutex_unlock(&pathMtx);

    return result;
}

/*
 * Return the path to where the 'dynamic config files' are stored.
 * This is a convenience function to obtain the IC_DYNAMIC_DIR_PROP
 * property + /etc (which defaults to /opt/etc)
 *
 * NOTE: the caller must free this memory
 */
char *getDynamicConfigPath()
{
    // get the 'dynamic' dir from the property (or default)
    //
    char *dir = getDynamicPath();

    // append /etc
    //
    char *retVal = (char *)malloc(strlen(dir) + strlen(CONFIG_SUBDIR) + 2);
    sprintf(retVal, "%s%s", dir, CONFIG_SUBDIR);

    // cleaup & return
    //
    free(dir);
    return retVal;
}

/*
 * Return the path to where the 'static files' are stored.
 * This is a convenience function to obtain the IC_STATIC_DIR_PROP
 * property (which defaults to /vendor)
 *
 * NOTE: the caller must free this memory
 */
char *getStaticPath()
{
    char *result = NULL;
    bool firstRead = false;

    pthread_mutex_lock(&pathMtx);

    if(strlen(staticPath) == 0) // this is our first time retrieving it
    {
        firstRead = true;

        // ask propsService for IC_STATIC_DIR_PROP
        //
        result = getPropertyAsPath(IC_STATIC_DIR_PROP, NULL);
        if (result != NULL)
        {
            // successfully extracted the property
            //
            goto exit;
        }

        // see if the IC_HOME environment variable is set.
        // attempting to handle the situation where services are
        // not running (yet), but need to get to the config dir.
        //
        char *possible = getenv("IC_HOME");
        if (possible != NULL && strlen(possible) > 0)
        {
            // use environment variable
            //
            result = strdup(possible);
            goto exit;
        }

        // propsService is not running AND no IC_HOME environment variable.
        // try to parse the /tmp/xh_env.sh script (should be
        // a link to the real env.sh) to extract the IC_HOME value
        //
        result = findPathFromScript("/tmp/xh_env.sh", "IC_HOME");
        if (result != NULL)
        {
            // return what was pulled from the environment script ( no need to dup )
            //
            goto exit;
        }

        // none of the above are possible, so return the default
        //
        // none of the above are possible, so return the default
        // NOTE: use platform specific default if available
        //
#ifdef CONFIG_STATIC_PATH
        result = strdup(CONFIG_STATIC_PATH);
#else
        result = strdup(DEFAULT_STATIC_PATH);
#endif
    }
    else
    {
        // we already had a cached path
        //
        result = strdup(staticPath);
    }

exit:

    if(firstRead)
    {
        strncpy(staticPath, result, PATH_MAX);
    }

    pthread_mutex_unlock(&pathMtx);

    return result;
}

/*
 * Return the path to where the static config files are stored.
 * This is a convenience function to obtain the IC_STATIC_DIR_PROP
 * property + /etc (which defaults to /vendor/etc)
 *
 * NOTE: the caller must free this memory
 */
char *getStaticConfigPath()
{
    // get the 'static' dir from the property (or default)
    //
    char *dir = getStaticPath();

    // append /etc
    //
    char *retVal = (char *)malloc(strlen(dir) + strlen(CONFIG_SUBDIR) + 2);
    sprintf(retVal, "%s%s", dir, CONFIG_SUBDIR);

    // cleaup & return
    //
    free(dir);
    return retVal;
}

char *getBrandDefaultsPath()
{
    char * dir = getStaticConfigPath();
    size_t buflen = strlen(dir) + strlen(DEFAULTS_SUBDIR) + 1;

    char * retVal = malloc(buflen);
    snprintf(retVal, buflen, "%s%s", dir, DEFAULTS_SUBDIR);

    free(dir);
    return retVal;
}

/*
 * Return the path to where the statistics files are stored.
 *
 * NOTE: the caller must free this memory
 */
char *getStatisticPath()
{
    size_t bufLen = strlen("/tmp") + strlen("/stats") + 1;
    char *retVal = malloc(bufLen);
    snprintf(retVal, bufLen, "%s%s", "/tmp", "/stats");

    return retVal;
}

/*
 * Return the path to where the Telemetry files are stored.
 *
 * NOTE: the caller must free this memory
 */
char *getTelemetryPath()
{
    size_t bufLen = strlen("/tmp") + strlen("/telemetry") + 1;
    char *retVal = malloc(bufLen);
    snprintf(retVal, bufLen, "%s%s", "/tmp", "/telemetry");

    return retVal;
}

/*
 * Retrieve a property as a "path" and return the value.
 * If the property is not defined, the "default" will be returned.
 *
 * NOTE: the caller must free this memory if not NULL
 *
 * @param propName - the property name to get from propsService
 * @param defValue - if not NULL, the default value to return if getting 'propName' fails
 */
char *getPropertyAsPath(const char *propName, const char *defValue)
{
    // same as the "helper" implementation
    //
    return getPropertyAsString(propName, defValue);
}


char *getCABundlePath(void)
{
#ifdef CONFIG_PLATFORM_RDK
    //just return standard linux ca-cert path
    return strdup("/etc/ssl/certs/ca-certificates.crt");
#else
    AUTO_CLEAN(free_generic__auto) char *brandConfDir = getBrandDefaultsPath();
    return stringBuilder("%s/"CERT_TRUST_LOC"/"MASTER_CA_CERT_FILE_NAME, brandConfDir);
#endif
}
