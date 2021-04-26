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

#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include <icBuildtime.h>
#include <icConcurrent/repeatingTask.h>
#include <icLog/logging.h>
#include <deviceDescriptors.h>
#include <deviceDriver.h>
#include <deviceService.h>
#include <zhal/zhal.h>
#include <propsMgr/paths.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/fileUtils.h>
#include <commonDeviceDefs.h>
#include <versionUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <watchdog/watchdogService_ipc.h>
#include <ipc/deviceEventProducer.h>
#include <icTime/timeUtils.h>
#include <icTime/timeTracker.h>
#include <icUtil/stringUtils.h>
#include <icConcurrent/timedWait.h>
#include <deviceHelper.h>
#include <icConcurrent/icThreadSafeWrapper.h>
#include <icConcurrent/threadUtils.h>
#include "zigbeeCommonIds.h"
#include "zigbeeSubsystem.h"
#include "zigbeeSubsystemPrivate.h"
#include "zigbeeEventHandler.h"
#include "zigbeeAttributeTypes.h"
#include "zigbeeEventTracker.h"
#include "deviceServicePrivate.h"
#include "deviceCommunicationWatchdog.h"
#include "deviceDriverManager.h"
#include "zigbeeDriverCommon.h"
#include "zigbeeHealthCheck.h"
#include "zigbeeDefender.h"
#include "zigbeeTelemetry.h"

#undef LOG_TAG
#define LOG_TAG "zigbeeSubsystem"

#define LOCAL_EUI64_PROPERTY_NAME "ZIGBEE_LOCAL_EUI64"
#define ZIGBEE_CORE_IP_PROPERTY_NAME "ZIGBEE_CORE_IP"
#define ZIGBEE_CORE_PORT_PROPERTY_NAME "ZIGBEE_CORE_PORT"
#define ZIGBEE_CORE_SIMPLE_NETWORK_CREATED "ZIGBEE_CORE_SIMPLE_NETWORK_CREATED"
#define ZIGBEE_PREVIOUS_CHANNEL_NAME "ZIGBEE_PREVIOUS_CHANNEL"
#define ZIGBEE_PAN_ID_CONFLICT_SHORT_PROPERTY_NAME "panIdConflict.enabled"
#define ZIGBEE_REJECT_UNKNOWN_DEVICES "ZIGBEE_REJECT_UNKNOWN_DEVICES"
#define ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT "ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT"

#define ZIGBEE_HEALTH_CHECK_PROPS_PREFIX "cpe.zigbee.healthCheck"
#define ZIGBEE_DEFENDER_PROPS_PREFIX     "cpe.zigbee.defender"
#define ZIGBEE_WATCHDOG_ENABLED_PROP      "cpe.zigbee.watchdog.enabled.flag"

#define DEFAULT_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES 30

#define DEFAULT_ZIGBEE_CORE_IP "127.0.0.1"
#define DEFAULT_ZIGBEE_CORE_PORT "18443"

#define DELAY_BETWEEN_INITIAL_HEARTBEATS_SECONDS 1

#define LEGACY_FIRMWARE_SUBDIR "legacy"
#define OTA_FIRMWARE_SUBDIR "ota"
#define ZIGBEE_FIRMWARE_SUBDIR "zigbeeFirmware"

#define EUI64_JSON_PROP "eui64"
#define MANUF_JSON_PROP "manufacturer"
#define MODEL_JSON_PROP "model"
#define HWVER_JSON_PROP "hwVer"
#define FWVER_JSON_PROP "fwVer"
#define APPVER_JSON_PROP "appVer"
#define ID_JSON_PROP "id"
#define IS_SERVER_JSON_PROP "isServer"
#define ATTRIBUTE_IDS_JSON_PROP "attributeIds"
#define PROFILEID_JSON_PROP "profileId"
#define DEVICEID_JSON_PROP "deviceId"
#define DEVICEVER_JSON_PROP "deviceVer"
#define SERVERCLUSTERINFOS_JSON_PROP "serverClusterInfos"
#define CLIENTCLUSTERINFOS_JSON_PROP "clientClusterInfos"
#define ENDDEVICE_JSON_PROP "end"
#define ROUTERDEVICE_JSON_PROP "router"
#define UNKNOWN_JSON_PROP "unknown"
#define DEVICETYPE_JSON_PROP "type"
#define MAINS_JSON_PROP "mains"
#define BATT_JSON_PROP "batt"
#define POWERSOURCE_JSON_PROP "power"
#define ENDPOINTS_JSON_PROP "endpoints"

#define DEVICE_USES_HASH_BASED_LINK_KEY_METADATA "usesHashBasedLinkKey"

#define MIN_ZIGBEE_CHANNEL 11
#define MAX_ZIGBEE_CHANNEL 26

#define DEFAULT_ZIGBEE_CHANNEL_SCAN_DUR_MILLIS 30
#define DEFAULT_ZIGBEE_PER_CHANNEL_NUMBER_OF_SCANS 16

#define MIN_COMM_FAIL_ALARM_DELAY_MINUTES 60
#define MIN_COMM_FAIL_TROUBLE_DELAY_MINUTES 56

//Our pre-HA 1.2 sensors/devices reported this as their device id.  We will make a risky assumption that
// any device with this ID is one of these and we will skip discovery of attributes on the basic cluster.
#define ICONTROL_BOGUS_DEVICE_ID 0xFFFF

// The amount we should increment the counters after things like RMA.  The values here are what we have historically
// used
#define NONCE_COUNTER_INCREMENT_AMOUNT 0x1000
#define FRAME_COUNTER_INCREMENT_AMOUNT 0x1000

#define MAX_NETWORK_INIT_RETRIES 3
#define MAX_INITIAL_ZIGBEECORE_RESTARTS 3

#define WILDCARD_EUI64 0xFFFFFFFFFFFFFFFF

static zhalCallbacks callbacks;

static bool isInitialized = false;

static bool isChannelChangeInProgress = false;

static pthread_mutex_t channelChangeMutex = PTHREAD_MUTEX_INITIALIZER;

static int initializeNetwork(const uint64_t *eui64, const char *networkBlob);

static uint64_t generateOrLoadLocalEui64(const char *cpeId);

static uint64_t generateLocalEui64(const char *cpeId);

static void prematureClusterCommandsFreeFunc(void *key, void *value);

static void zigbeeCoreWatchdog(void *arg);

static bool isDeviceAutoApsAcked(const icDevice *device);

static bool isDeviceUsingHashBasedLinkKey(const icDevice *device);

static bool setDeviceUsingHashBasedLinkKey(const icDevice *device, bool isUsingHashBasedKey, bool setMetadataOnly);

static void startChannelChangeDeviceWatchdog(uint8_t previousChannel, uint8_t targetedChannel);

static uint32_t getCommFailTimeoutAlarmValueInSeconds();

static uint32_t getCommFailTimeoutTroubleValueInSeconds();

typedef enum
{
    ZIGBEE_CORE_RESTART_FIRST,          // First enum (shouldn't be used)
    ZIGBEE_CORE_RESTART_HEARTBEAT,
    ZIGBEE_CORE_RESTART_COMM_FAIL,
    ZIGBEE_CORE_RESTART_LAST            // Last enum (shouldn't be used)
} ZigbeeCoreRestartReason;

static const char *zigbeeCoreRestartReasonLabels[] = {
        NULL,   // FIRST
        "Reboot reason: heartbeat",
        "Reboot reason: communication failure",
        NULL    // LAST
};

static const char *zigbeeCoreRestartReasonToLabel(ZigbeeCoreRestartReason reason);

static void restartZigbeeCore(ZigbeeCoreRestartReason reason);

static void *createDeviceCallbacks();

static bool releaseIfDeviceCallbacksEmpty(void *item);

static void deviceCallbackDestroy(void *item);

static void checkAllDevicesInCommFail(void);

typedef struct
{
    uint64_t eui64;
    ZigbeeSubsystemDeviceCallbacks *callbacks;
    bool registered;
} DeviceCallbacksRegisterContext;

static void deviceCallbacksRegister(void *item, void *context);

typedef struct
{
    uint64_t eui64;
    bool unregistered;
} DeviceCallbacksUnregisterContext;

static void deviceCallbacksUnregister(void *item, void *context);

static void deviceCallbacksAttributeReport(const void *item, const void *context);

typedef struct
{
    uint64_t eui64;
    bool isSecure;
} DeviceCallbacksRejoinContext;

static void deviceCallbacksRejoined(const void *item, const void *context);

static void deviceCallbacksLeft(const void *item, const void *context);

typedef enum
{
    DEVICE_FIRMWARE_UPGRADE_COMPLETE,
    DEVICE_FIRMWARE_UPGRADE_FAILED,
    DEVICE_FIRMWARE_UPGRADING,
    DEVICE_FIRMWARE_NOTIFY
} DeviceCallbacksFirmwareStatus;

typedef struct
{
    uint64_t eui64;
    DeviceCallbacksFirmwareStatus status;
    uint32_t currentVersion;
} DeviceCallbacksFirmwareStatusContext;

static void deviceCallbacksFirmwareStatus(const void *item, const void *context);

typedef struct
{
    ReceivedClusterCommand *command;
    bool deviceFound;
} DeviceCallbacksClusterCommandReceivedContext;

static void deviceCallbacksClusterCommandReceived(const void *item, const void *context);

static int discoveringRefCount = 0; // when zero we are not discovering...
static pthread_mutex_t discoveringRefCountMutex = PTHREAD_MUTEX_INITIALIZER;

static icLinkedList *discoveringDeviceCallbacks = NULL;

static pthread_mutex_t discoveringDeviceCallbacksMutex = PTHREAD_MUTEX_INITIALIZER;

static icThreadSafeWrapper deviceCallbacksWrapper = THREAD_SAFE_WRAPPER_INIT(createDeviceCallbacks, releaseIfDeviceCallbacksEmpty, deviceCallbackDestroy);

//in order to support the pairing process for legacy sensors, which send a command to us
// immediately after joining but before we have recognized it, we must hold on to commands
// sent from devices that are not yet paired while we are in discovery.
static icHashMap *prematureClusterCommands = NULL; //eui64 to linked list of ReceivedClusterCommands
static pthread_mutex_t prematureClusterCommandsMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t prematureClusterCommandsCond;

static subsystemInitializedFunc subsystemInitialized = NULL;
static subsystemReadyForDevicesFunc readyForDevices = NULL;
static bool networkInitialized = false;
static bool allDriversStarted = false;
static pthread_mutex_t readyForDevicesMutex = PTHREAD_MUTEX_INITIALIZER;

static char *mySubsystemId = NULL;

// ZigbeeCore watchdog details
static uint32_t zigbeeCoreWatchdogTask = 0;

static uint16_t zigbeeCorePingFailures = 0;

// Set a default for max ping failures, could make this configurable later
static uint16_t maxZigbeeCorePingFailures = 3;

// Set a default for watchdog run interval, could make this configurable later
static uint16_t zigbeeCoreWatchdogRunIntervalSecs = 60;

// A set of device UUIDs that are using hash based link keys which we were notified about before the device was saved
static icHashMap *earlyHashedBasedLinkKeyDevices = NULL;
static pthread_mutex_t earlyHashBasedLinkKeyDevicesMtx = PTHREAD_MUTEX_INITIALIZER;

// Set of devices that are in the process of being discovered.  We use this to know which devices are in the process of
// being discovered so that we can still route commands coming from them until they are persisted.
static icHashMap *devicesInDiscovery = NULL;
static pthread_mutex_t devicesInDiscoverMtx = PTHREAD_MUTEX_INITIALIZER;

/**
 * Check to see if the subsystem is completely ready for devices, if so, call the callback
 */
static void checkSendReadyForDevices()
{
    pthread_mutex_lock(&readyForDevicesMutex);
    bool isReady = networkInitialized == true && allDriversStarted == true;
    pthread_mutex_unlock(&readyForDevicesMutex);

    if (isReady)
    {
        if (readyForDevices != NULL && mySubsystemId != NULL)
        {
            readyForDevices(mySubsystemId);
        }
    }
}

static bool finalizeInitialization(const char *cpeId, const char *networkBlob)
{
    uint64_t localEui64 = generateOrLoadLocalEui64(cpeId);

    bool result = initializeNetwork(&localEui64, networkBlob) == 0;
    if (result)
    {
        result = zigbeeSubsystemSetAddresses() == 0; //load zigbee addresses into ZigbeeCore
    }

    // Cleanup any no longer needed firmware files
    zigbeeSubsystemCleanupFirmwareFiles();

    return result;
}

int zigbeeSubsystemInitialize(const char *cpeId,
                              subsystemInitializedFunc initializedCallback,
                              subsystemReadyForDevicesFunc readyForDevicesCallback,
                              const char *subsystemId)
{
    if (isInitialized)
    {
        icLogError(LOG_TAG, "%s: already initialized", __FUNCTION__);
        return 1;
    }

    icLogDebug(LOG_TAG, "zigbeeSubsystemInitialize: %s", cpeId);

    initTimedWaitCond(&prematureClusterCommandsCond);

    // Initialize our callbacks
    if (initializedCallback != NULL)
    {
        subsystemInitialized = initializedCallback;
    }
    if (readyForDevicesCallback != NULL)
    {
        readyForDevices = readyForDevicesCallback;
    }

    mySubsystemId = strdup(subsystemId);

    char *ip = NULL;
    char *port = NULL;

    deviceServiceGetSystemProperty(ZIGBEE_CORE_IP_PROPERTY_NAME, &ip);
    deviceServiceGetSystemProperty(ZIGBEE_CORE_PORT_PROPERTY_NAME, &port);

    if (ip == NULL)
    {
        ip = strdup(DEFAULT_ZIGBEE_CORE_IP);
    }

    if (port == NULL)
    {
        port = strdup(DEFAULT_ZIGBEE_CORE_PORT);
    }

    int portNum;
    sscanf(port, "%d", &portNum);

    memset(&callbacks, 0, sizeof(callbacks));
    zigbeeEventHandlerInit(&callbacks);

    zhalInit(ip, portNum, &callbacks, NULL);
    free(ip);
    free(port);

    // wait here until ZigbeeCore is functional
    bool waitForZigbeeCore = getPropertyAsBool(ZIGBEE_WATCHDOG_ENABLED_PROP, true);
    if (waitForZigbeeCore == false)
    {
        icLogDebug(LOG_TAG, "Zigbee Watchdog disabled, skipping wait for ZigbeeCore to start");
    }
    int zigbeeCoreRestartCount = 0;
    while(waitForZigbeeCore == true && zigbeeCoreRestartCount < MAX_INITIAL_ZIGBEECORE_RESTARTS)
    {
        int zhalHeartbeatRc;

        // start the timer with given seconds
        //
        timeTracker *timer = timeTrackerCreate();
        icLogDebug(LOG_TAG, "Starting timer of %d seconds to wait for Zigbee startup", CONFIG_SERVICE_DEVICE_ZIGBEE_STARTUP_TIMEOUT_SECONDS);
        timeTrackerStart(timer, CONFIG_SERVICE_DEVICE_ZIGBEE_STARTUP_TIMEOUT_SECONDS);
        while ((zhalHeartbeatRc = zhalHeartbeat()) != 0 &&
               timeTrackerExpired(timer) == false)
        {
            icLogDebug(LOG_TAG, "Waiting for ZigbeeCore to be ready.");
            sleep(DELAY_BETWEEN_INITIAL_HEARTBEATS_SECONDS);
        }

        // cleanup timer
        //
        timeTrackerDestroy(timer);

        // We have seen an issue where watchdog says its restarting ZigbeeCore after an xncp upgrade, but for some
        // reason ZigbeeCore doesn't really restart.  This is an attempt to catch that condition and try one more
        // watchdog restart to get things working.
        if (zhalHeartbeatRc != 0)
        {
            icLogWarn(LOG_TAG, "Restarting ZigbeeCore, count %d",zigbeeCoreRestartCount);
            restartZigbeeCore(ZIGBEE_CORE_RESTART_HEARTBEAT);
            zigbeeCoreRestartCount++;
        }
        else
        {
            // Either we got a heartbeat response, or we timed out a second time.
            waitForZigbeeCore = false;
        }
    }

    AUTO_CLEAN(free_generic__auto) char *blob = NULL;
    deviceServiceGetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, &blob);
    finalizeInitialization(cpeId, blob);

    // Check to see if we were in the middle of a channel change, awaiting for devices to rejoin.  If so,
    //  start the watchdog up again.
    char *channelStr = NULL;
    uint8_t previousChannel = 0;
    if(deviceServiceGetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, &channelStr) &&
       channelStr != NULL &&
       strlen(channelStr) > 0 &&
       stringToUint8(channelStr, &previousChannel))
    {
        icLogInfo(LOG_TAG, "%s: a channel change was in progress, starting channel change watchdog again",
                __FUNCTION__);

        zhalSystemStatus status;
        memset(&status, 0, sizeof(zhalSystemStatus));
        if (zhalGetSystemStatus(&status) == 0)
        {
            startChannelChangeDeviceWatchdog(previousChannel, status.channel);
        }
        else
        {
            icLogError(LOG_TAG, "%s: unable to restart channel change watchdog", __FUNCTION__);
        }
    }

    return 0;
}

