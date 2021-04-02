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
 * The device driver manager handles the registration and interactions with the
 * various device drivers, each of which is responsible for understanding how to
 * interact with various device classes.
 *
 * Created by Thomas Lea on 7/28/15.
 */

#include <icBuildtime.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <icLog/logging.h>
#include "deviceDriver.h"
#include <deviceService.h>

#define LOG_TAG "deviceDriverManager"

#ifdef CONFIG_SERVICE_DEVICE_PHILIPS_HUE
#include <philipsHue/philipsHueDeviceDriver.h>
#endif

#ifdef CONFIG_SERVICE_DEVICE_RTCOA_TSTAT
#include <rtcoaWifi/rtcoaWifiDeviceDriver.h>
#endif

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include <zigbeeLight/zigbeeLightDeviceDriver.h>
#include <zigbeeLegacyLight/zigbeeLegacyLightDeviceDriver.h>
#include <zigbeeSensor/zigbeeSensorDeviceDriver.h>
#include <zigbeeThermostat/zigbeeThermostatDeviceDriver.h>
#include <zigbeeDoorLock/zigbeeDoorLockDeviceDriver.h>
#include <zigbeeLegacySensor/zigbeeLegacySensorDeviceDriver.h>
#include <zigbeeSecurityController/zigbeeSecurityControllerDeviceDriver.h>
#include <zigbeeLegacySecurityController/zigbeeLegacySecurityControllerDeviceDriver.h>
#include <zigbeeLegacySirenRepeater/zigbeeLegacySirenRepeaterDeviceDriver.h>
#include <zigbeePresence/zigbeePresenceDeviceDriver.h>
#include <zigbeeLightController/zigbeeLightControllerDeviceDriver.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE_XBB
#include <xbb/xbbDeviceDriver.h>
#endif

#endif

#include <icUtil/stringUtils.h>

#include "deviceDriverManager.h"
#include "deviceServicePrivate.h"

//These device driver includes will go away once we have dynamic registration
#include "test/testDeviceDriver.h"
#include "openHomeCamera/openHomeCameraDeviceDriver.h"
#include "subsystemManager.h"

static DeviceServiceCallbacks deviceServiceInterface;

static bool deviceDriverManagerLoadDrivers();

static bool driverSupportsDeviceClass(const DeviceDriver* driver, const char* deviceClass);

static void destroyDeviceDriverMapEntry(void* key, void* value);

static icHashMap *deviceDrivers = NULL;
static icLinkedList *orderedDeviceDrivers = NULL; // an ordered index (by load) onto the drivers. Does not own drivers.

/*
 * Load up the device drivers and initialize them.
 */
bool deviceDriverManagerInitialize()
{
    icLogDebug(LOG_TAG, "deviceDriverManagerInitialize");

    memset(&deviceServiceInterface, 0, sizeof(DeviceServiceCallbacks));

    return deviceDriverManagerLoadDrivers();
}

/*
 * Tell each device driver that we are up and running and it can start.
 */
bool deviceDriverManagerStartDeviceDrivers()
{
    icLogDebug(LOG_TAG, "deviceDriverManagerStartDeviceDrivers");

    sbIcLinkedListIterator *it = linkedListIteratorCreate(orderedDeviceDrivers);
    while (linkedListIteratorHasNext(it))
    {
        DeviceDriver* driver = linkedListIteratorGetNext(it);
        driver->startup(driver->callbackContext);
    }

    return true;
}

/*
 * Tell each driver to shutdown and release all resources.
 */
bool deviceDriverManagerShutdown()
{
    icLogDebug(LOG_TAG, "deviceDriverManagerShutdown");

    if (deviceDrivers == NULL)
    {
        icLogError(LOG_TAG, "Not yet initialized!");
        return false;
    }

    icHashMapIterator* iterator = hashMapIteratorCreate(deviceDrivers);
    while (hashMapIteratorHasNext(iterator))
    {
        uint16_t keyLen;
        void* key;
        void* value;

        hashMapIteratorGetNext(iterator, &key, &keyLen, &value);

        DeviceDriver* driver = (DeviceDriver*) value;

        driver->shutdown(driver->callbackContext);
    }
    hashMapIteratorDestroy(iterator);

    hashMapDestroy(deviceDrivers, destroyDeviceDriverMapEntry);
    deviceDrivers = NULL;

    linkedListDestroy(orderedDeviceDrivers, standardDoNotFreeFunc);
    orderedDeviceDrivers = NULL;

    return true;
}

