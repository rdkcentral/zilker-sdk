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
// Created by Thomas Lea on 7/28/15.
// Updated by Jeff Gleason
//

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <cjson/cJSON.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icUtil/macAddrUtils.h>
#include <icConcurrent/delayedTask.h>
#include <icSystem/runtimeAttributes.h>
#include <commonDeviceDefs.h>
#include <resourceTypes.h>
#include <device/icDeviceResource.h>
#include <deviceDriver.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <openHomeCamera/ohcmDiscover.h>
#include <openHomeCamera/ohcm.h>
#include <deviceDescriptors.h>
#include <icConcurrent/repeatingTask.h>
#include <ssdp/ssdp.h>
#include <icConcurrent/threadUtils.h>
#include <icTime/timeUtils.h>
#include "deviceModelHelper.h"
#include "openHomeCameraDeviceDriver.h"
#include "cameraSet.h"
#include "cameraPrivate.h"
#include "cameraDevice.h"

// pre-determined locations for the "mutual TLS" files
#define MUTUAL_TLS_CERT_FILENAME        "/tmp/comcast/camera/bozsqpmod.in"     // named bizare as per Philly requirements
#define MUTUAL_TLS_KEY_FILENAME         "/tmp/comcast/camera/bozsqpmod.out"

//String constants used for building media URLs (should move to ohcm lib?)
#define VIDEO_STREAM_TYPE_FLV           "FLV"
#define VIDEO_STREAM_TYPE_MJPEG         "MJPEG"
#define VIDEO_STREAM_TYPE_RTSP          "RTSP"
#define VIDEO_STREAM_TYPE_SNAPSHOT      "SNAPSHOT"
#define VIDEO_CODEC_H264                "H264"
#define VIDEO_CODEC_MPEG4               "MPEG4"
#define OPENHOME_CHANNEL_URL_SLICE      "/openhome/streaming/channels"

//String constants to match the cameraNeedsFirmwareState enum
#define FW_UPGRADE_UNNEEDED_STRING      "unneeded"
#define FW_UPGRADE_DELAYABLE_STRING     "delayable"
#define FW_UPGRADE_NECESSARY_STRING     "necessary"

#define CAMERA_NEEDS_FIRMWARE_STATE_KEY "firmwareUpgradeNeededState"

#define CAMERA_DISCOVERY_IP_PREFIX_PROP "camera.discovery.ipPrefix"

#define IP_RECOVERY_INTERVAL_MINUTES    5

//Use zero to detect if the property doesn't exist. Doesn't make much sense to say "discover up to 0 cameras".
#define DEFAULT_MAX_CAMERAS_TO_DISCOVER 0

#define MONITOR_THREAD_DELAYED_STARTUP_INTERVAL_SECS 300 // 5 minutes

/*
 * private variables
 */
static DeviceServiceCallbacks* deviceServiceCallbacks = NULL;
static DeviceDriver* deviceDriver = NULL;
static cameraSet *pendingCameras = NULL;    // list of cameraDevice objects - ones discovered but not yet configured
static cameraSet *configuringCameras = NULL;// list of cameraDevice objects - currently in the configuration process
static cameraSet *allCameras = NULL;        // list of cameraDevice objects - configured / known devices

//discover state info
static pthread_mutex_t discoverMutex = PTHREAD_MUTEX_INITIALIZER;    // mutex for discovery state info below
static bool discoveryRunning = false;       // keeps track of whether discovery is running or not.
static uint32_t camerasDiscoveredCounter = 0;// a counter for the number of cameras discovered in the current session

static uint32_t motionBlackoutSeconds = 0;  // this is overwritten by value from server during startup
static uint32_t updateCameraTask = 0;       // handle for the delayed-task to upgrade cameras
static uint32_t ipAddressRecoveryTask = 0;  // handle for the repeating ip address recovery task
static uint32_t delayedCameraMonitorThreadStartupTask = 0; // handle for camera monitor thread delayed startup task

typedef enum
{
    FW_UPGRADE_UNNEEDED,        //No firmware upgrade is needed
    FW_UPGRADE_DELAYABLE,       //A firmware upgrade is needed, but is not immediately necessary
    FW_UPGRADE_NECESSARY,       //A firmware upgrade is needed and necessary to do now
} cameraNeedsFirmwareState;

/*
 * private functions
 */
static bool discoverStart(void* ctx, const char* deviceClass);
static void discoverStop(void* ctx, const char* deviceClass);
static void deviceRemoved(void* ctx, icDevice* device);
static bool configureDevice(void* ctx, icDevice* device, DeviceDescriptor* descriptor);
static bool fetchInitialResourceValues(void* ctx, icDevice* device, icInitialResourceValues *initialResourceValues);
static bool registerResources(void* ctx, icDevice* device, icInitialResourceValues *initialResourceValues);
static bool readResource(void* ctx, icDeviceResource* resource, char** value);
static bool writeResource(void* ctx, icDeviceResource* resource, const char* previousValue, const char* newValue);
static bool executeResource(void* ctx, icDeviceResource* resource, const char* arg, char** response);
static bool processDeviceDescriptor(void* ctx, icDevice* device, DeviceDescriptor* dd);
static void startupDriver(void* ctx);
static void shutdownDriver(void* ctx);
static bool restoreConfig(void *ctx, const char *tempRestoreDir, const char *dynamicConfigPath);

static void cameraDiscoveredCallback(const char* ipAddress, const char* macAddress);
static bool addDiscoveredCamera(const char* ipAddress, const char* macAddress);
static bool addRediscoveredCamera(cameraDevice* camera);
static char* extractStringResource(const char* deviceUuid, const char* endpointId, const char* resourceId);
static void loadCameraPersistenceResources(cameraDevice* device);
static void cameraDeviceCallback(cameraDevice* device, cameraAttrChange reason);
static ohcmCameraInfo* getCamInfo(const char* deviceUuid);
static char* getMediaUrl(const char* ipAddress, const char* streamType, const char* codec);
static cJSON* getVideoInformation(cameraDevice *camDevice);
static void scheduleDelayedCameraUpdateTask(bool forceRandomInterval);
static void delayedUpdateIteratorCallback(cameraDevice *camDevice, void *arg);
static void *performDelayedUpdate(void* arg);
static char* getCameraUpgradeFilename(icLinkedList* filenames);
static cameraNeedsFirmwareState earlyFirmwareVersionCompare(cameraDevice *camera, DeviceDescriptor *dd);
static void populateEarlyFWUpgradeMetadata(DeviceFoundDetails *details, cameraDevice *camera);
static void startIpAddressRecoveryTask();
static void delayedStartMonitorThreadCallback();
static void delayedStartMonitorThreadIteratorCallback(cameraDevice *camDevice, void *arg);

typedef struct _delayedUpdateThreadArgs
{
    DeviceDescriptor *descriptor;
    cameraDevice *device;
} delayedUpdateThreadArgs;

/*---------------------------------------------------------------------------------------
 * openHomeCameraDeviceDriverInitialize : initialize camera device driver callbacks
 *---------------------------------------------------------------------------------------*/
DeviceDriver* openHomeCameraDeviceDriverInitialize(DeviceServiceCallbacks* deviceService)
{
    icLogDebug(LOG_TAG, "openHomeCameraDeviceDriverInitialize");

    // fill in the function struct so the deviceService can
    // interact with this driver
    //
    deviceDriver = (DeviceDriver*) calloc(1, sizeof(DeviceDriver));
    if (deviceDriver != NULL)
    {
        deviceDriver->driverName = strdup(DEVICE_DRIVER_NAME);
        deviceDriver->startup = startupDriver;
        deviceDriver->shutdown = shutdownDriver;
        deviceDriver->discoverDevices = discoverStart;
        deviceDriver->stopDiscoveringDevices = discoverStop;
        deviceDriver->deviceRemoved = deviceRemoved;
        deviceDriver->configureDevice = configureDevice;
        deviceDriver->fetchInitialResourceValues = fetchInitialResourceValues;
        deviceDriver->registerResources = registerResources;
        deviceDriver->readResource = readResource;
        deviceDriver->writeResource = writeResource;
        deviceDriver->executeResource = executeResource;
        deviceDriver->processDeviceDescriptor = processDeviceDescriptor;
        deviceDriver->restoreConfig = restoreConfig;

        // We support regular and doorbell cameras
        //
        deviceDriver->supportedDeviceClasses = linkedListCreate();
        linkedListAppend(deviceDriver->supportedDeviceClasses, strdup(CAMERA_DC));
        linkedListAppend(deviceDriver->supportedDeviceClasses, strdup(DOORBELL_CAMERA_DC));

        // save the callback so we can query/inform the service
        //
        deviceServiceCallbacks = deviceService;

        // initialize the openhome camera library and provide the filenames
        // to enable 'mutual TLS' support (for cameras that support it) when these
        // files are present.
        //
        initOhcm();
        setOhcmMutualTlsMode(MUTUAL_TLS_CERT_FILENAME, MUTUAL_TLS_KEY_FILENAME);
    }
    else
    {
        icLogError(LOG_TAG, "failed to allocate DeviceDriver!");
    }

    return deviceDriver;
}

/*
 * return the number of seconds for the "motion blackout period"
 * (defined in cameraPrivate.h)
 */
uint32_t getMotionBlackoutSeconds()
{
    if (motionBlackoutSeconds != 0)
    {
        return motionBlackoutSeconds;
    }

    // not defined yet, so use the default
    //
    return DEFAULT_MOTION_BLACKOUT_SECONDS;
}

/*---------------------------------------------------------------------------------------
 * startupDriver : find all of our cameras and start event threads for each
 *---------------------------------------------------------------------------------------*/
