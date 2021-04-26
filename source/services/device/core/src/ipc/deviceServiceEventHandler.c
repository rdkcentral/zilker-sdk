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
 * Created by Thomas Lea on 10/23/15.
 */

#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/propsHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <deviceDescriptorHandler.h>
#include <propsMgr/commonProperties.h>
#include <inttypes.h>
#include <icSystem/hardwareCapabilities.h>
#include <icConcurrent/threadPool.h>
#include <deviceDriverManager.h>
#include <deviceService.h>
#include <commonDeviceDefs.h>
#include <deviceService/zoneChanged.h>
#include "subsystems/zigbee/zigbeeHealthCheck.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "deviceServiceEventHandler.h"

#define LOG_TAG "deviceServiceEventHandler"

#define EVENT_THREADS_MAX    10
#define EVENT_QUEUE_MAX      128

#define OTA_UPGRADE_DELAY_ARMED      1*60*60 // Wait for 1 hour (3600 seconds)
#define OTA_UPGRADE_DELAY_DISARMED   0

static void propsListener(cpePropertyEvent *event);

bool deviceServiceEventHandlerInit()
{
    register_cpePropertyEvent_eventListener(propsListener);
    return true;
}

bool deviceServiceEventHandlerShutdown()
{
    unregister_cpePropertyEvent_eventListener(propsListener);
    shutdownEventListener();

    return true;
}

static void propsListener(cpePropertyEvent *event)
{
    if (strcmp(event->propKey, DEVICE_DESCRIPTOR_LIST) == 0)
    {
        if (!hasProperty(DEVICE_DESC_WHITELIST_URL_OVERRIDE))
        {
            deviceDescriptorsUpdateWhitelist(event->propValue);
        }
        else
        {
            icLogInfo(LOG_TAG, "Ignoring new DDL URL %s as there is an override set", event->propValue);
        }
    }
    if (strcmp(event->propKey, DEVICE_DESC_WHITELIST_URL_OVERRIDE) == 0)
    {
        // Check if the override was deleted
        if (event->propValue != NULL)
        {
            deviceDescriptorsUpdateWhitelist(event->propValue);
        }
        else
        {
            // Restore the regular whitelist
            char *whitelistUrl = getPropertyAsString(DEVICE_DESCRIPTOR_LIST, NULL);
            if (whitelistUrl != NULL)
            {
                deviceDescriptorsUpdateWhitelist(whitelistUrl);
                free(whitelistUrl);
            }
        }
    }
    else if (strcmp(event->propKey, DEVICE_DESC_BLACKLIST) == 0)
    {
        deviceDescriptorsUpdateBlacklist(event->propValue);
    }
    else if (strcmp(event->propKey, POSIX_TIME_ZONE_PROP) == 0)
    {
        icLogDebug(LOG_TAG, "Got new time zone: %s", event->propValue);
        timeZoneChanged(event->propValue);
    }
    else if (strcmp(event->propKey, DEVICE_FIRMWARE_URL_NODE) == 0 ||
             strcmp(event->propKey, CAMERA_FIRMWARE_URL_NODE) == 0)
    {
        icLogDebug(LOG_TAG, "Got new firmware url: %s", event->propValue);
        // Got a new URL, its possible this could trigger new downloads that failed before, so
        // process device descriptors and attempt to download any needed firmware
        deviceServiceProcessDeviceDescriptors();
    }
    else if (strcmp(event->propKey, CPE_BLACKLISTED_DEVICES_PROPERTY_NAME) == 0)
    {
        icLogDebug(LOG_TAG, "Blacklisted devices property set to : %s", event->propValue);
        processBlacklistedDevices(event->propValue);
    }
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    else if (strncmp(event->propKey, ZIGBEE_PROPS_PREFIX, strlen(ZIGBEE_PROPS_PREFIX)) == 0 ||
             strncmp(event->propKey, TELEMETRY_PROPS_PREFIX, strlen(TELEMETRY_PROPS_PREFIX)) == 0)
    {
        zigbeeSubsystemHandlePropertyChange(event->propKey, event->propValue);
    }
#endif

    // Finally, give device service a chance to handle the event
    deviceServiceNotifyPropertyChange(event);

}

static void freeDeviceList(icLinkedList **list)
{
    linkedListDestroy(*list, standardDoNotFreeFunc);
}


