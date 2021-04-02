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
// Created by tlea on 2/25/19.
//

/**
 * This 'cluster' is quite different than the rest due to the complex nature of our legacy devices.  Unfortunate
 * direct calls to device service are made.
 */

#include <icBuildtime.h>
#include <stdlib.h>
#include <pthread.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <unistd.h>
#include <commonDeviceDefs.h>
#include <zigbeeLegacySecurityCommon/deviceNumberAllocator.h>
#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <deviceModelHelper.h>
#include <zigbeeDriverCommon.h>
#include <icUtil/fileUtils.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <errno.h>
#include <zigbeeLegacySecurityCommon/uc_common.h>
#include <icUtil/stringUtils.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "legacySecurityCluster.h"

#define LOG_TAG "legacySecurityCluster"

#define METADATA_GODPARENT_DEVNUM "godparent"
#define METADATA_GODPARENT_RSSI   "godparentRssi"
#define METADATA_GODPARENT_LQI    "godparentLqi"
#define METADATA_LOW_BATTERY_LATCHED "lowBatteryLatched"
#define METADATA_RECENT_TAMPER "recentTamper"
#define METADATA_RECENT_TAMPER_RESTORE "recentTamperRestore"

#define GODPARENT_LQI_THRESHOLD 236
#define GODPARENT_RSSI_THRESHOLD -85
#define MAX_KEYPAD_TEXT_LENGTH 16
#define LOW_BATTERY_COUNT_THRESHOLD 5
#define SIZE_OF_SENSOR_FIFO 16

#define BOOTLOADER_FILENAME_PREFIX "bootloader-updater"
#define LEGACY_DEVICE_TYPE_PROP "legacyDevType"
#define IN_BOOTLOADER "inBootloader"

/**
 * (from an email from Clay):
 * In the early days of zigbee sensor development, sensors did not have any debounce on
 * the reed switch and would send zone state changes as fast as they could. Today
 * sensors have a 200mS debounce, meaning they could never send more than 5 state
 * changes per second. Corey was testing one day with 5 sensors laying on his desk
 * and 4 magnets between his fingers. He would wave his hand over the sensors a
 * few times, get tons of zone events, and noticed that at the end, when all
 * sensors were faulted, sometimes one or more would end up in the restored
 * state on the TS. That is when we noticed that due to the amount of zigbee traffic,
 * some messages were being delivered out of order. In our analysis of the capture,
 * we saw that the final message delivered from a sensor was sometimes the first one.
 * That is when we added the sequence number checking. This delta was pretty large
 * since the rate of messages from a was not limited yet. The number 243 corresponds
 * to the sequence number going backwards by 13 (256-243=13). That means that we
 * are protected from a message being "stale" by up to 13 messages.
 *
 * Now, a little more info. This is not meant to confuse things, but just some more education…
 *
 * The sequence number used in the transmit of a packet on a zigbee device is simply
 * incremented each time a packet is sent. Note that it does not matter who the destination
 * is for this packet, the number is incremented. This gets complicated when you look
 * from the PIMs point of view, receiving messages from the TS. The PIM has to do a similar
 * sequence number checking to keep from processing stale zone events or panel status
 * changes from the TS, but since the TS is sending messages to the PIM, siren/repeater,
 * HA keypads, lights, thermostats, etc., the sequence number can jump by quite a bit
 * between messages when seen from the PIM's point of view. In fact, the PIM may see
 * 2 sequential messages that have sequence number differences of 1 (255), but they
 * may be 15 minutes apart. As a result, the PIM actually implements a timer along with
 * the sequence number check. To be discarded, a message has to meet the same sequence
 * number check AND has to have arrived within 60 seconds of the previous message.
 * The PIM must also track the sequence numbers for the different message types so that
 * a zone fault is not accidentally dropped because it was out of sequence when compared
 * to a panel status change.
 */
#define LEGACY_SEQ_NUM_ROLLOVER_MAX 243

typedef struct
{
    ZigbeeCluster cluster;

    const LegacySecurityClusterCallbacks *callbacks;
    const void *callbackContext;

    icHashMap *legacyDevices;
    pthread_mutex_t legacyDevicesMtx;
    icHashMap *pendingMessages;
    pthread_mutex_t pendingMessagesMtx;
} LegacySecurityCluster;

//sleepy devices use pending messages
typedef enum
{
    pendingMessageNull,
    pendingMessageRemove,
    pendingMessageEnterBootloader,
    pendingMessageSendPing,
    pendingMessageOkToSleep,
} PendingMessageType;

static void setPendingMessage(LegacySecurityCluster *cluster, uint64_t eui64, PendingMessageType);

static void sendPendingMessage(LegacySecurityCluster *cluster,
                               uint64_t eui64,
                               uint8_t apsSeqNum,
                               int8_t rssi,
                               uint8_t lqi);

static void requestPing(LegacySecurityCluster *cluster, uint64_t eui64);

static bool startFirmwareUpgrade(LegacySecurityCluster *cluster,
                                 uint64_t eui64,
                                 uint8_t apsSeqnum,
                                 int8_t rssi,
                                 uint8_t lqi,
                                 bool forceAllowed);

static void legacyDeviceFreeFunc(void *key, void *value);

static bool loadLegacyDeviceDetails(const DeviceServiceCallbacks *deviceService,
                                    uint64_t eui64,
                                    LegacyDeviceDetails **details);

static void setGodparentPingInfo(uint64_t eui64, uint8_t godparent, int8_t rssi, uint8_t lqi);

static bool sendApsAck(uint64_t eui64,
                       uint8_t endpointId,
                       uint8_t command,
                       uint8_t apsSeqNum,
                       uint8_t *payload,
                       uint8_t payloadLen,
                       int8_t rssi,
                       uint8_t lqi,
                       bool isAutoAcked);

static bool handleClusterCommand(ZigbeeCluster *cluster, ReceivedClusterCommand *command);

static void destroy(const ZigbeeCluster *cluster);

static bool validateFirmwareFiles(const DeviceDescriptor *dd, char **appFilename, char **bootloaderFilename);

static void legacySecurityClusterInitMetadata(const ZigbeeCluster *cluster,
                                              uint64_t eui64,
                                              icDevice *device);

static bool isInBootloader(uint64_t eui64);
static bool setInBootloader(LegacySecurityCluster *cluster, uint64_t eui64, bool inBootloader);

static bool checkKeyfobKeypadForLowBattery(LegacyDeviceDetails *details, uint64_t eui64, const uCStatusMessage *status);

const static DeviceServiceCallbacks *deviceService;

ZigbeeCluster *legacySecurityClusterCreate(const LegacySecurityClusterCallbacks *callbacks,
                                           const DeviceServiceCallbacks *deviceServiceCallbacks,
                                           const void *callbackContext)
{
    LegacySecurityCluster *result = (LegacySecurityCluster *) calloc(1, sizeof(LegacySecurityCluster));

    result->cluster.clusterId = IAS_ZONE_CLUSTER_ID;
    result->cluster.handleClusterCommand = handleClusterCommand;
    result->cluster.destroy = destroy;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    result->legacyDevices = hashMapCreate();
    pthread_mutex_init(&result->legacyDevicesMtx, NULL);

    result->pendingMessages = hashMapCreate();
    pthread_mutex_init(&result->pendingMessagesMtx, NULL);

    if (deviceService == NULL)
    {
        deviceService = deviceServiceCallbacks;
    }

    return (ZigbeeCluster *) result;
}

static bool isAutoAcked(const LegacyDeviceDetails *details)
{
    return (details->devType == TAKEOVER_1 || details->devType == REPEATER_SIREN_1) && details->isPairing == false;
}

static void destroy(const ZigbeeCluster *cluster)
{
    LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;

    hashMapDestroy(legacySecurityCluster->legacyDevices, legacyDeviceFreeFunc);
    pthread_mutex_destroy(&legacySecurityCluster->legacyDevicesMtx);
    hashMapDestroy(legacySecurityCluster->pendingMessages, NULL);
    pthread_mutex_destroy(&legacySecurityCluster->pendingMessagesMtx);
}

void legacySecurityClusterDevicesLoaded(const ZigbeeCluster *cluster,
                                        const DeviceServiceCallbacks *devService,
                                        icLinkedList *devices)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (devices == NULL || linkedListCount(devices) == 0)
    {
        icLogInfo(LOG_TAG, "No devices to load");
        return;
    }

    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *item = (icDevice *) linkedListIteratorGetNext(iterator);
        uint64_t eui64 = zigbeeSubsystemIdToEui64(item->uuid);
        legacySecurityClusterDeviceLoaded(cluster, devService, eui64);
    }
    linkedListIteratorDestroy(iterator);
}

void legacySecurityClusterDeviceLoaded(const ZigbeeCluster *cluster,
                                       const DeviceServiceCallbacks *devService,
                                       uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    LegacyDeviceDetails *details = NULL;
    if (loadLegacyDeviceDetails(devService, eui64, &details))
    {
        LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;
        pthread_mutex_lock(&legacySecurityCluster->legacyDevicesMtx);
        uint64_t *tmp = (uint64_t *) malloc(sizeof(uint64_t));
        *tmp = eui64;
        hashMapPut(legacySecurityCluster->legacyDevices, tmp, sizeof(uint64_t), (void *) details);
        pthread_mutex_unlock(&legacySecurityCluster->legacyDevicesMtx);

        /* if this device was last known to be in bootloader, it likely needs recovery.  Start that now */
        if (isInBootloader(eui64) == true)
        {
            icLogInfo(LOG_TAG, "%s: %"PRIx64" was previously known to be in bootloader.  Attempting rescue.",
                    __func__, eui64);

            //since it is in bootloader, it has a pending upgrade
            details->firmwareUpgradePending = true;

            DeviceDescriptor *dd = zigbeeDriverCommonGetDeviceDescriptor(details->manufacturer,
                                                                         details->model,
                                                                         details->hardwareVersion,
                                                                         convertLegacyFirmwareVersionToUint32(details->firmwareVer));

            if (!validateFirmwareFiles(dd,
                                       &details->upgradeAppFilename,
                                       &details->upgradeBootloaderFilename))
            {
                icLogError(LOG_TAG, "%s: unable to start firmware upgrade since the files were not valid", __FUNCTION__);
                free(details->upgradeAppFilename);
                details->upgradeAppFilename = NULL;
                free(details->upgradeBootloaderFilename);
                details->upgradeBootloaderFilename = NULL;
            }
            else
            {
                startFirmwareUpgrade((LegacySecurityCluster *) cluster, eui64, 0, 0, 0, true);
                // Remove the inBootloader metadata. We don't want to retry more than this one time.
                setInBootloader((LegacySecurityCluster *) cluster, eui64, false);
            }

            deviceDescriptorFree(dd);
        }
        else
        {
            /* Request a godparent ping if supported */
            requestPing(legacySecurityCluster, eui64);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to find legacy device details", __FUNCTION__);
    }
}

void legacySecurityClusterDeviceRemoved(const ZigbeeCluster *cluster, uint64_t eui64)
{
    LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;
    pthread_mutex_lock(&legacySecurityCluster->legacyDevicesMtx);
    hashMapDelete(legacySecurityCluster->legacyDevices, &eui64, sizeof(uint64_t), legacyDeviceFreeFunc);
    pthread_mutex_unlock(&legacySecurityCluster->legacyDevicesMtx);
}

bool legacySecurityClusterConfigureDevice(const ZigbeeCluster *cluster,
                                          uint64_t eui64,
                                          icDevice *device,
                                          const DeviceDescriptor *deviceDescriptor)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;
    uint8_t *payload = NULL;
    uint8_t payloadLen = 0;
    LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterGetDetailsCopy(cluster, eui64);

    if (legacyDeviceDetails == NULL)
    {
        icLogError(LOG_TAG, "%s: failed to find legacy device details", __FUNCTION__);
        return false;
    }

    uCDeviceType devType = legacyDeviceDetails->devType;

    //first, send an aps ack which sets the device number
    uint8_t devNum = allocateDeviceNumber(device);
    if (devNum == 0) //0 is invalid
    {
        icLogError(LOG_TAG, "%s: failed to allocate a device number", __FUNCTION__);
        goto exit;
    }
    uint8_t devNumPayload[1];
    devNumPayload[0] = devNum;
    if (sendApsAck(eui64,
                   legacyDeviceDetails->endpointId,
                   DEVICE_NUMBER,
                   legacyDeviceDetails->pendingApsAckSeqNum,
                   devNumPayload,
                   1,
                   0,
                   0,
                   isAutoAcked(legacyDeviceDetails)) == false)
    {
        icLogError(LOG_TAG, "%s: failed to set device number", __FUNCTION__);
        goto exit;
    }

    //send DEVICE_CONFIG, DEVICE_PAIRED, then OK_TO_SLEEP

    //DEVICE_CONFIG message
    getLegacyDeviceConfigMessage(devType, devNum, 0, &payload, &payloadLen);

    if (zigbeeSubsystemSendMfgCommand(eui64,
                                      1,
                                      IAS_ZONE_CLUSTER_ID,
                                      true,
                                      DEVICE_CONFIG,
                                      UC_MFG_ID,
                                      payload,
                                      payloadLen) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send device config", __FUNCTION__);
        goto exit;
    }

    //DEVICE_PAIRED
    payload[0] = 1; //paired
    if (zigbeeSubsystemSendMfgCommand(eui64, 1, IAS_ZONE_CLUSTER_ID, true, DEVICE_PAIRED, UC_MFG_ID, payload, 1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send device paired", __FUNCTION__);
        goto exit;
    }

    //OK_TO_SLEEP
    payload[0] = 1; //paired
    payload[1] = 0; //rssi
    payload[2] = 0; //lqi
    payload[3] = 0; //region
    if (zigbeeSubsystemSendMfgCommand(eui64, 1, IAS_ZONE_CLUSTER_ID, true, OK_TO_SLEEP, UC_MFG_ID, payload, 4) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to send ok to sleep", __FUNCTION__);
        goto exit;
    }

    requestPing(legacySecurityCluster, eui64);

    result = true;

    exit:
    free(payload);
    return result;
}

void legacySecurityClusterReleaseDetails(const ZigbeeCluster *cluster)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (cluster != NULL)
    {
        LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;

        pthread_mutex_unlock(&legacySecurityCluster->legacyDevicesMtx);
    }
}

LegacyDeviceDetails *legacySecurityClusterGetDetailsCopy(const ZigbeeCluster *cluster, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    const LegacyDeviceDetails *tmp = legacySecurityClusterAcquireDetails(cluster, eui64);
    LegacyDeviceDetails *legacyDeviceDetails = cloneLegacyDeviceDetails(tmp);

    if (legacyDeviceDetails != NULL)
    {
        legacySecurityClusterReleaseDetails(cluster);
    }

    return legacyDeviceDetails;
}

LegacyDeviceDetails *legacySecurityClusterAcquireDetails(const ZigbeeCluster *cluster, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;
    LegacyDeviceDetails *legacyDeviceDetails = NULL;

    if (legacySecurityCluster != NULL)
    {
        pthread_mutex_lock(&legacySecurityCluster->legacyDevicesMtx);
        legacyDeviceDetails = hashMapGet(legacySecurityCluster->legacyDevices,
                                         &eui64,
                                         sizeof(uint64_t));

        if (legacyDeviceDetails == NULL)
        {
            pthread_mutex_unlock(&legacySecurityCluster->legacyDevicesMtx);
            icLogWarn(LOG_TAG, "%s: Unknown device %"
                    PRIx64, __FUNCTION__, eui64);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: Passed invalid arguments: null cluster", __FUNCTION__);
    }

    return legacyDeviceDetails;
}

uint8_t legacySecurityClusterGetEndpointId(const ZigbeeCluster *cluster, uint64_t eui64)
{
    const LegacyDeviceDetails *details = legacySecurityClusterAcquireDetails(cluster, eui64);
    uint8_t endpointId = 0;

    if (details != NULL)
    {
        endpointId = details->endpointId;
        legacySecurityClusterReleaseDetails(cluster);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: BUG: endpointId not available, defaulting to 0", __FUNCTION__);
    }

    return endpointId;
}

static void legacySecurityClusterInitMetadata(const ZigbeeCluster *cluster,
                                              uint64_t eui64,
                                              icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    const LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails(cluster, eui64);

    if (legacyDeviceDetails == NULL)
    {
        return;
    }

    uCDeviceType devType = legacyDeviceDetails->devType;

    legacySecurityClusterReleaseDetails(cluster);

    //persist the devType and devNum in this device's metadata
    char buf[4]; //255 + \0 worst case
    snprintf(buf, 4, "%"PRIu8, devType);
    createDeviceMetadata(device, LEGACY_DEVICE_TYPE_PROP, buf);
}

bool legacySecurityClusterClaimDevice(const ZigbeeCluster *cluster, IcDiscoveredDeviceDetails *details,
                                      icHashMap *deviceTypeInclusionSet,
                                      icHashMap *deviceTypeExclusionSet)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    //must specify one or the other, but not both
    if ((deviceTypeInclusionSet == NULL && deviceTypeExclusionSet == NULL) ||
        (deviceTypeInclusionSet != NULL && deviceTypeExclusionSet != NULL))
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    bool claimDevice = false;

    LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;

    if (details->numEndpoints > 0 && details->endpointDetails[0].appDeviceId == LEGACY_ICONTROL_SENSOR_DEVICE_ID)
    {
        ReceivedClusterCommand *command = zigbeeSubsystemGetPrematureClusterCommand(details->eui64, DEVICE_INFO, 3);

        uCDeviceInfoMessage msg;
        if (command != NULL && parseDeviceInfoMessage(command->commandData, command->commandDataLen, &msg))
        {
            uint8_t devType = msg.devType;

            bool claimThis = false;
            if (deviceTypeExclusionSet)
            {
                claimThis = hashMapContains(deviceTypeExclusionSet, &devType, 1) == false;
            }
            else if (deviceTypeInclusionSet)
            {
                claimThis = hashMapContains(deviceTypeInclusionSet, &devType, 1) == true;
            }

            if (claimThis)
            {
                //clear out anything that was there before
                free(details->manufacturer);
                free(details->model);

                LegacyDeviceDetails *legacyDetails = NULL;
                uint32_t firmwareVersion = getFirmwareVersionFromDeviceInfoMessage(msg);
                if (getLegacyDeviceDetails(msg.devType, firmwareVersion, &legacyDetails))
                {
                    details->manufacturer = strdup(legacyDetails->manufacturer);
                    details->model = strdup(legacyDetails->model);
                    details->hardwareVersion = legacyDetails->hardwareVersion;
                    details->firmwareVersion = firmwareVersion;
                }

                //we will use these fields later when we update our state while creating our resources
                legacyDetails->isFaulted = msg.devStatus.fields1.primaryAlarm ? true : false;
                legacyDetails->isTampered = msg.devStatus.fields1.tamper ? true : false;
                legacyDetails->isPairing = true;
                legacyDetails->endpointId = details->endpointDetails[0].endpointId;

                claimDevice = true;

                uint64_t *eui64 = (uint64_t *) malloc(sizeof(uint64_t));
                *eui64 = command->eui64;
                pthread_mutex_lock(&legacySecurityCluster->legacyDevicesMtx);

                legacyDetails->pendingApsAckSeqNum = command->apsSeqNum;

                bool success = hashMapPut(legacySecurityCluster->legacyDevices, eui64, sizeof(uint64_t),
                           (void *) legacyDetails);
                if (success == false)
                {
                    free(eui64);
                    legacyDeviceDetailsDestroy(legacyDetails);
                }
                else
                {
                    icLogDebug(LOG_TAG, "%s: legacy device details stored for %"
                            PRIx64, __FUNCTION__, *eui64);
                }

                pthread_mutex_unlock(&legacySecurityCluster->legacyDevicesMtx);

                zigbeeSubsystemRemovePrematureClusterCommand(details->eui64, DEVICE_INFO);
            }
            else
            {
                icLogInfo(LOG_TAG, "%s: not claiming device (devType [%"PRIx8 "])", __FUNCTION__, details->deviceType);
            }
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: did not find DEVICE_INFO message!", __FUNCTION__);
        }
    }

    return claimDevice;
}

bool legacySecurityClusterFetchInitialResourceValues(const ZigbeeCluster *cluster,
                                                     uint64_t eui64,
                                                     icDevice *device,
                                                     IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                                     icInitialResourceValues *initialResourceValues)
{
    // Add NULL values to indicate we support these optional resources, even though we don't currently have values
    bool ok = initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_TEMPERATURE, NULL);
    ok &= initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE, NULL);

    return ok;
}

bool legacySecurityClusterRegisterResources(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            icDevice *device,
                                            IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                            icInitialResourceValues *initialResourceValues)
{
    // Common resources will be created by the common driver since we populated the resource values above

    // Make sure our metadata is set
    legacySecurityClusterInitMetadata(cluster, eui64, device);

    return true;
}

void legacySecurityClusterUpgradeFirmware(const ZigbeeCluster *cluster, uint64_t eui64, const DeviceDescriptor *dd)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    //save the firmware file(s) for when we can actually get the device in bootloader and do the upgrade
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails(cluster, eui64);

    if (legacyDeviceDetails != NULL)
    {
        //if its a mains powered device and we are going to upgrade it, we can do so without having to start with an
        // aps ack
        bool startUpgradeNow = false;

        if (!validateFirmwareFiles(dd,
                                   &legacyDeviceDetails->upgradeAppFilename,
                                   &legacyDeviceDetails->upgradeBootloaderFilename))
        {
            icLogError(LOG_TAG, "%s: unable to start firmware upgrade since the files were not valid", __FUNCTION__);
            free(legacyDeviceDetails->upgradeAppFilename);
            legacyDeviceDetails->upgradeAppFilename = NULL;
            free(legacyDeviceDetails->upgradeBootloaderFilename);
            legacyDeviceDetails->upgradeBootloaderFilename = NULL;
            legacyDeviceDetails->firmwareUpgradePending = false;
        }
        else
        {
            legacyDeviceDetails->firmwareUpgradePending = true;

            if(legacyDeviceDetails->isMainsPowered)
            {
                startUpgradeNow = true;
            }
        }

        legacySecurityClusterReleaseDetails(cluster);

        if(startUpgradeNow)
        {
            startFirmwareUpgrade((LegacySecurityCluster *) cluster, eui64, 0, 0, 0, false);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: legacy device not found", __FUNCTION__);
    }
}

void legacySecurityClusterHandleFirmwareUpgradeFailed(const ZigbeeCluster *cluster, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails(cluster, eui64);

    if (legacyDeviceDetails != NULL)
    {
        // Clear the upgrade pending flag. We would normally clear this when we go into the bootloader, but
        // it's possible for the device to not successfully go into bootloader, so we need to clear that flag here.
        legacyDeviceDetails->firmwareUpgradePending = false;
        legacySecurityClusterReleaseDetails(cluster);

        // In addition, callback to the drivers to indicate the upgrade is no longer in progress. This will prevent
        // ZigbeeDriverCommon from thinking we have a blocking upgrade forever.
        const LegacySecurityCluster *legacyCluster = (LegacySecurityCluster *) cluster;
        legacyCluster->callbacks->upgradeInProgress(eui64, false, legacyCluster->callbackContext);
    }
    else
    {
        icLogError(LOG_TAG, "%s: legacy device not found", __FUNCTION__);
    }
}

uint8_t *legacySecurityClusterStringToCode(const char *code)
{
    if (code == NULL || strlen(code) != 4)
    {
        return NULL;
    }

    uint8_t *numericCode = malloc(4);
    for (int i = 0;  i < 4; i++)
    {
        numericCode[i] = code[i] - '0';
    }

    return numericCode;
}

bool legacySecurityClusterRepeaterSetWarning(ZigbeeCluster *cluster,
                                             const uint64_t eui64,
                                             const uCWarningMessage *message)
{

    uint8_t payload[2];
    errno = 0;
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, sizeof(payload), ZIO_WRITE);
    bool ok = false;
    zigbeeIOPutUint8(zio, message->strobeMode.brightness);
    zigbeeIOPutUint8(zio, message->strobeMode.onTime);

    if (!errno)
    {
        uint8_t epId = legacySecurityClusterGetEndpointId(cluster, eui64);
        if (zigbeeSubsystemSendMfgCommand(eui64,
                                          epId,
                                          IAS_ZONE_CLUSTER_ID,
                                          true,
                                          SET_WHITE_LED,
                                          UC_MFG_ID,
                                          payload,
                                          sizeof(payload)) == 0)
        {
            ok = true;
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: failed to send warning tone command", __FUNCTION__);
        }
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "%s: unable to build payload: %s", __FUNCTION__, errStr);
    }

    return ok;
}

static void sendPendingMessage(LegacySecurityCluster *cluster,
                               uint64_t eui64,
                               uint8_t apsSeqNum,
                               int8_t rssi,
                               uint8_t lqi)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    PendingMessageType message = pendingMessageNull;

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterGetDetailsCopy((ZigbeeCluster *) cluster, eui64);

    pthread_mutex_lock(&cluster->pendingMessagesMtx);
    PendingMessageType *messagePtr = (PendingMessageType *) hashMapGet(cluster->pendingMessages, &eui64,
                                                                       sizeof(uint64_t));

    // if we have a firmware upgrade waiting for this device and no other message pending,
    // enter bootloader and start the upgrade.
    if (legacyDeviceDetails->firmwareUpgradePending == true &&
            (messagePtr == NULL || *messagePtr == pendingMessageNull))
    {
        message = pendingMessageEnterBootloader;
    }
    else if (messagePtr != NULL)
    {
        message = *messagePtr;
    }

    hashMapDelete(cluster->pendingMessages, &eui64, sizeof(uint64_t), NULL);
    pthread_mutex_unlock(&cluster->pendingMessagesMtx);

    switch (message)
    {
        case pendingMessageRemove:
            icLogDebug(LOG_TAG, "%s: sending DEVICE_REMOVE", __FUNCTION__);
            sendApsAck(eui64, legacyDeviceDetails->endpointId, DEVICE_REMOVE, apsSeqNum, NULL, 0, rssi, lqi,
                       isAutoAcked(legacyDeviceDetails));
            break;

        case pendingMessageEnterBootloader:
            icLogDebug(LOG_TAG, "%s: starting upgrade process", __FUNCTION__);
            if (startFirmwareUpgrade(cluster, eui64, apsSeqNum, rssi, lqi, false) == false)
            {
                //we couldnt start the upgrade, so we have to send a null aps ack
                sendApsAck(eui64, legacyDeviceDetails->endpointId, NULL_MESSAGE, apsSeqNum, NULL, 0, rssi, lqi,
                           isAutoAcked(legacyDeviceDetails));
            }
            break;

        case pendingMessageSendPing:
            setGodparentPingInfo(eui64, 0, -128, 0); //clear out data first
            icLogDebug(LOG_TAG, "%s: sending SEND_PING", __FUNCTION__);
            sendApsAck(eui64, legacyDeviceDetails->endpointId, SEND_PING, apsSeqNum, NULL, 0, rssi, lqi,
                       isAutoAcked(legacyDeviceDetails));
            break;

        case pendingMessageOkToSleep:
            icLogDebug(LOG_TAG, "%s: sending OK_TO_SLEEP", __FUNCTION__);
            sendApsAck(eui64, legacyDeviceDetails->endpointId, OK_TO_SLEEP, apsSeqNum, NULL, 0, rssi, lqi,
                       isAutoAcked(legacyDeviceDetails));
            break;

        default:
            icLogDebug(LOG_TAG, "%s: sending NULL_MESSAGE", __FUNCTION__);
            sendApsAck(eui64, legacyDeviceDetails->endpointId, NULL_MESSAGE, apsSeqNum, NULL, 0, rssi, lqi,
                       isAutoAcked(legacyDeviceDetails));
            break;
    }
}

static void setPendingMessage(LegacySecurityCluster *cluster, uint64_t eui64, PendingMessageType message)
{
    icLogDebug(LOG_TAG, "%s: setting pending message to %d for %"
            PRIx64, __FUNCTION__, message, eui64);

    uint64_t *eui64Copy = (uint64_t *) malloc(sizeof(uint64_t));
    *eui64Copy = eui64;
    PendingMessageType *messageCopy = (PendingMessageType *) malloc(sizeof(PendingMessageType));
    *messageCopy = message;

    pthread_mutex_lock(&cluster->pendingMessagesMtx);
    //delete any previous
    hashMapDelete(cluster->pendingMessages, &eui64, sizeof(uint64_t), NULL);
    hashMapPut(cluster->pendingMessages, eui64Copy, sizeof(uint64_t), messageCopy);
    pthread_mutex_unlock(&cluster->pendingMessagesMtx);
}

static bool sendApsAck(uint64_t eui64,
                       uint8_t endpointId,
                       uint8_t command,
                       uint8_t apsSeqNum,
                       uint8_t *payload,
                       uint8_t payloadLen,
                       int8_t rssi,
                       uint8_t lqi,
                       bool isAutoAcked)
{
    icLogDebug(LOG_TAG, "%s: sending command %02x", __FUNCTION__, command);

    //4 bytes header, 1 for command, 1 for rssi, 1 for lqi then payload (if any).  16 is plenty
    uint8_t fullPayload[16];

    if (payloadLen > 9) //16-7 leaves room for up to 9 bytes in payload
    {
        icLogError(LOG_TAG, "%s: payload too large", __FUNCTION__);
        return false;
    }

    //header
    fullPayload[0] = 0x14; //zcl frame control: mfg specific and disable default response
    fullPayload[1] = 0xA0; //mfg code
    fullPayload[2] = 0x10; //mfg code
    fullPayload[3] = 0; //sequence number (ignored)

    fullPayload[4] = command;

    if (payloadLen > 0)
    {
        memcpy(fullPayload + 5, payload, payloadLen);
    }

    fullPayload[5 + payloadLen] = (uint8_t) rssi;
    fullPayload[6 + payloadLen] = lqi;

    //For PIM and Siren Repeater, APS acks are sent for us automatically by ZHAL.
    // For those we convert any commands that arent just APS acks (those with command !=
    // LegacySecurityDeviceCommandIds.NULL_MESSAGE) into regular direct commands (not packaged in an APS ack)

    if (isAutoAcked)
    {
        if (command != NULL_MESSAGE)
        {
            int rc = zigbeeSubsystemSendMfgCommand(eui64,
                                                 endpointId,
                                                 IAS_ZONE_CLUSTER_ID,
                                                 true,
                                                 command,
                                                 UC_MFG_ID,
                                                 payload,
                                                 (uint16_t) (7 + payloadLen));

            if(command == ENTER_BOOTLOADER)
            {
                // our auto acked devices immediately go into bootloader without sending a valid response,
                // so we must assume success here.
                return true;
            }
            else
            {
                return rc == 0;
            }
        }
        else
        {
            return true;
        }
    }
    else
    {
        return zigbeeSubsystemSendViaApsAck(eui64,
                                            endpointId,
                                            IAS_ZONE_CLUSTER_ID,
                                            apsSeqNum,
                                            fullPayload,
                                            (uint16_t) (7 + payloadLen)) == 0;
    }
}

static void setGodparentPingInfo(uint64_t eui64, uint8_t godparent, int8_t rssi, uint8_t lqi)
{
    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    char buf[5]; //-127 + \0 worst case

    snprintf(buf, 5, "%"PRIu8, godparent);
    setMetadata(uuid, NULL, METADATA_GODPARENT_DEVNUM, buf);

    snprintf(buf, 5, "%"PRId8, rssi);
    setMetadata(uuid, NULL, METADATA_GODPARENT_RSSI, buf);

    snprintf(buf, 5, "%"PRIu8, lqi);
    setMetadata(uuid, NULL, METADATA_GODPARENT_LQI, buf);

    free(uuid);
}

static bool getGodparentPingInfo(uint64_t eui64, uint8_t *godparent, int8_t *rssi, uint8_t *lqi)
{
    bool result = false;
    if (eui64 == 0 || godparent == NULL || rssi == NULL || lqi == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char *devNumStr = getMetadata(uuid, NULL, METADATA_GODPARENT_DEVNUM);
    char *rssiStr = getMetadata(uuid, NULL, METADATA_GODPARENT_RSSI);
    char *lqiStr = getMetadata(uuid, NULL, METADATA_GODPARENT_LQI);

    if (devNumStr == NULL || strlen(devNumStr) == 0 ||
        rssiStr == NULL || strlen(rssiStr) == 0 ||
        lqiStr == NULL || strlen(lqiStr) == 0)
    {
        icLogError(LOG_TAG, "%s: failed to get godparent metadata", __FUNCTION__);
    }
    else
    {
        *godparent = strtoul(devNumStr, NULL, 0);
        *rssi = strtol(rssiStr, NULL, 0);
        *lqi = strtoul(lqiStr, NULL, 0);

        result = true;
    }

    free(uuid);
    free(devNumStr);
    free(rssiStr);
    free(lqiStr);

    return result;
}

/**
 * Part of HH-968 - Latching of low battery until new battery is inserted
 *
 * Handle edge case where sensor doesn't completely discharge before putting in a new battery and sensor FIFO
 * isn't cleared.
 *
 * If battery not latched and tamper recently restored, but battery is still reporting low, enter preventLatch
 * state then allow up to SIZE_OF_SENSOR_FIFO reports. At any point if battery goes above the lowBatteryThresh,
 * remove low battery and clear latch. If after SIZE_OF_SENSOR_FIFO reports the battery is still reporting low,
 * restart normal latch logic.
 *
 * @param deviceUuid the uuid of the device
 * @param recentRestore boolean value derived from user property for recent sensor tamper restore
 * @param lowBatteryLatched boolean value derived from user property for low battery latched state
 * @param currentBatteryVoltage the currently reported voltage
 * @param details the legacy device details for this device
 * @return boolean determining whether or not to prevent latching low battery logic from processing
 */
static bool checkForLatchPrevention(const char *deviceUuid,
                                    bool recentTamperRestore,
                                    bool lowBatteryLatched,
                                    uint16_t currentBatteryVoltage,
                                    LegacyDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool latchPrevention = false;

    if(recentTamperRestore && !lowBatteryLatched && (currentBatteryVoltage < details->lowBatteryVoltage))
    {
        if(details->preventLatchCount < SIZE_OF_SENSOR_FIFO)
        {
            details->preventLatchCount++;
            icLogDebug(LOG_TAG, "%s: latch prevented, preventLatchCount is now %"PRIu16,
                    __FUNCTION__,
                    details->preventLatchCount);
            latchPrevention = true;
        }
        else
        {
            details->preventLatchCount = 0;
            details->preventLatchWasReset = true;
            setBooleanMetadata(deviceUuid, NULL, METADATA_RECENT_TAMPER_RESTORE, false);
            latchPrevention = false;
            icLogDebug(LOG_TAG, "%s: No longer preventing latch", __FUNCTION__);
        }
    }

    return latchPrevention;
}

#undef DEBUG_BATTERY_LATCHING

/**
 * Check legacy devices for low battery.  Latch low battery status once reported LOW_BATTERY_COUNT_THRESHOLD times
 * to prevent "flapping"
 * HH-968 - Latching of low battery until new battery is inserted
 *
 * NOTE: this logic and its lower level dependent functions were simply ported from cpe_core legacy code.
 *
 * returns true upon latching low battery
 */
static bool checkDeviceForLowBattery(LegacySecurityCluster *cluster, uint64_t eui64, const uCStatusMessage *status)
{
    icLogDebug(LOG_TAG, "%s: %016"PRIx64, __FUNCTION__, eui64);

    bool lowBatteryLatched = false;

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    LegacyDeviceDetails *details = legacySecurityClusterAcquireDetails((ZigbeeCluster *) cluster, eui64);

    if (details != NULL)
    {
        if (details->isMainsPowered && !details->isBatteryBackedUp)
        {
            legacySecurityClusterReleaseDetails((ZigbeeCluster *) cluster);
            icLogDebug(LOG_TAG, "%s: Device %016"PRIx64 " does not have a battery.", __FUNCTION__, eui64);

            return false;
        }

        // If this is keypad/keyfob, we'll forego the latching logic below and just use a trivial counter. This is
        // because they do not have tamper switches.
        if (details->classification == UC_DEVICE_CLASS_KEYFOB || details->classification == UC_DEVICE_CLASS_KEYPAD)
        {
            lowBatteryLatched = checkKeyfobKeypadForLowBattery(details, eui64, status);
            legacySecurityClusterReleaseDetails((ZigbeeCluster *) cluster);

            return lowBatteryLatched;
        }

        bool recentTamper = getBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER);
        bool recentTamperRestore = getBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER_RESTORE);
        lowBatteryLatched = getBooleanMetadata(uuid, NULL, METADATA_LOW_BATTERY_LATCHED);

        // check if low battery bit is set for Smoke or CO sensors only
        bool isLowBatteryForSmokeOrCO = (status->status.fields1.lowBattery &&
                (IS_CO_SENSOR(details->devType) || IS_SMOKE_SENSOR(details->devType)));

        bool latchPrevented = checkForLatchPrevention(uuid, recentTamperRestore, lowBatteryLatched,
                status->batteryVoltage, details);

#ifdef DEBUG_BATTERY_LATCHING
        icLogDebug(LOG_TAG, "%s: batteryVoltage=%d lowBatteryCount=%d latchPrevented=%d preventLatchCount=%d lowBatteryLatched=%d recentTamper=%d recentTamperRestore=%d",
                __FUNCTION__, status->batteryVoltage, details->lowBatteryCount, latchPrevented, details->preventLatchCount, lowBatteryLatched, recentTamper, recentTamperRestore);
#endif
        if (latchPrevented && !isLowBatteryForSmokeOrCO)
        {
            setBooleanMetadata(uuid, NULL, METADATA_LOW_BATTERY_LATCHED, false);
            lowBatteryLatched = false;
        }
        else
        {
            if(!lowBatteryLatched)
            {
                // For Smoke/CO detectors, if the low battery bit is set, latch regardless of voltage/threshold
                if(isLowBatteryForSmokeOrCO)
                {
                    setBooleanMetadata(uuid, NULL, METADATA_LOW_BATTERY_LATCHED, true);
                    lowBatteryLatched = true;
                }
                else if(status->batteryVoltage < details->lowBatteryVoltage)
                {
                    details->lowBatteryCount++;

                    icLogDebug(LOG_TAG, "%s: lowBatteryCount=%"PRIu16", batteryVoltage=%"PRIu16, __FUNCTION__,
                            details->lowBatteryCount, status->batteryVoltage);

                    if(details->lowBatteryCount >= LOW_BATTERY_COUNT_THRESHOLD)
                    {
                        if (details->preventLatchWasReset)
                        {
                            icLogDebug(LOG_TAG, "%s: Resetting low battery count", __FUNCTION__);
                            details->lowBatteryCount = 0;
                            details->preventLatchWasReset = false;
                        }
                        else
                        {
                            setBooleanMetadata(uuid, NULL, METADATA_LOW_BATTERY_LATCHED, true);
                            lowBatteryLatched = true;
                            icLogDebug(LOG_TAG, "%s: low battery latched", __FUNCTION__);
                        }
                    }
                }
                else
                {
                    details->lowBatteryCount = 0;
                    details->preventLatchCount = 0;
                    setBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER_RESTORE, false);
                    setBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER, false);
                }
            }
            else //battery is latched
            {
                // If currently tampered, set recent tamper (if not already set).
                if(status->status.fields1.tamper && !recentTamper)
                {
                    setBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER, true);
                    setBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER_RESTORE, false);

                    icLogDebug(LOG_TAG, "%s: recentTamper set.  batteryVoltage=%"PRIu16,
                            __FUNCTION__, status->batteryVoltage);
                }
                // Per HH-968 AC, tamper restore implies a new battery may have been put in the sensor.
                else if (!status->status.fields1.tamper && recentTamper)
                {
                    if(isLowBatteryForSmokeOrCO)
                    {
                        icLogDebug(LOG_TAG, "%s: recentTamperRestore set.  Low battery bit set.  batteryVoltage=%"PRIu16,
                                __FUNCTION__, status->batteryVoltage);
                    }
                    else
                    {
                        setBooleanMetadata(uuid, NULL, METADATA_LOW_BATTERY_LATCHED, false);
                        lowBatteryLatched = false;

                        icLogDebug(LOG_TAG, "%s: recentTamperRestore set.  batteryVoltage=%"PRIu16,
                                __FUNCTION__, status->batteryVoltage);
                    }
                    setBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER, false);
                    setBooleanMetadata(uuid, NULL, METADATA_RECENT_TAMPER_RESTORE, true);
                }
            }
        }

        legacySecurityClusterReleaseDetails((const ZigbeeCluster *) cluster);
    }
    else
    {
        icLogWarn(LOG_TAG,
                  "%s: unable to get device details for %016"PRIx64 ", low battery will not be detected",
                  __FUNCTION__,
                  eui64);
    }

    return lowBatteryLatched;
}

/**
 * Keyfobs and Keypads are special in that they don't have tamper, so we can't do latching. We will perform a trivial
 * check for having low battery some number of messages in a row. This is analogous to legacy behavior.
 *
 * This function assumes the supplied device details lock is held
 *
 * @param details
 * @param eui64
 * @param status
 * @return
 */
static bool checkKeyfobKeypadForLowBattery(LegacyDeviceDetails *details, uint64_t eui64, const uCStatusMessage *status)
{
    icLogTrace(LOG_TAG, "%s: %016"PRIx64, __FUNCTION__, eui64);

    bool retVal = false;

    if (details != NULL)
    {
        if(status->batteryVoltage < details->lowBatteryVoltage)
        {
            details->lowBatteryCount++;

            icLogTrace(LOG_TAG, "%s: lowBatteryCount=%"PRIu16", batteryVoltage=%"PRIu16, __FUNCTION__,
                       details->lowBatteryCount, status->batteryVoltage);

            if(details->lowBatteryCount >= LOW_BATTERY_COUNT_THRESHOLD)
            {
                retVal = true;
            }
        }
        else
        {
            details->lowBatteryCount = 0;
        }
    }

    return retVal;
}

static void requestPing(LegacySecurityCluster *cluster, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterGetDetailsCopy((ZigbeeCluster *) cluster, eui64);

    bool shouldPing = true;
    if (cluster->callbacks->isGodparentPingSupported)
    {
        shouldPing = cluster->callbacks->isGodparentPingSupported(legacyDeviceDetails,
                                                                  cluster->callbackContext);
    }

    if (shouldPing)
    {
        if (legacyDeviceDetails->isMainsPowered)
        {
            setGodparentPingInfo(eui64, 0, -128, 0); //clear out data first
            zigbeeSubsystemSendMfgCommand(eui64,
                                          legacyDeviceDetails->endpointId,
                                          IAS_ZONE_CLUSTER_ID,
                                          true,
                                          SEND_PING,
                                          UC_MFG_ID,
                                          NULL,
                                          0);
        }
        else
        {
            //sleepy devices get this command via an aps ack
            setPendingMessage(cluster, eui64,
                              pendingMessageSendPing); //everyone starts off with a godparent ping request
        }
    }
    else
    {
        icLogWarn(LOG_TAG,
                  "Device %016"
                          PRIx64
                          " doesn't support godparent ping. It may get stuck in bootloader during upgrade.",
                  eui64);
    }
}

static bool
loadLegacyDeviceDetails(const DeviceServiceCallbacks *deviceService, uint64_t eui64, LegacyDeviceDetails **details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (details == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    //if the device has the LEGACY_DEVICE_TYPE_PROP, then it's legacy
    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) char *value = getMetadata(uuid, NULL, LEGACY_DEVICE_TYPE_PROP);
    unsigned long firmwareVersion = 0;
    char *bad = NULL;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to read device %s metadata", __FUNCTION__, LEGACY_DEVICE_TYPE_PROP);
        return false;
    }

    errno = 0;
    uCDeviceType deviceType = strtoul(value, &bad, 0);

    if (errno || *bad || deviceType > INT_MAX)
    {
        icLogError(LOG_TAG, "%s: unable to parse %s metadata", __FUNCTION__, LEGACY_DEVICE_TYPE_PROP);
        return false;
    }

    AUTO_CLEAN(resourceDestroy__auto)
    icDeviceResource *resource = deviceService->getResource(uuid, NULL, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to read device %s resource", __FUNCTION__,
                   COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);
        return false;
    }

    errno = 0;
    bad = NULL;
    firmwareVersion = strtoul(resource->value, &bad, 0);

    if (errno || *bad || firmwareVersion > LEGACY_FW_VER_MAX)
    {
        icLogError(LOG_TAG, "%s: unable to parse %s resource", __FUNCTION__, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);
        return false;
    }

    bool result = getLegacyDeviceDetails(deviceType, (uint32_t) firmwareVersion, details);

    if (result && *details != NULL)
    {
        //tack on any additional details persisted in metadata
        (*details)->devNum = getDeviceNumberForDevice(uuid);
    }

    return result;
}

static bool isLegacyCommand(ReceivedClusterCommand *command)
{
    return (command->clusterId == IAS_ZONE_CLUSTER_ID &&
            command->mfgSpecific &&
            (command->mfgCode == UC_MFG_ID || command->mfgCode == UC_MFG_ID_WRONG));
}

static bool isInBootloader(uint64_t eui64)
{
    bool result = false;

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    char *inBootloader = getMetadata(uuid, NULL, IN_BOOTLOADER);
    if (inBootloader != NULL && strcmp(inBootloader, "true") == 0)
    {
        result = true;
    }

    free(inBootloader);
    free(uuid);
    return result;
}

static bool setInBootloader(LegacySecurityCluster *cluster, uint64_t eui64, bool inBootloader)
{
    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    setMetadata(uuid, NULL, IN_BOOTLOADER, inBootloader ? "true" : "false");
    free(uuid);

    cluster->callbacks->upgradeInProgress(eui64, inBootloader, cluster->callbackContext);

    return true;
}


static bool godparentSanityCheck(uint64_t eui64, uint64_t *godparentEui64)
{
    bool result = false;

    uint8_t godparent;
    int8_t rssi;
    uint8_t lqi;

    if (getGodparentPingInfo(eui64, &godparent, &rssi, &lqi))
    {
        if ((rssi >= GODPARENT_RSSI_THRESHOLD) &&
            (lqi >= GODPARENT_LQI_THRESHOLD) &&
            getEui64ForDeviceNumber(godparent, godparentEui64) == true)
        {
            result = true;
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: godparent sanity check failed (rssi=%d, lqi=%d)", __FUNCTION__, rssi, lqi);
        }
    }

    return result;
}

//caller frees filenames
static bool validateFirmwareFiles(const DeviceDescriptor *dd, char **appFilename, char **bootloaderFilename)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (dd == NULL ||
        dd->latestFirmware == NULL ||
        dd->latestFirmware->filenames == NULL ||
        appFilename == NULL ||
        bootloaderFilename == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    //Free any previous filenames
    free(*appFilename);
    free(*bootloaderFilename);

    *appFilename = NULL;
    *bootloaderFilename = NULL;

    icLinkedListIterator *it = linkedListIteratorCreate(dd->latestFirmware->filenames);
    while (linkedListIteratorHasNext(it))
    {
        char *filename = linkedListIteratorGetNext(it);
        if (strcasestr(filename, BOOTLOADER_FILENAME_PREFIX) != NULL) // this is a bootloader file
        {
            *bootloaderFilename = filename;
        }
        else
        {
            *appFilename = filename;
        }
    }
    linkedListIteratorDestroy(it);

    if (*appFilename == NULL)
    {
        icLogError(LOG_TAG, "%s: did not find main firmware file in device descriptor", __FUNCTION__);
        return false;
    }

    char *firmwareDirectory = zigbeeSubsystemGetAndCreateFirmwareFileDirectory(dd->latestFirmware->type);
    char *tmpPath = (char *) malloc(strlen(*appFilename) + strlen(firmwareDirectory) + 2); // + '/' and \0
    sprintf(tmpPath, "%s/%s", firmwareDirectory, *appFilename);
    *appFilename = tmpPath;

    if (*bootloaderFilename != NULL)
    {
        tmpPath = (char *) malloc(strlen(*bootloaderFilename) + strlen(firmwareDirectory) + 2); // + '/' and \0
        sprintf(tmpPath, "%s/%s", firmwareDirectory, *bootloaderFilename);
        *bootloaderFilename = tmpPath;
    }
    free(firmwareDirectory);

    //ensure the files are present, readable, and not empty
    if (doesNonEmptyFileExist(*appFilename) == false)
    {
        icLogError(LOG_TAG, "%s: did not find main firmware file at %s", __FUNCTION__, *appFilename);
        return false;
    }

    if (*bootloaderFilename != NULL)
    {
        if (doesNonEmptyFileExist(*bootloaderFilename) == false)
        {
            icLogError(LOG_TAG, "%s: did not find bootloader file at %s", __FUNCTION__, *bootloaderFilename);
            return false;
        }
    }

    return true;
}

static bool startFirmwareUpgrade(LegacySecurityCluster *cluster,
                                 uint64_t eui64,
                                 uint8_t apsSeqnum,
                                 int8_t rssi,
                                 uint8_t lqi,
                                 bool forceAllowed)
{
    icLogDebug(LOG_TAG, "%s: %"
            PRIx64, __FUNCTION__, eui64);

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterGetDetailsCopy((ZigbeeCluster *) cluster, eui64);

    if (legacyDeviceDetails == NULL)
    {
        icLogError(LOG_TAG, "%s: legacy device details not found", __FUNCTION__);
        return false;
    }

    if (legacyDeviceDetails->upgradeAppFilename == NULL)
    {
        icLogError(LOG_TAG, "%s: no firmware file provided in device details", __FUNCTION__);
        return false;
    }

    if (legacyDeviceDetails->firmwareUpgradePending == false)
    {
        icLogError(LOG_TAG, "%s: firmwareUpgradePending is false", __FUNCTION__);
        return false;
    }

    uint64_t routerEui64 = 0;

    if (forceAllowed == false)
    {
        icLogInfo(LOG_TAG, "%s: firmware upgrades are not allowed now.", __FUNCTION__);
        return false;
    }

    //If this device needs a godparent, confirm we have one and the signal is good enough
    bool needsGodparent = true;
    if (cluster->callbacks->isGodparentPingSupported)
    {
        needsGodparent = cluster->callbacks->isGodparentPingSupported(legacyDeviceDetails, cluster->callbackContext);
    }

    if (needsGodparent && godparentSanityCheck(eui64, &routerEui64) == false)
    {
        icLogError(LOG_TAG, "%s: unable to start firmware upgrade since the godparent info is insufficient",
                   __FUNCTION__);
        setPendingMessage(cluster, eui64, pendingMessageSendPing);
        return false;
    }

    //Tell ZigbeeCore to do the upgrade first
    if (zigbeeSubsystemUpgradeDeviceFirmwareLegacy(eui64,
                                                   routerEui64,
                                                   legacyDeviceDetails->upgradeAppFilename,
                                                   legacyDeviceDetails->upgradeBootloaderFilename))
    {
        //Now enter bootloader if we got success from ZigbeeCore
        if (sendApsAck(eui64, legacyDeviceDetails->endpointId, ENTER_BOOTLOADER, apsSeqnum, NULL, 0, rssi, lqi,
                       isAutoAcked(legacyDeviceDetails)) ==
            false)
        {
            icLogError(LOG_TAG, "%s: failed to put device in bootloader", __FUNCTION__);
            return false;
        }

        setInBootloader(cluster, eui64, true);
    }

    return true;
}

bool legacySecurityClusterSendDeviceRemove(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(eui64,
                                         endpointId,
                                         IAS_ZONE_CLUSTER_ID,
                                         true,
                                         DEVICE_REMOVE,
                                         UC_MFG_ID,
                                         NULL,
                                         0) == 0;
}

static void legacyDeviceFreeFunc(void *key, void *value)
{
    LegacyDeviceDetails *details = (LegacyDeviceDetails *) value;
    legacyDeviceDetailsDestroy(details);
    free(key); //eui64
}

/********************************* Command/Message handlers ***********************************************************/

static bool handleDeviceAnnounceMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    return true;
}

static bool handleDeviceSerialNumberMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    return true;
}

static bool handleDeviceInfoMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uCDeviceInfoMessage msg;
    parseDeviceInfoMessage(command->commandData, command->commandDataLen, &msg);

    //if we got here it is a legacy icontrol device, but we dont yet support a few devices that we would discover
    // this way
    switch (msg.devType)
    {
        case TAKEOVER_1:
            result = false;
            break;

        default:
            result = true;
            break;
    }

    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails((ZigbeeCluster *) cluster,
                                                                                   command->eui64);
    bool putToSleep = true;
    if (legacyDeviceDetails != NULL)
    {
        uint32_t firmwareVersion = getFirmwareVersionFromDeviceInfoMessage(msg);
        AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(command->eui64);
        AUTO_CLEAN(free_generic__auto) char *newFw = getZigbeeVersionString(firmwareVersion);

        //if this device is pairing, we dont want to put it to sleep here
        if (legacyDeviceDetails->isPairing == true)
        {
            putToSleep = false;
        }

        //if this device just finished a firmware upgrade, clear out upgrade filenames
        if (memcmp(msg.firmwareVer, legacyDeviceDetails->firmwareVer, 3) != 0)
        {
            icLogInfo(LOG_TAG, "%s: %"
                    PRIx64
                    " just finished firmware upgrade", __FUNCTION__, command->eui64);
            free(legacyDeviceDetails->upgradeAppFilename);
            legacyDeviceDetails->upgradeAppFilename = NULL;
            free(legacyDeviceDetails->upgradeBootloaderFilename);
            legacyDeviceDetails->upgradeBootloaderFilename = NULL;
        }

        legacySecurityClusterReleaseDetails((ZigbeeCluster *) cluster);

        deviceService->updateResource(deviceUuid,
                                      NULL,
                                      COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                                      newFw,
                                      NULL);

        if (cluster->callbacks->firmwareVersionReceived != NULL)
        {
            cluster->callbacks->firmwareVersionReceived(command->eui64,
                                                        command->sourceEndpoint,
                                                        firmwareVersion,
                                                        cluster->callbackContext);

        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: no legacy device details found", __FUNCTION__);
    }

    if (putToSleep)
    {
        setPendingMessage(cluster, command->eui64, pendingMessageOkToSleep);
    }

    return result;
}

static bool handleDeviceStatusMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (command == NULL || command->commandData == NULL || command->commandDataLen == 0)
    {
        icLogError(LOG_TAG, "%s: invalid command data", __FUNCTION__);
        return false;
    }

    uCStatusMessage status;
    if (parseDeviceStatus(command->sourceEndpoint,
                          command->commandData,
                          command->commandDataLen,
                          &status))
    {
        bool isBatteryLow = checkDeviceForLowBattery(cluster, command->eui64, &status);

        legacyDeviceUpdateCommonResources(deviceService, command->eui64, &status, isBatteryLow);
        if (cluster->callbacks->deviceStatusChanged)
        {
            cluster->callbacks->deviceStatusChanged(command->eui64,
                                                    command->sourceEndpoint,
                                                    &status,
                                                    cluster->callbackContext);
        }
    }

    return result;
}

static bool handleDeviceCheckinMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool firmwareUpgradePending = false;
    //if we arent trying to get into bootloader mode, trigger a godparent ping
    const LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails((ZigbeeCluster *) cluster,
                                                                                         command->eui64);

    if (legacyDeviceDetails != NULL)
    {
        firmwareUpgradePending = legacyDeviceDetails->firmwareUpgradePending;
        legacySecurityClusterReleaseDetails((ZigbeeCluster *) cluster);
    }

    if (firmwareUpgradePending == false)
    {
        setPendingMessage(cluster, command->eui64, pendingMessageSendPing);
    }

    // process like a device status message
    return handleDeviceStatusMessage(cluster, command);
}

static bool handlePingMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    uint8_t devNum = command->commandData[0];

    icLogDebug(LOG_TAG, "%s: %"
            PRIx64
            " reports devNum %"
            PRIu8
            ", rssi %"
            PRId8
            ", lqi %"
            PRIu8,
               __FUNCTION__, command->eui64, devNum, command->rssi, command->lqi);

    //we heard the ping ourselves, start off with us as the godparent
    setGodparentPingInfo(command->eui64, 0, command->rssi, command->lqi);

    //if we have a firmware update pending (and we are not mains powered), queue up the bootload request
    const LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails((ZigbeeCluster *) cluster,
                                                                                         command->eui64);

    bool firmwareUpgradePending = false;
    bool isMainsPowered = false;
    if (legacyDeviceDetails != NULL)
    {
        firmwareUpgradePending = legacyDeviceDetails->firmwareUpgradePending;
        isMainsPowered = legacyDeviceDetails->isMainsPowered;
        legacySecurityClusterReleaseDetails((ZigbeeCluster *) cluster);
    }

    if (firmwareUpgradePending && isMainsPowered == false)
    {
        setPendingMessage(cluster, command->eui64, pendingMessageEnterBootloader);
    }

    return true;
}

static bool handleKeyfobEventMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (cluster->callbacks->securityControllerCallbacks != NULL &&
        cluster->callbacks->securityControllerCallbacks->handleKeyfobMessage != NULL)
    {
        uCKeyfobMessage kfMsg;
        if (parseKeyfobMessage(command->commandData, command->commandDataLen, &kfMsg))
        {
            // NOTE: legacy keyfobs do not send device check-in or status messages because they are considered 'mobile'
            // devices. That is, they do a rejoin each time you push a button to prevent the battery from dying when
            // the user takes them on the go. Thus, we have to pull what data we can from a keyfob message and update
            // the common resources from that.
            uCStatusMessage statusMessage;
            statusMessage.lqi = kfMsg.lqi;
            statusMessage.rssi = kfMsg.rssi;
            statusMessage.batteryVoltage = (kfMsg.batteryVoltage[0] << 8) + (uint16_t)(kfMsg.batteryVoltage[1] & 0xFF);

            bool isBatteryLow = checkDeviceForLowBattery(cluster, command->eui64, &statusMessage);

            legacyDeviceUpdateCommonResources(deviceService, command->eui64, &statusMessage, isBatteryLow);

            cluster->callbacks->securityControllerCallbacks->handleKeyfobMessage(command->eui64,
                                                                                 command->sourceEndpoint,
                                                                                 &kfMsg,
                                                                                 cluster->callbackContext);
            handled = true;
        }
    }

    return handled;
}

static bool handleKeypadEventMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    bool handled = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (cluster->callbacks->securityControllerCallbacks != NULL &&
        cluster->callbacks->securityControllerCallbacks->handleKeypadMessage != NULL)
    {
        uCKeypadMessage kpMsg;
        if (parseKeypadMessage(command->commandData, command->commandDataLen, &kpMsg))
        {
            cluster->callbacks->securityControllerCallbacks->handleKeypadMessage(command->eui64,
                                                                                 command->sourceEndpoint,
                                                                                 &kpMsg,
                                                                                 cluster->callbackContext);
            handled = true;
        }
    }

    return handled;
}

/*
 * This message tells us how well the sending device heard the godparent ping from another legacy device.
 *
 * This logic ported from legacy cpe_core
 */
static bool handleGodparentInfoMessage(LegacySecurityCluster *cluster, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    bool result = false;

    if (command == NULL || command->commandDataLen < 12)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint8_t router = command->commandData[0];
    uint8_t targetDeviceNum = command->commandData[1];
    int8_t rssi = command->commandData[10];
    uint8_t lqi = command->commandData[11];

    uint64_t targetDeviceEui64;

    if (getEui64ForDeviceNumber(targetDeviceNum, &targetDeviceEui64) == false || targetDeviceEui64 == 0)
    {
        icLogError(LOG_TAG, "%s: failed to get EUI64 for device number!", __FUNCTION__);
    }
    else
    {
        uint8_t godparent = 0;
        int8_t godparentRssi = -128;
        uint8_t godparentLqi = 0;

        if (getGodparentPingInfo(targetDeviceEui64, &godparent, &godparentRssi, &godparentLqi))
        {
            if (godparent == 0)
            {
                // the target device currently has the coordinator as godparent.  In order for the device
                // that sent this message to possibly become the godparent, the coordinator must be
                // a weak godparent.
                if (godparentLqi >= GODPARENT_LQI_THRESHOLD && godparentRssi >= GODPARENT_RSSI_THRESHOLD)
                {
                    //the godparent is the coordinator and its good enough to stay.
                    icLogDebug(LOG_TAG, "%s: the coordinator is already godparent, and it is good enough",
                               __FUNCTION__);
                }
                else
                {
                    //the godparent is either a router or is not good enough to skip comparing to the message sender
                    if (lqi > godparentLqi || (lqi == godparentLqi && rssi > godparentRssi))
                    {
                        //the message sender wins and is the new godparent
                        icLogDebug(LOG_TAG, "%s: router %"
                                PRIx64
                                " is the new godparent of %"
                                PRIx64,
                                   __FUNCTION__, command->eui64, targetDeviceEui64);

                        setGodparentPingInfo(targetDeviceEui64, router, rssi, lqi);
                    }
                }
            }

            result = true;
        }
    }

    return result;
}

static bool handleClusterCommand(ZigbeeCluster *cluster, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s: commandId 0x%02x, mfgId=0x%04x, isMfgSpecifc=%s", __FUNCTION__, command->commandId,
               command->mfgCode, command->mfgSpecific ? "true" : "false");

    bool result = false;
    bool sendApsAck = true;

    if (!isLegacyCommand(command))
    {
        icLogError(LOG_TAG, "%s: not a legacy command", __FUNCTION__);
        return false;
    }

    /* Make sure the device is known and update sequence if necessary */
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterAcquireDetails(cluster, command->eui64);

    if (legacyDeviceDetails == NULL)
    {
        icLogError(LOG_TAG, "%s: unknown device", __FUNCTION__);
        return false;
    }

    if (isInBootloader(command->eui64))
    {
        icLogInfo(LOG_TAG, "%"
                PRIx64
                " returned from bootloader", command->eui64);
        setInBootloader((LegacySecurityCluster *) cluster, command->eui64, false);
        legacyDeviceDetails->firmwareUpgradePending = false;
    }

    LegacySecurityCluster *legacySecurityCluster = (LegacySecurityCluster *) cluster;

    // validate the sequence number and discard out of order messages
    uint8_t seqNum = command->seqNum;
    uint8_t seqNumDelta = seqNum - legacyDeviceDetails->legacyDeviceSeqNum;
    bool aceAndZoneShouldProcess = true;
    // zigbee uses a byte to track the sequence numbers; bytes roll over at 255; This is the
    // way that the legacy sensors work
    if (((seqNumDelta <= LEGACY_SEQ_NUM_ROLLOVER_MAX) || (legacyDeviceDetails->legacyDeviceSeqNum == 0)) &&
        (seqNum != legacyDeviceDetails->legacyDeviceSeqNum))
    {
        // the sequence number is ok, process it
        legacyDeviceDetails->legacyDeviceSeqNum = seqNum;
    }
    else
    {
        icLogInfo(LOG_TAG, "%s: tossing message from %"
                PRIx64
                " msg seqNum=%"
                PRIu8
                ", last seqNum=%"
                PRIu8,
                  __FUNCTION__,
                  command->eui64,
                  seqNum,
                  legacyDeviceDetails->legacyDeviceSeqNum);

        aceAndZoneShouldProcess = false;
    }

    legacySecurityClusterReleaseDetails(cluster);

    switch (command->commandId)
    {
        case DEVICE_ANNOUNCE:
            result = handleDeviceAnnounceMessage(legacySecurityCluster, command);
            break;

        case DEVICE_SERIAL_NUM:
            result = handleDeviceSerialNumberMessage(legacySecurityCluster, command);
            break;

        case DEVICE_INFO:
            result = handleDeviceInfoMessage(legacySecurityCluster, command);
            break;

        case DEVICE_STATUS:
            result = handleDeviceStatusMessage(legacySecurityCluster, command);
            break;

        case DEVICE_CHECKIN:
            result = handleDeviceCheckinMessage(legacySecurityCluster, command);
            break;

        case PING_MSG:
            result = handlePingMessage(legacySecurityCluster, command);
            sendApsAck = false; //these messages are not transactional... they are interpan
            break;

        case DEVICE_KEYFOB_EVENT:
            result = handleKeyfobEventMessage(legacySecurityCluster, command);
            break;

        case DEVICE_KEYPAD_EVENT:
            result = handleKeypadEventMessage(legacySecurityCluster, command);
            break;

        case GODPARENT_INFO:
            result = handleGodparentInfoMessage(legacySecurityCluster, command);
            break;

        default:
            icLogWarn(LOG_TAG, "%s: not handling command %d", __FUNCTION__, command->commandId);
            break;
    }

    // send the pending message
    if (sendApsAck)
    {
        sendPendingMessage(legacySecurityCluster, command->eui64, command->apsSeqNum, command->rssi, command->lqi);
    }

    return result;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE
