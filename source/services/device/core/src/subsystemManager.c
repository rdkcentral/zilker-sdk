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

/*
 * Created by Thomas Lea on 3/10/16.
 */

#include <inttypes.h>
#include <icBuildtime.h>
#include <icLog/logging.h>

#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <zhal/zhal.h>
#include <string.h>
#include <stdio.h>
#include <propsMgr/paths.h>
#include <deviceService.h>
#include <icTypes/icHashMap.h>
#include <ipc/deviceEventProducer.h>
#include <pthread.h>

#include "subsystemManager.h"

#include "deviceServicePrivate.h"

#define LOG_TAG "deviceService"

static icHashMap *subsystemsReady = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static subsystemManagerInitializedFunc initializedCB = NULL;
static subsystemManagerReadyForDevicesFunc readyForDevicesCB = NULL;

static void setSubsystemReady(const char *subsystem, bool isReady)
{
    pthread_mutex_lock(&mutex);
    bool *readyVal = (bool *)hashMapGet(subsystemsReady, (void *)subsystem, strlen(subsystem));
    if (readyVal != NULL)
    {
        // Update the value
        *readyVal = isReady;
    }
    else
    {
        //  Create the value
        readyVal = (bool *)malloc(sizeof(bool));
        *readyVal = isReady;
        hashMapPut(subsystemsReady, strdup(subsystem), strlen(subsystem), readyVal);
    }
    pthread_mutex_unlock(&mutex);
}

static void subsystemInitialized(const char *subsystem)
{
    if (initializedCB != NULL)
    {
        initializedCB(subsystem);
    }
}

static void subsystemReadyForDevices(const char *subsystem)
{
    // Mark the subsystem as ready
    setSubsystemReady(subsystem, true);

    // If everything is now ready, callback
    if (subsystemManagerIsReadyForDevices())
    {
        if (readyForDevicesCB != NULL)
        {
            readyForDevicesCB();
        }
    }
}

int subsystemManagerInitialize(const char* cpeId, subsystemManagerInitializedFunc initializedCallback, subsystemManagerReadyForDevicesFunc readyForDevicesCallback)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, cpeId);

    pthread_mutex_lock(&mutex);
    initializedCB = initializedCallback;
    readyForDevicesCB = readyForDevicesCallback;
    if (subsystemsReady == NULL)
    {
        subsystemsReady = hashMapCreate();
    }
    pthread_mutex_unlock(&mutex);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    setSubsystemReady(ZIGBEE_SUBSYSTEM_ID, false);
    zigbeeSubsystemInitialize(cpeId, subsystemInitialized, subsystemReadyForDevices, ZIGBEE_SUBSYSTEM_ID);
#endif

    pthread_mutex_lock(&mutex);
    // Safety net if we have no subsystems
    if (hashMapCount(subsystemsReady) == 0)
    {
        if (readyForDevicesCB != NULL)
        {
            readyForDevicesCB();
        }
    }
    pthread_mutex_unlock(&mutex);

    return 0;
}

void subsystemManagerShutdown()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&mutex);
    readyForDevicesCB = NULL;
    hashMapDestroy(subsystemsReady, NULL);
    subsystemsReady = NULL;
    pthread_mutex_unlock(&mutex);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    zigbeeSubsystemShutdown();
#endif

}

void subsystemManagerAllDriversStarted()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    zigbeeSubsystemAllDriversStarted();
#endif

}

void subsystemManagerAllServicesAvailable()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    zigbeeSubsystemAllServicesAvailable();
#endif

}

void subsystemManagerPostRestoreConfig()
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    zigbeeSubsystemPostRestoreConfig();
#endif

}

/*
 * Check if a specific subsystem is ready for devices
 */
bool subsystemManagerIsSubsystemReady(const char *subsystem)
{
    bool result = false;

    pthread_mutex_lock(&mutex);
    bool *ready = hashMapGet(subsystemsReady, (void*)subsystem, strlen(subsystem));
    if (ready != NULL)
    {
        result = *ready;
    }
    pthread_mutex_unlock(&mutex);

    return result;
}

/*
 * Check if all subsystems are ready for devices
 */
bool subsystemManagerIsReadyForDevices()
{
    bool allReady = true;
    pthread_mutex_lock(&mutex);
    icHashMapIterator *iter = hashMapIteratorCreate(subsystemsReady);
    while(hashMapIteratorHasNext(iter))
    {
        char *key;
        uint16_t keyLen;
        bool *value;
        hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&value);
        if (!*value)
        {
            allReady = false;
            break;
        }
    }
    hashMapIteratorDestroy(iter);
    pthread_mutex_unlock(&mutex);

    return allReady;
}

/*
 * Restore config for RMA
 */
bool subsystemManagerRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    bool result = false;
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
     if (zigbeeSubsystemRestoreConfig(tempRestoreDir, dynamicConfigPath) == true)
     {
         result = true;
     }
     else
     {
         icLogWarn(LOG_TAG, "Failed to restore config for zigbee subsystem");
     }
#endif

    return result;
}