icLinkedList* deviceDriverManagerGetDeviceDriversByDeviceClass(const char* deviceClass)
{
    icLinkedList* result = NULL;

    icLogDebug(LOG_TAG, "deviceDriverManagerGetDeviceDriversByDeviceClass: deviceClass=%s", deviceClass);

    if (deviceDrivers == NULL)
    {
        icLogError(LOG_TAG, "Not yet initialized!");
        return NULL;
    }

    if (deviceClass != NULL)
    {
        result = linkedListCreate();

        icHashMapIterator* iterator = hashMapIteratorCreate(deviceDrivers);
        while (hashMapIteratorHasNext(iterator))
        {
            char* driverName;
            uint16_t driverNameLen; //will be 1 more than strlen
            DeviceDriver* driver;
            hashMapIteratorGetNext(iterator, (void**) &driverName, &driverNameLen, (void**) &driver);
            if (driverSupportsDeviceClass(driver, deviceClass))
            {
                linkedListAppend(result, driver);
            }
        }
        hashMapIteratorDestroy(iterator);
    }

    if (result == NULL)
    {
        icLogWarn(LOG_TAG,
                  "deviceDriverManagerGetDeviceDriversByDeviceClass: deviceClass=%s: NO DRIVER FOUND",
                  deviceClass);
    }

    return result;
}

icLinkedList *deviceDriverManagerGetDeviceDriversBySubsystem(const char *subsystem)
{
    icLinkedList* result = NULL;

    icLogDebug(LOG_TAG, "%s: subsystem=%s", __FUNCTION__ , subsystem);

    if (deviceDrivers == NULL)
    {
        icLogError(LOG_TAG, "%s: not yet initialized!", __FUNCTION__ );
        return NULL;
    }

    if (subsystem != NULL)
    {
        result = linkedListCreate();

        icHashMapIterator* iterator = hashMapIteratorCreate(deviceDrivers);
        while (hashMapIteratorHasNext(iterator))
        {
            char* driverName;
            uint16_t driverNameLen; // will be 1 more than strlen()

            DeviceDriver* driver;
            hashMapIteratorGetNext(iterator, (void**) &driverName, &driverNameLen, (void**) &driver);

            if (stringCompare(driver->subsystemName, subsystem, false) == 0)
            {
                linkedListAppend(result, driver);
            }

        }
        hashMapIteratorDestroy(iterator);
    }

    if (result == NULL)
    {
        icLogWarn(LOG_TAG, "%s: subsystem=%s: NO DRIVER FOUND", __FUNCTION__ , subsystem);
    }

    return result;

}

DeviceDriver* deviceDriverManagerGetDeviceDriver(const char* driverName)
{
    DeviceDriver* result = NULL;

    if (deviceDrivers != NULL)
    {
        result = (DeviceDriver*) hashMapGet(deviceDrivers, (void*) driverName, (int16_t) (strlen(driverName) + 1));
    }
    else
    {
        icLogWarn(LOG_TAG, "deviceDriverManagerGetDeviceDriver did not find driver for name %s", driverName);
    }

    return result;
}

icLinkedList* deviceDriverManagerGetDeviceDrivers()
{
    return linkedListClone(orderedDeviceDrivers);
}

typedef DeviceDriver* (* driverInitFn)(DeviceServiceCallbacks* deviceService);

static void loadDriver(driverInitFn fn)
{
    DeviceDriver* driver = fn(&deviceServiceInterface);

    if (driver != NULL && driver->driverName != NULL)
    {
        icLogDebug(LOG_TAG, "Loading device driver %s", driver->driverName);
        hashMapPut(deviceDrivers, driver->driverName, (uint16_t) (strlen(driver->driverName) + 1), driver);
        linkedListAppend(orderedDeviceDrivers, driver);
    }
}