static void startupDriver(void* ctx)
{
    // create our containers
    //
    pendingCameras = createCameraSet();     // hold discovered devices
    configuringCameras = createCameraSet(); // hold configuring devices
    allCameras = createCameraSet();         // cache of all 'paired' cameraDevice objects

    // set the motion blackout seconds to the server value or the default if 0
    //
    motionBlackoutSeconds = getPropertyAsUInt32(MOTION_EVENT_BLACKOUT_NODE, DEFAULT_MOTION_BLACKOUT_SECONDS);
    if (motionBlackoutSeconds == 0)
    {
        motionBlackoutSeconds = DEFAULT_MOTION_BLACKOUT_SECONDS;
    }

    // get all cameras that we are responsible for and start event threads for each
    //
    uint32_t total = 0;
    icLinkedList* devices = deviceServiceCallbacks->getDevicesByDeviceDriver(DEVICE_DRIVER_NAME);
    icLinkedListIterator* iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        // get the details from the attributes stored in deviceService
        //
        icDevice* device = (icDevice*) linkedListIteratorGetNext(iterator);
        ohcmCameraInfo* info = getCamInfo(device->uuid);
        if (info != NULL)
        {
            // create the cameraDevice object with basic info
            //
            ohcmResultCode rc;
            cameraDevice* obj = createCameraDevice(device->uuid, info->cameraIP, info->macAddress,
                                                   info->userName, info->password, cameraDeviceCallback, false, &rc);

            // fill in more details from our persistence
            //
            loadCameraPersistenceResources(obj);
            appendCameraToSet(allCameras, obj);

            // cleanup before going to the next
            //
            destroyOhcmCameraInfo(info);
            total++;
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    if (total > 0)
    {

        // If CPE uptime is more than or equal to 300 seconds (5 min), start cameraMonitorThread(s) immediately.
        // If CPE uptime is less than 300 seconds, schedule cameraMonitorThread(s) to start after 300 seconds minus uptime seconds.
        //
        uint64_t uptimeSeconds =  getMonotonicMillis() / 1000;

        if (uptimeSeconds < MONITOR_THREAD_DELAYED_STARTUP_INTERVAL_SECS)
        {
            uint16_t delaySeconds = MONITOR_THREAD_DELAYED_STARTUP_INTERVAL_SECS - uptimeSeconds;

            icLogDebug(LOG_TAG,
                       "Less than %d seconds since bootup, scheduling delayed task to start camera monitor thread(s) after %"PRIu16" seconds",
                       MONITOR_THREAD_DELAYED_STARTUP_INTERVAL_SECS,
                       delaySeconds);

            // setup a delayed task to start camera monitor thread
            //
            delayedCameraMonitorThreadStartupTask = scheduleDelayTask(delaySeconds,
                                                                      DELAY_SECS,
                                                                      delayedStartMonitorThreadCallback,
                                                                      NULL);
        }
        else
        {
            delayedStartMonitorThreadCallback();
        }

        // setup a delayed task to check for firmware updates
        //
        scheduleDelayedCameraUpdateTask(false);

        // start our background IP address recovery task
        //
        startIpAddressRecoveryTask();
    }
}

/*---------------------------------------------------------------------------------------
 * shutdownDriver : take down the device driver and free the memory
 *---------------------------------------------------------------------------------------*/
static void shutdownDriver(void* ctx)
{
    icLogDebug(LOG_TAG, "shutdown");

    // cancel delayed task
    //
    if (updateCameraTask > 0)
    {
        cancelDelayTask(updateCameraTask);
        updateCameraTask = 0;
    }

    // cancel the repeating ip address recovery task
    //
    if (ipAddressRecoveryTask > 0)
    {
        cancelRepeatingTask(ipAddressRecoveryTask);
        ipAddressRecoveryTask = 0;
    }

    // cancel the delayed camera monitor thread startup task
    if (delayedCameraMonitorThreadStartupTask > 0)
    {
        cancelDelayTask(delayedCameraMonitorThreadStartupTask);
        delayedCameraMonitorThreadStartupTask = 0;
    }

    // release our cameraSet containers.  as each cameraDevice is
    // destroyed, it will stop the monitoring threads
    //
    destroyCameraSet(pendingCameras);
    destroyCameraSet(configuringCameras);
    destroyCameraSet(allCameras);
    pendingCameras = NULL;
    allCameras = NULL;
    configuringCameras = NULL;

    if (deviceDriver != NULL)
    {
        free(deviceDriver->driverName);
        linkedListDestroy(deviceDriver->supportedDeviceClasses, NULL);
        free(deviceDriver);
        deviceDriver = NULL;
    }

    deviceServiceCallbacks = NULL;
}

/*---------------------------------------------------------------------------------------
 * discoverStart : start discovering cameras
 *
 * Starts a thread to discover cameras in the background.
 *
 * Shutdown and cleanup of the thread and the resources below are handled in discoverStop.
 *---------------------------------------------------------------------------------------*/
static bool discoverStart(void* ctx, const char* deviceClass)
{
    bool result = false;

    icLogDebug(LOG_TAG, "discoverStart: deviceClass=%s", deviceClass);
    if (deviceServiceCallbacks == NULL)
    {
        icLogError(LOG_TAG, "Device driver not yet initialized!");
        return false;
    }

    pthread_mutex_lock(&discoverMutex);
    if (discoveryRunning == false)
    {
        // clear our pending list
        //
        clearCameraSet(pendingCameras);

        //Should already be zero, but just in case reset it.
        camerasDiscoveredCounter = 0;

        // start discovery, calling our 'cameraDiscoveredCallback' function when
        // devices are located via SSDP
        //
        if (ohcmDiscoverStart(cameraDiscoveredCallback) == OPEN_HOME_CAMERA_CODE_SUCCESS)
        {
            result = true;
            discoveryRunning = true;
        }
    }
    pthread_mutex_unlock(&discoverMutex);

    return result;
}

static void* addRediscoveredCameraThread(void* arg)
{
    // arg should be the 'cameraDevice' to re-add
    //
    cameraDevice* target = (cameraDevice*) arg;
    addRediscoveredCamera(target);

    return NULL;
}

/*
 * callback from SSDP when a device is discovered
 */
static void cameraDiscoveredCallback(const char* ipAddress, const char* macAddress)
{
    // Check if it passes our ip prefix filter
    if (ipAddress != NULL)
    {
        char *ipPrefix = getPropertyAsString(CAMERA_DISCOVERY_IP_PREFIX_PROP, NULL);
        if (ipPrefix != NULL)
        {
            bool matches = strstr(ipAddress, ipPrefix) == ipAddress;
            free(ipPrefix);
            if (!matches)
            {
                icLogDebug(LOG_TAG, "Discarding camera at %s which does not match our IP prefix", ipAddress);
                return;
            }
        }
    }

    // if the 'macAddress' is provided, look to see if we already have this
    // device in our inventory
    //
    if (macAddress != NULL)
    {
        char uuid[MAC_ADDR_BYTES + 1];

        // get the uuid from the mac address
        //
        macAddrToUUID(uuid, macAddress);

        // check if this mac is in our inventory
        cameraDevice* impl = findCameraByUuid(allCameras, uuid);
        if (impl != NULL)
        {
            // found a camera we already have.  quick check to see if the device
            // was reset to factory or still configured with the user/pass we have saved
            //
            ohcmCameraInfo info;
            info.cameraIP = impl->ipAddress;
            info.macAddress = impl->macAddress;
            info.userName = impl->adminCredentials->username;
            info.password = impl->adminCredentials->password;

            // run the 'ping' as the test
            //
            ohcmResultCode rc = isOhcmAlive(&info, 1);
            if (rc == OHCM_SUCCESS)
            {
                // good to go
                //
                icLogDebug(LOG_TAG, "discovered existing camera %s/%s, skipping", impl->macAddress, impl->ipAddress);
            }
            else if (rc == OHCM_LOGIN_FAIL)
            {
                // user/pass has changed.  see if 'default'
                //
                info.userName = DEFAULTED_ADMIN_USERNAME;
                info.password = DEFAULTED_ADMIN_PASSWORD;

                if ((rc = isOhcmAlive(&info, 1)) == OHCM_SUCCESS)
                {
                    // attempt to re-configure this camera, however need to do so in a background thread
                    // since that will involve removing the existing camera object and creating a new one
                    // (which can deadlock between SSDP discovery and the camera monitoring thread).
                    //
                    icLogInfo(LOG_TAG,
                              "discovered existing camera %s/%s; however it was 'reset to factory'.  attempting to re-configure...",
                              impl->macAddress,
                              impl->ipAddress);

                    createDetachedThread(addRediscoveredCameraThread, impl, "ohcmReAdd");
                }
            }

            // no need to move forward from here.  existing camera
            //
            return;
        }
    }

    pthread_mutex_lock(&discoverMutex);
    //Check our counter to see if we've hit the max for this session.
    camerasDiscoveredCounter += 1;
    uint32_t maximumToDiscover = getPropertyAsUInt32(MAX_CAMERAS_TO_DISCOVER_PROP_NAME, DEFAULT_MAX_CAMERAS_TO_DISCOVER);
    //If the property isn't set, we aren't going to enforce stopping at a maximum.
    if (maximumToDiscover != DEFAULT_MAX_CAMERAS_TO_DISCOVER)
    {
        if (camerasDiscoveredCounter == maximumToDiscover)
        {
            icLogDebug(LOG_TAG, "Found enough cameras, stopping discovery");
            //Usually used as callback for deviceService, but arguments are unused so we won't pass it anything.
            //Release the lock so ssdpStop can join its threadpool without us sitting on the lock for too long.
            pthread_mutex_unlock(&discoverMutex);
            discoverStop(NULL, NULL);
            //Don't really need the lock again, but so we don't try to unlock a lock we don't have.
            pthread_mutex_lock(&discoverMutex);
        }
        //Could end up with more than max getting to this point due to ssdp picking extras up before the
        //discoverStop call above has gone all the way through. If that happens, just don't continue with the discover
        else if(camerasDiscoveredCounter > maximumToDiscover)
        {
            icLogDebug(LOG_TAG, "Extra cameras reported by ssdp, discarding");
            pthread_mutex_unlock(&discoverMutex);
            return;
        }
    }
    pthread_mutex_unlock(&discoverMutex);

    // not an existing device, so add as a new camera
    //
    addDiscoveredCamera(ipAddress, macAddress);
}

/*---------------------------------------------------------------------------------------
 * discoverStop : stop discovering cameras
 *
 * Stop the discovery thread (if its running) and delete any 'pending' cameras (cameras
 * that have not been configured). Assumes discoverMutex is NOT held
 *---------------------------------------------------------------------------------------*/
static void discoverStop(void* ctx, const char* deviceClass)
{
    //unused
    (void) ctx;
    (void) deviceClass;

    icLogDebug(LOG_TAG, "discoverStop");

    // stop the SSDP discovery
    //
    if (discoveryRunning == true)
    {
        ohcmDiscoverStop();
        pthread_mutex_lock(&discoverMutex);
        discoveryRunning = false;
        camerasDiscoveredCounter = 0;
        pthread_mutex_unlock(&discoverMutex);
    }

}

/*---------------------------------------------------------------------------------------
 * deviceRemoved : adhere to 'deviceRemoved()' function as defined in deviceDriver.h
 * cleanup internal memory AFTER the device has been removed from the database
 *---------------------------------------------------------------------------------------*/
static void deviceRemoved(void* ctx, icDevice* device)
{
    if (device != NULL && device->uuid != NULL)
    {
        icLogDebug(LOG_TAG, "deviceRemoved: %s", device->uuid);

        // locate this from our 'allCameras' set, then
        // attempt to reset the device to factory (handy
        // in lab environments)
        //
        cameraDevice* impl = findCameraByUuid(allCameras, device->uuid);
        if (impl != NULL)
        {
            // first stop monitoring, however it is possible that the monitor
            // thread is stuck waiting for an SSDP locate of the device and
            // we don't want to cause a deadlock
            //
            bool waitForMonitorHalt = true;
            if (impl->opState == CAMERA_OP_STATE_OFFLINE)
            {
                waitForMonitorHalt = false;
            }
            cameraDeviceStopMonitorThread(impl, waitForMonitorHalt);
            cameraDeviceDestroyMediaTunnel(impl, NULL);

            // attempt to reset the camera to factory defaults
            //
            if (impl->opState != CAMERA_OP_STATE_OFFLINE)
            {
                cameraDeviceResetToFactory(impl);
            }

            // now delete this from our 'allCamera' set
            //
            destroyCameraDeviceFromSet(allCameras, device->uuid);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "unable to remove device, missing uuid");
    }
}

/*---------------------------------------------------------------------------------------
 * configureDevice : adhere to 'configureDevice()' function as defined in deviceDriver.h
 * configure the camera according to the 'device descriptor'
 *---------------------------------------------------------------------------------------*/
static bool configureDevice(void* ctx, icDevice* device, DeviceDescriptor* descriptor)
{
    icLogDebug(LOG_TAG, "configureDevice: uuid=%s", device->uuid);
    bool result = false;

    // need to configure a newly discovered device.  it should be
    // cached and sitting in our pendingCamera set.
    //
    cameraDevice* camDevice = findCameraByUuid(pendingCameras, device->uuid);
    if (camDevice == NULL)
    {
        icLogError(LOG_TAG, "configureDevice could not locate the pending camera %s!", device->uuid);
        return false;
    }

    //Move the camera over to configuring set
    moveCameraDeviceToSet(camDevice->uuid, pendingCameras, configuringCameras);

    // We should upgrade now if camera firmware version upgrade state is NECESSARY
    //
    cameraNeedsFirmwareState camFirmwareState =
                                    earlyFirmwareVersionCompare(camDevice, descriptor);

    if (camFirmwareState == FW_UPGRADE_NECESSARY)
    {
        // perform the upgrade
        //
        icLogDebug(LOG_TAG,
                   "upgrading camera %s to firmware version %s",
                   device->uuid,
                   descriptor->latestFirmware->version);

        if (cameraDevicePerformUpgrade(camDevice, getCameraUpgradeFilename(descriptor->latestFirmware->filenames),
                                       descriptor->latestFirmware->version, DETAULT_FW_UPDATE_TIMEOUT_SECS) == false)
        {
            // failed to upgrade, cannot continue
            //
            icLogWarn(LOG_TAG,
                      "error upgrading camera firmware of %s; unable to proceed with 'configureDevice'",
                      device->uuid);
            return false;
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "camera meets minimum fw version.  not upgrading at this time");
    }

    // First, configure the camera for the desired settings (but need to see if this is a re-config or not)
    //
    bool isReconfig = false;
    if (camDevice->opState == CAMERA_OP_STATE_OFFLINE)
    {
        // only devices that are OFFLINE would be a 'reconfigure'
        isReconfig = true;
    }

    result = cameraDeviceConfigure(camDevice, (CameraDeviceDescriptor*) descriptor, isReconfig);
    if (result == false)
    {
        //Failed to configure the camera

        //We're going to try and factory default the camera, so the user doesn't need to.
        if (camDevice->opState != CAMERA_OP_STATE_OFFLINE)
        {
            cameraDeviceResetToFactory(camDevice);
        }

        // remove this from the 'pending' list
        //
        icLogError(LOG_TAG, "Error - camera configuration failed");
        destroyCameraDeviceFromSet(configuringCameras, camDevice->uuid);
    }
    else
    {
        // transfer from 'configuringCameras' to 'allCamera'
        //
        moveCameraDeviceToSet(camDevice->uuid, configuringCameras, allCameras);

        // now start the monitoring of this device
        //
        cameraDeviceStartMonitorThread(camDevice);

        // setup a delayed task to check for firmware updates.  this handles situations
        // where the camera met the min firmware version, but is lower then the desired.
        // This must be scheduled before we process device descriptors so that we use
        // the "camera.fw.update.delay.seconds" property instead of a random hour.
        //
        scheduleDelayedCameraUpdateTask(false);

        // start our ip address recovery task (if it isnt already started) to relocate
        // cameras that have gone into comm fail due to IP address change
        //
        startIpAddressRecoveryTask();
    }
    return result;
}

/*---------------------------------------------------------------------------------------
 * fetchInitialResourceValues : adhere to 'fetchInitialResourceValues()' function as defined in deviceDriver.h
 * Set initial values for camera's resources
 *---------------------------------------------------------------------------------------*/
static bool fetchInitialResourceValues(void* ctx, icDevice* device, icInitialResourceValues *initialResourceValues)
{
    cameraDevice* camDevice = findCameraByUuid(allCameras, device->uuid);

    if (camDevice == NULL)
    {
        icLogError(LOG_TAG, "fetchInitialResourceValues could not locate the camera %s!", device->uuid);
        return false;
    }

    // note that we'll make 1 device and 2 endpoints:
    //  1. camera settings endpoint (resolution, stream info, etc)
    //  2. motion sensor endpoint
    //

    // first, make the device
    //
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_MAC_ADDRESS, camDevice->macAddress);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_IP_ADDRESS, camDevice->ipAddress);
    // Firmware version on the device is init by the device found details, but we might have done a mandatory upgrade.
    // Update it from a correct source.
    if (camDevice->details != NULL)
    {
        initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, camDevice->details->firmwareVersion);
    }

    char portStr[6]; //65535+\0
    sprintf(portStr, "%u", HTTPS_PORT);
    initialResourceValuesPutDeviceValue(initialResourceValues, CAMERA_PROFILE_RESOURCE_PORT_NUMBER, portStr);

    // support 'timezone'
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_TIMEZONE, "");

    //Signal strength resource (note we don't cache this so we can update this every time its requested)
    initialResourceValuesPutDeviceValue(initialResourceValues, CAMERA_PROFILE_RESOURCE_SIGNAL_STRENGTH, NULL);

    // hardware info
    if (camDevice->details != NULL)
    {
        initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER,
                                            camDevice->details->serialNumber);
    }

    // now, setup the 'camera settings endpoint'
    //
    uint16_t labelCounter = cameraSetCount(allCameras);
    char *defaultLabel = stringBuilder("My Camera %d", labelCounter);
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          COMMON_ENDPOINT_RESOURCE_LABEL, defaultLabel);
    free(defaultLabel);

    // login credentials
    if (camDevice->adminCredentials != NULL)
    {
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID,
                                              camDevice->adminCredentials->username);
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD,
                                              camDevice->adminCredentials->password);
    }

    if (camDevice->userCredentials != NULL)
    {
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_USER_USER_ID,
                                              camDevice->userCredentials->username);
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_USER_PASSWORD,
                                              camDevice->userCredentials->password);
    }

    //Picture URL
    char *picURL = getMediaUrl(camDevice->ipAddress, "SNAPSHOT", NULL);
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          CAMERA_PROFILE_RESOURCE_PIC_URL,
                                          picURL);
    free(picURL);

    //Video Formats/Codecs
    cJSON *parentObject = getVideoInformation(camDevice);
    char *videoObjectString = cJSON_PrintUnformatted(parentObject);
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          CAMERA_PROFILE_RESOURCE_VIDEO_INFORMATION,
                                          videoObjectString);
    free(videoObjectString);
    cJSON_Delete(parentObject);

    // API version
    if (camDevice->details != NULL)
    {
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_API_VERSION,
                                              camDevice->details->apiVersion);
    }

    // video settings
    if (camDevice->videoSettings != NULL)
    {
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_RESOLUTION,
                                              camDevice->videoSettings->videoResolution);
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                              CAMERA_PROFILE_RESOURCE_ASPECT_RATIO,
                                              camDevice->videoSettings->aspectRatio);
    }

    // functionality flags.  most are just setting up for operations later on
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          CAMERA_PROFILE_RESOURCE_RECORDABLE,
                                          "true");
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          CAMERA_PROFILE_RESOURCE_MOTION_CAPABLE,
                                          (camDevice->motionPossible) ? "true" : "false");

    // now the second endpoint, motion sensor
    //
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                          SENSOR_PROFILE_RESOURCE_MOTION_SENSITIVITY,
                                          "low");
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                          SENSOR_PROFILE_RESOURCE_FAULTED,
                                          "false");
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                          SENSOR_PROFILE_RESOURCE_TAMPERED,
                                          "false");
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                          SENSOR_PROFILE_RESOURCE_TYPE,
                                          SENSOR_PROFILE_MOTION_TYPE);
    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                          SENSOR_PROFILE_RESOURCE_BYPASSED,
                                          camDevice->motionEnabled ? "false" : "true");

    // add values for the button endpoint if the camera has one
    if (camDevice->hasUserButton == true)
    {
        initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_BUTTON_PROFILE_ENDPOINT_ID,
                                              BUTTON_PROFILE_RESOURCE_PRESSED,
                                              NULL);
    }

    // add values for the speaker endpoint if the camera has one
    if (camDevice->hasSpeaker == true)
    {
        // No values here currently, just executable resources
    }

    return true;
}

/*---------------------------------------------------------------------------------------
 * registerResources : adhere to 'registerResources()' function as defined in deviceDriver.h
 * Register the camera's resources with device service.
 *---------------------------------------------------------------------------------------*/
static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    cameraDevice *camDevice = findCameraByUuid(allCameras, device->uuid);

    if (camDevice == NULL)
    {
        icLogError(LOG_TAG, "registerResources could not locate the camera %s!", device->uuid);
        return false;
    }

    // note that we'll make 1 device and 2 endpoints:
    //  1. camera settings endpoint (resolution, stream info, etc)
    //  2. motion sensor endpoint
    //

    // first, make the device
    //
    bool result = createDeviceResourceIfAvailable(device,
                                                  COMMON_DEVICE_RESOURCE_MAC_ADDRESS,
                                                  initialResourceValues,
                                                  RESOURCE_TYPE_MAC_ADDRESS,
                                                  RESOURCE_MODE_READABLE,
                                                  CACHING_POLICY_ALWAYS) != NULL;

    // NOTE: if the ip address changes, we will find it again via ssdp discovery and update the cached resource value
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_IP_ADDRESS,
                                              initialResourceValues,
                                              RESOURCE_TYPE_IP_ADDRESS,
                                              RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                              RESOURCE_MODE_EMIT_EVENTS,
                                              CACHING_POLICY_ALWAYS) != NULL;

    result &= createDeviceResourceIfAvailable(device,
                                              CAMERA_PROFILE_RESOURCE_PORT_NUMBER,
                                              initialResourceValues,
                                              RESOURCE_TYPE_IP_PORT,
                                              RESOURCE_MODE_READABLE,
                                              CACHING_POLICY_ALWAYS) != NULL;

    // support 'timezone'
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_TIMEZONE,
                                              initialResourceValues,
                                              RESOURCE_TYPE_TIMEZONE,
                                              RESOURCE_MODE_READWRITEABLE,
                                              CACHING_POLICY_ALWAYS) != NULL;

    //Signal strength resource (note we don't cache this so we can update this every time its requested)
    result &= createDeviceResourceIfAvailable(device,
                                              CAMERA_PROFILE_RESOURCE_SIGNAL_STRENGTH,
                                              initialResourceValues,
                                              RESOURCE_TYPE_STRING,
                                              RESOURCE_MODE_READABLE,
                                              CACHING_POLICY_NEVER) != NULL;

    // reboot & ping functions
    result &= createDeviceResource(device,
                                   CAMERA_PROFILE_FUNCTION_REBOOT,
                                   NULL,
                                   RESOURCE_TYPE_REBOOT_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER) != NULL;

    // WiFi credential functions
    result &= createDeviceResource(device,
                                   CAMERA_PROFILE_FUNCTION_WIFI_CREDENTIALS,
                                   NULL,
                                   RESOURCE_TYPE_WIFI_CREDENTIALS_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER) != NULL;

    result &= createDeviceResource(device,
                                   CAMERA_PROFILE_FUNCTION_PING,
                                   NULL,
                                   RESOURCE_TYPE_PING_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER) != NULL;

    // hardware info
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_SERIAL_NUMBER,
                                              initialResourceValues,
                                              RESOURCE_TYPE_SERIAL_NUMBER,
                                              RESOURCE_MODE_READABLE,
                                              CACHING_POLICY_ALWAYS) != NULL;

    // now, make the 'camera settings endpoint'
    //
    icDeviceEndpoint *camEndpoint = createEndpoint(device, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID, CAMERA_PROFILE, true);

    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                COMMON_ENDPOINT_RESOURCE_LABEL,
                                                initialResourceValues,
                                                RESOURCE_TYPE_LABEL,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    // login credentials
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID,
                                                initialResourceValues,
                                                RESOURCE_TYPE_USER_ID,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS |
                                                RESOURCE_MODE_SENSITIVE,
                                                CACHING_POLICY_ALWAYS) != NULL;
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD,
                                                initialResourceValues,
                                                RESOURCE_TYPE_PASSWORD,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS |
                                                RESOURCE_MODE_SENSITIVE,
                                                CACHING_POLICY_ALWAYS) != NULL;
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_USER_USER_ID,
                                                initialResourceValues,
                                                RESOURCE_TYPE_USER_ID,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS |
                                                RESOURCE_MODE_SENSITIVE,
                                                CACHING_POLICY_ALWAYS) != NULL;
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_USER_PASSWORD,
                                                initialResourceValues,
                                                RESOURCE_TYPE_PASSWORD,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS |
                                                RESOURCE_MODE_SENSITIVE,
                                                CACHING_POLICY_ALWAYS) != NULL;


    //Picture URL
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_PIC_URL,
                                                initialResourceValues,
                                                RESOURCE_TYPE_STRING,
                                                RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    //Video Formats/Codecs
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_VIDEO_INFORMATION,
                                                initialResourceValues,
                                                RESOURCE_TYPE_STRING,
                                                RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    // API version
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_API_VERSION,
                                                initialResourceValues,
                                                RESOURCE_TYPE_VERSION,
                                                RESOURCE_MODE_READABLE,
                                                CACHING_POLICY_ALWAYS) != NULL;

    // video settings
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_RESOLUTION,
                                                initialResourceValues,
                                                RESOURCE_TYPE_VIDEO_RESOLUTION,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_ASPECT_RATIO,
                                                initialResourceValues,
                                                RESOURCE_TYPE_VIDEO_ASPECT_RATIO,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    // functionality flags.  most are just setting up for operations later on
    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_RECORDABLE,
                                                initialResourceValues,
                                                RESOURCE_TYPE_BOOLEAN,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResourceIfAvailable(camEndpoint,
                                                CAMERA_PROFILE_RESOURCE_MOTION_CAPABLE,
                                                initialResourceValues,
                                                RESOURCE_TYPE_BOOLEAN,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResource(camEndpoint,
                                     CAMERA_PROFILE_FUNCTION_CREATE_MEDIA_TUNNEL,
                                     NULL,
                                     RESOURCE_TYPE_CREATE_MEDIA_TUNNEL_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != NULL;

    result &= createEndpointResource(camEndpoint,
                                     CAMERA_PROFILE_FUNCTION_DESTROY_MEDIA_TUNNEL,
                                     NULL,
                                     RESOURCE_TYPE_DESTROY_MEDIA_TUNNEL_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != NULL;

    result &= createEndpointResource(camEndpoint,
                                     CAMERA_PROFILE_FUNCTION_GET_PICTURE,
                                     NULL,
                                     RESOURCE_TYPE_GET_PICTURE_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != NULL;

    result &= createEndpointResource(camEndpoint,
                                     CAMERA_PROFILE_FUNCTION_UPLOAD_VIDEO_CLIP,
                                     NULL,
                                     RESOURCE_TYPE_UPLOAD_VIDEO_CLIP_OPERATION,
                                     RESOURCE_MODE_EXECUTABLE,
                                     CACHING_POLICY_NEVER) != NULL;

    // now the second endpoint, motion sensor
    //
    icDeviceEndpoint *motionEndpoint = createEndpoint(device,
                                                      CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                                      SENSOR_PROFILE,
                                                      true);

    result &= createEndpointResourceIfAvailable(motionEndpoint,
                                                SENSOR_PROFILE_RESOURCE_MOTION_SENSITIVITY,
                                                initialResourceValues,
                                                RESOURCE_TYPE_MOTION_SENSITIVITY,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResourceIfAvailable(motionEndpoint,
                                                SENSOR_PROFILE_RESOURCE_FAULTED,
                                                initialResourceValues,
                                                RESOURCE_TYPE_BOOLEAN,
                                                RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResourceIfAvailable(motionEndpoint,
                                                SENSOR_PROFILE_RESOURCE_TAMPERED,
                                                initialResourceValues,
                                                RESOURCE_TYPE_BOOLEAN,
                                                RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResourceIfAvailable(motionEndpoint,
                                                SENSOR_PROFILE_RESOURCE_TYPE,
                                                initialResourceValues,
                                                RESOURCE_TYPE_SENSOR_TYPE,
                                                RESOURCE_MODE_READABLE,
                                                CACHING_POLICY_ALWAYS) != NULL;

    result &= createEndpointResourceIfAvailable(motionEndpoint,
                                                SENSOR_PROFILE_RESOURCE_BYPASSED,
                                                initialResourceValues,
                                                RESOURCE_TYPE_BOOLEAN,
                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                CACHING_POLICY_ALWAYS) != NULL;

    // add the button endpoint if the camera has one
    if (camDevice->hasUserButton == true)
    {
        icDeviceEndpoint *buttonEndpoint = createEndpoint(device,
                                                          CAMERA_DC_BUTTON_PROFILE_ENDPOINT_ID,
                                                          BUTTON_PROFILE,
                                                          true);

        result &= createEndpointResourceIfAvailable(buttonEndpoint,
                                                    BUTTON_PROFILE_RESOURCE_PRESSED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BUTTON_PRESSED,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_NEVER) != NULL;
    }

    // add the speaker endpoint if the camera has one
    if (camDevice->hasSpeaker == true)
    {
        icDeviceEndpoint *buttonEndpoint = createEndpoint(device,
                                                          CAMERA_DC_SPEAKER_PROFILE_ENDPOINT_ID,
                                                          SPEAKER_PROFILE,
                                                          true);

        result &= createEndpointResource(buttonEndpoint,
                                         SPEAKER_PROFILE_FUNCTION_CREATE_MEDIA_TUNNEL,
                                         NULL,
                                         RESOURCE_TYPE_CREATE_MEDIA_TUNNEL_OPERATION,
                                         RESOURCE_MODE_EXECUTABLE,
                                         CACHING_POLICY_NEVER) != NULL;

        result &= createEndpointResource(buttonEndpoint,
                                         SPEAKER_PROFILE_FUNCTION_DESTROY_MEDIA_TUNNEL,
                                         NULL,
                                         RESOURCE_TYPE_DESTROY_MEDIA_TUNNEL_OPERATION,
                                         RESOURCE_MODE_EXECUTABLE,
                                         CACHING_POLICY_NEVER) != NULL;
    }

    return result;
}

/*
 * callback function provided to cameraDevice so we get notified
 * when something changes on the camera.
 */
static void cameraDeviceCallback(cameraDevice* device, cameraAttrChange reason)
{
    // save info in persistence, based on the attribute that changed
    //
    switch (reason)
    {
        case CAMERA_OFFLINE_CHANGE:
        {
            // Log line used for Telemetry... DO NOT CHANGE
            //
            icLogDebug(LOG_TAG, "persisting that camera is offline from commFail, camera %s", device->uuid);

            deviceServiceCallbacks->updateResource(device->uuid, 0, COMMON_DEVICE_RESOURCE_COMM_FAIL, "true", NULL);
            break;
        }

        case CAMERA_ONLINE_CHANGE:
        {
            // Log line used for Telemetry... DO NOT CHANGE
            //
            icLogDebug(LOG_TAG, "persisting that camera is online from commFailRestore, camera %s", device->uuid);

            // save the fact the camera is NOT in comm failure, and produce an event
            //
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   0,
                                                   COMMON_DEVICE_RESOURCE_COMM_FAIL,
                                                   "false",
                                                   NULL);
            break;
        }

        case CAMERA_FIRMWARE_CHANGE:
        {
            // save the new firmware version (requires the mutex)
            //
            pthread_mutex_lock(&device->mutex);
            icLogDebug(LOG_TAG,
                       "persisting that camera %s has new firmware version %s",
                       device->uuid,
                       device->details->firmwareVersion);
            deviceServiceCallbacks->updateResource(device->uuid, 0, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                                                   device->details->firmwareVersion, NULL);
            pthread_mutex_unlock(&device->mutex);
            break;
        }

        case CAMERA_MOTION_FAULT_CHANGE:
        {
            icLogDebug(LOG_TAG, "persisting that camera %s has MOTION", device->uuid);
            deviceServiceCallbacks->updateResource(device->uuid, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                                   SENSOR_PROFILE_RESOURCE_FAULTED, "true", NULL);
            break;
        }

        case CAMERA_MOTION_CLEAR_CHANGE:
        {
            icLogDebug(LOG_TAG, "persisting that camera %s has NO-MOTION", device->uuid);
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                                   SENSOR_PROFILE_RESOURCE_FAULTED,
                                                   "false",
                                                   NULL);
            break;
        }

        case CAMERA_BUTTON_PRESSED:
        {
            deviceServiceCallbacks->updateResource(device->uuid, CAMERA_DC_BUTTON_PROFILE_ENDPOINT_ID,
                                                   BUTTON_PROFILE_RESOURCE_PRESSED, NULL, NULL);
            break;
        }
    }
}

/*---------------------------------------------------------------------------------------
 * readResource : adhere to 'readResource()' function as defined in deviceDriver.h
 *---------------------------------------------------------------------------------------*/
static bool readResource(void* ctx, icDeviceResource* resource, char** value)
{
    bool retVal = false;
    if (resource == NULL || value == NULL)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "readResource %s", resource->id);

    if(resource->endpointId == NULL)
    {
        if (strcmp(resource->id, CAMERA_PROFILE_RESOURCE_SIGNAL_STRENGTH) == 0)
        {
            ohcmCameraInfo *camera = getCamInfo(resource->deviceUuid);
            if (camera != NULL)
            {
                //Try to get the wireless status with up to 5 retries.
                ohcmWirelessStatus *status = createOhcmWirelessStatus();
                char *netIfaceId = "0";
                ohcmResultCode resultCode = getWirlessStatusOhcmCamera(camera, netIfaceId, status, 5);
                if (resultCode == OHCM_SUCCESS && status != NULL)
                {
                    *value = stringBuilder("%d", status->rssidB);
                    retVal = true;
                }
                else
                {
                    icLogWarn(LOG_TAG, "Failed to fetch resource: signal strength");
                }
                destroyOhcmWirelessStatus(status);
                destroyOhcmCameraInfo(camera);
            }
        }
    }
    else
    {
        //All endpoint resources are cached.
    }

    return retVal;
}

/*
 * called by 'writeResource', so react to attribute changes specific to the "camera endpoint".
 * anything here should be attributes with a cache policy of NEVER - meaning the driver is responsible
 * for saving/applying the values.  in this case, we tell the camera to do stuff...
 */
static bool executeCameraEndpointResource(icDeviceResource* resource, const char* arg, char** response)
{
    bool result = true;

    // look for attribute changes that we need to react to.
    // for the most part, applying those to the device
    //
    cameraDevice* camDevice = findCameraByUuid(allCameras, resource->deviceUuid);
    if (camDevice == NULL)
    {
        icLogDebug(LOG_TAG,
                   "unable to process request '%s', cannot locate camera with uuid of %s",
                   resource->id,
                   resource->deviceUuid);
        return false;
    }

    if (strcmp(CAMERA_PROFILE_FUNCTION_CREATE_MEDIA_TUNNEL, resource->id) == 0)
    {
        if (arg != NULL)
        {
            *response = cameraDeviceCreateMediaTunnel(camDevice, (char*) arg);
            if (*response != NULL)
            {
                result = true;
            }
        }
    }
    else if (strcmp(CAMERA_PROFILE_FUNCTION_DESTROY_MEDIA_TUNNEL, resource->id) == 0)
    {
        if (arg != NULL)
        {
            result = cameraDeviceDestroyMediaTunnel(camDevice, (char*) arg);
        }
    }
/** TODO: do we need this?  doesn't look like it's used anywhere
    else if (strcmp(CAMERA_PROFILE_FUNCTION_GET_MEDIA_TUNNEL_STATUS, resource->id) == 0)
    {
        MEDIATUNNELSTATUSLIST *status;

        RET_TYPE ret = getMediaTunnel(cam, &status, CONNECTION_RETRY_COUNT);

        cJSON *top = cJSON_CreateObject();

        if (ret == SUCCESS && status != NULL)
        {
            MEDIATUNNELSTATUSLIST *stat = status;

            cJSON *statuses = cJSON_CreateArray();
            cJSON_AddItemToObject(top, "statuses", statuses);

            while (stat != NULL)
            {
                cJSON *s = cJSON_CreateObject();

                cJSON_AddStringToObject(s, "sessionID", stat->sessionID);
                cJSON_AddNumberToObject(s, "elapsedTime", *stat->elapsedTime);
                cJSON_AddStringToObject(s, "state", stat->state);
                cJSON_AddStringToObject(s, "transportSecurity", stat->transportSecurity);

                char time_buf[265];
                strftime(time_buf, sizeof(time_buf), "\"%m-%d-%Y %H:%M", stat->startTime);
                cJSON_AddStringToObject(s, "startTime", time_buf);

                cJSON_AddItemToArray(statuses, s);
                stat = stat->Next;
            }

            cJSON_AddTrueToObject(top, "success");
            destroyMediaTunnelStruct(status);
            result = true;
        }
        else
        {
            cJSON_AddFalseToObject(top, "success");
            result = false;
        }

        *response = cJSON_Print(top);
        cJSON_Delete(top);
    }
  **/
    else if (strcmp(CAMERA_PROFILE_FUNCTION_GET_PICTURE, resource->id) == 0)
    {
        // take a picture, saving in the filename 'arg'
        //
        result = cameraDeviceTakePicture(camDevice, (char*) arg);
    }
    else if (strcmp(CAMERA_PROFILE_FUNCTION_UPLOAD_VIDEO_CLIP, resource->id) == 0)
    {
        result = cameraDeviceTakeVideoClip(camDevice, (char*) arg, 15);
    }

    return result;
}

/*
 * called by 'writeResource', so react to attribute changes specific to the "speaker endpoint".
 * anything here should be attributes with a cache policy of NEVER - meaning the driver is responsible
 * for saving/applying the values.  in this case, we tell the camera to do stuff...
 */
static bool executeSpeakerEndpointResource(icDeviceResource* resource, const char* arg, char** response)
{
    bool result = true;

    cameraDevice* camDevice = findCameraByUuid(allCameras, resource->deviceUuid);
    if (camDevice == NULL)
    {
        icLogDebug(LOG_TAG,
                   "unable to process request '%s', cannot locate camera with uuid of %s",
                   resource->id,
                   resource->deviceUuid);
        return false;
    }

    if (strcmp(SPEAKER_PROFILE_FUNCTION_CREATE_MEDIA_TUNNEL, resource->id) == 0)
    {
        if (arg != NULL)
        {
            *response = cameraDeviceCreateMediaTunnel(camDevice, (char*) arg);
            if (*response != NULL)
            {
                result = true;
            }
        }
    }
    else if (strcmp(SPEAKER_PROFILE_FUNCTION_DESTROY_MEDIA_TUNNEL, resource->id) == 0)
    {
        if (arg != NULL)
        {
            result = cameraDeviceDestroyMediaTunnel(camDevice, (char*) arg);
        }
    }

    return result;
}

/*
 * called by 'writeResource', so react to attribute changes specific to the "motion sensor endpoint".
 * anything here should be attributes with a cache policy of NEVER - meaning the driver is responsible
 * for saving/applying the values.  in this case, we tell the camera to do stuff...
 */
static bool writeMotionEndpointResource(icDeviceResource* resource, const char* previousValue, const char* newValue)
{
    // look for attribute changes that we need to react to.
    // for the most part, applying those to the device
    //
    cameraDevice* camDevice = findCameraByUuid(allCameras, resource->deviceUuid);
    if (camDevice == NULL)
    {
        return false;
    }

    if (strcmp(resource->id, SENSOR_PROFILE_RESOURCE_BYPASSED) == 0 &&
        newValue != NULL &&
        previousValue != NULL &&
        strcmp(previousValue, newValue) != 0)
    {
        // if bypass == "true", then disable motion
        //
        if (strcmp(newValue, "true") == 0)
        {
            // stop monitoring
            // TODO: see if we have the eventThreadsMutex locked.  all other calls to stopEventThread do...
            icLogDebug(LOG_TAG, "Disabling motion detection for UUID = %s\n", resource->deviceUuid);
            cameraDeviceEnableMotionDetection(camDevice, false);
        }
            /* else - enable the motion detection */
        else
        {
            // start monitoring for motion events
            icLogDebug(LOG_TAG, "Enabling motion detection for UUID = %s\n", resource->deviceUuid);
            cameraDeviceEnableMotionDetection(camDevice, true);
        }

        // reset our fault resource
        deviceServiceCallbacks->updateResource(resource->deviceUuid, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID,
                                               SENSOR_PROFILE_RESOURCE_FAULTED, "false", NULL);
    }
    else if (strcmp(resource->id, SENSOR_PROFILE_RESOURCE_MOTION_SENSITIVITY) == 0)
    {
        if (newValue != NULL)
        {
            /* handle 'low', 'medium', 'high' sensitivity.  DO NOT HANDLE 'off' (that is bypass above) */
            cameraMotionSensitivity level = CAMERA_MOTION_SENSITIVITY_LOW;
            if (strcmp(newValue, "low") == 0)
            {
                level = CAMERA_MOTION_SENSITIVITY_LOW;
            }
            else if (strcmp(newValue, "medium") == 0)
            {
                level = CAMERA_MOTION_SENSITIVITY_MEDIUM;
            }
            else if (strcmp(newValue, "high") == 0)
            {
                level = CAMERA_MOTION_SENSITIVITY_HIGH;
            }
            else
            {
                // invalid
                icLogWarn(LOG_TAG, "unable to set motion sensitivity on %s to %s", camDevice->uuid, newValue);
                return false;
            }

            icLogDebug(LOG_TAG, "Processing Motion Sensitivity change from %s to %s for UUID = %s\n",
                       previousValue, newValue, resource->deviceUuid);
            return cameraDeviceSetMotionDetectionSensitivity(camDevice, level);
        }
        else
        {
            icLogWarn(LOG_TAG, "unable to set motion sensitivity on %s to NULL", camDevice->uuid);
            return false;
        }
    }

    return true;
}

/*---------------------------------------------------------------------------------------
 * executeResource : adhere to 'executeResource()' function as defined in deviceDriver.h
 *---------------------------------------------------------------------------------------*/
static bool executeResource(void* ctx, icDeviceResource* resource, const char* arg, char** response)
{
    bool result = true;

    if (resource == NULL)
    {
        icLogDebug(LOG_TAG, "executeResource: invalid arguments");
        return false;
    }

    cameraDevice* camDevice = findCameraByUuid(allCameras, resource->deviceUuid);
    if (camDevice == NULL)
    {
        icLogWarn(LOG_TAG, "executeResource: unable to find camera %s", resource->deviceUuid);
        return false;
    }

    if (resource->endpointId == NULL)
    {
        icLogDebug(LOG_TAG, "executeResource on device: id=%s", resource->id);
        icLogTrace(LOG_TAG, "executeResource arguments=%s", arg);

        // altering the camera device
        //
        if (strcmp(CAMERA_PROFILE_FUNCTION_REBOOT, resource->id) == 0)
        {
            // being asked to perform a reboot
            //
            if (camDevice->isIntegratedPeripheral == false)
            {
                /* Reboot the external camera, block until it is back up */
                if (cameraDeviceReboot(camDevice, true, CAMERA_REBOOT_TIMEOUT_SECONDS) == true)
                {
                    icLogDebug(LOG_TAG, "Camera rebooted");
                    result = true;
                }
                else
                {
                    icLogWarn(LOG_TAG, "Camera failed to reboot");
                }
            }
            else
            {
                /* if camera is the hub, just return true */
                icLogDebug(LOG_TAG, "Camera reboot - not rebooting since camera is the integrated device");
                result = true;
            }
        }
        else if (strcmp(CAMERA_PROFILE_FUNCTION_PING, resource->id) == 0)
        {
            // being asked to perform a ping
            //
            if (cameraDevicePing(camDevice, 15) == true)
            {
                icLogDebug(LOG_TAG, "success ping of camera %s", camDevice->uuid);
                result = true;
            }
            else
            {
                icLogWarn(LOG_TAG, "failed to ping camera %s", camDevice->uuid);
                result = false;
            }
        }
        else if (strcmp(resource->id, CAMERA_PROFILE_FUNCTION_WIFI_CREDENTIALS) == 0)
        {
            cJSON* credsJson = cJSON_Parse(arg);

            if (credsJson)
            {
                const char* ssid;
                const char* passphrase;

                cJSON* json;

                json = cJSON_GetObjectItem(credsJson, "ssid");
                ssid = (json == NULL) ? NULL : json->valuestring;

                json = cJSON_GetObjectItem(credsJson, "passphrase");
                passphrase = (json == NULL) ? NULL : json->valuestring;

                result = cameraDeviceSetWiFiNetworkCredentials(camDevice, ssid, passphrase);
                if (!result)
                {
                    icLogWarn(LOG_TAG, "Failed to set new WiFi credentials for camera.");
                }

                cJSON_Delete(credsJson);
            }
            else
            {
                icLogWarn(LOG_TAG, "Unable to write resource for wifiCredentials");
                result = false;
            }
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "executeResource on endpoint %s: id=%s, arg=%s", resource->endpointId, resource->id, arg);
        if (strcmp(CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID, resource->endpointId) == 0)
        {
            result = executeCameraEndpointResource(resource, arg, response);
        }
        else if (strcmp(CAMERA_DC_SPEAKER_PROFILE_ENDPOINT_ID, resource->endpointId) == 0)
        {
            result = executeSpeakerEndpointResource(resource, arg, response);
        }
        else
        {
            result = false;
        }
    }

    return result;
}

/*---------------------------------------------------------------------------------------
 * writeResource : adhere to 'writeResource()' function as defined in deviceDriver.h
 *---------------------------------------------------------------------------------------*/
static bool writeResource(void* ctx, icDeviceResource* resource, const char* previousValue, const char* newValue)
{
    bool result = true;

    if (resource == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource: invalid arguments");
        return false;
    }

    cameraDevice* camDevice = findCameraByUuid(allCameras, resource->deviceUuid);
    if (camDevice == NULL)
    {
        icLogWarn(LOG_TAG, "writeResource: unable to find camera %s", resource->deviceUuid);
        return false;
    }

    if (resource->endpointId == NULL)
    {
        icLogDebug(LOG_TAG,
                   "writeResource on device: id=%s, previousValue=%s, newValue=%s",
                   resource->id,
                   previousValue,
                   newValue);

        // altering the camera device
        //
        if (newValue != NULL)
        {
            if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_TIMEZONE) == 0)
            {
                // TODO: move 'timezone' support to cameraDevice

                // adjust the timezone of the camera.  the 'newValue' should be POSIX 1003.1 compliant,
                // which is what the OpenHome spec requires.
                //
                ohcmCameraInfo* info = getCamInfo(resource->deviceUuid);
                if (info == NULL)
                {
                    return false;
                }

                if (isIcLogPriorityTrace() == true)
                {
                    // get the current timezone
                    //
                    char theZone[128];
                    if (getOhcmTimeZoneInfo(info, theZone, 1) == OHCM_SUCCESS)
                    {
                        icLogTrace(LOG_TAG, "camera %s has timezone set to '%s'", info->cameraIP, theZone);
                    }
                }

                // apply the timezone
                //
                if (setOhcmTimeZoneInfo(info, newValue, 1) == OHCM_SUCCESS)
                {
                    icLogDebug(LOG_TAG, "success applying timezone '%s' to camera %s", newValue, info->cameraIP);
                }
                else
                {
                    icLogWarn(LOG_TAG, "error applying timezone '%s' to camera %s", newValue, info->cameraIP);
                }

                destroyOhcmCameraInfo(info);
            }
        }
    }
    else
    {
        icLogDebug(LOG_TAG,
                   "writeResource on endpoint %s: id=%s, previousValue=%s, newValue=%s",
                   resource->endpointId,
                   resource->id,
                   previousValue,
                   newValue);
        if (strcmp(CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID, resource->endpointId) == 0)
        {
            result = writeMotionEndpointResource(resource, previousValue, newValue);
        }
        else if (strcmp(CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID, resource->endpointId) == 0)
        {
            //TODO we just blindly say that the write was successful.  We need to check that the attribute was
            // actually writable, and send any changes along to the camera if required.  Stuff like changing the label
            // are just handled from the updateResource call below.
            result = true;
        }
        else
        {
            result = false;
        }
    }

    //TODO if this is an resource that actually goes to the camera, send it along.  For now, we will just pass up and pretend everything is good
    if (result)
    {
        //We should only update the resource if we haven't run into any problems thus far. Otherwise, it's possible for
        // an endpoint resource write to fail but the resource here to still get updated, causing a corrupted state.
        deviceServiceCallbacks->updateResource(resource->deviceUuid,
                                               resource->endpointId,
                                               resource->id,
                                               newValue,
                                               NULL);
    }

    return result;
}

static const char* getCameraClassForModel(char* model)
{
    // TODO: the fact that this is a doorbell camera should come from somewhere like the device descriptor
    //char* deviceClass;
    if (model != NULL && strcmp("DBC831", model) == 0)
    {
        return DOORBELL_CAMERA_DC;
    }
    return CAMERA_DC;
}

/*---------------------------------------------------------------------------------------
* addDiscoveredCamera : add a newly-discovered camera to the device service
*---------------------------------------------------------------------------------------*/
static bool addDiscoveredCamera(const char* ipAddress, const char* macAddress)
{
    // create a cameraDevice, and let it probe the device for details
    //
    bool retVal = false;
    ohcmResultCode rc;
    cameraDevice* discovered = createCameraDevice(NULL, ipAddress, NULL,
                                                  DEFAULTED_ADMIN_USERNAME, DEFAULTED_ADMIN_PASSWORD,
                                                  cameraDeviceCallback, true, &rc);
    if (rc == OHCM_SUCCESS && discovered->uuid != NULL && discovered->macAddress != NULL)
    {
        icLogDebug(LOG_TAG,
                   "Found Camera Device Info: UUID = %s, Model = %s, Manufacturer = %s, Firmware Ver = %s, Hardware Ver = %s\n",
                   discovered->uuid,
                   (discovered->details->model != NULL) ? discovered->details->model : "NULL",
                   (discovered->details->manufacturer != NULL) ? discovered->details->manufacturer : "NULL",
                   (discovered->details->firmwareVersion != NULL) ? discovered->details->firmwareVersion : "NULL",
                   (discovered->details->hardwareVersion != NULL) ? discovered->details->hardwareVersion : "NULL");

        // assign the correct class of camera
        //
        const char* deviceClass = getCameraClassForModel(discovered->details->model);

        icStringHashMap* endpointProfileMap = stringHashMapCreate();
        stringHashMapPut(endpointProfileMap, strdup(CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID), strdup(CAMERA_PROFILE));

        DeviceFoundDetails deviceFoundDetails = {
                .deviceDriver = deviceDriver,
                .deviceMigrator = NULL,
                .deviceClass = deviceClass,
                .deviceClassVersion = DEVICE_CLASS_VERSION,
                //Use copies in case discovered is freed by discoverStop call
                .deviceUuid = strdup(discovered->uuid),
                .manufacturer = strdup(discovered->details->manufacturer),
                .model = strdup(discovered->details->model),
                .hardwareVersion = strdup(discovered->details->hardwareVersion),
                .firmwareVersion = strdup(discovered->details->firmwareVersion),
                .metadata = NULL,
                .endpointProfileMap = endpointProfileMap
        };

        //Do an early firmware version check so we can set some metadata for the UI
        populateEarlyFWUpgradeMetadata(&deviceFoundDetails, discovered);

        // add to our 'pending' list
        //
        appendCameraToSet(pendingCameras, discovered);

        deviceServiceCallbacks->deviceFound(&deviceFoundDetails, false);

        // Cleanup
        stringHashMapDestroy(endpointProfileMap, NULL);
        stringHashMapDestroy(deviceFoundDetails.metadata, NULL);
        //We allocated these earlier, so we're gonna free them now.
        free((char *) deviceFoundDetails.deviceUuid);
        free((char *) deviceFoundDetails.manufacturer);
        free((char *) deviceFoundDetails.model);
        free((char *) deviceFoundDetails.hardwareVersion);
        free((char *) deviceFoundDetails.firmwareVersion);

        // note that there are additional resources we need/want to save.
        // returning 'true' should find it's way to 'configureDevice', which
        // will call "createCameraPersistenceResources", where more details are saved
        //
        retVal = true;
    }
    else
    {
        // couldn't get the mac and/or calculate the uuid.
        //
        icLogError(LOG_TAG, "Unable to communicate with discovered camera; rc=%d %s", rc, ohcmResultCodeLabels[rc]);
        destroyCameraDevice(discovered);
    }
    return retVal;
}


/*---------------------------------------------------------------------------------------
 * addRediscoveredCamera : add a camera that was reset and being re-added
 *---------------------------------------------------------------------------------------*/
static bool addRediscoveredCamera(cameraDevice* camera)
{
    // similar to 'addDiscoveredCamera', except that we already have the details
    // and want to simply re-configure this camera exactly as it used to be.
    // because of the underlying dbase, we need to:
    //  1.  clone 'camera'
    //  2.  delete the icDevice this camera represents
    //  3.  add a new icDevice with the contents of camera
    //      (and hope the configureDevice callback leverages existing info such as admin user/pass)
    //

    // step 1 - clone 'camera'
    // first 'read' from the camera because it could have changed while offline
    //
    ohcmResultCode rc;
    cameraDevice* clone = createCameraDevice(camera->uuid, camera->ipAddress, camera->macAddress,
                                             DEFAULTED_ADMIN_USERNAME, DEFAULTED_ADMIN_PASSWORD,
                                             cameraDeviceCallback, true, &rc);

    // now copy information from the original into the clone
    //
    if (camera->adminCredentials->username != NULL)
    {
        free(clone->adminCredentials->username);
        clone->adminCredentials->username = strdup(camera->adminCredentials->username);
    }
    if (camera->adminCredentials->password != NULL)
    {
        free(clone->adminCredentials->password);
        clone->adminCredentials->password = strdup(camera->adminCredentials->password);
    }
    if (camera->userCredentials->username != NULL)
    {
        free(clone->userCredentials->username);
        clone->userCredentials->username = strdup(camera->userCredentials->username);
    }
    if (camera->userCredentials->password != NULL)
    {
        free(clone->userCredentials->password);
        clone->userCredentials->password = strdup(camera->userCredentials->password);
    }
    if (camera->videoSettings->videoResolution != NULL)
    {
        free(clone->videoSettings->videoResolution);
        clone->videoSettings->videoResolution = strdup(camera->videoSettings->videoResolution);
    }
    if (camera->videoSettings->aspectRatio != NULL)
    {
        free(clone->videoSettings->aspectRatio);
        clone->videoSettings->aspectRatio = strdup(camera->videoSettings->aspectRatio);
    }

    // extract 'icDevice' information that we'll want to restore
    //
    char* camLabel = extractStringResource(camera->uuid,
                                           CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                           COMMON_ENDPOINT_RESOURCE_LABEL);

    // since we're replacing, set state to OFFLINE
    //
    clone->opState = CAMERA_OP_STATE_OFFLINE;

    // assign the correct class of camera
    //
    const char* deviceClass = getCameraClassForModel(clone->details->model);

    // now delete the existing icDevice object, then add the cloned one
    //
    deviceServiceCallbacks->removeDevice(camera->uuid);

    icStringHashMap* endpointProfileMap = stringHashMapCreate();
    stringHashMapPut(endpointProfileMap, strdup(CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID), strdup(CAMERA_PROFILE));

    DeviceFoundDetails deviceFoundDetails = {
            .deviceDriver = deviceDriver,
            .deviceMigrator = NULL,
            .deviceClass = deviceClass,
            .deviceClassVersion = DEVICE_CLASS_VERSION,
            //Use copies in case discovered is freed by discoverStop call
            .deviceUuid = strdup(clone->uuid),
            .manufacturer = strdup(clone->details->manufacturer),
            .model = strdup(clone->details->model),
            .hardwareVersion = strdup(clone->details->hardwareVersion),
            .firmwareVersion = strdup(clone->details->firmwareVersion),
            .metadata = NULL,
            .endpointProfileMap = endpointProfileMap
    };

    //Do an early firmware version check so we can set some metadata for the UI
    populateEarlyFWUpgradeMetadata(&deviceFoundDetails, clone);

    // add to our 'pending' list
    //
    appendCameraToSet(pendingCameras, clone);

    bool succeeded = deviceServiceCallbacks->deviceFound(&deviceFoundDetails, false);

    // Cleanup
    stringHashMapDestroy(endpointProfileMap, NULL);
    //We allocated these earlier, so we're gonna free them now.
    free((char *) deviceFoundDetails.deviceUuid);
    free((char *) deviceFoundDetails.manufacturer);
    free((char *) deviceFoundDetails.model);
    free((char *) deviceFoundDetails.hardwareVersion);
    free((char *) deviceFoundDetails.firmwareVersion);

    // if we have a label to apply, do it now
    //
    if (camLabel != NULL && succeeded == true)
    {
        deviceServiceCallbacks->updateResource(clone->uuid,
                                               CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                               COMMON_ENDPOINT_RESOURCE_LABEL,
                                               camLabel,
                                               NULL);
    }
    else
    {
        free(camLabel);
    }

    // note that there are additional resources we need/want to save.
    // returning 'true' should find it's way to 'configureDevice', which
    // will call "createCameraPersistenceResources", where more details are saved
    //
    return true;
}

/*---------------------------------------------------------------------------------------
* openHomeCameraDeviceDriverAddMigratedCamera : add a migrated camera to be processed by device service
*---------------------------------------------------------------------------------------*/
bool openHomeCameraDeviceDriverAddMigratedCamera(cameraDevice *discovered, DeviceMigrator *migrator)
{
    // create a cameraDevice, and let it probe the device for details
    //
    icLogDebug(LOG_TAG,
               "Found Migrated Camera Device Info: UUID = %s, Model = %s, Manufacturer = %s, Firmware Ver = %s, Hardware Ver = %s\n",
               discovered->uuid,
               (discovered->details->model != NULL) ? discovered->details->model : "NULL",
               (discovered->details->manufacturer != NULL) ? discovered->details->manufacturer : "NULL",
               (discovered->details->firmwareVersion != NULL) ? discovered->details->firmwareVersion : "NULL",
               (discovered->details->hardwareVersion != NULL) ? discovered->details->hardwareVersion : "NULL");

    // assign the correct class of camera
    //
    const char* deviceClass = getCameraClassForModel(discovered->details->model);

    icStringHashMap* endpointProfileMap = stringHashMapCreate();
    stringHashMapPut(endpointProfileMap, strdup(CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID), strdup(CAMERA_PROFILE));

    DeviceFoundDetails deviceFoundDetails = {
            .deviceDriver = deviceDriver,
            .deviceMigrator = migrator,
            .deviceClass = deviceClass,
            .deviceClassVersion = DEVICE_CLASS_VERSION,
            //No discovery stop for migration, so no need to copy
            .deviceUuid = discovered->uuid,
            .manufacturer = discovered->details->manufacturer,
            .model = discovered->details->model,
            .hardwareVersion = discovered->details->hardwareVersion,
            .firmwareVersion = discovered->details->firmwareVersion,
            .metadata = NULL,
            .endpointProfileMap = endpointProfileMap
    };

    // add to our list, as we are just going to skip the configure step
    //
    appendCameraToSet(allCameras, discovered);

    bool retVal = deviceServiceCallbacks->deviceFound(&deviceFoundDetails, false);

    if (retVal)
    {
        // now start the monitoring of this device
        //
        cameraDeviceStartMonitorThread(discovered);
    }

    // Cleanup
    stringHashMapDestroy(endpointProfileMap, NULL);

    return retVal;
}

cameraDevice *
openHomeCameraDeviceDriverCreateCameraDevice(const char *macAddress, const char *ipAddress, const char *adminUserId,
                                             const char *adminPassword, bool fetchDetails)
{
    ohcmResultCode resultCode;
    return createCameraDevice(NULL, ipAddress, macAddress, adminUserId, adminPassword, cameraDeviceCallback, fetchDetails,
                              &resultCode);
}

/*
 * internal function to extract a string resource, and return a duplicated string
 * so it can be safely saved in memory
 */
static char* extractStringResource(const char* deviceUuid, const char* endpointId, const char* resourceId)
{
    char* retVal = NULL;

    // get the resource from deviceService
    //
    icDeviceResource* resource = deviceServiceCallbacks->getResource(deviceUuid, endpointId, resourceId);
    if (resource != NULL && resource->value != NULL)
    {
        // return the duplicated string
        //
        retVal = strdup(resource->value);
    }

    // cleanup and return
    //
    resourceDestroy(resource);
    return retVal;
}

/*
 * populate a cameraDevice with information stored in persistence.
 * assume this comes from our startup, so most values within 'device'
 * will be NULL
 */
static void loadCameraPersistenceResources(cameraDevice* device)
{
    const char* uuid = (const char*) device->uuid;

//    createDeviceResource(device, COMMON_DEVICE_RESOURCE_MANUFACTURER, manufacturer, RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
//    createDeviceResource(device, COMMON_DEVICE_RESOURCE_MODEL, model, RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
//    createDeviceResource(device, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, hardwareVersion, RESOURCE_TYPE_VERSION, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
//    createDeviceResource(device, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, firmwareVersion, RESOURCE_TYPE_VERSION, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS); //the device driver will update after firmware upgrade

    // first, device information
    //
    if (device->macAddress == NULL)     // only get if needed
    {
        device->macAddress = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_MAC_ADDRESS);
    }
    if (device->ipAddress == NULL)      // only get if needed
    {
        device->ipAddress = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_IP_ADDRESS);
        if (device->ipAddress != NULL && strcmp(device->ipAddress, "127.0.0.1") == 0)
        {
            // ensure we properly capture the device when we're running on it
            //
            device->isIntegratedPeripheral = true;
        }
    }
    device->details->serialNumber = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER);
    device->details->manufacturer = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_MANUFACTURER);
    device->details->model = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_MODEL);
    device->details->hardwareVersion = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION);
    device->details->firmwareVersion = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    // now camera endpoint info
    //
    device->adminCredentials->username = extractStringResource(uuid,
                                                               CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                               CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID);
    device->adminCredentials->password = extractStringResource(uuid,
                                                               CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                               CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD);
    device->userCredentials->username = extractStringResource(uuid,
                                                              CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                              CAMERA_PROFILE_RESOURCE_USER_USER_ID);
    device->userCredentials->password = extractStringResource(uuid,
                                                              CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                              CAMERA_PROFILE_RESOURCE_USER_PASSWORD);
    device->details->apiVersion = extractStringResource(uuid,
                                                        CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                        CAMERA_PROFILE_RESOURCE_API_VERSION);
    device->videoSettings->videoResolution = extractStringResource(uuid,
                                                                   CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                                   CAMERA_PROFILE_RESOURCE_RESOLUTION);
    device->videoSettings->aspectRatio = extractStringResource(uuid,
                                                               CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                               CAMERA_PROFILE_RESOURCE_ASPECT_RATIO);

    // motion endpoint
    //
    char* flag = extractStringResource(uuid, CAMERA_DC_SENSOR_PROFILE_ENDPOINT_ID, SENSOR_PROFILE_RESOURCE_BYPASSED);
    if (flag != NULL)
    {
        if (strcmp(flag, "false") == 0)
        {
            device->motionEnabled = true;
        }
        else
        {
            device->motionEnabled = false;
        }
        free(flag);
    }

    // is motion possible (driven by whitelist for this device)
    //
    flag = extractStringResource(uuid, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID, CAMERA_PROFILE_RESOURCE_MOTION_CAPABLE);
    if (flag != NULL)
    {
        if (strcmp(flag, "true") == 0)
        {
            device->motionPossible = true;
        }
        else
        {
            device->motionPossible = false;
        }
        free(flag);
    }

    // does this device have a user button
    //
    flag = deviceServiceCallbacks->getMetadata(uuid,
                                               CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                               USER_BUTTON_PRESENT_PROPNAME);
    if (flag != NULL)
    {
        if (strcmp(flag, "true") == 0)
        {
            device->hasUserButton = true;
        }
        else
        {
            device->hasUserButton = false;
        }
        free(flag);
    }

    // does this device have a speaker
    //
    flag = deviceServiceCallbacks->getMetadata(uuid, CAMERA_DC_SPEAKER_PROFILE_ENDPOINT_ID, SPEAKER_PRESENT_PROPNAME);
    if (flag != NULL)
    {
        if (strcmp(flag, "true") == 0)
        {
            device->hasSpeaker = true;
        }
        else
        {
            device->hasSpeaker = false;
        }
        free(flag);
    }

    // does this device use Sercomm's proprietary http event push mechanism
    //
    flag = deviceServiceCallbacks->getMetadata(uuid,
                                               CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                               USE_SERCOMM_PUSH_EVENT_PROPNAME);
    if (flag != NULL)
    {
        if (strcmp(flag, "true") == 0)
        {
            device->useSercommEventPush = true;
        }
        else
        {
            device->useSercommEventPush = false;
        }
        free(flag);
    }

    // last known status
    //
    device->opState = CAMERA_OP_STATE_READY;
    char* offline = extractStringResource(uuid, 0, COMMON_DEVICE_RESOURCE_COMM_FAIL);
    if (offline != NULL)
    {
        if (strcmp("true", offline) == 0)
        {
            // in comm failure
            //
            device->opState = CAMERA_OP_STATE_OFFLINE;
        }

        free(offline);
    }
}