void zigbeeSubsystemAllDriversStarted()
{
    bool checkSendReady = false;
    pthread_mutex_lock(&readyForDevicesMutex);
    if (allDriversStarted == false)
    {
        allDriversStarted = true;
        checkSendReady = true;
    }
    pthread_mutex_unlock(&readyForDevicesMutex);
    if (checkSendReady == true)
    {
        checkSendReadyForDevices();
    }
}

void zigbeeSubsystemAllServicesAvailable()
{
#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
    zigbeeTelemetryInitialize();
#endif
}

void zigbeeSubsystemShutdown()
{
    icLogDebug(LOG_TAG, "zigbeeSubsystemShutdown");

#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
    zigbeeTelemetryShutdown();
#endif

    zigbeeHealthCheckStop();

    readyForDevices = NULL;
    free(mySubsystemId);
    mySubsystemId = NULL;
    if (isInitialized)
    {
        zhalTerm();
        isInitialized = false;
    }
    if (zigbeeCoreWatchdogTask > 0)
    {
        cancelRepeatingTask(zigbeeCoreWatchdogTask);
        zigbeeCoreWatchdogTask = 0;
    }

    //clean up any premature cluster commands we may have received while in discovery
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    hashMapDestroy(prematureClusterCommands, prematureClusterCommandsFreeFunc);
    prematureClusterCommands = NULL;
    pthread_mutex_unlock(&prematureClusterCommandsMtx);

    //clean up any devices in our map/set that may have updated to the hash based link key
    // before we were ready to save that fact.
    pthread_mutex_lock(&earlyHashBasedLinkKeyDevicesMtx);
    if(earlyHashedBasedLinkKeyDevices != NULL)
    {
        hashMapDestroy(earlyHashedBasedLinkKeyDevices, NULL);
        earlyHashedBasedLinkKeyDevices = NULL;
    }
    pthread_mutex_unlock(&earlyHashBasedLinkKeyDevicesMtx);
}

static void incrementNetworkCountersIfRequired()
{
    char *incrementCounters = NULL;
    if (deviceServiceGetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, &incrementCounters) == true &&
        stringCompare(incrementCounters, "true", true) == 0)
    {
        // Do the counter increment
        if (zhalIncrementNetworkCounters(NONCE_COUNTER_INCREMENT_AMOUNT, FRAME_COUNTER_INCREMENT_AMOUNT) == false)
        {
            icLogWarn(LOG_TAG, "Failed to increment zigbee counters");
        }
        else
        {
            icLogDebug(LOG_TAG, "Successfully incremented zigbee counters");

            // Reset to not increment
            deviceServiceSetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, "false");
        }
    }
    free(incrementCounters);
}

int zigbeeSubsystemInitializeNetwork(uint64_t *eui64)
{
    // Get the network blob.. if it doesn't exist, ZigbeeCore will configure the network and send a zhal 'networkConfigChanged'
    // event, which will write this property (see zigbeeEventHandler networkConfigChanged)
    AUTO_CLEAN(free_generic__auto) char *blob = NULL;
    deviceServiceGetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, &blob);

    return initializeNetwork(eui64, blob);
}

static int initializeNetwork(const uint64_t *eui64, const char *networkBlob)
{
    static pthread_mutex_t initMtx = PTHREAD_MUTEX_INITIALIZER;

    int initResult = ZHAL_STATUS_FAIL;
    uint64_t localEui64;

    if (eui64 == NULL)
    {
        //gotta load from config
        char *localEui64String = NULL;
        if (deviceServiceGetSystemProperty(LOCAL_EUI64_PROPERTY_NAME, &localEui64String) == false ||
            localEui64String == NULL)
        {
            icLogError(LOG_TAG,
                       "zigbeeSubsystemInitializeNetwork: no eui64 argument and none found in config!  Not initializing network");
            return ZHAL_STATUS_FAIL;
        }

        //convert our string eui64 into an uint64_t
        sscanf(localEui64String, "%016" PRIx64, &localEui64);
        free(localEui64String);
    }
    else
    {
        localEui64 = *eui64;
    }

    // create custom properties needed for ZigbeeCore
    //
    icStringHashMap *properties = stringHashMapCreate();

    // get pan id conflict enabled flag and add into properties list
    //
    bool panIdConflictFlag = getPropertyAsBool(PAN_ID_CONFLICT_ENABLED_PROPERTY_NAME, false);
    stringHashMapPut(properties, strdup(ZIGBEE_PAN_ID_CONFLICT_SHORT_PROPERTY_NAME),
            panIdConflictFlag ? strdup("true") : strdup("false"));

    //If we are already in the middle of initializing the network, then ignore this request.
    // zhalNetworkInit, when creating a new network, will trigger a zhalStartup event which is typically
    // used to indicate that ZigbeeCore restarted (or the NCP reset).  Skipping this instance
    // prevents us from initializing the network twice.  Note that if ZigbeeCore crashes in the middle
    // of this initialization, that initialization will fail and the zhalStartup event will be ignored
    // due to this protection.  In this case, the initial zhalNetworkInit will time out and the retry
    // logic will try again.

    //if we did not get the lock, then we were in the middle of network initialization already
    if (pthread_mutex_trylock(&initMtx) == EBUSY)
    {
        icLogDebug(LOG_TAG, "%s: not initializing since we are in the middle of init already", __FUNCTION__);
        stringHashMapDestroy(properties, NULL);
        return ZHAL_STATUS_FAIL;
    }

    for (int i = 0; i < MAX_NETWORK_INIT_RETRIES; ++i)
    {
        initResult = zhalNetworkInit(localEui64, "US", networkBlob, properties);

        if (initResult == 0)
        {
            incrementNetworkCountersIfRequired();

            isInitialized = true;
            zigbeeEventHandlerSystemReady();

            if (localEui64 == 0) //simple network
            {
                deviceServiceSetSystemProperty(ZIGBEE_CORE_SIMPLE_NETWORK_CREATED, "true");
            }

            pthread_mutex_lock(&readyForDevicesMutex);
            bool checkSendReady = false;
            if (networkInitialized == false)
            {
                networkInitialized = true;
                checkSendReady = true;
            }
            pthread_mutex_unlock(&readyForDevicesMutex);
            if (checkSendReady == true)
            {
                checkSendReadyForDevices();
            }

            zigbeeHealthCheckStart();

            zigbeeDefenderConfigure();

            break;
        }
        else
        {
            icLogError(LOG_TAG, "zhalNetworkInit failed(rc=%d)!!! Retries remaining = %d",
                    initResult,
                    MAX_NETWORK_INIT_RETRIES - i - 1);
        }
    }

    pthread_mutex_unlock(&initMtx);

    // Start the zigbee core watchdog whether we are successful or not.  Its possible that ZigbeeCore comes up
    // and runs, but it is wedged and non-functional.  We need to be able to restart it in that case too
    if (zigbeeCoreWatchdogTask == 0 && getPropertyAsBool(ZIGBEE_WATCHDOG_ENABLED_PROP, true))
    {
        zigbeeCoreWatchdogTask = createRepeatingTask(zigbeeCoreWatchdogRunIntervalSecs, DELAY_SECS,
                                                              zigbeeCoreWatchdog, NULL);
    }

    // Cleanup
    stringHashMapDestroy(properties, NULL);

    return initResult;
}

bool zigbeeSubsystemReconfigureNetwork(uint64_t eui64, const char *networkBlob, const char *cpeId)
{
    char *localEui64 = zigbeeSubsystemEui64ToId(eui64);
    // store network blob and local eui64
    // Note: This may be overwritten before reaching initializeNetwork, but is here for failure recovery
    deviceServiceSetSystemProperty(NETWORK_BLOB_PROPERTY_NAME, networkBlob);
    deviceServiceSetSystemProperty(LOCAL_EUI64_PROPERTY_NAME, localEui64);
    // Go ahead and increment counters to be safe
    deviceServiceSetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, "true");
    free(localEui64);

    // Now we can finalize initialization
    return finalizeInitialization(cpeId, networkBlob);
}

int zigbeeSubsystemRegisterDiscoveryHandler(const char *name, ZigbeeSubsystemDeviceDiscoveredHandler *handler)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, name);

    pthread_mutex_lock(&discoveringDeviceCallbacksMutex);

    if (discoveringDeviceCallbacks == NULL)
    {
        discoveringDeviceCallbacks = linkedListCreate();
    }

    linkedListAppend(discoveringDeviceCallbacks, handler);
    pthread_mutex_unlock(&discoveringDeviceCallbacksMutex);

    return 0;
}

int zigbeeSubsystemUnregisterDiscoveryHandler(ZigbeeSubsystemDeviceDiscoveredHandler *handler)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, handler->driverName);

    pthread_mutex_lock(&discoveringDeviceCallbacksMutex);
    icLinkedListIterator *iterator = linkedListIteratorCreate(discoveringDeviceCallbacks);
    while (linkedListIteratorHasNext(iterator))
    {
        ZigbeeSubsystemDeviceDiscoveredHandler *item = (ZigbeeSubsystemDeviceDiscoveredHandler *) linkedListIteratorGetNext(
                iterator);
        if (handler == item)
        {
            linkedListIteratorDeleteCurrent(iterator, standardDoNotFreeFunc);
        }
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&discoveringDeviceCallbacksMutex);

    return 0;
}

int zigbeeSubsystemRegisterDeviceListener(uint64_t eui64, ZigbeeSubsystemDeviceCallbacks *callbacks)
{
    DeviceCallbacksRegisterContext context = {
            .eui64 = eui64,
            .callbacks = callbacks,
            .registered = false
    };
    threadSafeWrapperModifyItem(&deviceCallbacksWrapper, deviceCallbacksRegister, &context);

    return context.registered == true ? 0 : -1;
}

static void freeDeviceCallbackEntry(void *key, void *value)
{
    free(key);
}

int zigbeeSubsystemUnregisterDeviceListener(uint64_t eui64)
{
    DeviceCallbacksUnregisterContext context = {
            .eui64 = eui64,
            .unregistered = false
    };
    threadSafeWrapperModifyItem(&deviceCallbacksWrapper, deviceCallbacksUnregister, &context);

    return context.unregistered == true ? 0 : -1;
}

void zigbeeSubsystemDumpDeviceDiscovered(IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "IcDiscoveredDeviceDetails:");
    icLogDebug(LOG_TAG, "\tEUI64: %016"
            PRIx64, details->eui64);
    switch (details->deviceType)
    {
        case deviceTypeEndDevice:
            icLogDebug(LOG_TAG, "\tDevice Type: end device");
            break;
        case deviceTypeRouter:
            icLogDebug(LOG_TAG, "\tDevice Type: router");
            break;

        default:
            icLogDebug(LOG_TAG, "\tDevice Type: unknown");
            break;
    }
    switch (details->powerSource)
    {
        case powerSourceMains:
            icLogDebug(LOG_TAG, "\tPower Source: mains");
            break;
        case powerSourceBattery:
            icLogDebug(LOG_TAG, "\tPower Source: battery");
            break;

        default:
            icLogDebug(LOG_TAG, "\tPower Source: unknown");
            break;
    }
    icLogDebug(LOG_TAG, "\tManufacturer: %s", details->manufacturer);
    icLogDebug(LOG_TAG, "\tModel: %s", details->model);
    icLogDebug(LOG_TAG, "\tHardware Version: 0x%"
            PRIx64, details->hardwareVersion);
    icLogDebug(LOG_TAG, "\tFirmware Version: 0x%"
            PRIx64, details->firmwareVersion);
    icLogDebug(LOG_TAG, "\tNumber of endpoints: %d", details->numEndpoints);
    for (int i = 0; i < details->numEndpoints; i++)
    {
        icLogDebug(LOG_TAG, "\t\tEndpoint ID: %d", details->endpointDetails[i].endpointId);
        icLogDebug(LOG_TAG, "\t\tProfile ID: 0x%04"
                PRIx16, details->endpointDetails[i].appProfileId);
        icLogDebug(LOG_TAG, "\t\tDevice ID: 0x%04"
                PRIx16, details->endpointDetails[i].appDeviceId);
        icLogDebug(LOG_TAG, "\t\tDevice Version: %d", details->endpointDetails[i].appDeviceVersion);

        icLogDebug(LOG_TAG, "\t\tServer Cluster IDs:");
        for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
        {
            icLogDebug(LOG_TAG, "\t\t\t0x%04"
                    PRIx16, details->endpointDetails[i].serverClusterDetails[j].clusterId);

            icLogDebug(LOG_TAG, "\t\t\tAttribute IDs:");
            for (int k = 0; k < details->endpointDetails[i].serverClusterDetails[j].numAttributeIds; k++)
            {
                icLogDebug(LOG_TAG, "\t\t\t\t0x%04"
                        PRIx16, details->endpointDetails[i].serverClusterDetails[j].attributeIds[k]);
            }
        }

        icLogDebug(LOG_TAG, "\t\tClient Cluster IDs:");
        for (int j = 0; j < details->endpointDetails[i].numClientClusterDetails; j++)
        {
            icLogDebug(LOG_TAG, "\t\t\t0x%04"
                    PRIx16, details->endpointDetails[i].clientClusterDetails[j].clusterId);

            icLogDebug(LOG_TAG, "\t\t\tAttribute IDs:");
            for (int k = 0; k < details->endpointDetails[i].clientClusterDetails[j].numAttributeIds; k++)
            {
                icLogDebug(LOG_TAG, "\t\t\t\t0x%04"
                        PRIx16, details->endpointDetails[i].clientClusterDetails[j].attributeIds[k]);
            }
        }
    }
}

bool zigbeeSubsystemClaimDiscoveredDevice(IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator)
{
    pthread_mutex_lock(&discoveringDeviceCallbacksMutex);
    icLinkedListIterator *iterator = linkedListIteratorCreate(discoveringDeviceCallbacks);
    bool deviceClaimed = false;

    while (linkedListIteratorHasNext(iterator) && deviceClaimed == false)
    {
        ZigbeeSubsystemDeviceDiscoveredHandler *item =
                (ZigbeeSubsystemDeviceDiscoveredHandler *) linkedListIteratorGetNext(iterator);
        deviceClaimed = item->callback(item->callbackContext, details, deviceMigrator);
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&discoveringDeviceCallbacksMutex);

    return deviceClaimed;
}

void zigbeeSubsystemDeviceDiscovered(IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zigbeeSubsystemDumpDeviceDiscovered(details);

    // Mark this device as being in discovery, so we know not to reject commands from it
    pthread_mutex_lock(&devicesInDiscoverMtx);
    if (devicesInDiscovery == NULL)
    {
        devicesInDiscovery = hashMapCreate();
    }
    hashMapPutCopy(devicesInDiscovery, &details->eui64, sizeof(details->eui64), NULL, 0);
    pthread_mutex_unlock(&devicesInDiscoverMtx);

    bool deviceClaimed = zigbeeSubsystemClaimDiscoveredDevice(details, NULL);

    // All done, its either out now, or persisted
    pthread_mutex_lock(&devicesInDiscoverMtx);
    hashMapDelete(devicesInDiscovery, &details->eui64, sizeof(details->eui64), NULL);
    if (hashMapCount(devicesInDiscovery) == 0)
    {
        hashMapDestroy(devicesInDiscovery, NULL);
        devicesInDiscovery = NULL;
    }
    pthread_mutex_unlock(&devicesInDiscoverMtx);

    if (deviceClaimed == false)
    {
        //nobody claimed this device, tell it to leave
        zhalRequestLeave(details->eui64, false, false);
    }
}

void zigbeeSubsystemSetRejectUnknownDevices(bool doReject)
{
    deviceServiceSetSystemProperty(ZIGBEE_REJECT_UNKNOWN_DEVICES, doReject ? "true" : "false");
}