static bool deviceDriverManagerLoadDrivers()
{
    //for now we are hard coding the loading of the device drivers.  Next iteration should use cpluff to
    // dynamically load the device drivers

    deviceServiceInterface.deviceFound = deviceServiceDeviceFound;

    deviceServiceInterface.getDevicesByDeviceDriver = deviceServiceGetDevicesByDeviceDriver;
    deviceServiceInterface.getDevice = deviceServiceGetDevice;
    deviceServiceInterface.getEndpoint = deviceServiceGetEndpointById;
    deviceServiceInterface.getResource = deviceServiceGetResourceById;
    deviceServiceInterface.updateResource = updateResource;
    deviceServiceInterface.getMetadata = getMetadata;
    deviceServiceInterface.setMetadata = setMetadata;
    deviceServiceInterface.removeDevice = deviceServiceRemoveDevice;
    deviceServiceInterface.discoverStart = deviceServiceDiscoverStart;
    deviceServiceInterface.discoverStop = deviceServiceDiscoverStop;
    deviceServiceInterface.addEndpoint = deviceServiceAddEndpoint;
    deviceServiceInterface.enableEndpoint = deviceServiceUpdateEndpoint;

    deviceDrivers = hashMapCreate();
    orderedDeviceDrivers = linkedListCreate();

    //ORDER MATTERS FOR ZIGBEE DEVICE DRIVERS!  When it comes to device discovery these drivers will be invoked in order
    // to see who owns the discovered device.
    loadDriver(openHomeCameraDeviceDriverInitialize);
    loadDriver(testDeviceDriverInitialize);

#ifdef CONFIG_SERVICE_DEVICE_PHILIPS_HUE
    loadDriver(philipsHueDeviceDriverInitialize);
#endif

#ifdef CONFIG_SERVICE_DEVICE_RTCOA_TSTAT
    loadDriver(rtcoaWifiDeviceDriverInitialize);
#endif

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE_XBB
    loadDriver(xbbDeviceDriverInitialize);
#endif
    loadDriver(zigbeeLegacyLightDeviceDriverInitialize);
    loadDriver(zigbeeLightDeviceDriverInitialize);
    loadDriver(zigbeeSensorDeviceDriverInitialize);
    loadDriver(zigbeeLegacySensorDeviceDriverInitialize);
    loadDriver(zigbeeThermostatDeviceDriverInitialize);
    loadDriver(zigbeeDoorLockDeviceDriverInitialize);
    loadDriver(zigbeeKeypadDeviceDriverInitialize);
    loadDriver(zigbeeKeyfobDeviceDriverInitialize);
    loadDriver(zigbeeLegacyKeypadDeviceDriverInitialize);
    loadDriver(zigbeeLegacyKeyfobDeviceDriverInitialize);
    loadDriver(zigbeeLegacySirenRepeaterDriverInitialize);
    loadDriver(zigbeePresenceDeviceDriverInitialize);
    loadDriver(zigbeeLightControllerDeviceDriverInitialize);

#endif

    return true;
}

static bool driverSupportsDeviceClass(const DeviceDriver* driver, const char* deviceClass)
{
    bool result = false;
    if (driver != NULL && driver->supportedDeviceClasses != NULL)
    {
        icLinkedListIterator* iterator = linkedListIteratorCreate(driver->supportedDeviceClasses);
        while (linkedListIteratorHasNext(iterator))
        {
            char* supportedDeviceClass = (char*) linkedListIteratorGetNext(iterator);
            if (strcmp(deviceClass, supportedDeviceClass) == 0)
            {
                result = true;
                break;
            }
        }

        linkedListIteratorDestroy(iterator);
    }

    return result;
}

static void destroyDeviceDriverMapEntry(void* key, void* value)
{
    //do nothing since its already been freed during shutdown
}