/*
 * create a CameraInfo container with the information
 * saved in our persistence (for a single device)
 */
static ohcmCameraInfo* getCamInfo(const char* deviceUuid)
{
    ohcmCameraInfo* result = createOhcmCameraInfo();
    result->cameraIP = extractStringResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_IP_ADDRESS);
    if (result->cameraIP != NULL)
    {
        // valid device, so get more
        //
        result->macAddress = extractStringResource(deviceUuid, 0, COMMON_DEVICE_RESOURCE_MAC_ADDRESS);
        result->userName = extractStringResource(deviceUuid,
                                                 CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                 CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID);
        result->password = extractStringResource(deviceUuid,
                                                 CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                 CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD);
    }
    else
    {
        // nothing to return
        //
        destroyOhcmCameraInfo(result);
        result = NULL;
    }

    return result;
}

/*
 * Builds the media url for the various stream types. Codec is currently unused as the only available codecs are valid,
 * and thus do not alter the various url slice values.
 */
static char* getMediaUrl(const char* ipAddress, const char* streamType, const char* codec)
{
    char* result = NULL;
    if (ipAddress == NULL || streamType == NULL)
    {
        return result;
    }

    //Defined by openhome
    const char* channelURLSlice = OPENHOME_CHANNEL_URL_SLICE;

    //Currently always https
    const char* protocol;
    const char* port;
    const char* channelId;
    const char* streamTypeUrlSlice;

    //Do a bunch of comparisons to populate the various slices.
    if (strcmp(streamType, VIDEO_STREAM_TYPE_FLV) == 0)
    {
        protocol = "https://";
        channelId = "/0";
        port = ":443";
        streamTypeUrlSlice = "/flv";
    }
    else if(strcmp(streamType, VIDEO_STREAM_TYPE_MJPEG) == 0)
    {
        protocol = "https://";
        channelId = "/2";
        port = ":443";
        streamTypeUrlSlice = "/mjpeg";
    }
    else if(strcmp(streamType, VIDEO_STREAM_TYPE_RTSP) == 0)
    {
        protocol = "rtsp://";
        channelId = "/1";
        port = ":554";
        streamTypeUrlSlice = "/rtsp";
    }
    else if(strcmp(streamType, VIDEO_STREAM_TYPE_SNAPSHOT) == 0)
    {
        protocol = "https://";
        channelId = "/0";
        port = ":443";
        streamTypeUrlSlice = "/picture";
    }
    else
    {
        return result;
    }

    //Now build the URL
    size_t resultLen = 1+strlen(protocol)+strlen(ipAddress)+strlen(port)+strlen(channelURLSlice)+strlen(channelId)
                       +strlen(streamTypeUrlSlice);
    result = malloc(resultLen*sizeof(char));

    sprintf(result, "%s%s%s%s%s%s", protocol, ipAddress, port, channelURLSlice, channelId, streamTypeUrlSlice);

    return result;
}