static bool deviceShouldBeRejected(uint64_t eui64, bool *discovering)
{
    bool result = false;

    pthread_mutex_lock(&discoveringRefCountMutex);
    *discovering = discoveringRefCount > 0;
    pthread_mutex_unlock(&discoveringRefCountMutex);

    // First check if we are rejecting these devices is enabled.  If the property isn't there we assume its enabled
    bool rejectEnabled = true;
    char *value;
    if (deviceServiceGetSystemProperty(ZIGBEE_REJECT_UNKNOWN_DEVICES, &value) == true)
    {
        rejectEnabled = (value != NULL && stringCompare(value, "true", true) == 0);
        free(value);
    }

    if (rejectEnabled == true)
    {
        //if we are discovering, allow device to talk to us, otherwise see if we know it
        if(*discovering == false)
        {
            // Discovery might have ended but the device might not yet be persisted, so check if its still in process
            bool deviceInDiscoveryProcess = false;
            pthread_mutex_lock(&devicesInDiscoverMtx);
            if (devicesInDiscovery != NULL)
            {
                deviceInDiscoveryProcess = hashMapContains(devicesInDiscovery, &eui64, sizeof(eui64));
            }
            pthread_mutex_unlock(&devicesInDiscoverMtx);

            // If its not known to be in discovery, check whether its already persisted in device service
            if (deviceInDiscoveryProcess == false)
            {
                char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
                if (!deviceServiceIsDeviceKnown(deviceUuid))
                {
                    icLogWarn(LOG_TAG, "%s: received message from unknown device %s!", __FUNCTION__, deviceUuid);
                    result = true;
                }
                free(deviceUuid);
            }
        }
    }

    return result;
}

void zigbeeSubsystemAttributeReportReceived(ReceivedAttributeReport *report)
{
    bool discovering;
    if(deviceShouldBeRejected(report->eui64, &discovering))
    {
        zhalRequestLeave(report->eui64, false, false);
    }
    else
    {
        threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksAttributeReport, report);

        // add event to tracker after device driver(s) have a chance with it
        zigbeeEventTrackerAddAttributeReportEvent(report);
    }
}

void zigbeeSubsystemClusterCommandReceived(ReceivedClusterCommand *command)
{
    bool discovering;
    if(deviceShouldBeRejected(command->eui64, &discovering))
    {
        zhalRequestLeave(command->eui64, false, false);
    }
    else
    {
        DeviceCallbacksClusterCommandReceivedContext context = {
                .command = command,
                .deviceFound = false
        };
        threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksClusterCommandReceived, &context);

        bool repairing = deviceServiceIsInRecoveryMode();
        if (context.deviceFound == false || repairing == true)
        {
            // we got a command for a device we do not know or we are doing repairing.  If we are in discovery,
            // save this command in case we need it (legacy security devices)
            if (discovering)
            {
                icLogDebug(LOG_TAG, "%s: saving premature cluster command for uuid %"PRIx64" device while repairing = %s and device found = %s",
                           __FUNCTION__, command->eui64, stringValueOfBool(repairing), stringValueOfBool(context.deviceFound));

                zigbeeSubsystemAddPrematureClusterCommand(command);
            }
        }

        // add event to tracker after device driver(s) have a chance with it
        zigbeeEventTrackerAddClusterCommandEvent(command);
    }
}

void zigbeeSubsystemDeviceLeft(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "zigbeeSubsystemDeviceLeft: NOT IMPLEMENTED");

    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksLeft, &eui64);
}

void zigbeeSubsystemDeviceRejoined(uint64_t eui64, bool isSecure)
{
    DeviceCallbacksRejoinContext context = {
            .eui64 = eui64,
            .isSecure = isSecure
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksRejoined, &context);

    // add event to tracker after device driver(s) have a chance with it
    zigbeeEventTrackerAddRejoinEvent(eui64, isSecure);
}

void zigbeeSubsystemLinkKeyUpdated(uint64_t eui64, bool isUsingHashBasedKey)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (eui64 == WILDCARD_EUI64)
    {
        // We need to perform action for all the devices.
        icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);

        while (linkedListIteratorHasNext(iterator))
        {
            icDevice *device = linkedListIteratorGetNext(iterator);
            setDeviceUsingHashBasedLinkKey(device, isUsingHashBasedKey, true);
        }
        linkedListIteratorDestroy(iterator);
        linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
        icDevice *device = deviceServiceGetDevice(uuid);
        if (device != NULL)
        {
            setDeviceUsingHashBasedLinkKey(device, isUsingHashBasedKey, false);
            deviceDestroy(device);
        }
        // if its call to clear and we don't have device yet, do not worry about it for now.
        // only keep if its call to set.
        else if (isUsingHashBasedKey == true)
        {
            //device service doesnt yet know about this device which must be in the middle of discovery.  Save
            // off this device for use later when we set the devices with their flags.
            pthread_mutex_lock(&earlyHashBasedLinkKeyDevicesMtx);
            if (earlyHashedBasedLinkKeyDevices == NULL)
            {
                earlyHashedBasedLinkKeyDevices = hashMapCreate();
            }
            hashMapPut(earlyHashedBasedLinkKeyDevices, strdup(uuid), strlen(uuid), NULL);
            pthread_mutex_unlock(&earlyHashBasedLinkKeyDevicesMtx);
        }
    }
}

void zigbeeSubsystemApsAckFailure(uint64_t eui64)
{
    // TODO: notify any device drivers??

    // add event to tracker after device driver(s) have a chance with it
    zigbeeEventTrackerAddApsAckFailureEvent(eui64);
}

void zigbeeSubsystemDeviceFirmwareUpgrading(uint64_t eui64)
{
    DeviceCallbacksFirmwareStatusContext context = {
            .eui64 = eui64,
            .status = DEVICE_FIRMWARE_UPGRADING
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksFirmwareStatus, &context);
}

void zigbeeSubsystemDeviceFirmwareUpgradeCompleted(uint64_t eui64)
{
    DeviceCallbacksFirmwareStatusContext context = {
            .eui64 = eui64,
            .status = DEVICE_FIRMWARE_UPGRADE_COMPLETE
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksFirmwareStatus, &context);

    // add event to tracker after device driver(s) have a chance with it
    zigbeeEventTrackerAddDeviceFirmwareUpgradeSuccessEvent();
}

void zigbeeSubsystemDeviceFirmwareUpgradeFailed(uint64_t eui64)
{
    DeviceCallbacksFirmwareStatusContext context = {
            .eui64 = eui64,
            .status = DEVICE_FIRMWARE_UPGRADE_FAILED
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksFirmwareStatus, &context);

    // add event to tracker after device driver(s) have a chance with it
    zigbeeEventTrackerAddDeviceFirmwareUpgradeFailureEvent(eui64);
}

void zigbeeSubsystemDeviceFirmwareVersionNotify(uint64_t eui64, uint32_t currentVersion)
{
    DeviceCallbacksFirmwareStatusContext context = {
            .eui64 = eui64,
            .status = DEVICE_FIRMWARE_NOTIFY,
            .currentVersion = currentVersion
    };
    threadSafeWrapperReadItem(&deviceCallbacksWrapper, deviceCallbacksFirmwareStatus, &context);
}

static void *enableJoinThreadProc(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalNetworkEnableJoin();

    return NULL;
}

/*
 * Enter discovery mode if we are not already.  Increment discovery counter and send the enable join command if we are starting for the first time.
 */
int zigbeeSubsystemStartDiscoveringDevices()
{
    bool enableJoin = false;

    pthread_mutex_lock(&discoveringRefCountMutex);

    icLogDebug(LOG_TAG, "%s: discoveringRefCount = %d", __FUNCTION__, discoveringRefCount);

    if (discoveringRefCount++ == 0)
    {
        enableJoin = true;

        //clean up any premature cluster commands we may have received while in prior discovery
        pthread_mutex_lock(&prematureClusterCommandsMtx);
        hashMapDestroy(prematureClusterCommands, prematureClusterCommandsFreeFunc);
        pthread_mutex_unlock(&prematureClusterCommandsMtx);
    }
    pthread_mutex_unlock(&discoveringRefCountMutex);

    if (enableJoin)
    {
        zigbeeEventHandlerDiscoveryRunning(true);

        pthread_mutex_lock(&prematureClusterCommandsMtx);
        prematureClusterCommands = hashMapCreate();
        pthread_mutex_unlock(&prematureClusterCommandsMtx);

        //this can block for a while... put it in the background
        createDetachedThread(enableJoinThreadProc, NULL, "zbEnableJoin");
    }

    return 0;
}

/*
 * Decrement our discovery counter and stop discovery if we are at zero.
 */
int zigbeeSubsystemStopDiscoveringDevices()
{
    bool disableJoin = false;

    pthread_mutex_lock(&discoveringRefCountMutex);

    icLogDebug(LOG_TAG, "%s: discoveringRefCount = %d", __FUNCTION__, discoveringRefCount);

    if (--discoveringRefCount == 0)
    {
        disableJoin = true;
    }

    if (discoveringRefCount < 0)
    {
        icLogError(LOG_TAG,
                   "zigbeeSubsystemStopDiscoveringDevices: discoveringRefCount is negative! %d",
                   discoveringRefCount);
    }

    pthread_mutex_unlock(&discoveringRefCountMutex);

    if (disableJoin)
    {
        //no more devices being discovered... we can stop
        zhalNetworkDisableJoin();

        zigbeeEventHandlerDiscoveryRunning(false);
    }

    return 0;
}

int zigbeeSubsystemSendCommand(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint8_t commandId,
                               uint8_t *message,
                               uint16_t messageLen)
{
    //for now just pass through
    return zhalSendCommand(eui64, endpointId, clusterId, toServer, commandId, message, messageLen);
}

int zigbeeSubsystemSendMfgCommand(uint64_t eui64,
                                  uint8_t endpointId,
                                  uint16_t clusterId,
                                  bool toServer,
                                  uint8_t commandId,
                                  uint16_t mfgId,
                                  uint8_t *message,
                                  uint16_t messageLen)
{
    //for now just pass through
    return zhalSendMfgCommand(eui64, endpointId, clusterId, toServer, commandId, mfgId, message, messageLen);
}

int zigbeeSubsystemSendViaApsAck(uint64_t eui64,
                                 uint8_t endpointId,
                                 uint16_t clusterId,
                                 uint8_t sequenceNum,
                                 uint8_t *message,
                                 uint16_t messageLen)
{
    //for now just pass through
    return zhalSendViaApsAck(eui64, endpointId, clusterId, sequenceNum, message, messageLen);
}

static int readString(uint64_t eui64,
                      uint8_t endpointId,
                      uint16_t clusterId,
                      bool isMfgSpecific,
                      uint16_t mfgId,
                      bool toServer,
                      uint16_t attributeId,
                      char **value)
{
    int result = -1;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "zigbeeSubsystemReadString: invalid arguments");
        return -1;
    }

    if (toServer)
    {
        uint16_t attributeIds[1] = {attributeId};
        zhalAttributeData attributeData[1];
        memset(attributeData, 0, sizeof(zhalAttributeData));

        if (isMfgSpecific)
        {
            result = zhalAttributesReadMfgSpecific(eui64,
                                                   endpointId,
                                                   clusterId,
                                                   mfgId,
                                                   toServer,
                                                   attributeIds,
                                                   1,
                                                   attributeData);
        }
        else
        {
            result = zhalAttributesRead(eui64, endpointId, clusterId, toServer, attributeIds, 1, attributeData);
        }

        if (result == 0)
        {
            *value = (char *) calloc(1, attributeData[0].data[0] + 1);
            memcpy(*value, attributeData[0].data + 1, attributeData[0].data[0]);
        }
        else
        {
            icLogError(LOG_TAG, "zigbeeSubsystemReadString: zhal failed to read attribute");
        }

        if (attributeData[0].data != NULL)
        {
            free(attributeData[0].data);
        }
    }
    else
    {
        icLogError(LOG_TAG, "zigbeeSubsystemReadString: reading client attributes not yet supported");
    }

    return result;
}

int zigbeeSubsystemReadString(uint64_t eui64,
                              uint8_t endpointId,
                              uint16_t clusterId,
                              bool toServer,
                              uint16_t attributeId,
                              char **value)
{
    return readString(eui64, endpointId, clusterId, false, 0xFFFF, toServer, attributeId, value);
}

int zigbeeSubsystemReadStringMfgSpecific(uint64_t eui64,
                                         uint8_t endpointId,
                                         uint16_t clusterId,
                                         uint16_t mfgId,
                                         bool toServer,
                                         uint16_t attributeId,
                                         char **value)
{
    return readString(eui64, endpointId, clusterId, true, mfgId, toServer, attributeId, value);
}

static int readNumber(uint64_t eui64,
                      uint8_t endpointId,
                      uint16_t clusterId,
                      bool isMfgSpecific,
                      uint16_t mfgId,
                      bool toServer,
                      uint16_t attributeId,
                      uint64_t *value)
{
    int result = -1;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "zigbeeSubsystemReadNumber: invalid arguments");
        return -1;
    }

    *value = 0;

    uint16_t attributeIds[1] = {attributeId};
    zhalAttributeData attributeData[1];
    memset(attributeData, 0, sizeof(zhalAttributeData));

    if (isMfgSpecific)
    {
        result = zhalAttributesReadMfgSpecific(eui64,
                                               endpointId,
                                               clusterId,
                                               mfgId,
                                               toServer,
                                               attributeIds,
                                               1,
                                               attributeData);
    }
    else
    {
        result = zhalAttributesRead(eui64, endpointId, clusterId, toServer, attributeIds, 1, attributeData);
    }

    if (result == 0)
    {
        if (attributeData[0].dataLen > 0 && attributeData[0].dataLen <= 8) //these will fit in uint64_t
        {
            for (int i = 0; i < attributeData[0].dataLen; i++)
            {
                (*value) += ((uint64_t) attributeData[0].data[i]) << (i * 8);
            }
            result = 0;
        }
        else
        {
            icLogError(LOG_TAG, "zigbeeSubsystemReadNumber: error, no data returned");
            result = -1;
        }
    }
    else
    {
        icLogError(LOG_TAG, "zigbeeSubsystemReadNumber: zhal failed to read attribute");
    }

    if (attributeData[0].data != NULL)
    {
        free(attributeData[0].data);
    }

    return result;
}

int zigbeeSubsystemReadNumber(uint64_t eui64,
                              uint8_t endpointId,
                              uint16_t clusterId,
                              bool toServer,
                              uint16_t attributeId,
                              uint64_t *value)
{
    return readNumber(eui64, endpointId, clusterId, false, 0xFFFF, toServer, attributeId, value);
}

int zigbeeSubsystemReadNumberMfgSpecific(uint64_t eui64,
                                         uint8_t endpointId,
                                         uint16_t clusterId,
                                         uint16_t mfgId,
                                         bool toServer,
                                         uint16_t attributeId,
                                         uint64_t *value)
{
    return readNumber(eui64, endpointId, clusterId, true, mfgId, toServer, attributeId, value);
}

static int writeNumber(uint64_t eui64,
                       uint8_t endpointId,
                       uint16_t clusterId,
                       bool isMfgSpecific,
                       uint16_t mfgId,
                       bool toServer,
                       uint16_t attributeId,
                       ZigbeeAttributeType attributeType,
                       uint64_t value,
                       uint8_t numBytes)
{
    zhalAttributeData attributeData;
    memset(&attributeData, 0, sizeof(zhalAttributeData));
    attributeData.attributeInfo.id = attributeId;
    attributeData.attributeInfo.type = attributeType;
    attributeData.data = (uint8_t *) calloc(numBytes, 1);

    int retVal = 0;

    //note that this implementation only supports writing up to 8 bytes (what fits in uint64_t)
    for (int i = 0; i < numBytes; i++)
    {
        attributeData.data[i] = (uint8_t) ((value >> 8 * i) & 0xff);
    }
    attributeData.dataLen = numBytes;

    if (isMfgSpecific)
    {
        retVal = zhalAttributesWriteMfgSpecific(eui64, endpointId, clusterId, mfgId, toServer, &attributeData, 1);
    }
    else
    {
        retVal = zhalAttributesWrite(eui64, endpointId, clusterId, toServer, &attributeData, 1);
    }

    //cleanup
    free(attributeData.data);

    return retVal;
}

int zigbeeSubsystemWriteNumber(uint64_t eui64,
                               uint8_t endpointId,
                               uint16_t clusterId,
                               bool toServer,
                               uint16_t attributeId,
                               ZigbeeAttributeType attributeType,
                               uint64_t value,
                               uint8_t numBytes)
{
    return writeNumber(eui64,
                       endpointId,
                       clusterId,
                       false,
                       0xFFFF,
                       toServer,
                       attributeId,
                       attributeType,
                       value,
                       numBytes);
}

