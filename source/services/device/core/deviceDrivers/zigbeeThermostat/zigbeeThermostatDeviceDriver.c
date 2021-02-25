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
// Created by tlea on 2/20/19.
//

#include <icBuildtime.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <stdio.h>
#include <resourceTypes.h>
#include <commonDeviceDefs.h>
#include <deviceModelHelper.h>
#include <memory.h>
#include <zigbeeClusters/thermostatCluster.h>
#include <zigbeeClusters/fanControlCluster.h>
#include <zigbeeClusters/pollControlCluster.h>
#include <zigbeeClusters/powerConfigurationCluster.h>
#include <icUtil/stringUtils.h>
#include "zigbeeDriverCommon.h"
#include "zigbeeThermostatDeviceDriver.h"

#define LOG_TAG "zigbeeThermostatDD"
#define DRIVER_NAME "zigbeeThermostat"
#define DEVICE_CLASS_NAME "thermostat"
#define MY_DC_VERSION 1
#define MAX_TEMP_VALUE 9999
#define FAST_POLL_RATE_QS 2
#define REGULAR_POLL_RATE_QS 28
#define CENTRALITE_MANUFACTURER_NAME "CentraLite Systems"
#define CENTRALITE_MODEL_NAME "3156105"
#define RTCOA_MANUFACTURER_NAME "RTCOA"
#define RTCOA_MODEL_NAME "CT30S"

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

static uint16_t myDeviceIds[] =
        {
                THERMOSTAT_DEVICE_ID
        };

static bool claimDevice(ZigbeeDriverCommon *ctx,
                        IcDiscoveredDeviceDetails *details);

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails);

static void deviceRejoined(ZigbeeDriverCommon *ctx,
                           uint64_t eui64,
                           bool isSecure,
                           IcDiscoveredDeviceDetails *details);

static bool devicePersisted(ZigbeeDriverCommon *ctx, icDevice *device);

static void handleAlarms(ZigbeeDriverCommon *ctx,
                         uint64_t eui64,
                         uint8_t endpointId,
                         const ZigbeeAlarmTableEntry *alarms,
                         int numAlarms);

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static bool preConfigureCluster(ZigbeeDriverCommon *ctx,
                                ZigbeeCluster *cluster,
                                DeviceConfigurationContext *deviceConfigContext);

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

static void localTemperatureChanged(uint64_t eui64,
                                    uint8_t endpointId,
                                    int16_t temp,
                                    const void *ctx);

static void occupiedHeatingSetpointChanged(uint64_t eui64,
                                           uint8_t endpointId,
                                           int16_t temp,
                                           const void *ctx);

static void occupiedCoolingSetpointChanged(uint64_t eui64,
                                           uint8_t endpointId,
                                           int16_t temp,
                                           const void *ctx);

static void systemModeChanged(uint64_t eui64,
                              uint8_t endpointId,
                              uint8_t mode,
                              const void *ctx);

static void runningStateChanged(uint64_t eui64,
                                uint8_t endpointId,
                                uint16_t state,
                                const void *ctx);

static void setpointHoldChanged(uint64_t eui64,
                                uint8_t endpointId,
                                bool holdOn,
                                const void *ctx);

static void ctrlSeqOpChanged(uint64_t eui64,
                             uint8_t endpointId,
                             uint8_t ctrlSeqOp,
                             const void *ctx);

static void fanModeChanged(uint64_t eui64,
                           uint8_t endpointId,
                           uint8_t mode,
                           const void *ctx);

static void legacyOperationInfoReceived(uint64_t eui64,
                                        uint8_t endpointId,
                                        uint8_t runningMode,      // 0=off, 1=heat, 2=cool
                                        bool holdOn,
                                        uint8_t runningState,     // 0=off, 1=heat, 2=cool, 0xff=not used
                                        uint8_t fanRunningState); // 0=off, 1=running, 0xff=not used

static void localTemperatureCalibrationChanged(uint64_t eui64,
                                               uint8_t endpointId,
                                               int8_t calibrationTemp,
                                               const void *ctx);

static bool isLegacyThermostat(const char *model);

static bool isLegacyRtcoaThermostat(const char *model);

static bool isLegacyCentraliteThermostat(const char *model);

static const ZigbeeDriverCommonCallbacks commonCallbacks =
        {
                .fetchInitialResourceValues = fetchInitialResourceValues,
                .registerResources = registerResources,
                .writeEndpointResource = writeEndpointResource,
                .mapDeviceIdToProfile = mapDeviceIdToProfile,
                .preConfigureCluster = preConfigureCluster,
                .claimDevice = claimDevice,
                .configureDevice = configureDevice,
                .deviceRejoined = deviceRejoined,
                .devicePersisted = devicePersisted,
                .handleAlarms = handleAlarms,
                .synchronizeDevice = synchronizeDevice,
        };

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;