/*
 * Forms a cJSON struct that contains a camera's supported video formats, video codecs, and the openhome API URLs for
 * those video formats. Note that it is up to the caller to handle the destruction of the returned cJSON struct.
 * @param camDevice
 * @return a cJSON struct that will contain the video formats, codecs, and format API URLs.
 */
static cJSON* getVideoInformation(cameraDevice *camDevice)
{
    const char *videoFormats[] = {VIDEO_STREAM_TYPE_MJPEG, VIDEO_STREAM_TYPE_FLV, VIDEO_STREAM_TYPE_RTSP};
    const char *videoCodecs[] = {VIDEO_CODEC_H264, VIDEO_CODEC_MPEG4};
    const size_t numFormats = sizeof(videoFormats) / sizeof(char *);
    const size_t numCodecs = sizeof(videoCodecs) / sizeof(char *);

    cJSON *parent = cJSON_CreateObject();
    cJSON *formatsObject = cJSON_CreateStringArray(videoFormats, numFormats);
    cJSON *codecsObject = cJSON_CreateStringArray(videoCodecs, numCodecs);
    cJSON *urlObject = cJSON_CreateObject();
    for (int i = 0; i<numFormats; i++)
    {
        char *mediaURL = getMediaUrl(camDevice->ipAddress, videoFormats[i], NULL);
        cJSON_AddStringToObject(urlObject, videoFormats[i], mediaURL);
        free(mediaURL);
    }
    cJSON_AddItemToObject(parent, "videoFormats", formatsObject);
    cJSON_AddItemToObject(parent, "videoCodecs", codecsObject);
    cJSON_AddItemToObject(parent, "formatURLs", urlObject);

    return parent;
}

/*---------------------------------------------------------------------------------------
 * processDeviceDescriptor : adhere to 'processDeviceDescriptor()' function as defined in deviceDriver.h
 *---------------------------------------------------------------------------------------*/
static bool processDeviceDescriptor(void* ctx,
                                    icDevice* device,
                                    DeviceDescriptor* dd)
{
    bool result = true;

    if (dd == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: NULL dd argument; ignoring");
        return true;
    }

    // find the cameraDevice
    //
    icLogDebug(LOG_TAG, "processDeviceDescriptor: %s", device->uuid);

    // schedule a background task to check all cameras that might need an update. We will force using a random interval
    // in case this is due to a DDL push from the sever. That way, there won't be a bajillion cameras hitting the bundle
    // server around the same time.
    //
    scheduleDelayedCameraUpdateTask(true);

    cameraDevice* camDevice = findCameraByUuid(allCameras, device->uuid);
    if (camDevice != NULL)
    {
        // see if the motion settings in the device descriptor differ
        // from that of this device (i.e. motion disabled, but device has it on)
        //

        // typecast to a CameraDeviceDescriptor
        //
        CameraDeviceDescriptor* cdes = (CameraDeviceDescriptor*) dd;

        // only apply the the device only if needed
        //
        bool clearMotionSettings = false;

        pthread_mutex_lock(&camDevice->mutex);
        if (cdes->defaultMotionSettings.enabled != camDevice->motionPossible)
        {
            // device descriptor is different then current value
            //
            camDevice->motionPossible = cdes->defaultMotionSettings.enabled;
            icLogDebug(LOG_TAG, "setting 'motionPossible' to %s for camera %s",
                       (camDevice->motionPossible == true) ? "true" : "false", device->uuid);

            // if we're disabling the possibility of motion, then shutdown the
            // motion detection (if currently set on the device)
            //
            if (camDevice->motionPossible == false && camDevice->motionEnabled == true)
            {
                // set flag to do this after we released the lock
                //
                clearMotionSettings = true;
            }

            // apply in our database and blast an event about the change
            //
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                   CAMERA_PROFILE_RESOURCE_MOTION_CAPABLE,
                                                   (camDevice->motionPossible) ? "true" : "false",
                                                   NULL);
        }

        // now see if the new device descriptor enabled or disabled the user button and store in our metadata
        //
        char* userButtonPresent = stringHashMapGet(dd->metadata, USER_BUTTON_PRESENT_PROPNAME);
        camDevice->hasUserButton = (userButtonPresent != NULL && strcmp(userButtonPresent, "true") == 0) ? true : false;
        deviceServiceCallbacks->setMetadata(device->uuid,
                                            CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                            USER_BUTTON_PRESENT_PROPNAME,
                                            (camDevice->hasUserButton) ? "true" : "false");

        // now see if the new device descriptor enabled or disabled a speaker and store in our metadata
        //
        char* speakerPresent = stringHashMapGet(dd->metadata, SPEAKER_PRESENT_PROPNAME);
        camDevice->hasSpeaker = (speakerPresent != NULL && strcmp(speakerPresent, "true") == 0) ? true : false;
        deviceServiceCallbacks->setMetadata(device->uuid,
                                            CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                            SPEAKER_PRESENT_PROPNAME,
                                            (camDevice->hasSpeaker) ? "true" : "false");

        // load the option to use the proprietary sercomm http event push mechanism instead of our openhome polling
        // and save in our metadata
        //
        char* useSercommEventPush = stringHashMapGet(dd->metadata, USE_SERCOMM_PUSH_EVENT_PROPNAME);
        camDevice->useSercommEventPush = (useSercommEventPush != NULL && strcmp(useSercommEventPush, "true") == 0)
                                         ? true : false;
        deviceServiceCallbacks->setMetadata(device->uuid,
                                            CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                            USE_SERCOMM_PUSH_EVENT_PROPNAME,
                                            (camDevice->useSercommEventPush) ? "true" : "false");

        pthread_mutex_unlock(&camDevice->mutex);

        // now safe to alter the camera (released the lock)
        //
        if (clearMotionSettings == true)
        {
            icLogDebug(LOG_TAG, "setting motion=false on camera %s due to device descriptor update", device->uuid);
            cameraDeviceEnableMotionDetection(camDevice, false);
        }
    }

    return result;
}

/**
 * Meant to do an early comparison between the camera's firmware version and the versions listed in the device descriptor.
 * Note that we aren't doing a firmware upgrade at this time.
 * @param camera
 * @param dd
 * @return an enum with the value FW_UPGRADE_NECESSARY if the device version is less than the minimum version.
 *FW_UPGRADE_NECESSARY if the device version is between the minimum and latest,and camera.nonupgarde.flag is set to false
 *FW_UPGRADE_DELAYABLE if the device version is between the minimum and latest, and camera.nonupgarde.flag is set to true
 *FW_UPGRADED_UNNEEDED if the device firmware is up to date
 */
static cameraNeedsFirmwareState earlyFirmwareVersionCompare(cameraDevice *camera, DeviceDescriptor *dd)
{
    //Default to unneeded
    cameraNeedsFirmwareState retVal = FW_UPGRADE_UNNEEDED;

    if (camera != NULL && dd != NULL)
    {
        // check to see if the new device descriptor has a "minimum" firmware version.
        //
        if (dd->minSupportedFirmwareVersion != NULL && dd->latestFirmware != NULL &&
            dd->latestFirmware->filenames != NULL)
        {
            icLogDebug(LOG_TAG, "Checking early firmware upgrade needed state for %s", camera->macAddress);
            //Check if the latest descriptor version is higher than the cam version
            if (cameraDeviceCheckForUpgrade(camera, dd, false) == true)
            {
                //See if the minimum descriptor version is higher than the cam version
                if (cameraDeviceCheckForUpgrade(camera, dd, true) == true)
                {
                    //We don't even meet the minimum, need to upgrade
                    retVal = FW_UPGRADE_NECESSARY;
                }
                else
                {
                    //check if camera.noupgrade.flag is set to true
                    bool doNotUpgradeFlag = getPropertyAsBool(NO_CAMERA_UPGRADE_BOOL_PROPERTY, false);

                    if (doNotUpgradeFlag == true)
                    {
                        //firmware version between minimum and latest but camera.noupgrade.flag property set true so delaying upgrade
                        retVal = FW_UPGRADE_DELAYABLE;
                    }
                    else
                    {
                        //upgrading, we're between minimum and latest
                        icLogDebug(LOG_TAG, "fw version is in between minimum & latest. noupgrade flag is unset, force upgrade!");
                        retVal = FW_UPGRADE_NECESSARY;
                    }
                }
            }
        }
        icLogDebug(LOG_TAG, "Firmware upgrade needed state for %s is: %s", camera->macAddress,
                (retVal == FW_UPGRADE_UNNEEDED) ? FW_UPGRADE_UNNEEDED_STRING :
                (retVal == FW_UPGRADE_DELAYABLE) ? FW_UPGRADE_DELAYABLE_STRING : FW_UPGRADE_NECESSARY_STRING);
    }
    else
    {
        icLogError(LOG_TAG, "%s: One or more provided arguments are null. Defaulting to FW_UPGRADE_UNNEEDED", __FUNCTION__);
    }

    return retVal;
}

/**
 * Adds a firmwareUpgradeNeededState metadata key/value pair to the provided DeviceFoundDetails argument.
 * @param details
 * @param camera
 */
static void populateEarlyFWUpgradeMetadata(DeviceFoundDetails *details, cameraDevice *camera)
{
    if (details == NULL || camera == NULL)
    {
        icLogWarn(LOG_TAG, "%s: Null argument provided", __FUNCTION__);
        return;
    }

    DeviceDescriptor *camDeviceDescriptor = deviceDescriptorsGet(camera->details->manufacturer,
                                                                 camera->details->model,
                                                                 camera->details->hardwareVersion,
                                                                 camera->details->firmwareVersion);
    cameraNeedsFirmwareState camFirmwareState = earlyFirmwareVersionCompare(camera, camDeviceDescriptor);

    if (details->metadata == NULL)
    {
        details->metadata = stringHashMapCreate();
    }

    switch(camFirmwareState)
    {
        case FW_UPGRADE_UNNEEDED:
            stringHashMapPutCopy(details->metadata, CAMERA_NEEDS_FIRMWARE_STATE_KEY, FW_UPGRADE_UNNEEDED_STRING);
            break;
        case FW_UPGRADE_DELAYABLE:
            stringHashMapPutCopy(details->metadata, CAMERA_NEEDS_FIRMWARE_STATE_KEY, FW_UPGRADE_DELAYABLE_STRING);
            break;
        case FW_UPGRADE_NECESSARY:
            stringHashMapPutCopy(details->metadata, CAMERA_NEEDS_FIRMWARE_STATE_KEY, FW_UPGRADE_NECESSARY_STRING);
            break;
        default:
            icLogWarn(LOG_TAG, "%s: Illegal cameraNeedsFirmwareState - %d", __FUNCTION__, camFirmwareState);
            break;
    }

    deviceDescriptorFree(camDeviceDescriptor);
}

/*
 * 'taskCallbackFunc' function to upgrade cameras as part of 'scheduleDelayedCameraUpdateTask()'
 */
static void performDelayedCameraUpdates(void* arg)
{
    icLogDebug(LOG_TAG, "executing scheduled task; 'check cameras for upgrade'");

    // loop through all of our configured devices (not pending ones)
    //
    cameraSetIterate(allCameras, delayedUpdateIteratorCallback, NULL);

    // reset the task handler since we're done (and to allow
    // a subsequent schedule if another device is added)
    //
    updateCameraTask = 0;
}

/*
 * a callback function to be applied to each camera in a set of cameras. Tries to perform a camera upgrade for a given
 * camera.
 */
static void delayedUpdateIteratorCallback(cameraDevice *camDevice, void *arg)
{
    // examine each camera to see if it needs an upgrade
    //
    if (camDevice->details == NULL)
    {
        return;
    }

    // need to find the device descriptor for this camera
    //
    DeviceDescriptor* descriptor = deviceDescriptorsGet(camDevice->details->manufacturer,
                                                        camDevice->details->model,
                                                        camDevice->details->hardwareVersion,
                                                        camDevice->details->firmwareVersion);
    if (descriptor != NULL)
    {
        // see if this camera is below the 'desired' version (not minimum version)
        //
        if (cameraDeviceCheckForUpgrade(camDevice, descriptor, false) == true)
        {
            //We'll need to update
            delayedUpdateThreadArgs *threadArgs = calloc(1, sizeof(delayedUpdateThreadArgs));
            threadArgs->device = camDevice;
            threadArgs->descriptor = descriptor;

            char *name = stringBuilder("FWUpd:%s", camDevice->uuid);
            createDetachedThread(performDelayedUpdate, threadArgs, "FWUpd:%s");
            free(name);
        }
        else
        {
            deviceDescriptorFree(descriptor);
        }
    }
    else
    {
        icLogWarn(LOG_TAG,
                  "unable to check if camera %s needs an upgrade; unable to obtain matching device descriptor",
                  camDevice->uuid);
    }
}

/*
 * An entry point for a detached thread to attempt a firmware upgrade on a camera
 */
static void *performDelayedUpdate(void* arg)
{
    delayedUpdateThreadArgs *threadArgs = (delayedUpdateThreadArgs *) arg;
    cameraDevice *camDevice = threadArgs->device;
    DeviceDescriptor *descriptor = threadArgs->descriptor;

    // perform the upgrade
    //
    icLogDebug(LOG_TAG,
               "upgrading camera %s to firmware version %s",
               camDevice->uuid,
               descriptor->latestFirmware->version);
    if (cameraDevicePerformUpgrade(camDevice, getCameraUpgradeFilename(descriptor->latestFirmware->filenames),
                                   descriptor->latestFirmware->version, DETAULT_FW_UPDATE_TIMEOUT_SECS) == false)
    {
        // failed to upgrade, cannot continue
        //
        icLogWarn(LOG_TAG, "error upgrading firmware of camera %s", camDevice->uuid);
    }
    else
    {
        // save new firmware version and send an event
        //
        pthread_mutex_lock(&camDevice->mutex);
        deviceServiceCallbacks->updateResource(camDevice->uuid,
                                               0,
                                               COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                                               camDevice->details->firmwareVersion,
                                               NULL);
        pthread_mutex_unlock(&camDevice->mutex);
    }

    //cleanup
    deviceDescriptorFree(descriptor);
    free(threadArgs);
    pthread_exit(NULL);
}

/*
 * called during startup and after a camera was added.
 * if necessary, schedule a delayed task to upgrade any
 * cameras that are not at the desired firmware version
 */
static void scheduleDelayedCameraUpdateTask(bool forceRandomInterval)
{
    if (updateCameraTask > 0)
    {
        // nothing to do, already have a task scheduled
        icLogDebug(LOG_TAG, "Delayed camera update already scheduled, not scheduling another.");
        return;
    }

    // check properties for the "amount of sleep time before checking for camera upgrades".
    // chances are this is not set, but need to check anyhow.
    //
    int64_t pauseBeforeUpgrades = getPropertyAsInt64(CAMERA_FW_UPGRADE_DELAY_SECONDS_PROPERTY, 0);
    // Coalesce negative numbers to 0. Not sure why, but MP/Userver allows negative numbers for this property.
    pauseBeforeUpgrades = (pauseBeforeUpgrades < 0) ? 0 : pauseBeforeUpgrades;

    if (forceRandomInterval || pauseBeforeUpgrades == 0)
    {
        // property not set or asked to force a random interval, so pick a random number of hours (between 1-24)
        //
        srandom((unsigned int) time(NULL));
        uint16_t hours = (uint16_t) (random() % 24);
        if (hours < 1)
        {
            hours = 1;
        }

        // now convert from hours to seconds
        //
        pauseBeforeUpgrades = (uint32_t) (hours * 60 * 60);
    }

    // schedule our task
    //
    icLogDebug(LOG_TAG, "scheduling 'check cameras for upgrade' to fire in %"PRIi64" seconds", pauseBeforeUpgrades);
    updateCameraTask = scheduleDelayTask(pauseBeforeUpgrades, DELAY_SECS, performDelayedCameraUpdates, NULL);
}

/**
 * Extract the single filename for the camera firmware upgrade
 * @param filenames the list of filenames
 * @return the first filename
 */
static char* getCameraUpgradeFilename(icLinkedList* filenames)
{
    char* filename = NULL;
    icLinkedListIterator* iterator = linkedListIteratorCreate(filenames);
    if (linkedListIteratorHasNext(iterator))
    {
        filename = (char*) linkedListIteratorGetNext(iterator);
    }
    linkedListIteratorDestroy(iterator);

    return filename;
}

/*
 * For every camera device found during SSDP recovery (not all will be in comm fail), check to see if the
 * camera with the provided macAddress is in commfail, and if so, update its IP address so that regular
 * monitoring done in cameraDevice can find it on its next iteration.
 */
static void cameraRecoveryCallback(const char *ipAddress, const char *macAddress)
{
    icLogDebug(LOG_TAG, "%s: found %s at %s", __FUNCTION__, macAddress, ipAddress);

    char uuid[MAC_ADDR_BYTES + 1];
    macAddrToUUID(uuid, macAddress);

    cameraDevice* camera = findCameraByUuid(allCameras, uuid);
    if(camera != NULL)
    {
        if(camera->opState == CAMERA_OP_STATE_OFFLINE)
        {
            pthread_mutex_lock(&camera->mutex);

            if (strcmp(ipAddress, camera->ipAddress) != 0)
            {
                icLogDebug(LOG_TAG, "%s: %s was found at a new ip address %s (previously %s)",
                           __FUNCTION__,
                           uuid,
                           ipAddress,
                           camera->ipAddress);

                free(camera->ipAddress);
                camera->ipAddress = strdup(ipAddress);

                deviceServiceCallbacks->updateResource(uuid, 0, COMMON_DEVICE_RESOURCE_IP_ADDRESS,
                                                       ipAddress, NULL);

                //We also need to update URLs because they depend on the IP address
                char *picURL = getMediaUrl(ipAddress, "SNAPSHOT", NULL);
                deviceServiceCallbacks->updateResource(uuid,
                                                       CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                       CAMERA_PROFILE_RESOURCE_PIC_URL,
                                                       picURL,
                                                       NULL);
                free(picURL);

                //As well as JSON video information
                cJSON *parentObject = getVideoInformation(camera);
                char *videoObjectString = cJSON_PrintUnformatted(parentObject);
                deviceServiceCallbacks->updateResource(uuid,
                                                       CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                                       CAMERA_PROFILE_RESOURCE_VIDEO_INFORMATION,
                                                       videoObjectString,
                                                       NULL);
                free(videoObjectString);
                cJSON_Delete(parentObject);
            }

            pthread_mutex_unlock(&camera->mutex);
        }
    }
    else
    {
        icLogInfo(LOG_TAG, "%s: ignoring unknown camera", __FUNCTION__);
    }
}

static void checkCameraForCommFail(cameraDevice *camera, void *arg)
{
    if(camera->opState == CAMERA_OP_STATE_OFFLINE)
    {
        *((bool*)arg) = true;
    }
}

/*
 * Attempt to recover IP addresses for offline cameras through SSDP discovery.  Offline cameras discovered
 * this way will have their IP addresses updated so that the regular camera polling will succeed
 * and bring the cameras out of comm fail.
 */
static void performIpAddressRecovery(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // If discovery is running, lets skip this iteration to avoid thrash
    //
    if(!discoveryRunning)
    {
        // if there aren't any cameras in comm fail, there is nothing for us to do.
        //
        bool atLeastOneCameraInCommFail = false;
        cameraSetIterate(allCameras, checkCameraForCommFail, &atLeastOneCameraInCommFail);

        if(atLeastOneCameraInCommFail)
        {
            if (ohcmDiscoverStart(cameraRecoveryCallback) == OPEN_HOME_CAMERA_CODE_SUCCESS)
            {
                // wait for results to filter in
                //TODO this can delay shutdown.  should add a conditional with a timed wait or so.
                sleep(RECOVERY_TIMEOUT_SECONDS);

                ohcmDiscoverStop();
            }
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: no cameras in comm fail, nothing to do", __FUNCTION__);
        }
    }
}

static void startIpAddressRecoveryTask()
{
    if(ipAddressRecoveryTask == 0)
    {
        ipAddressRecoveryTask = createRepeatingTask(IP_RECOVERY_INTERVAL_MINUTES,
                                                    DELAY_MINS,
                                                    performIpAddressRecovery,
                                                    NULL);
    }
}

static bool restoreConfig(void *ctx, const char *tempRestoreDir, const char *dynamicConfigPath)
{
    return true;
}

/*
 * callback for cameraSetIterate, starts camera monitor thread for given camera device
 */
static void delayedStartMonitorThreadIteratorCallback(cameraDevice *camDevice,
                                                      void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    // ask the device to start monitoring for motion/availability
    //
    cameraDeviceStartMonitorThread(camDevice);
}

/*
 * Iterates through available cameras and calls delayedStartMonitorThreadIteratorCallback
 * for each of them
 */
static void delayedStartMonitorThreadCallback()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    delayedCameraMonitorThreadStartupTask = 0;

    // loop through all of our configured devices (not pending ones)
    //
    cameraSetIterate(allCameras,
                     delayedStartMonitorThreadIteratorCallback,
                     NULL);
}