int zigbeeSubsystemWriteNumberMfgSpecific(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          uint16_t mfgId,
                                          bool toServer,
                                          uint16_t attributeId,
                                          ZigbeeAttributeType attributeType,
                                          uint64_t value,
                                          uint8_t numBytes)
{
    return writeNumber(eui64,
                       endpointId,
                       clusterId,
                       true,
                       mfgId,
                       toServer,
                       attributeId,
                       attributeType,
                       value,
                       numBytes);
}

int zigbeeSubsystemBindingSet(uint64_t eui64, uint8_t endpointId, uint16_t clusterId)
{
    return zhalBindingSet(eui64, endpointId, clusterId);
}

icLinkedList *zigbeeSubsystemBindingGet(uint64_t eui64)
{
    return zhalBindingGet(eui64);
}

int zigbeeSubsystemBindingClear(uint64_t eui64, uint8_t endpointId, uint16_t clusterId)
{
    return zhalBindingClear(eui64, endpointId, clusterId);
}

int zigbeeSubsystemBindingClearTarget(uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t clusterId,
                                      uint64_t targetEui64,
                                      uint8_t targetEndpointId)
{
    return zhalBindingClearTarget(eui64, endpointId, clusterId, targetEui64, targetEndpointId);
}


int zigbeeSubsystemAttributesSetReporting(uint64_t eui64,
                                          uint8_t endpointId,
                                          uint16_t clusterId,
                                          zhalAttributeReportingConfig *configs,
                                          uint8_t numConfigs)
{
    return zhalAttributesSetReporting(eui64, endpointId, clusterId, configs, numConfigs);
}

int zigbeeSubsystemAttributesSetReportingMfgSpecific(uint64_t eui64,
                                                     uint8_t endpointId,
                                                     uint16_t clusterId,
                                                     uint16_t mfgId,
                                                     zhalAttributeReportingConfig *configs,
                                                     uint8_t numConfigs)
{
    return zhalAttributesSetReportingMfgSpecific(eui64, endpointId, clusterId, mfgId, configs, numConfigs);
}

int zigbeeSubsystemGetEndpointIds(uint64_t eui64, uint8_t **endpointIds, uint8_t *numEndpointIds)
{
    return zhalGetEndpointIds(eui64, endpointIds, numEndpointIds);
}

int zigbeeSubsystemDiscoverAttributes(uint64_t eui64,
                                      uint8_t endpointId,
                                      uint16_t clusterId,
                                      bool toServer,
                                      zhalAttributeInfo **infos,
                                      uint16_t *numInfos)
{
    return zhalGetAttributeInfos(eui64, endpointId, clusterId, toServer, infos, numInfos);
}

uint64_t getLocalEui64()
{
    uint64_t result = 0;

    char *localEui64String = NULL;
    if (deviceServiceGetSystemProperty(LOCAL_EUI64_PROPERTY_NAME, &localEui64String) == true)
    {
        sscanf(localEui64String, "%016" PRIx64, &result);
    }

    if (localEui64String != NULL)
    {
        free(localEui64String);
    }

    return result;
}

//generate or load our local eui64
static uint64_t generateOrLoadLocalEui64(const char *cpeId)
{
    uint64_t localEui64 = 0;

    // if this is the first time we have initialized the network we wont have generated our local EUI64 yet...
    char *localEui64String = NULL;
    if (deviceServiceGetSystemProperty(LOCAL_EUI64_PROPERTY_NAME, &localEui64String) == false ||
        localEui64String == NULL)
    {
        if (localEui64String != NULL)
        {
            free(localEui64String);
        }

        //gotta generate it
        localEui64 = generateLocalEui64(cpeId);

        icLogDebug(LOG_TAG, "generated eui64 %016"
                PRIx64, localEui64);

        localEui64String = (char *) malloc(21); //max uint64_t is 18446744073709551615
        snprintf(localEui64String, 21, "%016" PRIx64, localEui64);

        deviceServiceSetSystemProperty(LOCAL_EUI64_PROPERTY_NAME, localEui64String);
    }
    else
    {
        sscanf(localEui64String, "%016" PRIx64, &localEui64);
    }

    free(localEui64String);

    return localEui64;
}

int zigbeeSubsystemSetAddresses()
{
    //get all paired zigbee devices and set their addresses in zigbee core
    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);

    uint16_t numEui64s = linkedListCount(devices);

    if (numEui64s > 0)
    {
        zhalDeviceEntry *deviceEntries = calloc(numEui64s, sizeof(zhalDeviceEntry));

        int i = 0;

        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
        while (linkedListIteratorHasNext(iterator))
        {
            icDevice *device = linkedListIteratorGetNext(iterator);

            deviceEntries[i].eui64 = zigbeeSubsystemIdToEui64(device->uuid);

            if (isDeviceAutoApsAcked(device))
            {
                deviceEntries[i].flags.bits.isAutoApsAcked = 1;
            }

            if (isDeviceUsingHashBasedLinkKey(device))
            {
                deviceEntries[i].flags.bits.useHashBasedLinkKey = 1;
            }
            else
            {
                //check to see if we got an early notification that this device uses hash based link key
                pthread_mutex_lock(&earlyHashBasedLinkKeyDevicesMtx);
                if(hashMapDelete(earlyHashedBasedLinkKeyDevices, device->uuid, strlen(device->uuid), NULL) == true)
                {
                    deviceEntries[i].flags.bits.useHashBasedLinkKey = 1;

                    //since we found it here, that means that it has not yet been set as metadata on the device
                    // set it now.  Don't send to zhal from here since we do it below.
                    // we only add device to earlyHashedBasedLinkKeyDevices if it has flag set. so mark as using it.
                    setDeviceUsingHashBasedLinkKey(device, true, true);
                }
                pthread_mutex_unlock(&earlyHashBasedLinkKeyDevicesMtx);
            }

            i++;
        }
        linkedListIteratorDestroy(iterator);

        zhalSetDevices(deviceEntries, numEui64s);

        free(deviceEntries);
    }

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return 0;
}

int zigbeeSubsystemFinalizeStartup()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__ );

    if (subsystemInitialized != NULL)
    {
        // this callback must be invoked after zigbeeSubsystemSetAddresses() for a device driver
        // to be able to make zhal requests with a device uuid else ZigbeeCore won't have record
        // of the device.
        //
        subsystemInitialized(ZIGBEE_SUBSYSTEM_NAME);
    }
    return 0;
}

int zigbeeSubsystemRemoveDeviceAddress(uint64_t eui64)
{
    return zhalRemoveDeviceAddress(eui64);
}

static uint64_t generateLocalEui64(const char *cpeId)
{
    uint64_t result = 0;

    if (cpeId == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to generate eui64: null cpeId", __FUNCTION__);
        return result;
    }

    // assemble our eui64 using ancient logic from zigbeeUtilities.c
    uint8_t euiBytes[8];

    // First use the iControl OUI (this used to be the uControl OUI: 00 18 5A)
    euiBytes[7] = 0x00;
    euiBytes[6] = 0x1B;
    euiBytes[5] = 0xAD;

    // Next use the two least significant bytes from the cpe id (MAC
    // address)
    int macByte;
    sscanf(cpeId + strlen(cpeId) - 2, "%02x", &macByte);
    euiBytes[4] = (uint8_t) macByte;
    sscanf(cpeId + strlen(cpeId) - 4, "%02x", &macByte);
    euiBytes[3] = (uint8_t) macByte;

    // Finally we use the three least significant bytes of a random number
    srand((unsigned int) time(NULL));
    int r = rand();
    euiBytes[2] = (uint8_t) ((r & 0xff0000) >> 16);
    euiBytes[1] = (uint8_t) ((r & 0xff00) >> 8);
    euiBytes[0] = (uint8_t) (r & 0xff);

    memcpy(&result, euiBytes, 8);

    return result;
}

static void prematureClusterCommandsFreeFunc(void *key, void *value)
{
    icLinkedList *list = (icLinkedList *) value;
    linkedListDestroy(list, (linkedListItemFreeFunc) freeReceivedClusterCommand);
    free(key);
}

char *zigbeeSubsystemEui64ToId(uint64_t eui64)
{
    char *result = (char *) malloc(21); //max uint64_t
    sprintf(result, "%016" PRIx64, eui64);
    return result;
}

char *zigbeeSubsystemEndpointIdAsString(uint8_t epId)
{
    /* max uint8_t + NULL */
    char *endpoinId = malloc(4);
    sprintf(endpoinId, "%" PRIu8, epId);
    return endpoinId;
}

uint64_t zigbeeSubsystemIdToEui64(const char *uuid)
{
    uint64_t eui64 = 0;

    if (sscanf(uuid, "%016" PRIx64, &eui64) != 1)
    {
        icLogError(LOG_TAG, "idToEui64: failed to parse %s", uuid);
    }

    return eui64;
}

/*
 * Create (if necessary) the directory where firmware files are stored, and return the path.
 * Caller is responsible for freeing the result
 */
char *zigbeeSubsystemGetAndCreateFirmwareFileDirectory(DeviceFirmwareType firmwareType)
{
    char *dynamicPath = getDynamicPath();

    char *lastSubdir = firmwareType == DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY ?
                       LEGACY_FIRMWARE_SUBDIR : OTA_FIRMWARE_SUBDIR;
    // 2 path delimiters and NULL terminator
    int directoryLen = 3 + strlen(dynamicPath) + strlen(ZIGBEE_FIRMWARE_SUBDIR) + strlen(lastSubdir);
    char *firmwareFileDirectory = (char *) malloc(sizeof(char) * directoryLen);

    sprintf(firmwareFileDirectory, "%s/%s/%s", dynamicPath, ZIGBEE_FIRMWARE_SUBDIR, lastSubdir);

    struct stat buffer;
    if (stat(firmwareFileDirectory, &buffer) != 0)
    {
        // Create directories if they don't exist
        if (mkdir_p(firmwareFileDirectory, 0777) != 0)
        {
            icLogError(LOG_TAG, "Failed to create firmware directory %s with error %d", firmwareFileDirectory, errno);
            // Cleanup and return NULL to signal error
            free(firmwareFileDirectory);
            firmwareFileDirectory = NULL;
        }
    }

    // cleanup
    free(dynamicPath);

    return firmwareFileDirectory;
}

IcDiscoveredDeviceDetails *createIcDiscoveredDeviceDetails()
{
    IcDiscoveredDeviceDetails *details = calloc(1, sizeof(IcDiscoveredDeviceDetails));

    return details;
}

static void cloneIcDiscoveredClusterDetails(const IcDiscoveredClusterDetails *original,
                                            IcDiscoveredClusterDetails *target)
{
    //copy the static stuff
    memcpy(target, original, sizeof(IcDiscoveredClusterDetails));
    target->numAttributeIds = original->numAttributeIds;
    // Passing 0 for calloc nmemb param can actually cause memory to be allocated.  Skip that if nothing is there.
    if (original->numAttributeIds > 0 && original->attributeIds != NULL)
    {
        target->attributeIds = (uint16_t *) calloc(original->numAttributeIds, sizeof(uint16_t));
        memcpy(target->attributeIds, original->attributeIds, original->numAttributeIds * sizeof(uint16_t));
        // Copy attribute values if they are there
        if (original->attributeValues != NULL)
        {
            target->attributeValues = (IcDiscoveredAttributeValue *) calloc(original->numAttributeIds,
                                                                             sizeof(IcDiscoveredAttributeValue));
            for(int i = 0; i < target->numAttributeIds; ++i)
            {
                target->attributeValues[i].attributeType = original->attributeValues[i].attributeType;
                target->attributeValues[i].dataLen = original->attributeValues[i].dataLen;
                if (target->attributeValues[i].dataLen > 0)
                {
                    target->attributeValues[i].data = malloc(original->attributeValues[i].dataLen);
                    memcpy(target->attributeValues[i].data, original->attributeValues[i].data,
                           original->attributeValues[i].dataLen);
                }
            }
        }
        else
        {
            target->attributeValues = NULL;
        }
    }
    else
    {
        target->numAttributeIds = 0;
        target->attributeIds = NULL;
        target->attributeValues = NULL;
    }
}

static void cloneIcDiscoveredEndpointDetails(const IcDiscoveredEndpointDetails *original,
                                             IcDiscoveredEndpointDetails *target)
{
    //copy the static stuff
    memcpy(target, original, sizeof(IcDiscoveredEndpointDetails));

    //deal with dynamic parts

    //server clusters
    target->numServerClusterDetails = original->numServerClusterDetails;
    // Passing 0 for calloc nmemb param can actually cause memory to be allocated.  Skip that if nothing is there.
    if (original->numServerClusterDetails > 0 && original->serverClusterDetails != NULL)
    {
        target->serverClusterDetails = (IcDiscoveredClusterDetails *) calloc(original->numServerClusterDetails,
                                                                             sizeof(IcDiscoveredClusterDetails));
        memcpy(target->serverClusterDetails,
               original->serverClusterDetails,
               original->numServerClusterDetails * sizeof(IcDiscoveredClusterDetails));

        for (int i = 0; i < original->numServerClusterDetails; i++)
        {
            cloneIcDiscoveredClusterDetails(&original->serverClusterDetails[i], &target->serverClusterDetails[i]);
        }
    }
    else
    {
        target->numServerClusterDetails = 0;
        target->serverClusterDetails = NULL;
    }

    //client clusters
    target->numClientClusterDetails = original->numClientClusterDetails;
    // Passing 0 for calloc nmemb param can actually cause memory to be allocated.  Skip that if nothing is there.
    if (original->numClientClusterDetails > 0 && original->clientClusterDetails != NULL)
    {
        target->clientClusterDetails = (IcDiscoveredClusterDetails *) calloc(original->numClientClusterDetails,
                                                                             sizeof(IcDiscoveredClusterDetails));
        memcpy(target->clientClusterDetails,
               original->clientClusterDetails,
               original->numClientClusterDetails * sizeof(IcDiscoveredClusterDetails));

        for (int i = 0; i < original->numClientClusterDetails; i++)
        {
            cloneIcDiscoveredClusterDetails(&original->clientClusterDetails[i], &target->clientClusterDetails[i]);
        }
    }
    else
    {
        target->numClientClusterDetails = 0;
        target->clientClusterDetails = NULL;
    }
}

IcDiscoveredDeviceDetails *cloneIcDiscoveredDeviceDetails(const IcDiscoveredDeviceDetails *original)
{
    if (original == NULL)
    {
        return NULL;
    }

    IcDiscoveredDeviceDetails *result = (IcDiscoveredDeviceDetails *) calloc(1, sizeof(IcDiscoveredDeviceDetails));
    memcpy(result, original, sizeof(IcDiscoveredDeviceDetails));
    if (original->manufacturer != NULL)
    {
        result->manufacturer = strdup(original->manufacturer);
    }
    if (original->model != NULL)
    {
        result->model = strdup(original->model);
    }
    if (original->numEndpoints > 0 && original->endpointDetails != NULL)
    {
        result->endpointDetails = (IcDiscoveredEndpointDetails *) calloc(original->numEndpoints,
                                                                         sizeof(IcDiscoveredEndpointDetails));
        result->numEndpoints = original->numEndpoints;

        for (int i = 0; i < original->numEndpoints; i++)
        {
            cloneIcDiscoveredEndpointDetails(&original->endpointDetails[i], &result->endpointDetails[i]);
        }
    }
    else
    {
        result->numEndpoints = 0;
        result->endpointDetails = NULL;
    }

    return result;
}

void freeIcDiscoveredDeviceDetails(IcDiscoveredDeviceDetails *details)
{
    if (details != NULL)
    {
        free(details->manufacturer);
        free(details->model);

        for (int i = 0; i < details->numEndpoints; i++)
        {
            for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
            {
                free(details->endpointDetails[i].serverClusterDetails[j].attributeIds);
                if (details->endpointDetails[i].serverClusterDetails[j].attributeValues != NULL)
                {
                    for(int k = 0; k < details->endpointDetails[i].serverClusterDetails[j].numAttributeIds; ++k)
                    {
                        free(details->endpointDetails[i].serverClusterDetails[j].attributeValues[k].data);
                    }
                    free(details->endpointDetails[i].serverClusterDetails[j].attributeValues);
                }
            }
            free(details->endpointDetails[i].serverClusterDetails);

            for (int j = 0; j < details->endpointDetails[i].numClientClusterDetails; j++)
            {
                free(details->endpointDetails[i].clientClusterDetails[j].attributeIds);
                if (details->endpointDetails[i].clientClusterDetails[j].attributeValues != NULL)
                {
                    for(int k = 0; k < details->endpointDetails[i].clientClusterDetails[j].numAttributeIds; ++k)
                    {
                        free(details->endpointDetails[i].clientClusterDetails[j].attributeValues[k].data);
                    }
                    free(details->endpointDetails[i].clientClusterDetails[j].attributeValues);
                }
            }
            free(details->endpointDetails[i].clientClusterDetails);
        }

        free(details->endpointDetails);
        free(details);
    }
}

static cJSON *icDiscoveredClusterDetailsToJson(const IcDiscoveredClusterDetails *details)
{
    cJSON *cluster = cJSON_CreateObject();

    cJSON_AddNumberToObject(cluster, ID_JSON_PROP, details->clusterId);
    cJSON_AddBoolToObject(cluster, IS_SERVER_JSON_PROP, details->isServer);

    cJSON *attributeIds = cJSON_CreateArray();
    for (int i = 0; i < details->numAttributeIds; i++)
    {
        cJSON_AddItemToArray(attributeIds, cJSON_CreateNumber(details->attributeIds[i]));
    }
    cJSON_AddItemToObject(cluster, ATTRIBUTE_IDS_JSON_PROP, attributeIds);

    return cluster;
}

static cJSON *icDiscoveredEndpointDetailsToJson(const IcDiscoveredEndpointDetails *details)
{
    cJSON *endpoint = cJSON_CreateObject();

    cJSON_AddNumberToObject(endpoint, ID_JSON_PROP, details->endpointId);
    cJSON_AddNumberToObject(endpoint, PROFILEID_JSON_PROP, details->appProfileId);
    cJSON_AddNumberToObject(endpoint, DEVICEID_JSON_PROP, details->appDeviceId);
    cJSON_AddNumberToObject(endpoint, DEVICEVER_JSON_PROP, details->appDeviceVersion);

    cJSON *serverClusterInfos = cJSON_CreateArray();
    for (int j = 0; j < details->numServerClusterDetails; j++)
    {
        cJSON_AddItemToArray(serverClusterInfos, icDiscoveredClusterDetailsToJson(&details->serverClusterDetails[j]));
    }
    cJSON_AddItemToObject(endpoint, SERVERCLUSTERINFOS_JSON_PROP, serverClusterInfos);

    cJSON *clientClusterInfos = cJSON_CreateArray();
    for (int j = 0; j < details->numClientClusterDetails; j++)
    {
        cJSON_AddItemToArray(clientClusterInfos, icDiscoveredClusterDetailsToJson(&details->clientClusterDetails[j]));
    }
    cJSON_AddItemToObject(endpoint, CLIENTCLUSTERINFOS_JSON_PROP, clientClusterInfos);

    return endpoint;
}

cJSON *icDiscoveredDeviceDetailsToJson(const IcDiscoveredDeviceDetails *details)
{
    if (details == NULL)
    {
        return NULL;
    }

    cJSON *result = cJSON_CreateObject();

    char *eui64 = zigbeeSubsystemEui64ToId(details->eui64);
    cJSON_AddStringToObject(result, EUI64_JSON_PROP, eui64);
    free(eui64);
    cJSON_AddStringToObject(result, MANUF_JSON_PROP, details->manufacturer);
    cJSON_AddStringToObject(result, MODEL_JSON_PROP, details->model);
    cJSON_AddNumberToObject(result, HWVER_JSON_PROP, details->hardwareVersion);
    cJSON_AddNumberToObject(result, FWVER_JSON_PROP, details->firmwareVersion);
    cJSON_AddNumberToObject(result, APPVER_JSON_PROP, details->appVersion);

    cJSON *endpoints = cJSON_CreateArray();

    for (int i = 0; i < details->numEndpoints; i++)
    {
        cJSON_AddItemToArray(endpoints, icDiscoveredEndpointDetailsToJson(&details->endpointDetails[i]));
    }

    char *deviceType;
    switch (details->deviceType)
    {
        case deviceTypeEndDevice:
            deviceType = ENDDEVICE_JSON_PROP;
            break;

        case deviceTypeRouter:
            deviceType = ROUTERDEVICE_JSON_PROP;
            break;

        case deviceTypeUnknown:
        default:
            deviceType = UNKNOWN_JSON_PROP;
            break;
    }
    cJSON_AddStringToObject(result, DEVICETYPE_JSON_PROP, deviceType);

    char *powerSource;
    switch (details->powerSource)
    {
        case powerSourceMains:
            powerSource = MAINS_JSON_PROP;
            break;

        case powerSourceBattery:
            powerSource = BATT_JSON_PROP;
            break;

        case powerSourceUnknown:
        default:
            powerSource = UNKNOWN_JSON_PROP;
            break;
    }
    cJSON_AddStringToObject(result, POWERSOURCE_JSON_PROP, powerSource);

    cJSON_AddItemToObject(result, ENDPOINTS_JSON_PROP, endpoints);

    return result;
}

static bool icDiscoveredClusterDetailsFromJson(const cJSON *detailsJson, IcDiscoveredClusterDetails *details)
{
    bool success = true;

    cJSON *attributeIds = NULL;
    cJSON *attributeId = NULL;
    int j = 0;

    int tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, ID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->clusterId = (uint16_t) tmpInt;

    if (!(success = getCJSONBool(detailsJson, IS_SERVER_JSON_PROP, &details->isServer)))
    {
        goto exit;
    }

    attributeIds = cJSON_GetObjectItem(detailsJson, ATTRIBUTE_IDS_JSON_PROP);
    if (attributeIds == NULL)
    {
        success = false;
        goto exit;
    }

    details->numAttributeIds = (uint8_t) cJSON_GetArraySize(attributeIds);
    details->attributeIds = (uint16_t *) calloc(details->numAttributeIds, sizeof(uint16_t));
    cJSON_ArrayForEach(attributeId, attributeIds)
    {
        details->attributeIds[j++] = (uint16_t) attributeId->valueint;
    }

    exit:
    return success;
}

static bool icDiscoveredEndpointDetailsFromJson(const cJSON *detailsJson, IcDiscoveredEndpointDetails *details)
{
    bool success = true;

    cJSON *serverClusterInfos = NULL;
    cJSON *serverClusterInfo = NULL;
    cJSON *clientClusterInfos = NULL;
    cJSON *clientClusterInfo = NULL;
    int j = 0;

    int tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, ID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->endpointId = (uint8_t) tmpInt;

    tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, PROFILEID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->appProfileId = (uint16_t) tmpInt;

    tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, DEVICEID_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->appDeviceId = (uint16_t) tmpInt;

    tmpInt = 0;
    if (!(success = getCJSONInt(detailsJson, DEVICEVER_JSON_PROP, &tmpInt)))
    {
        goto exit;
    }
    details->appDeviceVersion = (uint8_t) tmpInt;

    //server cluster infos
    serverClusterInfos = cJSON_GetObjectItem(detailsJson, SERVERCLUSTERINFOS_JSON_PROP);
    if (serverClusterInfos == NULL)
    {
        success = false;
        goto exit;
    }
    details->numServerClusterDetails = (uint8_t) cJSON_GetArraySize(serverClusterInfos);
    details->serverClusterDetails = (IcDiscoveredClusterDetails *) calloc(details->numServerClusterDetails,
                                                                          sizeof(IcDiscoveredClusterDetails));
    j = 0;
    cJSON_ArrayForEach(serverClusterInfo, serverClusterInfos)
    {
        if (!(success = icDiscoveredClusterDetailsFromJson(serverClusterInfo, &details->serverClusterDetails[j++])))
        {
            goto exit;
        }
    }

    //client cluster infos
    clientClusterInfos = cJSON_GetObjectItem(detailsJson, CLIENTCLUSTERINFOS_JSON_PROP);
    if (clientClusterInfos == NULL)
    {
        success = false;
        goto exit;
    }
    details->numClientClusterDetails = (uint8_t) cJSON_GetArraySize(clientClusterInfos);
    details->clientClusterDetails = (IcDiscoveredClusterDetails *) calloc(details->numClientClusterDetails,
                                                                          sizeof(IcDiscoveredClusterDetails));
    j = 0;
    cJSON_ArrayForEach(clientClusterInfo, clientClusterInfos)
    {
        if (!(success = icDiscoveredClusterDetailsFromJson(clientClusterInfo, &details->clientClusterDetails[j++])))
        {
            goto exit;
        }
    }

    exit:
    return success;
}

IcDiscoveredDeviceDetails *icDiscoveredDeviceDetailsFromJson(const cJSON *detailsJson)
{
    bool success = true;
    cJSON *endpoints = NULL;
    cJSON *endpoint = NULL;
    char *deviceType = NULL;
    char *powerSource = NULL;
    double tmpVal = 0;
    int i = 0;

    if (detailsJson == NULL)
    {
        return NULL;
    }

    IcDiscoveredDeviceDetails *result = (IcDiscoveredDeviceDetails *) calloc(1, sizeof(IcDiscoveredDeviceDetails));

    char *eui64Str = getCJSONString(detailsJson, EUI64_JSON_PROP);
    if (eui64Str == NULL)
    {
        success = false;
        goto exit;
    }
    result->eui64 = zigbeeSubsystemIdToEui64(eui64Str);
    free(eui64Str);

    result->manufacturer = getCJSONString(detailsJson, MANUF_JSON_PROP);
    if (result->manufacturer == NULL)
    {
        success = false;
        goto exit;
    }

    result->model = getCJSONString(detailsJson, MODEL_JSON_PROP);
    if (result->model == NULL)
    {
        success = false;
        goto exit;
    }

    tmpVal = 0;
    if (!(success = getCJSONDouble(detailsJson, HWVER_JSON_PROP, &tmpVal)))
    {
        goto exit;
    }
    result->hardwareVersion = (uint64_t) tmpVal;

    tmpVal = 0;
    if (!(success = getCJSONDouble(detailsJson, FWVER_JSON_PROP, &tmpVal)))
    {
        goto exit;
    }
    result->firmwareVersion = (uint64_t) tmpVal;

    tmpVal = 0;
    if (!(success = getCJSONDouble(detailsJson, APPVER_JSON_PROP, &tmpVal)))
    {
        goto exit;
    }
    result->appVersion = (uint64_t) tmpVal;

    endpoints = cJSON_GetObjectItem(detailsJson, ENDPOINTS_JSON_PROP);
    if (endpoints == NULL)
    {
        success = false;
        goto exit;
    }

    result->numEndpoints = (uint8_t) cJSON_GetArraySize(endpoints);
    result->endpointDetails = (IcDiscoveredEndpointDetails *) calloc(result->numEndpoints,
                                                                     sizeof(IcDiscoveredEndpointDetails));
    i = 0;
    cJSON_ArrayForEach(endpoint, endpoints)
    {
        if (!(success = icDiscoveredEndpointDetailsFromJson(endpoint, &result->endpointDetails[i])))
        {
            goto exit;
        }

        i++;
    }

    deviceType = getCJSONString(detailsJson, DEVICETYPE_JSON_PROP);
    if (deviceType == NULL)
    {
        success = false;
        goto exit;
    }
    if (strcmp(deviceType, ENDDEVICE_JSON_PROP) == 0)
    {
        result->deviceType = deviceTypeEndDevice;
    }
    else if (strcmp(deviceType, ROUTERDEVICE_JSON_PROP) == 0)
    {
        result->deviceType = deviceTypeRouter;
    }
    else
    {
        result->deviceType = deviceTypeUnknown;
    }
    free(deviceType);

    powerSource = getCJSONString(detailsJson, POWERSOURCE_JSON_PROP);
    if (powerSource == NULL)
    {
        success = false;
        goto exit;
    }
    if (strcmp(powerSource, MAINS_JSON_PROP) == 0)
    {
        result->powerSource = powerSourceMains;
    }
    else if (strcmp(powerSource, BATT_JSON_PROP) == 0)
    {
        result->powerSource = powerSourceBattery;
    }
    else
    {
        result->powerSource = powerSourceUnknown;
    }
    free(powerSource);

    exit:
    if (!success)
    {
        icLogError(LOG_TAG, "%s: failed to parse", __FUNCTION__);
        freeIcDiscoveredDeviceDetails(result);
        result = NULL;
    }
    else
    {
        zigbeeSubsystemDumpDeviceDiscovered(result);
    }
    return result;
}

static void freeFirmwareFilesMap(void *key, void *value)
{
    // key is the firmware file, which is contained within the device descriptor which is the value
    DeviceDescriptor *dd = (DeviceDescriptor *) value;
    deviceDescriptorFree(dd);
}

static void cleanupFirmwareFilesByType(DeviceFirmwareType deviceFirmwareType, icHashMap *allFirmwareFiles)
{
    // Get the path to the firmware files for this type
    char *dirPath = zigbeeSubsystemGetAndCreateFirmwareFileDirectory(deviceFirmwareType);

    icLogDebug(LOG_TAG, "Checking for firmware files to cleanup in %s", dirPath);

    DIR *dir = opendir(dirPath);

    if (dir != NULL)
    {
        struct dirent *dp;
        while ((dp = readdir(dir)) != NULL)
        {
            char *filePath = malloc(sizeof(char) * (strlen(dirPath) + 1 + strlen(dp->d_name) + 1));
            sprintf(filePath, "%s/%s", dirPath, dp->d_name);
            struct stat statBuf;
            if (lstat(filePath, &statBuf) == 0)
            {
                // Skip directories and symlinks
                if ((statBuf.st_mode & S_IFMT) != S_IFDIR && (statBuf.st_mode & S_IFMT) != S_IFLNK)
                {
                    // Check if we still need the file
                    DeviceDescriptor *dd = (DeviceDescriptor *) hashMapGet(allFirmwareFiles, dp->d_name,
                                                                           strlen(dp->d_name) + 1);
                    // Either it wasn't found, or the file is of a different firmware type(and as such is in another directory)
                    // This second case is weird, but just covers the fact that you could technically have legacy and OTA
                    // firmware files with the same name
                    if (dd == NULL || dd->latestFirmware->type != deviceFirmwareType)
                    {
                        if (remove(filePath) == 0)
                        {
                            icLogInfo(LOG_TAG, "Removed unused firmware file %s", filePath);
                        }
                        else
                        {
                            icLogError(LOG_TAG, "Failed to remove firmware file %s", filePath);
                        }
                    }
                    else
                    {
                        icLogDebug(LOG_TAG, "Firmware file %s is still needed", filePath);
                    }
                }
            }
            else
            {
                icLogDebug(LOG_TAG, "Unable to stat file at path %s", filePath);
            }
            free(filePath);
        }
        closedir(dir);
    }
    else
    {
        icLogError(LOG_TAG, "Could not read firmware files in directory %s", dirPath);
    }


    free(dirPath);
}

static bool findDeviceResource(void *searchVal, void *item)
{
    icDeviceResource *resourceItem = (icDeviceResource *) item;
    return strcmp(searchVal, resourceItem->id) == 0;
}

/*
 * Cleanup any unused firmware files
 */
void zigbeeSubsystemCleanupFirmwareFiles()
{
    icLogDebug(LOG_TAG, "Scanning for unused firmware files...");

    // Get all our devices
    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);

    // Build all firmware files to retain
    icHashMap *allFirmwareFiles = hashMapCreate();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);
        if (dd != NULL && dd->latestFirmware != NULL && dd->latestFirmware->filenames != NULL)
        {
            icLogDebug(LOG_TAG, "For device %s, found device descriptor uuid %s", device->uuid, dd->uuid);
            // Get the device's current firmware version, we only want to keep what we still need, e.g.
            // if there is a newer firmware version that we haven't upgraded to yet.
            icDeviceResource *firmwareVersionResource = (icDeviceResource *) linkedListFind(
                    device->resources, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, findDeviceResource);
            if (firmwareVersionResource != NULL)
            {
                icLogDebug(LOG_TAG, "For device %s we are at version %s, latest version is %s",
                           device->uuid, firmwareVersionResource->value, dd->latestFirmware->version);
                // Check if latest version is newer than our version
                int versionComparison = compareVersionStrings(dd->latestFirmware->version,
                                                              firmwareVersionResource->value);
                if (versionComparison == -1)
                {
                    icLinkedListIterator *fileIter = linkedListIteratorCreate(dd->latestFirmware->filenames);
                    while (linkedListIteratorHasNext(fileIter))
                    {
                        char *filename = (char *) linkedListIteratorGetNext(fileIter);
                        icLogDebug(LOG_TAG, "For device %s we need firmware file %s", device->uuid, filename);
                        //Have to put clones in the map in case there is more than one filename
                        DeviceDescriptor *ddClone = deviceDescriptorClone(dd);
                        //Have to use the clone's filename as the key for memory reasons.
                        icLinkedListIterator *cloneFileIter = linkedListIteratorCreate(
                                ddClone->latestFirmware->filenames);
                        while (linkedListIteratorHasNext(cloneFileIter))
                        {
                            char *cloneFilename = (char *) linkedListIteratorGetNext(cloneFileIter);
                            if (strcmp(filename, cloneFilename) == 0)
                            {
                                hashMapPut(allFirmwareFiles, cloneFilename, strlen(filename) + 1, ddClone);
                                ddClone = NULL;
                            }
                        }
                        linkedListIteratorDestroy(cloneFileIter);
                        deviceDescriptorFree(ddClone);
                    }
                    linkedListIteratorDestroy(fileIter);
                }
            }
        }
        // The map will clean up the clones, but we need to free this copy here.
        deviceDescriptorFree(dd);

    }
    // Go ahead and cleanup devices as we don't need those anymore
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    // Cleanup both types
    cleanupFirmwareFilesByType(DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY, allFirmwareFiles);
    cleanupFirmwareFilesByType(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA, allFirmwareFiles);

    // Cleanup our hashmap
    hashMapDestroy(allFirmwareFiles, freeFirmwareFilesMap);
}

static void prematureClusterCommandFreeKeyFunc(void *key, void *value)
{
    (void) value; //unused
    //only free the key (an eui64)
    free(key);
}

icLinkedList *zigbeeSubsystemGetPrematureClusterCommands(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    result = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
    hashMapDelete(prematureClusterCommands, &eui64, sizeof(uint64_t), prematureClusterCommandFreeKeyFunc);
    pthread_mutex_unlock(&prematureClusterCommandsMtx);
    return result;
}

void zigbeeSubsystemDestroyPrematureClusterCommands(uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    result = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
    if (result)
    {
        hashMapDelete(prematureClusterCommands, &eui64, sizeof(uint64_t), prematureClusterCommandFreeKeyFunc);
        linkedListDestroy(result, (linkedListItemFreeFunc) freeReceivedClusterCommand);
    }
    pthread_mutex_unlock(&prematureClusterCommandsMtx);
}

ReceivedClusterCommand *zigbeeSubsystemGetPrematureClusterCommand(uint64_t eui64,
                                                                  uint8_t commandId,
                                                                  uint32_t timeoutSeconds)
{
    icLogDebug(LOG_TAG, "%s: looking for command 0x%02x for %016"PRIx64" for %"PRIu32" seconds",
            __FUNCTION__,
            commandId,
            eui64,
            timeoutSeconds);

    ReceivedClusterCommand *result = NULL;
    uint32_t iterations = 0;

    pthread_mutex_lock(&prematureClusterCommandsMtx);
    while(result == NULL && iterations++ < timeoutSeconds)
    {
        icLinkedList *commands = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
        if (commands)
        {
            icLinkedListIterator *it = linkedListIteratorCreate(commands);
            while (linkedListIteratorHasNext(it))
            {
                ReceivedClusterCommand *command = linkedListIteratorGetNext(it);
                if (command->commandId == commandId)
                {
                    icLogDebug(LOG_TAG, "%s: found the command", __FUNCTION__);
                    result = command;
                    break;
                }
            }
            linkedListIteratorDestroy(it);
        }

        if(result == NULL)
        {
            incrementalCondTimedWait(&prematureClusterCommandsCond, &prematureClusterCommandsMtx, 1);
        }
    }
    pthread_mutex_unlock(&prematureClusterCommandsMtx);

    return result;
}

void zigbeeSubsystemRemovePrematureClusterCommand(uint64_t eui64, uint8_t commandId)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    icLinkedList *result = NULL;
    pthread_mutex_lock(&prematureClusterCommandsMtx);
    result = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
    if (result)
    {
        icLinkedListIterator *it = linkedListIteratorCreate(result);
        while(linkedListIteratorHasNext(it))
        {
            ReceivedClusterCommand *command = linkedListIteratorGetNext(it);
            if(command->commandId == commandId)
            {
                linkedListIteratorDeleteCurrent(it, (linkedListItemFreeFunc) freeReceivedClusterCommand);
            }
        }
        linkedListIteratorDestroy(it);

        //if the list is now empty, go ahead and destroy the whole enchilada
        if(linkedListCount(result) == 0)
        {
            hashMapDelete(prematureClusterCommands, &eui64, sizeof(uint64_t), prematureClusterCommandFreeKeyFunc);
            linkedListDestroy(result, (linkedListItemFreeFunc) freeReceivedClusterCommand);
        }
    }
    pthread_mutex_unlock(&prematureClusterCommandsMtx);
}

/**
 * Add a premature cluster command
 * @param command the command to add
 */
void zigbeeSubsystemAddPrematureClusterCommand(const ReceivedClusterCommand *command)
{
    if (command != NULL)
    {
        icLogDebug(LOG_TAG, "Adding premature cluster command for device %016"
                PRIx64, command->eui64);

        // save it
        pthread_mutex_lock(&prematureClusterCommandsMtx);

        if (prematureClusterCommands == NULL)
        {
            prematureClusterCommands = hashMapCreate();
        }

        uint64_t eui64 = command->eui64;

        icLinkedList *list = hashMapGet(prematureClusterCommands, &eui64, sizeof(uint64_t));
        if (list == NULL)
        {
            list = linkedListCreate();
            uint64_t *eui = (uint64_t *) malloc(sizeof(uint64_t));
            *eui = command->eui64;
            hashMapPut(prematureClusterCommands, eui, sizeof(uint64_t), list);
        }
        linkedListAppend(list, receivedClusterCommandClone(command));

        pthread_cond_broadcast(&prematureClusterCommandsCond);
        pthread_mutex_unlock(&prematureClusterCommandsMtx);
    }
}

bool icDiscoveredDeviceDetailsGetAttributeEndpoint(const IcDiscoveredDeviceDetails *details,
                                                   uint16_t clusterId,
                                                   uint16_t attributeId,
                                                   uint8_t *endpointId)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; i < details->numEndpoints; i++)
        {
            for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
            {
                IcDiscoveredClusterDetails *clusterDetails = &details->endpointDetails[i].serverClusterDetails[j];

                if (clusterDetails->clusterId == clusterId &&
                    clusterDetails->isServer)
                {
                    for (int k = 0; k < clusterDetails->numAttributeIds; k++)
                    {
                        if (clusterDetails->attributeIds[k] == attributeId)
                        {
                            if (endpointId != NULL)
                            {
                                *endpointId = details->endpointDetails[i].endpointId;
                            }
                            result = true;
                        }
                    }
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsGetClusterEndpoint(const IcDiscoveredDeviceDetails *details,
                                                 uint16_t clusterId,
                                                 uint8_t *endpointId)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; i < details->numEndpoints; i++)
        {
            for (int j = 0; j < details->endpointDetails[i].numServerClusterDetails; j++)
            {
                IcDiscoveredClusterDetails *clusterDetails = &details->endpointDetails[i].serverClusterDetails[j];

                if (clusterDetails->clusterId == clusterId && clusterDetails->isServer)
                {
                    if (endpointId != NULL)
                    {
                        *endpointId = details->endpointDetails[i].endpointId;
                    }
                    result = true;
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsEndpointHasCluster(const IcDiscoveredDeviceDetails *details,
                                                 uint8_t endpointId,
                                                 uint16_t clusterId,
                                                 bool wantServer)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; i < details->numEndpoints; i++)
        {
            IcDiscoveredEndpointDetails epDetails = details->endpointDetails[i];
            if (epDetails.endpointId == endpointId)
            {
                int detailsLen = wantServer ? epDetails.numServerClusterDetails : epDetails.numClientClusterDetails;
                for (int j = 0; j < detailsLen; j++)
                {
                    IcDiscoveredClusterDetails *clusterDetails = wantServer ? &epDetails.serverClusterDetails[j]
                                                                            : &epDetails.clientClusterDetails[j];

                    if (clusterDetails->clusterId == clusterId && clusterDetails->isServer == wantServer)
                    {
                        result = true;
                        break;
                    }
                }

                break;
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsClusterHasAttribute(const IcDiscoveredDeviceDetails *details,
                                                  uint8_t endpointId,
                                                  uint16_t clusterId,
                                                  bool wantServer,
                                                  uint16_t attributeId)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; result == false && i < details->numEndpoints; i++)
        {
            if (details->endpointDetails[i].endpointId == endpointId)
            {
                IcDiscoveredEndpointDetails epDetails = details->endpointDetails[i];
                int detailsLen = wantServer ? epDetails.numServerClusterDetails : epDetails.numClientClusterDetails;
                for (int j = 0; j < detailsLen; j++)
                {
                    IcDiscoveredClusterDetails *clusterDetails = wantServer ? &epDetails.serverClusterDetails[j]
                                                                            : &epDetails.clientClusterDetails[j];

                    if (clusterDetails->clusterId == clusterId)
                    {
                        for (int k = 0; k < clusterDetails->numAttributeIds; k++)
                        {
                            if (clusterDetails->attributeIds[k] == attributeId)
                            {
                                result = true;
                                break;
                            }
                        }
                        // Found the cluster we want, we are done
                        break;
                    }
                }
            }
        }
    }

    return result;
}

bool icDiscoveredDeviceDetailsClusterGetAttributeValue(const IcDiscoveredDeviceDetails *details,
                                                       uint8_t endpointId,
                                                       uint16_t clusterId,
                                                       bool wantServer,
                                                       uint16_t attributeId,
                                                       IcDiscoveredAttributeValue **attributeValue)
{
    bool result = false;

    if (details != NULL)
    {
        for (int i = 0; result == false && i < details->numEndpoints; i++)
        {
            if (details->endpointDetails[i].endpointId == endpointId)
            {
                IcDiscoveredEndpointDetails epDetails = details->endpointDetails[i];
                int detailsLen = wantServer ? epDetails.numServerClusterDetails : epDetails.numClientClusterDetails;
                for (int j = 0; j < detailsLen; j++)
                {
                    IcDiscoveredClusterDetails *clusterDetails = wantServer ? &epDetails.serverClusterDetails[j]
                                                                            : &epDetails.clientClusterDetails[j];

                    if (clusterDetails->clusterId == clusterId)
                    {
                        for (int k = 0; k < clusterDetails->numAttributeIds; k++)
                        {
                            if (clusterDetails->attributeIds[k] == attributeId)
                            {
                                if (clusterDetails->attributeValues != NULL)
                                {
                                    *(attributeValue) = &clusterDetails->attributeValues[k];
                                    result = true;
                                }
                                break;
                            }
                        }
                        // Found the cluster we want, we are done
                        break;
                    }
                }
            }
        }
    }

    return result;
}

int zigbeeSubsystemGetSystemStatus(zhalSystemStatus *status)
{
    if (status == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return -1;
    }

    return zhalGetSystemStatus(status);
}

cJSON *zigbeeSubsystemGetAndClearCounters()
{
    return zhalGetAndClearCounters();
}

static uint8_t calculateBestChannel()
{
    uint8_t result = 0; //invalid channel default

    uint32_t scanDurationMillis = getPropertyAsUInt32(CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS,
                                                      DEFAULT_ZIGBEE_CHANNEL_SCAN_DUR_MILLIS);

    uint32_t scanCount = getPropertyAsUInt32(CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS,
                                             DEFAULT_ZIGBEE_PER_CHANNEL_NUMBER_OF_SCANS);

    uint8_t channels[] = {15, 19, 20, 25};
    int8_t bestScore = 0;

    icLinkedList *scanResults = zhalPerformEnergyScan(channels, sizeof(channels), scanDurationMillis, scanCount);
    if (scanResults != NULL)
    {
        sbIcLinkedListIterator *it = linkedListIteratorCreate(scanResults);
        while (linkedListIteratorHasNext(it))
        {
            zhalEnergyScanResult *scanResult = linkedListIteratorGetNext(it);

            if (scanResult->score > bestScore)
            {
                bestScore = scanResult->score;
                result = scanResult->channel;
                icLogDebug(LOG_TAG, "%s: channel %"
                        PRIu8
                        " is now the best channel", __FUNCTION__, result);
            }
        }

        if (bestScore == 0)
        {
            icLogWarn(LOG_TAG, "%s: no channel had non-zero score, returning invalid channel", __FUNCTION__);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to perform energy scan, returning invalid channel", __FUNCTION__);
    }

    return result;
}

static icHashMap *getDeviceIdsInCommFail()
{
    icHashMap *result = hashMapCreate();

    icLinkedList *allDevices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    sbIcLinkedListIterator *it = linkedListIteratorCreate(allDevices);
    while (linkedListIteratorHasNext(it))
    {
        icDevice *device = linkedListIteratorGetNext(it);

        icDeviceResource *commFailResource = deviceServiceGetResourceById(device->uuid,
                                                                          NULL,
                                                                          COMMON_DEVICE_RESOURCE_COMM_FAIL);

        if(commFailResource != NULL && strcmp(commFailResource->value, "true") == 0)
        {
            hashMapPut(result, strdup(device->uuid), strlen(device->uuid), NULL);
        }

        resourceDestroy(commFailResource);
    }

    linkedListDestroy(allDevices, (linkedListItemFreeFunc) deviceDestroy);

    return result;
}

static bool isTimedOut(uint64_t dateOfLastContactMillis, uint64_t maxRejoinTimeoutMillis)
{
    return ((getCurrentUnixTimeMillis() - dateOfLastContactMillis) > maxRejoinTimeoutMillis);
}

typedef struct
{
    icHashMap *deviceIdsPreviouslyInCommFail;
    uint8_t previousChannel;
    uint8_t targetedChannel;
    uint64_t maxRejoinTimeoutMillis;
} ChannelChangeDeviceWatchdogArg;

static void channelChangeDeviceWatchdogTask(void *arg)
{
    uint8_t previousChannel = 0;
    ChannelChangeDeviceWatchdogArg *myArg = (ChannelChangeDeviceWatchdogArg*)arg;
    bool needsToFallBackToPreviousChannel = false;

    icLogDebug(LOG_TAG, "%s: checking to see if all zigbee devices rejoined", __FUNCTION__);

    icLinkedList *allDevices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    pthread_mutex_lock(&channelChangeMutex);

    sbIcLinkedListIterator *it = linkedListIteratorCreate(allDevices);
    while (linkedListIteratorHasNext(it))
    {
        icDevice *device = linkedListIteratorGetNext(it);

        //if we have not heard from this device and it was not in comm fail before the channel
        // change, then we have a problem and need to change back to the original channel.

        if (hashMapContains(myArg->deviceIdsPreviouslyInCommFail, device->uuid, strlen(device->uuid)) == true)
        {
            icLogDebug(LOG_TAG, "%s: device %s was previously in comm fail -- ignoring", __FUNCTION__, device->uuid);
            continue;
        }

        if(isTimedOut(getDeviceDateLastContacted(device->uuid), myArg->maxRejoinTimeoutMillis) == true)
        {
            icLogWarn(LOG_TAG, "%s: device %s has not joined back in time.  Reverting to previous channel.",
                      __FUNCTION__, device->uuid);

            needsToFallBackToPreviousChannel = true;
            break;
        }
    }

    if(needsToFallBackToPreviousChannel)
    {
        char *channelStr = NULL;
        if(deviceServiceGetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, &channelStr) &&
            channelStr != NULL &&
            strlen(channelStr) > 0 &&
            stringToUint8(channelStr, &previousChannel))
        {
            zhalNetworkChangeRequest networkChangeRequest;
            memset(&networkChangeRequest, 0, sizeof(zhalNetworkChangeRequest));
            networkChangeRequest.channel = previousChannel;

            if (zhalNetworkChange(&networkChangeRequest) == 0)
            {
                icLogDebug(LOG_TAG, "%s: successfully reverted back to channel %d", __FUNCTION__, previousChannel);
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to change back to previous channel", __FUNCTION__);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: needed to change back to previous channel, but no previous channel found!",
                    __FUNCTION__);
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "%s: channel change request fully completed successfully", __FUNCTION__);
    }

    //set the previous channel to empty string
    deviceServiceSetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, "");
    isChannelChangeInProgress = false;

    sendZigbeeChannelChangedEvent(!needsToFallBackToPreviousChannel,
                                  needsToFallBackToPreviousChannel ? previousChannel : myArg->targetedChannel,
                                  myArg->targetedChannel);

    hashMapDestroy(myArg->deviceIdsPreviouslyInCommFail, NULL);
    free(myArg);

    linkedListDestroy(allDevices, (linkedListItemFreeFunc) deviceDestroy);

    pthread_mutex_unlock(&channelChangeMutex);
}

static void startChannelChangeDeviceWatchdog(uint8_t previousChannel, uint8_t targetedChannel)
{
    //Get the list of device ids for devices that are in comm fail before we even try to change
    // channels.  These devices wont prevent a channel change if they don't follow to the new
    // channel.
    icHashMap *deviceIdsInCommFail = getDeviceIdsInCommFail();

    uint32_t rejoinTimeoutMinutes =
            getPropertyAsUInt32(CPE_ZIGBEE_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES,
                                DEFAULT_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES);

    //the task will clean up the deviceIdsInCommFail and the arg instance
    ChannelChangeDeviceWatchdogArg *arg = calloc(1, sizeof(*arg));
    arg->deviceIdsPreviouslyInCommFail = deviceIdsInCommFail;
    arg->previousChannel = previousChannel;
    arg->targetedChannel = targetedChannel;
    arg->maxRejoinTimeoutMillis = rejoinTimeoutMinutes * 60ULL * 1000ULL;
    scheduleDelayTask(rejoinTimeoutMinutes, DELAY_MINS, channelChangeDeviceWatchdogTask, arg);
}

ChannelChangeResponse zigbeeSubsystemChangeChannel(uint8_t channel, bool dryRun)
{
    ChannelChangeResponse result = {.channelNumber = 0, .responseCode = channelChangeFailed};

    pthread_mutex_lock(&channelChangeMutex);

    if(getPropertyAsBool(CPE_ZIGBEE_CHANNEL_CHANGE_ENABLED_KEY, true) == false)
    {
        icLogWarn(LOG_TAG, "%s: attempt to change to channel while %s=false.  Denied", __FUNCTION__,
                 CPE_ZIGBEE_CHANNEL_CHANGE_ENABLED_KEY);
        result.responseCode = channelChangeNotAllowed;
    }
    else if (isChannelChangeInProgress)
    {
        result.responseCode = channelChangeInProgress;
    }
    else if (channel != 0 && (channel < MIN_ZIGBEE_CHANNEL || channel > MAX_ZIGBEE_CHANNEL))  //0 means 'calculate'
    {
        icLogWarn(LOG_TAG, "%s: attempt to change to channel out of range %d", __FUNCTION__, channel);
        result.responseCode = channelChangeInvalidChannel;
        result.channelNumber = channel;
    }
    else
    {
        if (channel == 0)
        {
            icLogDebug(LOG_TAG, "%s: no channel given, so calculate the 'best' one", __FUNCTION__);
            channel = calculateBestChannel();
        }

        result.channelNumber = channel;

        if (channel == 0) //we did not find a good channel
        {
            result.responseCode = channelChangeUnableToCalculate;
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: attempting channel change to %d", __FUNCTION__, channel);

            zhalSystemStatus status;
            memset(&status, 0, sizeof(zhalSystemStatus));
            if (zhalGetSystemStatus(&status) == 0)
            {
                // We are already at that channel, so nothing to do
                if (status.channel == channel)
                {
                    result.responseCode = channelChangeSuccess;
                    icLogDebug(LOG_TAG, "%s: we are already on channel %d", __FUNCTION__, channel);
                }
                else if (dryRun == false)
                {
                    // Record the previous version so we can swap back if needed
                    char buf[4];
                    snprintf(buf, sizeof(buf), "%"PRIu8, status.channel);
                    deviceServiceSetSystemProperty(ZIGBEE_PREVIOUS_CHANNEL_NAME, buf);
                    zhalNetworkChangeRequest networkChangeRequest;
                    memset(&networkChangeRequest, 0, sizeof(zhalNetworkChangeRequest));
                    networkChangeRequest.channel = channel;

                    if (zhalNetworkChange(&networkChangeRequest) == 0)
                    {
                        icLogDebug(LOG_TAG, "%s: successfully changed channel, now we wait for devices to move.",
                                __FUNCTION__);

                        isChannelChangeInProgress = true;

                        result.responseCode = channelChangeSuccess;

                        startChannelChangeDeviceWatchdog(status.channel, channel);
                    }
                }
                else
                {
                    icLogDebug(LOG_TAG, "%s: channel change was a dry run.", __FUNCTION__);
                    result.responseCode = channelChangeSuccess;
                }
            }
        }
    }

    pthread_mutex_unlock(&channelChangeMutex);

    return result;
}

static int32_t findLqiInTable(uint64_t eui64, icLinkedList *lqiTable)
{
    int32_t lqi = -1;
    if (lqiTable != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(lqiTable);
        while (linkedListIteratorHasNext(iter))
        {
            zhalLqiData *item = (zhalLqiData *) linkedListIteratorGetNext(iter);
            if (item->eui64 == eui64)
            {
                lqi = item->lqi;
                break;
            }
        }
        linkedListIteratorDestroy(iter);
    }

    return lqi;
}

static void freeNextCloserHopToLqi(void *key, void *value)
{
    (void) key; // Not owned by map
    icLinkedList *lqiTable = (icLinkedList *) value;
    linkedListDestroy(lqiTable, NULL);
}

/*
 * Populate the zigbee network map
 * @return linked list of ZigbeeSubsystemNetworkMapEntry
 */
icLinkedList *zigbeeSubsystemGetNetworkMap()
{
    icLinkedList *networkMap = linkedListCreate();
    icHashMap *nextCloserHopToLqi = hashMapCreate();

    zhalSystemStatus status;
    memset(&status, 0, sizeof(zhalSystemStatus));
    zhalGetSystemStatus(&status);

    uint64_t ourEui64 = status.eui64;
    uint64_t blankEui64 = UINT64_MAX;

    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    icLinkedListIterator *iter = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iter))
    {
        icDevice *item = (icDevice *) linkedListIteratorGetNext(iter);
        uint64_t deviceEui64 = zigbeeSubsystemIdToEui64(item->uuid);
        // Default is the device is child of ours
        uint64_t nextCloserHop = ourEui64;
        int lqi = -1;

        if (zhalDeviceIsChild(deviceEui64) == false)
        {
            // Discover the nextCloserHop
            icLinkedList *hops = zhalGetSourceRoute(deviceEui64);
            if (hops == NULL)
            {
                icLogInfo(LOG_TAG, "Device %s is not a child or in the source route table", item->uuid);
                lqi = 0;
                nextCloserHop = blankEui64;
            }
            else if (linkedListCount(hops) > 0)
            {
                uint64_t *hop = (uint64_t *) linkedListGetElementAt(hops, 0);
                nextCloserHop = *hop;
            }
            // Otherwise its our child, which is the default

            // Cleanup
            linkedListDestroy(hops, NULL);
        }

        ZigbeeSubsystemNetworkMapEntry *entry = (ZigbeeSubsystemNetworkMapEntry *) calloc(1,
                                                                                          sizeof(ZigbeeSubsystemNetworkMapEntry));
        entry->address = deviceEui64;
        entry->nextCloserHop = nextCloserHop;
        // Get the lqiTable
        icLinkedList *lqiTable = hashMapGet(nextCloserHopToLqi, &entry->nextCloserHop, sizeof(uint64_t));
        if (lqiTable == NULL)
        {
            // Fetch it if we haven't gotten it yet
            lqiTable = zhalGetLqiTable(entry->nextCloserHop);
            if (lqiTable != NULL)
            {
                hashMapPut(nextCloserHopToLqi, &entry->nextCloserHop, sizeof(uint64_t), lqiTable);
            }
            else
            {
                icLogWarn(LOG_TAG, "getLqiTable return NULL lqiTable");
            }
        }
        // Populate the lqi from our entry in the table
        entry->lqi = findLqiInTable(entry->address, lqiTable);
        linkedListAppend(networkMap, entry);
    }
    linkedListIteratorDestroy(iter);

    // Cleanup
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
    hashMapDestroy(nextCloserHopToLqi, freeNextCloserHopToLqi);

    return networkMap;
}

bool zigbeeSubsystemUpgradeDeviceFirmwareLegacy(uint64_t eui64,
                                                uint64_t routerEui64,
                                                const char *appFilename,
                                                const char *bootloaderFilename)
{
    return zhalUpgradeDeviceFirmwareLegacy(eui64, routerEui64, appFilename, bootloaderFilename) == 0;
}

static const char *zigbeeCoreRestartReasonToLabel(ZigbeeCoreRestartReason reason)
{
    const char *retVal = NULL;

    if (reason > ZIGBEE_CORE_RESTART_FIRST && reason < ZIGBEE_CORE_RESTART_LAST)
    {
        retVal = zigbeeCoreRestartReasonLabels[reason];
    }
    else
    {
        retVal = "Reboot reason: unknown";
    }

    return retVal;
}

static void restartZigbeeCore(ZigbeeCoreRestartReason reason)
{
    bool success = false;
    const char *reasonString = zigbeeCoreRestartReasonToLabel(reason);
    AUTO_CLEAN(free_generic__auto) char *troubleString = stringBuilder("ZigbeeCore was not responding. %s", reasonString);

    // Watchdog will send out a death event since we are restarting for recovery reasons
    // Kinda stinks that we have to have such intimate knowledge of the service name here....
    IPCCode retVal = watchdogService_request_RESTART_SERVICE_FOR_RECOVERY("ZigbeeCore", &success);
    if (retVal != IPC_SUCCESS || !success)
    {
        icLogWarn(LOG_TAG, "Failed to restart ZigbeeCore: IPCCode=%s, success=%s. %s", IPCCodeLabels[retVal],
                  success ? "true" : "false", reasonString);
    }
    else
    {
        icLogDebug(LOG_TAG, "Successfully restarted ZigbeeCore. %s", reasonString);
    }
}

static void zigbeeCoreWatchdog(void *arg)
{
    if (zhalHeartbeat() == 0)
    {
        zigbeeCorePingFailures = 0;
    }
    else
    {
        ++zigbeeCorePingFailures;
        icLogDebug(LOG_TAG, "Failed to ping ZigbeeCore, failureCount=%d, maxFailures=%d", zigbeeCorePingFailures,
                   maxZigbeeCorePingFailures);
        if (zigbeeCorePingFailures >= maxZigbeeCorePingFailures)
        {
            // Do the restart
            restartZigbeeCore(ZIGBEE_CORE_RESTART_HEARTBEAT);
            // Reset so we won't trigger again until we exceed the threshold again
            zigbeeCorePingFailures = 0;
        }
    }
}

static bool isDeviceAutoApsAcked(const icDevice *device)
{
    return false;
}

static bool isDeviceUsingHashBasedLinkKey(const icDevice *device)
{
    bool result = false;

    const char *item = deviceGetMetadata(device, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA);
    if(item != NULL && strcmp(item, "true") == 0)
    {
        result = true;
    }

    return result;
}

static bool setDeviceUsingHashBasedLinkKey(const icDevice *device, bool isUsingHashBasedKey, bool setMetadataOnly)
{
    bool result = true;

    const char *item = deviceGetMetadata(device, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA);
    const char *isUsingHashBasedKeyStr = stringValueOfBool(isUsingHashBasedKey);
    bool shouldSetMetadata = true;
    // only update if it has changed.
    if (item != NULL && stringCompare(item, isUsingHashBasedKeyStr, true) == 0)
    {
        shouldSetMetadata = false;
    }

    if (shouldSetMetadata == true)
    {
        icLogDebug(LOG_TAG, "%s : Setting metadata %s for device %s to %s", __FUNCTION__, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA, device->uuid, isUsingHashBasedKeyStr);
        AUTO_CLEAN(free_generic__auto) char *uri = createDeviceMetadataUri(device->uuid, DEVICE_USES_HASH_BASED_LINK_KEY_METADATA);
        result = deviceServiceSetMetadata(uri, isUsingHashBasedKeyStr);

        if (setMetadataOnly == false)
        {
            //update the device flags in ZigbeeCore/xNCP
            zigbeeSubsystemSetAddresses();
        }
    }

    return result;
}

IcDiscoveredDeviceDetails *zigbeeSubsystemDiscoverDeviceDetails(uint64_t eui64)
{
    bool basicDiscoverySucceeded = true;
    IcDiscoveredDeviceDetails *details = NULL;
    uint8_t *endpointIds = NULL;
    uint8_t numEndpointIds;
    if (zhalGetEndpointIds(eui64, &endpointIds, &numEndpointIds) == 0 && numEndpointIds > 0)
    {
        details = createIcDiscoveredDeviceDetails();

        details->eui64 = eui64;

        for (uint8_t i = 0; i < numEndpointIds; i++)
        {
            zhalEndpointInfo endpointInfo;
            if (zhalGetEndpointInfo(eui64, endpointIds[i], &endpointInfo) == 0)
            {
                bool hasOtaCluster = false;

                if (endpointInfo.appProfileId != HA_PROFILE_ID)
                {
                    icLogInfo(LOG_TAG, "%s: ignoring non HA profile endpoint", __func__);
                    continue;
                }

                // allocate room for another endpoint details
                details->numEndpoints++;
                details->endpointDetails = realloc(details->endpointDetails,
                                                   details->numEndpoints * sizeof(IcDiscoveredEndpointDetails));

                IcDiscoveredEndpointDetails *epDetails = &details->endpointDetails[details->numEndpoints-1];
                memset(epDetails, 0, sizeof(*epDetails));

                epDetails->endpointId = endpointInfo.endpointId;
                epDetails->appProfileId = endpointInfo.appProfileId;
                epDetails->appDeviceId = endpointInfo.appDeviceId;
                epDetails->appDeviceVersion = endpointInfo.appDeviceVersion;
                epDetails->numServerClusterDetails = endpointInfo.numServerClusterIds;
                epDetails->serverClusterDetails = (IcDiscoveredClusterDetails *) calloc(
                        endpointInfo.numServerClusterIds,
                        sizeof(IcDiscoveredClusterDetails));
                for (uint8_t j = 0; j < endpointInfo.numServerClusterIds; j++)
                {
                    epDetails->serverClusterDetails[j].clusterId = endpointInfo.serverClusterIds[j];
                    epDetails->serverClusterDetails[j].isServer = true;
                }

                epDetails->numClientClusterDetails = endpointInfo.numClientClusterIds;
                epDetails->clientClusterDetails = (IcDiscoveredClusterDetails *) calloc(
                        endpointInfo.numClientClusterIds,
                        sizeof(IcDiscoveredClusterDetails));
                for (uint8_t j = 0; j < endpointInfo.numClientClusterIds; j++)
                {
                    if (endpointInfo.clientClusterIds[j] == OTA_UPGRADE_CLUSTER_ID)
                    {
                        hasOtaCluster = true;
                    }

                    epDetails->clientClusterDetails[j].clusterId = endpointInfo.clientClusterIds[j];
                    epDetails->clientClusterDetails[j].isServer = false;
                }

                if (endpointInfo.appDeviceId != ICONTROL_BOGUS_DEVICE_ID)
                {
                    //we will get the manufacturer and model from the first endpoint.  We currently have never heard of
                    // a device with different manufacturer and models on different endpoints, and that doesnt really
                    // make sense anyway.  The complexity to handle that scenario is not worth it at this time.
                    if (details->manufacturer == NULL)
                    {
                        //if it fails we will just try again on the next endpoint
                        //coverity[check_return]
                        zigbeeSubsystemReadString(eui64, endpointIds[i], BASIC_CLUSTER_ID, true,
                                                  BASIC_MANUFACTURER_NAME_ATTRIBUTE_ID, &details->manufacturer);
                    }
                    if (details->model == NULL)
                    {
                        //if it fails we will just try again on the next endpoint
                        //coverity[check_return]
                        zigbeeSubsystemReadString(eui64, endpointIds[i], BASIC_CLUSTER_ID, true,
                                                  BASIC_MODEL_IDENTIFIER_ATTRIBUTE_ID, &details->model);
                    }
                    if (details->hardwareVersion == 0)
                    {
                        //if it fails we will just try again on the next endpoint
                        //coverity[check_return]
                        zigbeeSubsystemReadNumber(eui64, endpointIds[i], BASIC_CLUSTER_ID, true,
                                                  BASIC_HARDWARE_VERSION_ATTRIBUTE_ID, &details->hardwareVersion);
                    }
                    if (details->appVersion == 0)
                    {
                        //if it fails we will just try again on the next endpoint
                        //coverity[check_return]
                        zigbeeSubsystemReadNumber(eui64, endpointIds[i], BASIC_CLUSTER_ID, true,
                                                  BASIC_APPLICATION_VERSION_ATTRIBUTE_ID, &details->appVersion);
                    }
                    if (details->firmwareVersion == 0 && hasOtaCluster)
                    {
                        //if it fails we will just try again on the next endpoint
                        //coverity[check_return]
                        zigbeeSubsystemReadNumber(eui64, endpointIds[i], OTA_UPGRADE_CLUSTER_ID, false,
                                                  OTA_CURRENT_FILE_VERSION_ATTRIBUTE_ID, &details->firmwareVersion);
                    }
                }

                basicDiscoverySucceeded = true;
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to get endpoint info for %d", __FUNCTION__, i);
                basicDiscoverySucceeded = false;
                break;
            }
        }
    }
    else
    {
        icLogError(LOG_TAG, "failed to get endpoint ids for device");
        basicDiscoverySucceeded = false;
    }

    if (basicDiscoverySucceeded == false)
    {
        if (details != NULL)
        {
            freeIcDiscoveredDeviceDetails(details);
            details = NULL;
        }
    }

    // Cleanup
    free(endpointIds);

    return details;
}

/*
 * Get the zigbee module's firmware version.
 * @return the firmware version, or NULL on failure.  Caller must free.
 */
char *zigbeeSubsystemGetFirmwareVersion()
{
    return zhalGetFirmwareVersion();
}

/*
 * Restore config for RMA
 */
bool zigbeeSubsystemRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    // Set property to increment counters on next init
    deviceServiceSetSystemProperty(ZIGBEE_INCREMENT_COUNTERS_ON_NEXT_INIT, "true");
    return true;
}

/*
 * Helper function to determine if a device is monitored at all in LPM.
 *
 * returns - true if we should consider monitoring this device while in LPM
 */
static bool isLPMMonitoredDevice(const char* deviceUUID)
{
    bool result = false;

    // get the LPM policy metadata value
    //
    char *deviceMetadataUri = createDeviceMetadataUri(deviceUUID, LPM_POLICY_METADATA);
    char *deviceMetadataValue = NULL;
    if (deviceServiceGetMetadata(deviceMetadataUri, &deviceMetadataValue) && deviceMetadataValue != NULL)
    {
        result = true;
    }

    // cleanup
    free(deviceMetadataValue);
    free(deviceMetadataUri);

    return result;
}

/*
 * Helper function to determine if a Device's LPM policy and
 * the current state of the system allow for a device to
 * wake the system from LPM.
 *
 * returns - the message handling type for device
 */
static zhalMessageHandlingType determineLPMDeviceMessage(const char* deviceUUID)
{
    zhalMessageHandlingType retVal = MESSAGE_HANDLING_IGNORE_ALL;

    // get the LPM policy metadata value
    //
    char *deviceMetadataUri = createDeviceMetadataUri(deviceUUID, LPM_POLICY_METADATA);
    char *deviceMetadataValue = NULL;
    if (deviceServiceGetMetadata(deviceMetadataUri, &deviceMetadataValue) && deviceMetadataValue != NULL)
    {
        // look at the metadata data value and the current state of the system,
        // to determine if device needs to be added
        //
        if (stringCompare(deviceMetadataValue, lpmPolicyPriorityLabels[LPM_POLICY_ALWAYS], false) == 0)
        {
            retVal = MESSAGE_HANDLING_NORMAL;
        }
        else if (stringCompare(deviceMetadataValue, lpmPolicyPriorityLabels[LPM_POLICY_ARMED_NIGHT], false) == 0)
        {
            retVal = MESSAGE_HANDLING_NORMAL;
        }
        else if (stringCompare(deviceMetadataValue, lpmPolicyPriorityLabels[LPM_POLICY_ARMED_AWAY], false) == 0)
        {
            retVal = MESSAGE_HANDLING_NORMAL;
        }

        // cleanup
        free(deviceMetadataValue);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: unable to find the metadata value for %s on device %s",
                  __FUNCTION__, LPM_POLICY_METADATA, deviceUUID);
    }

    // cleanup
    free(deviceMetadataUri);

    return retVal;
}

/*
 * Set Zigbee OTA firmware upgrade delay
 */
void zigbeeSubsystemSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    zhalSetOtaUpgradeDelay(delaySeconds);
}

/*
 * Tell Zigbee Subsystem to enter LPM
 */
void zigbeeSubsystemEnterLPM()
{
    // create a list to hold the zigbee devices we want to monitor during LPM
    //
    icLinkedList *lpmDevices = linkedListCreate();

    // stop monitoring the zigbee network's health
    //
    zigbeeHealthCheckStop();

    // get the comm fail trouble delay in seconds.
    // use default time if received a value less than what is expected
    // default time should be 56 minutes... Security service does the same thing.
    // it only matters if we are disarmed.
    //
    uint32_t commFailDelaySeconds = getCommFailTimeoutTroubleValueInSeconds();

    // get all zigbee devices
    //
    icLinkedList *deviceList = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    if (deviceList != NULL)
    {
        // loop and create items for the monitored device info for LPM if we find a device that we care about
        //
        icLinkedListIterator *listIter = linkedListIteratorCreate(deviceList);
        while (linkedListIteratorHasNext(listIter))
        {
            icDevice *device = (icDevice *) linkedListIteratorGetNext(listIter);
            if (device == NULL || device->uuid == NULL)
            {
                continue;
            }

            // only consider adding devices that we could possibly be interested in
            //
            if (isLPMMonitoredDevice(device->uuid) == true)
            {
                // get the message handling type and the comm fail time remaining
                //
                // if message handling is IGNORE; then zigbeeCore will ignore all messages
                // if the commFail time remaining is less then 0; then zigbeeCore will ignore monitoring for commFail
                //
                zhalMessageHandlingType messageHandlingType = determineLPMDeviceMessage(device->uuid);

                // if the device is in already comm fail then send -1 as secRemaining
                // xNCP will not start timer of the already comm failed devices
                //
                int32_t secsRemaining = deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(device->uuid, commFailDelaySeconds);

                // fill in all of the information
                //
                zhalLpmMonitoredDeviceInfo *tmpDeviceInfo = calloc(1, sizeof(zhalLpmMonitoredDeviceInfo));
                tmpDeviceInfo->eui64 = zigbeeSubsystemIdToEui64(device->uuid);
                tmpDeviceInfo->messageHandling = messageHandlingType;
                tmpDeviceInfo->timeoutSeconds = secsRemaining;

                // finally add to the list
                //
                linkedListAppend(lpmDevices, tmpDeviceInfo);
            }
            else
            {
                icLogDebug(LOG_TAG, "%s: not monitoring device %s since it's not an LPM monitored device", __func__,
                        device->uuid);
            }
        }

        // cleanup loop
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(deviceList, (linkedListItemFreeFunc) deviceDestroy);
    }

    // set communication fail timeout
    //
    zhalSetCommunicationFailTimeout(commFailDelaySeconds);

    // notify zhal to enter low power mode
    //
    zhalEnterLowPowerMode(lpmDevices);

    // cleanup
    linkedListDestroy(lpmDevices, NULL);
}

/*
 * Tell Zigbee Subsystem to exit LPM
 */
void zigbeeSubsystemExitLPM()
{
    // tell zhal to exit LPM first
    //
    zhalExitLowPowerMode();

    // get the monitored devices info
    // we will be using timeout seconds sent by xNCP as the communication
    // failure time out value for the devices.
    //
    icLinkedList *monitoredDevicesInfoList = zhalGetMonitoredDevicesInfo();
    if (monitoredDevicesInfoList != NULL)
    {
        icLinkedListIterator *listIter = linkedListIteratorCreate(monitoredDevicesInfoList);
        while (linkedListIteratorHasNext(listIter) == true)
        {
            zhalLpmMonitoredDeviceInfo *monitoredDeviceInfo = (zhalLpmMonitoredDeviceInfo *) linkedListIteratorGetNext(listIter);

            AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(monitoredDeviceInfo->eui64);

            // update the timeout for the device
            //
            uint32_t timeout = monitoredDeviceInfo->timeoutSeconds;
            deviceCommunicationWatchdogResetTimeoutForDevice(uuid, timeout);
        }

        // cleanup
        //
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(monitoredDevicesInfoList, NULL);
    }

    // Resume zigbee network health monitoring
    //
    zigbeeHealthCheckStart();
}

void zigbeeSubsystemHandlePropertyChange(const char *prop, const char *value)
{
    icLogDebug(LOG_TAG, "%s: prop=%s, value=%s", __FUNCTION__, prop, value);
    if(prop == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return;
    }

    if(strncmp(prop, ZIGBEE_HEALTH_CHECK_PROPS_PREFIX, strlen(ZIGBEE_HEALTH_CHECK_PROPS_PREFIX)) == 0)
    {
        //some property related to zigbee network health check changed, let that code determine what to do about it
        zigbeeHealthCheckStart();
    }
    else if(strncmp(prop, ZIGBEE_DEFENDER_PROPS_PREFIX, strlen(ZIGBEE_DEFENDER_PROPS_PREFIX)) == 0)
    {
        //some property related to zigbee defender changed, let that code determine what to do about it
        zigbeeDefenderConfigure();
    }
    else if(strncmp(prop, TELEMETRY_PROPS_PREFIX, strlen(TELEMETRY_PROPS_PREFIX)) == 0)
    {
#ifdef CONFIG_CAP_ZIGBEE_TELEMETRY
        zigbeeTelemetrySetProperty(prop, value);
#endif
    }
    else if(strncmp(prop, PAN_ID_CONFLICT_ENABLED_PROPERTY_NAME, strlen(PAN_ID_CONFLICT_ENABLED_PROPERTY_NAME)) == 0)
    {
        //oof, ZigbeeCore is looking for the short name for this property, so convert from long to short
        zhalSetProperty(ZIGBEE_PAN_ID_CONFLICT_SHORT_PROPERTY_NAME, value);
    }
    else
    {
        //pass all other properties down to the stack
        zhalSetProperty(prop, value);
    }
}

icLinkedList *zigbeeSubsystemPerformEnergyScan(const uint8_t *channelsToScan,
                                               uint8_t numChannelsToScan,
                                               uint32_t scanDurationMillis,
                                               uint32_t numScans)
{
    return zhalPerformEnergyScan(channelsToScan,
                                 numChannelsToScan,
                                 scanDurationMillis,
                                 numScans);
}

void zigbeeSubsystemNotifyDeviceCommFail(icDevice *device)
{
    // Currently unused
    (void) device;

    checkAllDevicesInCommFail();
}

static void checkAllDevicesInCommFail(void)
{
    // We can be in a state where ZigbeeCore is responding to heartbeats, but for whatever reason all of
    // our devices are in comm fail. When this happens, we need to bounce ZigbeeCore.

    icLinkedList *allDevices = deviceServiceGetDevicesBySubsystem(ZIGBEE_SUBSYSTEM_NAME);
    sbIcLinkedListIterator *it = linkedListIteratorCreate(allDevices);
    // Only init to false if we have no devices.
    bool devicesInCommFail = linkedListCount(allDevices) > 0;

    // Iterate over devices until we find one not in comm fail.
    while (linkedListIteratorHasNext(it) && devicesInCommFail)
    {
        icDevice *device = linkedListIteratorGetNext(it);
        devicesInCommFail = deviceServiceIsDeviceInCommFail(device->uuid);
    }

    if (devicesInCommFail)
    {
        // All zigbee devices in comm fail. Restart ZigbeeCore
        restartZigbeeCore(ZIGBEE_CORE_RESTART_COMM_FAIL);
    }

    linkedListDestroy(allDevices, (linkedListItemFreeFunc) deviceDestroy);
}

static uint32_t getCommFailTimeoutAlarmValueInSeconds()
{
    return MIN_COMM_FAIL_ALARM_DELAY_MINUTES * 60;
}

static uint32_t getCommFailTimeoutTroubleValueInSeconds()
{
    return MIN_COMM_FAIL_TROUBLE_DELAY_MINUTES * 60;
}

static void *createDeviceCallbacks()
{
    return hashMapCreate();
}

static bool releaseIfDeviceCallbacksEmpty(void *item)
{
    icHashMap *map = (icHashMap *)item;
    return hashMapCount(map) == 0;
}

static void deviceCallbackDestroy(void *item)
{
    icHashMap *map = (icHashMap *)item;
    hashMapDestroy(map, freeDeviceCallbackEntry);
}

static void deviceCallbacksRegister(void *item, void *context)
{
    icHashMap *map = (icHashMap *)item;
    DeviceCallbacksRegisterContext *ctx = (DeviceCallbacksRegisterContext *)context;

    if (hashMapGet(map, &ctx->eui64, sizeof(uint64_t)) != NULL)
    {
        icLogError(LOG_TAG, "zigbeeSubsystemRegisterDeviceListener: listener already registered for %016"
                PRIx64
                "!", ctx->eui64);
        ctx->registered = false;
    }
    else
    {
        uint64_t *key = (uint64_t *) malloc(sizeof(uint64_t));
        *key = ctx->eui64;
        hashMapPut(map, key, sizeof(uint64_t), ctx->callbacks);
        ctx->registered = true;
    }
}

static void deviceCallbacksUnregister(void *item, void *context)
{
    icHashMap *map = (icHashMap *)item;
    DeviceCallbacksUnregisterContext *ctx = (DeviceCallbacksUnregisterContext *)context;

    if (hashMapDelete(map, &ctx->eui64, sizeof(uint64_t), freeDeviceCallbackEntry) == false)
    {
        icLogError(LOG_TAG, "zigbeeSubsystemUnregisterDeviceListener: no listener registered for %016"
                PRIx64
                "!", ctx->eui64);
        ctx->unregistered = false;
    }
    else
    {
        ctx->unregistered = true;
    }
}

static void deviceCallbacksAttributeReport(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *)item;
    ReceivedAttributeReport *report = (ReceivedAttributeReport *)context;

    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &report->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->attributeReportReceived != NULL)
        {
            cbs->attributeReportReceived(cbs->callbackContext, report);
        }
    }
}

static void deviceCallbacksRejoined(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *)item;
    DeviceCallbacksRejoinContext *rejoinContext = (DeviceCallbacksRejoinContext *)context;

    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &rejoinContext->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->deviceRejoined != NULL)
        {
            cbs->deviceRejoined(cbs->callbackContext, rejoinContext->eui64, rejoinContext->isSecure);
        }
    }
}

static void deviceCallbacksLeft(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *)item;
    uint64_t eui64 = *(uint64_t*)context;

    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->deviceLeft != NULL)
        {
            cbs->deviceLeft(cbs->callbackContext, eui64);
        }
    }
}

static void deviceCallbacksFirmwareStatus(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *)item;
    DeviceCallbacksFirmwareStatusContext *firmwareStatusContext = (DeviceCallbacksFirmwareStatusContext *)context;
    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &firmwareStatusContext->eui64, sizeof(uint64_t))) != NULL)
    {
        switch(firmwareStatusContext->status)
        {

            case DEVICE_FIRMWARE_UPGRADE_COMPLETE:
                if (cbs->firmwareUpdateCompleted != NULL)
                {
                    cbs->firmwareUpdateCompleted(cbs->callbackContext, firmwareStatusContext->eui64);
                }
                break;
            case DEVICE_FIRMWARE_UPGRADE_FAILED:
                if (cbs->firmwareUpdateFailed != NULL)
                {
                    cbs->firmwareUpdateFailed(cbs->callbackContext, firmwareStatusContext->eui64);
                }
                break;
            case DEVICE_FIRMWARE_UPGRADING:
                if (cbs->firmwareUpdateStarted != NULL)
                {
                    cbs->firmwareUpdateStarted(cbs->callbackContext, firmwareStatusContext->eui64);
                }
                break;
            case DEVICE_FIRMWARE_NOTIFY:
                if (cbs->firmwareVersionNotify != NULL)
                {
                    cbs->firmwareVersionNotify(cbs->callbackContext, firmwareStatusContext->eui64,
                                               firmwareStatusContext->currentVersion);
                }
                break;
                break;
        }

    }
}

static void deviceCallbacksClusterCommandReceived(const void *item, const void *context)
{
    icHashMap *map = (icHashMap *)item;
    DeviceCallbacksClusterCommandReceivedContext *ctx = (DeviceCallbacksClusterCommandReceivedContext *)context;
    ZigbeeSubsystemDeviceCallbacks *cbs = NULL;
    if ((cbs = hashMapGet(map, &ctx->command->eui64, sizeof(uint64_t))) != NULL)
    {
        if (cbs->clusterCommandReceived != NULL)
        {
            cbs->clusterCommandReceived(cbs->callbackContext, ctx->command);
        }
        ctx->deviceFound = true;
    }
    else
    {
        ctx->deviceFound = false;
    }
}

void zigbeeSubsystemPostRestoreConfig()
{
    zigbeeSubsystemInitializeNetwork(NULL);
    zigbeeSubsystemSetAddresses();
}