static ZigbeeCluster *thermostatCluster = NULL;

static ZigbeeCluster *fanControlCluster = NULL;

static ThermostatClusterCallbacks thermostatClusterCallbacks =
        {
                .systemModeChanged = systemModeChanged,
                .localTemperatureChanged = localTemperatureChanged,
                .occupiedHeatingSetpointChanged = occupiedHeatingSetpointChanged,
                .occupiedCoolingSetpointChanged = occupiedCoolingSetpointChanged,
                .runningStateChanged = runningStateChanged,
                .setpointHoldChanged = setpointHoldChanged,
                .ctrlSeqOpChanged = ctrlSeqOpChanged,
                .legacyOperationInfoReceived = legacyOperationInfoReceived,
                .localTemperatureCalibrationChanged = localTemperatureCalibrationChanged,
        };

static FanControlClusterCallbacks fanControlClusterCallbacks =
        {
                .fanModeChanged = fanModeChanged,
        };

DeviceDriver *zigbeeThermostatDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  sizeof(myDeviceIds) / sizeof(uint16_t),
                                                                  deviceService,
                                                                  &commonCallbacks);

    deviceServiceCallbacks = deviceService;

    thermostatCluster = thermostatClusterCreate(&thermostatClusterCallbacks, myDriver);
    zigbeeDriverCommonAddCluster(myDriver, thermostatCluster);

    fanControlCluster = fanControlClusterCreate(&fanControlClusterCallbacks, myDriver);
    zigbeeDriverCommonAddCluster(myDriver, fanControlCluster);

    return myDriver;
}

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (ctx == NULL || device == NULL || discoveredDeviceDetails == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    if(isLegacyRtcoaThermostat(discoveredDeviceDetails->model) == true)
    {
        uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

        result = thermostatClusterSetAbsoluteSetpointModeLegacy(thermostatCluster,
                                                                eui64,
                                                                discoveredDeviceDetails->endpointDetails[0].endpointId);
    }

    return result;
}

static bool claimDevice(ZigbeeDriverCommon *ctx,
                        IcDiscoveredDeviceDetails *details)
{
    bool result = false;

    if (ctx == NULL || details == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    if (details->numEndpoints == 1)
    {
        IcDiscoveredEndpointDetails *endpoint = &details->endpointDetails[0];

        if (endpoint->appDeviceId == 0x301 &&
            details->manufacturer == NULL &&
            details->hardwareVersion == 0 &&
            details->model == NULL &&
            endpoint->endpointId == 10)
        {
            //all that matches RTCoA so we can proceed to the final check that it has a server cluster id 0x800.
            for (uint8_t i = 0; i < endpoint->numServerClusterDetails; i++)
            {
                if (endpoint->serverClusterDetails[i].clusterId == 0x800)
                {
                    icLogDebug(LOG_TAG, "%s: claiming RTCoA thermostat", __FUNCTION__);
                    result = true;
                    details->manufacturer = strdup(RTCOA_MANUFACTURER_NAME);
                    details->model = strdup(RTCOA_MODEL_NAME);
                    details->hardwareVersion = 199; //only versions 192 and 199 are supported anyway.  This is a guess.
                    details->firmwareVersion = 0x00000592; //we do not manage firmware on these, and this was the latest
                    break;
                }
            }
        }
        else if (endpoint->appDeviceId == 0x301 &&
                 details->manufacturer != NULL &&
                 strcmp(CENTRALITE_MANUFACTURER_NAME, details->manufacturer) == 0 &&
                 details->hardwareVersion == 2 &&
                 details->appVersion == 2 &&
                 details->model != NULL &&
                 strcmp(CENTRALITE_MODEL_NAME, details->model) == 0 &&
                 endpoint->endpointId == 1)
        {
            //all that matches CentraLite so we can proceed to the final check that it has a server cluster id 0x204.
            for (uint8_t i = 0; i < endpoint->numServerClusterDetails; i++)
            {
                if (endpoint->serverClusterDetails[i].clusterId == 0x204)
                {
                    icLogDebug(LOG_TAG, "%s: claiming CentraLite thermostat", __FUNCTION__);
                    result = true;
                    break;
                }
            }
        }
    }

    if (result == true)
    {
        //we are claiming this legacy device.  Crank up its poll frequency until configuration is complete.
        thermostatClusterSetPollRateLegacy(thermostatCluster,
                                           details->eui64,
                                           details->endpointDetails[0].endpointId,
                                           FAST_POLL_RATE_QS);
    }

    return result;
}

// legacy thermostats that rejoin may have rebooted and they need some stuff to be reconfigured.
static void deviceRejoined(ZigbeeDriverCommon *ctx,
                           uint64_t eui64,
                           bool isSecure,
                           IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (ctx == NULL || details == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    bool isLegacyRtcoa = isLegacyRtcoaThermostat(details->model);
    bool isLegacyCentralite = isLegacyCentraliteThermostat(details->model);

    if(isLegacyRtcoa || isLegacyCentralite)
    {
        if(isLegacyRtcoa == true)
        {
            uint8_t endpointId =details->endpointDetails[0].endpointId;

            //set absolute set point mode
            thermostatClusterSetAbsoluteSetpointModeLegacy(thermostatCluster,
                                                           eui64,
                                                           endpointId);

            //restore occupied heating and cooling set points and system mode from our cached resource values
            char epName[4]; //max uint8_t + \0
            sprintf(epName, "%" PRIu8, endpointId);
            AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);

            AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *cooling =
                    deviceServiceCallbacks->getResource(uuid, epName, THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT);

            int16_t temp;

            if(cooling != NULL && thermostatClusterGetTemperatureValue(cooling->value, &temp))
            {
                thermostatClusterSetOccupiedCoolingSetpoint(thermostatCluster,
                                                            eui64,
                                                            endpointId,
                                                            temp);
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to re-set cooling setpoint", __FUNCTION__);
            }

            AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *heating =
                    deviceServiceCallbacks->getResource(uuid, epName, THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT);

            if(heating != NULL && thermostatClusterGetTemperatureValue(heating->value, &temp))
            {
                thermostatClusterSetOccupiedHeatingSetpoint(thermostatCluster,
                                                            eui64,
                                                            endpointId,
                                                            temp);
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to re-set heating setpoint", __FUNCTION__);
            }

            AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *mode =
                    deviceServiceCallbacks->getResource(uuid, epName, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE);

            if(mode != NULL)
            {
                thermostatClusterSetSystemMode(thermostatCluster,
                                               eui64,
                                               endpointId,
                                               thermostatClusterGetSystemModeFromString(mode->value));
            }

            //reapply attribute reportings
            AUTO_CLEAN(deviceDescriptorFree__auto) DeviceDescriptor *dd =
                    zigbeeDriverCommonGetDeviceDescriptor(details->manufacturer,
                                                          details->model,
                                                          details->hardwareVersion,
                                                          details->firmwareVersion);

            zigbeeDriverCommonConfigureEndpointClusters(eui64,
                                                        endpointId,
                                                        ctx,
                                                        details,
                                                        dd);
        }

        //for both of these legacy thermostats we clear any low battery on rejoin
       zigbeeDriverCommonUpdateBatteryChargeStatus((DeviceDriver*)ctx, eui64, false);
    }
}

static bool devicePersisted(ZigbeeDriverCommon *ctx, icDevice *device)
{
    bool result = false;

    if (ctx == NULL || device == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //for legacy thermostats, we need to trigger it to send us the mfg specific command to get real values
    //  for the resources mapped to the operational info message.
    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *model =
            deviceServiceCallbacks->getResource(device->uuid,
                                                NULL,
                                                COMMON_DEVICE_RESOURCE_MODEL);
    if (model != NULL && model->value != NULL)
    {
        if (isLegacyThermostat(model->value) == true)
        {
            //its safe to use just the first endpoint on these legacy devices since they have only 1
            icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);
            if(endpoint != NULL)
            {
                uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
                uint8_t endpointId;
                if (stringToUint8(endpoint->id, &endpointId) == true)
                {
                    result = thermostatClusterRequestOperationalInfoLegacy(thermostatCluster, eui64, endpointId);
                }

                //Turn back down the fast poll rate now that we are done with onboarding.  Discard any error here.
                thermostatClusterSetPollRateLegacy(thermostatCluster,
                                                   eui64,
                                                   endpointId,
                                                   REGULAR_POLL_RATE_QS);
            }
        }
        else
        {
            result = true; // not a legacy tstat
        }
    }

    return result;
}

static void handleAlarms(ZigbeeDriverCommon *ctx,
                         uint64_t eui64,
                         uint8_t endpointId,
                         const ZigbeeAlarmTableEntry *alarms,
                         int numAlarms)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (ctx == NULL || alarms == NULL || numAlarms == 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    //All basic/common alarm handling is done in the cluster or the common driver except for the legacy
    // CentraLite thermostat which sends alarm code 1 on the power configuration cluster for low battery
    //  instead of the correct 0x10.  For efficiency, we will first check the cluster and alarm code.
    for (int i = 0; i < numAlarms; i++)
    {
        if(alarms[i].clusterId == POWER_CONFIGURATION_CLUSTER_ID && alarms[i].alarmCode == 0x1)
        {
            AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
            AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *model = deviceServiceCallbacks->getResource(uuid,
                                                                                                            NULL,
                                                                                                            COMMON_DEVICE_RESOURCE_MODEL);

            if (model != NULL && model->value != NULL && isLegacyCentraliteThermostat(model->value) == true)
            {
                zigbeeDriverCommonUpdateBatteryChargeStatus((DeviceDriver*)ctx, eui64, true);
            }
        }
    }
}

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (ctx == NULL || device == NULL || details == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    //reapply attribute reportings for legacy thermostats on startup
    if (isLegacyThermostat(details->model) == true)
    {
        // Crank up the poll rate while we do this work
        thermostatClusterSetPollRateLegacy(thermostatCluster,
                                           details->eui64,
                                           details->endpointDetails[0].endpointId,
                                           FAST_POLL_RATE_QS);

        AUTO_CLEAN(deviceDescriptorFree__auto) DeviceDescriptor *dd =
                zigbeeDriverCommonGetDeviceDescriptor(details->manufacturer,
                                                      details->model,
                                                      details->hardwareVersion,
                                                      details->firmwareVersion);

        icLinkedListIterator *it = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(it))
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(it);
            uint8_t endpointId = zigbeeDriverCommonGetEndpointNumber(ctx, endpoint);
            zigbeeDriverCommonConfigureEndpointClusters(eui64,
                                                        endpointId,
                                                        ctx,
                                                        details,
                                                        dd);
        }
        linkedListIteratorDestroy(it);
    }
    else
    {
        // crank up poll rate for standard thermostats
        pollControlClusterSetLongPollInterval(eui64, details->endpointDetails[0].endpointId, FAST_POLL_RATE_QS);
    }

    //Fetch the current state of the thermostat in case we missed an attribute report during reboot, etc.
    icLinkedListIterator *it = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(it))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(it);
        uint8_t endpointId = zigbeeDriverCommonGetEndpointNumber(ctx, endpoint);
        int16_t temp; // used for various temperature reads

        // Local temperature
        if(thermostatClusterGetLocalTemperature(thermostatCluster, eui64, endpointId, &temp) == true)
        {
            char *tempStr = thermostatClusterGetTemperatureString(temp);
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   endpoint->id,
                                                   THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP,
                                                   tempStr,
                                                   NULL);
            free(tempStr);
        }

        // System mode
        uint8_t systemMode;
        if(thermostatClusterGetSystemMode(thermostatCluster, eui64, endpointId, &systemMode) == true)
        {
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   endpoint->id,
                                                   THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE,
                                                   thermostatClusterGetSystemModeString(systemMode),
                                                   NULL);
        }

        // Heat setpoint
        if(thermostatClusterGetOccupiedHeatingSetpoint(thermostatCluster, eui64, endpointId, &temp) == true)
        {
            char *tempStr = thermostatClusterGetTemperatureString(temp);
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   endpoint->id,
                                                   THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT,
                                                   tempStr,
                                                   NULL);
            free(tempStr);
        }

        // Cool setpoint
        if(thermostatClusterGetOccupiedCoolingSetpoint(thermostatCluster, eui64, endpointId, &temp) == true)
        {
            char *tempStr = thermostatClusterGetTemperatureString(temp);
            deviceServiceCallbacks->updateResource(device->uuid,
                                                   endpoint->id,
                                                   THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT,
                                                   tempStr,
                                                   NULL);
            free(tempStr);
        }

        // legacy thermostats get THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON, THERMOSTAT_PROFILE_RESOURCE_FAN_ON, and
        // THERMOSTAT_PROFILE_RESOURCE_HOLD_ON from a command response instead
        if (isLegacyThermostat(details->model) == false)
        {
            uint16_t state;
            if (thermostatClusterGetRunningState(thermostatCluster, eui64, endpointId, &state) == true)
            {
                deviceServiceCallbacks->updateResource(device->uuid,
                                                       endpoint->id,
                                                       THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON,
                                                       thermostatClusterIsSystemOn(state) ? "true" : "false",
                                                       NULL);

                deviceServiceCallbacks->updateResource(device->uuid,
                                                       endpoint->id,
                                                       THERMOSTAT_PROFILE_RESOURCE_FAN_ON,
                                                       thermostatClusterIsFanOn(state) ? "true" : "false",
                                                       NULL);
            }

            bool isHoldOn;
            if (thermostatClusterIsHoldOn(thermostatCluster, eui64, endpointId, &isHoldOn) == true)
            {
                deviceServiceCallbacks->updateResource(device->uuid,
                                                       endpoint->id,
                                                       THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                                       isHoldOn ? "true" : "false",
                                                       NULL);
            }
        }
        else
        {
            // for legacy thermostats, just send the mfg command to trigger it to update us for these resources
            thermostatClusterRequestOperationalInfoLegacy(thermostatCluster, eui64, endpointId);
        }
    }
    linkedListIteratorDestroy(it);

    // turn down the poll rate to standard now that we are done
    if (isLegacyThermostat(details->model) == true)
    {
        thermostatClusterSetPollRateLegacy(thermostatCluster,
                                           details->eui64,
                                           details->endpointDetails[0].endpointId,
                                           REGULAR_POLL_RATE_QS);
    }
    else
    {
        pollControlClusterSetLongPollInterval(eui64, details->endpointDetails[0].endpointId, REGULAR_POLL_RATE_QS);
    }
}

static bool fetchInitialResourceValuesCommon(ZigbeeDriverCommon *ctx,
                                             uint64_t eui64,
                                             icDevice *device,
                                             IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                             icInitialResourceValues *initialResourceValues)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        uint8_t mode;
        if (thermostatClusterGetSystemMode(thermostatCluster, eui64, endpointId, &mode) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get system mode", __FUNCTION__);
            goto exit;
        }

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE,
                                              thermostatClusterGetSystemModeString(mode));

        if (fanControlClusterGetFanMode(thermostatCluster, eui64, endpointId, &mode) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get fan mode", __FUNCTION__);
            goto exit;
        }

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_FAN_MODE,
                                              fanControlClusterGetFanModeString(mode));

        int16_t temp;
        if (thermostatClusterGetLocalTemperature(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get local temp", __FUNCTION__);
            goto exit;
        }

        char *tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for local temperature attribute", __FUNCTION__);
            goto exit;
        }

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP,
                                              tmpStr);
        free(tmpStr);

        if (thermostatClusterGetAbsMinHeatSetpoint(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get abs min heat setpoint", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for abs min heat attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_HEAT,
                                              tmpStr);
        free(tmpStr);

        if (thermostatClusterGetAbsMaxHeatSetpoint(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get abs max heat setpoint", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for abs max heat attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_HEAT,
                                              tmpStr);
        free(tmpStr);

        if (thermostatClusterGetAbsMinCoolSetpoint(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get abs min cool setpoint", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for abs min cool attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_COOL,
                                              tmpStr);
        free(tmpStr);

        if (thermostatClusterGetAbsMaxCoolSetpoint(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get abs max cool setpoint", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for abs max cool attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_COOL,
                                              tmpStr);
        free(tmpStr);

        int8_t cal;
        if (thermostatClusterGetLocalTemperatureCalibration(thermostatCluster, eui64, endpointId, &cal) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get local temperature calibration", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(cal);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for local temp calibration attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP_CALIBRATION,
                                              tmpStr);
        free(tmpStr);

        if (thermostatClusterGetOccupiedHeatingSetpoint(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get occupied heating setpoint", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for occupied heating setpoint attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT,
                                              tmpStr);
        free(tmpStr);

        if (thermostatClusterGetOccupiedCoolingSetpoint(thermostatCluster, eui64, endpointId, &temp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get occupied cooling setpoint", __FUNCTION__);
            goto exit;
        }

        tmpStr = thermostatClusterGetTemperatureString(temp);
        if (tmpStr == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to get valid value for occupied cooling setpoint attribute", __FUNCTION__);
            goto exit;
        }
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT,
                                              tmpStr);
        free(tmpStr);

        uint8_t ctrlSeqOp;
        if (thermostatClusterGetCtrlSeqOp(thermostatCluster, eui64, endpointId, &ctrlSeqOp) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get control sequence of operation", __FUNCTION__);
            goto exit;
        }

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_CONTROL_SEQ,
                                              thermostatClusterGetCtrlSeqOpString(ctrlSeqOp));
    }

    result = true;

    exit:
    return result;
}

static bool fetchAdditionalInitialResourceValuesLegacy(ZigbeeDriverCommon *ctx,
                                                       uint64_t eui64,
                                                       icDevice *device,
                                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                                       icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        //Since we cant read these values as attributes, assume false for now.
        // once this device is persisted we will trigger it to send us an 'operational info request' with
        // the real values for these.
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON,
                                              "false");
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_FAN_ON,
                                              "false");
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                              "false");
    }

    return result;
}

static bool fetchAdditionalInitialResourceValuesStandard(ZigbeeDriverCommon *ctx,
                                                         uint64_t eui64,
                                                         icDevice *device,
                                                         IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                                         icInitialResourceValues *initialResourceValues)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        uint16_t state;
        if (thermostatClusterGetRunningState(thermostatCluster, eui64, endpointId, &state) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get running state", __FUNCTION__);
            goto exit;
        }

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON,
                                              thermostatClusterIsSystemOn(state) ? "true" : "false");
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_FAN_ON,
                                              thermostatClusterIsFanOn(state) ? "true" : "false");

        bool isOn;
        if (thermostatClusterIsHoldOn(thermostatCluster, eui64, endpointId, &isOn) == false)
        {
            icLogError(LOG_TAG, "%s: failed to get is hold on", __FUNCTION__);
            goto exit;
        }

        initialResourceValuesPutEndpointValue(initialResourceValues, epName, THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                              isOn ? "true" : "false");

    }

    result = true;

    exit:
    return result;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    if (ctx == NULL || device == NULL || discoveredDeviceDetails == NULL || initialResourceValues == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    if(fetchInitialResourceValuesCommon(ctx, eui64, device, discoveredDeviceDetails, initialResourceValues) == true)
    {
        if (isLegacyThermostat(discoveredDeviceDetails->model) == true)
        {
            result = fetchAdditionalInitialResourceValuesLegacy(ctx,
                                                                eui64,
                                                                device,
                                                                discoveredDeviceDetails,
                                                                initialResourceValues);
        }
        else
        {
            result = fetchAdditionalInitialResourceValuesStandard(ctx,
                                                                  eui64,
                                                                  device,
                                                                  discoveredDeviceDetails,
                                                                  initialResourceValues);
        }
    }

    return result;
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    if (ctx == NULL || device == NULL || discoveredDeviceDetails == NULL || initialResourceValues == NULL)
    {
         icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
         return false;
    }

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; result && i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);
        icDeviceEndpoint *endpoint = createEndpoint(device, epName, THERMOSTAT_PROFILE, true);

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_FAN_ON,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TSTAT_SYSTEM_MODE,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;
        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_FAN_MODE,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TSTAT_FAN_MODE,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_HEAT,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_HEAT,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                               THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_COOL,
                               initialResourceValues,
                               RESOURCE_TYPE_TEMPERATURE,
                               RESOURCE_MODE_READABLE,
                               CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_COOL,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP_CALIBRATION,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TEMPERATURE,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        result &= createEndpointResourceIfAvailable(endpoint,
                                                    THERMOSTAT_PROFILE_RESOURCE_CONTROL_SEQ,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_TSTAT_CTRL_SEQ_OP,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
    }

    return result;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    uint8_t numDeviceIds = sizeof(myDeviceIds) / sizeof(uint16_t);

    for (int i = 0; i < numDeviceIds; ++i)
    {
        if (myDeviceIds[i] == deviceId)
        {
            return THERMOSTAT_PROFILE;
        }
    }

    return NULL;
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool result = false;

    (void) baseDriverUpdatesResource; //unused

    if (ctx == NULL || resource == NULL || endpointNumber == 0 || newValue == NULL)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: endpoint %s: id=%s, previousValue=%s, newValue=%s",
               __FUNCTION__,
               resource->endpointId,
               resource->id,
               previousValue == NULL ? "(null)" : previousValue,
               newValue);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);
    uint8_t endpointId = (uint8_t) endpointNumber;

    if (strcmp(THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT, resource->id) == 0)
    {
        uint16_t newTemp;
        if (!thermostatClusterGetTemperatureValue(newValue, &newTemp))
        {
            result = false;
        }
        else
        {
            result = thermostatClusterSetOccupiedHeatingSetpoint(thermostatCluster, eui64, endpointId, newTemp);
        }
    }
    else if (strcmp(THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT, resource->id) == 0)
    {
        uint16_t newTemp;
        if (!thermostatClusterGetTemperatureValue(newValue, &newTemp))
        {
            result = false;
        }
        else
        {
            result = thermostatClusterSetOccupiedCoolingSetpoint(thermostatCluster, eui64, endpointId, newTemp);
        }
    }
    else if (strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE) == 0)
    {
        uint8_t mode = thermostatClusterGetSystemModeFromString(newValue);

        if (mode == 0xff)
        {
            icLogError(LOG_TAG, "invalid system mode %s", newValue);
        }
        else
        {
            result = thermostatClusterSetSystemMode(thermostatCluster, eui64, endpointId, mode);
        }
    }
    else if (strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_FAN_MODE) == 0)
    {
        uint8_t mode = thermostatClusterGetFanModeFromString(newValue);

        if (mode == 0xff)
        {
            icLogError(LOG_TAG, "invalid fan mode %s", newValue);
        }
        else
        {
            result = fanControlClusterSetFanMode(thermostatCluster, eui64, endpointId, mode);
        }
    }
    else if (strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_HOLD_ON) == 0)
    {
        bool holdOn = strcmp("true", newValue) == 0;

        //For RTCoA thermostats, we dont use the hold attribute on the device, but instead just track
        // it as a simple read/writable resource
        AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *model =
                deviceServiceCallbacks->getResource(resource->deviceUuid,
                                                    NULL,
                                                    COMMON_DEVICE_RESOURCE_MODEL);
        if (model != NULL && model->value != NULL)
        {
            if (isLegacyRtcoaThermostat(model->value) == true)
            {
                result = true;
            }
            else
            {
                result = thermostatClusterSetHold(thermostatCluster, eui64, endpointId, holdOn);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: unable to get device's model", __FUNCTION__);
            result = false;
        }
    }
    else if (strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_CONTROL_SEQ) == 0)
    {
        uint8_t ctrlSeqOp = thermostatClusterGetCtrlSeqOpFromString(newValue);

        if (ctrlSeqOp == 0xff)
        {
            icLogError(LOG_TAG, "invalid control sequence of operation %s", newValue);
        }
        else
        {
            result = thermostatClusterSetCtrlSeqOp(thermostatCluster, eui64, endpointId, ctrlSeqOp);
        }
    }
    else if (strcmp(THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP_CALIBRATION, resource->id) == 0)
    {
        int16_t newCalibration;
        if (!thermostatClusterGetTemperatureValue(newValue, &newCalibration))
        {
            result = false;
        }
        else
        {
            result = thermostatClusterSetLocalTemperatureCalibration(thermostatCluster,
                                                                     eui64,
                                                                     endpointId,
                                                                     newCalibration);
        }
    }

    if (result)
    {
        deviceServiceCallbacks->updateResource(resource->deviceUuid,
                                               resource->endpointId,
                                               resource->id,
                                               newValue,
                                               NULL);
    }

    return result;
}

static bool preConfigureCluster(ZigbeeDriverCommon *ctx,
                                ZigbeeCluster *cluster,
                                DeviceConfigurationContext *deviceConfigContext)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (ctx == NULL || cluster == NULL || deviceConfigContext == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    if(isLegacyThermostat(deviceConfigContext->discoveredDeviceDetails->model))
    {
        if (cluster->clusterId == THERMOSTAT_CLUSTER_ID)
        {
            thermostatClusterSetBindingEnabled(deviceConfigContext, false);
        }
        else if (cluster->clusterId == FAN_CONTROL_CLUSTER_ID)
        {
            fanControlClusterSetBindingEnabled(deviceConfigContext, false);
        }
        else if (cluster->clusterId == POLL_CONTROL_CLUSTER_ID)
        {
            pollControlClusterSetBindingEnabled(deviceConfigContext, false);
        }
        else if (cluster->clusterId == ALARMS_CLUSTER_ID)
        {
            alarmsClusterSetBindingEnabled(deviceConfigContext, false);
        }
        else if (cluster->clusterId == POWER_CONFIGURATION_CLUSTER_ID)
        {
            powerConfigurationClusterSetBindingEnabled(deviceConfigContext, false);

            powerConfigurationClusterSetConfigureBatteryAlarmMask(deviceConfigContext, true);
        }
    }

    if (cluster->clusterId == POLL_CONTROL_CLUSTER_ID)
    {
        char qs[6]; //32767 + \0 worst case
        snprintf(qs, sizeof(qs), "%"PRIu16, REGULAR_POLL_RATE_QS);
        //set the long poll interval to 54 quarter seconds, which is 14 seconds.
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata,
                LONG_POLL_INTERVAL_QS_METADATA, qs);
    }

    return true;
}

static void localTemperatureChanged(uint64_t eui64,
                                    uint8_t endpointId,
                                    int16_t temp,
                                    const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *tempStr = thermostatClusterGetTemperatureString(temp);
    if (tempStr != NULL)
    {
        deviceServiceCallbacks->updateResource(uuid,
                                               epName,
                                               THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP,
                                               tempStr,
                                               NULL);
    }
    free(tempStr);
    free(uuid);
}

static void occupiedHeatingSetpointChanged(uint64_t eui64,
                                           uint8_t endpointId,
                                           int16_t temp,
                                           const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *tempStr = thermostatClusterGetTemperatureString(temp);
    if (tempStr != NULL)
    {
        deviceServiceCallbacks->updateResource(uuid,
                                               epName,
                                               THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT,
                                               tempStr,
                                               NULL);
    }
    free(tempStr);
    free(uuid);
}

static void occupiedCoolingSetpointChanged(uint64_t eui64,
                                           uint8_t endpointId,
                                           int16_t temp,
                                           const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *tempStr = thermostatClusterGetTemperatureString(temp);
    if (tempStr != NULL)
    {
        deviceServiceCallbacks->updateResource(uuid,
                                               epName,
                                               THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT,
                                               tempStr,
                                               NULL);
    }
    free(tempStr);
    free(uuid);
}

static void systemModeChanged(uint64_t eui64,
                              uint8_t endpointId,
                              uint8_t mode,
                              const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE,
                                           thermostatClusterGetSystemModeString(mode),
                                           NULL);
    free(uuid);
}

static void runningStateChanged(uint64_t eui64,
                                uint8_t endpointId,
                                uint16_t state,
                                const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON,
                                           thermostatClusterIsSystemOn(state) ? "true" : "false",
                                           NULL);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_FAN_ON,
                                           thermostatClusterIsFanOn(state) ? "true" : "false",
                                           NULL);
    free(uuid);
}

static void setpointHoldChanged(uint64_t eui64,
                                uint8_t endpointId,
                                bool holdOn,
                                const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    //For RTCoA thermostats, we dont use the hold attribute on the device, but instead just track
    // it as a simple read/writable resource
    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *model =
            deviceServiceCallbacks->getResource(uuid,
                                                NULL,
                                                COMMON_DEVICE_RESOURCE_MODEL);
    if (model != NULL && model->value != NULL)
    {
        //just ignore this change for rtcoa
        if (isLegacyRtcoaThermostat(model->value) == false)
        {
            deviceServiceCallbacks->updateResource(uuid,
                                                   epName,
                                                   THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                                   holdOn ? "true" : "false",
                                                   NULL);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: unable to get device's model", __FUNCTION__);
    }

    free(uuid);
}

static void ctrlSeqOpChanged(uint64_t eui64,
                             uint8_t endpointId,
                             uint8_t ctrlSeqOp,
                             const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_CONTROL_SEQ,
                                           thermostatClusterGetCtrlSeqOpString(ctrlSeqOp),
                                           NULL);
    free(uuid);
}

static void fanModeChanged(uint64_t eui64,
                           uint8_t endpointId,
                           uint8_t mode,
                           const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_FAN_MODE,
                                           fanControlClusterGetFanModeString(mode),
                                           NULL);
    free(uuid);
}

static void legacyOperationInfoReceived(uint64_t eui64,
                                        uint8_t endpointId,
                                        uint8_t runningMode,      // 0=off, 1=heat, 2=cool
                                        bool holdOn,
                                        uint8_t runningState,     // 0=off, 1=heat, 2=cool, 0xff=not used
                                        uint8_t fanRunningState)  // 0=off, 1=running, 0xff=not used
{
    icLogDebug(LOG_TAG, "%s: runningMode=%d, holdOn=%s, runningState=%d, fanRunningState=%d",
            __FUNCTION__,
            runningMode,
            holdOn ? "true" : "false",
            runningState,
            fanRunningState);

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);

    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON,
                                           (runningState == 1 || runningState == 2) ? "true" : "false",
                                           NULL);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           THERMOSTAT_PROFILE_RESOURCE_FAN_ON,
                                           (fanRunningState == 1) ? "true" : "false",
                                           NULL);

    //RTCoA thermostats should ignore the holdOn field
    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *model =
            deviceServiceCallbacks->getResource(uuid,
                                                NULL,
                                                COMMON_DEVICE_RESOURCE_MODEL);
    if (model != NULL && model->value != NULL)
    {
        if (isLegacyRtcoaThermostat(model->value) == false)
        {
            deviceServiceCallbacks->updateResource(uuid,
                                                   epName,
                                                   THERMOSTAT_PROFILE_RESOURCE_HOLD_ON,
                                                   holdOn ? "true" : "false",
                                                   NULL);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: could not get model", __FUNCTION__);
    }
}

static void localTemperatureCalibrationChanged(uint64_t eui64,
                                               uint8_t endpointId,
                                               int8_t calibrationTemp,
                                               const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *tempStr = thermostatClusterGetTemperatureString(calibrationTemp);
    if (tempStr != NULL)
    {
        deviceServiceCallbacks->updateResource(uuid,
                                               epName,
                                               THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP_CALIBRATION,
                                               tempStr,
                                               NULL);
    }
    free(tempStr);
    free(uuid);
}

//since we have already claimed it, the check can simply be on the model name
static bool isLegacyRtcoaThermostat(const char *model)
{
    return strcmp(model, RTCOA_MODEL_NAME) == 0;
}

//since we have already claimed it, the check can simply be on the model name
static bool isLegacyCentraliteThermostat(const char *model)
{
    return strcmp(model, CENTRALITE_MODEL_NAME) == 0;
}

static bool isLegacyThermostat(const char *model)
{
    return isLegacyRtcoaThermostat(model) || isLegacyCentraliteThermostat(model);
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE



