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

#include <xbb.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include <deviceService/deviceService_event.h>
#include <deviceService/deviceService_ipc.h>
#include <deviceService/deviceService_eventAdapter.h>
#include <deviceHelper.h>
#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <xbb.h>

#define XBB_ENDPOINT_ID "1"

#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(...) do{ fprintf( stdout, __VA_ARGS__ ); } while( false )
#else
#define DEBUG_PRINT(...) do{ } while ( false )
#endif


static char *deviceId = NULL;
static bool initialized = false;

static void deviceAddedEventHandler(DeviceServiceDeviceAddedEvent *event)
{
    DEBUG_PRINT("device added! deviceId=%s, uri=%s, deviceClass=%s, deviceClassVersion=%d\n",
           event->details->deviceId,
           event->details->uri,
           event->details->deviceClass,
           event->details->deviceClassVersion);

    if(strcmp("xbb", event->details->deviceClass) != 0)
    {
        DEBUG_PRINT("the device that was added was not a battery! Ignoring\n");
        return;
    }

    if(deviceId != NULL)
    {
        DEBUG_PRINT("We already had a battery registered!  Discarding and using the new one...\n");
        free(deviceId);
        deviceId = NULL;
    }

    deviceId = strdup(event->details->deviceId);
}

static void deviceRemovedEventHandler(DeviceServiceDeviceRemovedEvent *event)
{
    DEBUG_PRINT("device removed! deviceId=%s, deviceClass=%s\n", event->deviceId, event->deviceClass);

    if(deviceId != NULL)
    {
        if(strcmp(deviceId, event->deviceId) == 0)
        {
            free(deviceId);
            deviceId = NULL;
        }
        else
        {
            DEBUG_PRINT("The device that was removed wasn't a battery\n");
        }
    }
    else
    {
        DEBUG_PRINT("we didn't have a battery registered yet... how could it have been removed?\n");
    }
}

static void registerEventHandlers()
{
    register_DeviceServiceDeviceAddedEvent_eventListener(deviceAddedEventHandler);
    register_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedEventHandler);
}

static void unregisterEventHandlers()
{
    unregister_DeviceServiceDeviceAddedEvent_eventListener(deviceAddedEventHandler);
    unregister_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedEventHandler);
}

static void initialize()
{
    if(initialized)
    {
        return;
    }

    // init logging subsystem if necessary
    //
    initIcLogger();

    setIcLogPriorityFilter(IC_LOG_NONE);

    bool isBatteryPaired;
    if(xbbIsBatteryPaired(&isBatteryPaired))
    {
        if(isBatteryPaired)
        {
            DEBUG_PRINT("Found already paired battery %s\n", deviceId);
        }
        else
        {
            DEBUG_PRINT("No previously paired battery detected.\n");
        }

        registerEventHandlers();

        initialized = true;
    }
    else
    {
        DEBUG_PRINT("Error locating an already paired battery\n");
    }
}

static bool verifyBatteryPaired()
{
    return deviceId != NULL;
}

//This will have the side-effect of saving off the deviceId
bool xbbIsBatteryPaired(bool *isPaired)
{
    bool result = false;

    if (isPaired == NULL)
    {
        DEBUG_PRINT("xbbIsBatteryPaired: invalid arguments\n");
        return false;
    }

    *isPaired = false;

    // init logging subsystem if necessary
    //
    initIcLogger();

    setIcLogPriorityFilter(IC_LOG_NONE);

    DSDeviceList *output = create_DSDeviceList();
    if(deviceService_request_GET_DEVICES_BY_DEVICE_CLASS("xbb", output) == IPC_SUCCESS &&
       output->devices != NULL)
    {
        if(linkedListCount(output->devices) > 0)
        {
            DSDevice *device = (DSDevice*)linkedListGetElementAt(output->devices, 0);
            deviceId = strdup(device->id);
            *isPaired = true;
        }
        else
        {
            *isPaired = false;
        }

        result = true;
    }
    destroy_DSDeviceList(output);

    return result;
}

static bool batteryInCommFail()
{
    bool result = false;

    char *value = NULL;
    if(deviceHelperReadDeviceResource(deviceId, COMMON_DEVICE_RESOURCE_COMM_FAIL, &value))
    {
        if (value != NULL && strncasecmp("true", value, 4) == 0)
        {
            result = true;
        }
    }

    free(value);

    return result;
}

bool xbbGetStatus(XbbStatus *status)
{
    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbGetStatus: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbGetStatus: battery is in communication failure\n");
        return false;
    }

    if(status == NULL)
    {
        DEBUG_PRINT("xbbGetStatus: invalid argument\n");
        return false;
    }

    memset(status, 0, sizeof(XbbStatus));

    char *value;
    if(deviceHelperReadDeviceResource(deviceId, COMMON_DEVICE_RESOURCE_MANUFACTURER, &value))
    {
        strncpy(status->manufacturer, value, MAX_MANUFACTURER_LEN-1);
        free(value);
    }

    if(deviceHelperReadDeviceResource(deviceId, COMMON_DEVICE_RESOURCE_MODEL, &value))
    {
        strncpy(status->model, value, MAX_MODEL_LEN-1);
        free(value);
    }

    if(deviceHelperReadDeviceResource(deviceId, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, &value))
    {
        sscanf(value, "%" SCNu8, &status->hardwareVersion);
        free(value);
    }

    if(deviceHelperReadDeviceResource(deviceId, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, &value))
    {
        sscanf(value, "%" SCNx64, &status->firmwareVersion);
        free(value);
    }

    if(deviceHelperReadEndpointResource(deviceId, XBB_ENDPOINT_ID, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER, &value))
    {
        strncpy(status->serialNumber, value, MAX_SERIAL_NUMBER_LEN-1);
        free(value);
    }

    if(deviceHelperReadEndpointResource(deviceId, XBB_ENDPOINT_ID, "status", &value))
    {
        cJSON *json = cJSON_Parse(value);
        cJSON *item;

        if((item = cJSON_GetObjectItem(json, "BatteryStatus")) != NULL)
        {
            status->batteryStatus = (BatteryStatus) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve BatteryStatus\n");
        }

        if((item = cJSON_GetObjectItem(json, "BatteryHealth")) != NULL)
        {
            status->batteryHealth = (BatteryHealth) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve BatteryHealth\n");
        }

        if((item = cJSON_GetObjectItem(json, "ChargingStatus")) != NULL)
        {
            status->isCharging = item->valueint != 0;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve ChargingStatus\n");
        }

        if((item = cJSON_GetObjectItem(json, "TestingStatus")) != NULL)
        {
            status->testingState = (BatteryTestingState) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve TestingStatus\n");
        }

        if((item = cJSON_GetObjectItem(json, "TestingState")) != NULL)
        {
            status->isUnderTest = item->valueint != 0;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve TestingState\n");
        }

        if((item = cJSON_GetObjectItem(json, "ChargingSystemHealth")) != NULL)
        {
            status->chargingSystemHealth = (BatteryChargingSystemHealth) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve ChargingSystemHealth\n");
        }

        if((item = cJSON_GetObjectItem(json, "SecondsOnBattery")) != NULL)
        {
            status->secondsOnBattery = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve SecondsOnBattery\n");
        }

        if((item = cJSON_GetObjectItem(json, "EstimatedMinutesRemaining")) != NULL)
        {
            status->estimatedMinutesRemaining = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve EstimatedMinutesRemaining\n");
        }

        if((item = cJSON_GetObjectItem(json, "EstimatedChargeRemaining")) != NULL)
        {
            status->estimatedChargeRemainingPercent = (uint8_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve EstimatedChargeRemaining\n");
        }

        if((item = cJSON_GetObjectItem(json, "CurrentTemperature")) != NULL)
        {
            status->currentTemperature = (int16_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve CurrentTemperature\n");
        }

        if((item = cJSON_GetObjectItem(json, "MinTempExperienced")) != NULL)
        {
            status->minTempExperienced = (int16_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve MinTempExperienced\n");
        }

        if((item = cJSON_GetObjectItem(json, "MaxTempExperienced")) != NULL)
        {
            status->maxTempExperienced = (int16_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve MaxTempExperienced\n");
        }

        if((item = cJSON_GetObjectItem(json, "AlarmCount")) != NULL)
        {
            status->hasAlarms = item->valueint > 0;
        }
        else
        {
            DEBUG_PRINT("xbbGetStatus: Unable to retrieve AlarmCount\n");
        }

        if((item = cJSON_GetObjectItem(json, "VendorSpecific")) != NULL)
        {
            strncpy(status->vendorSpecificData, item->valuestring, MAX_VENDOR_SPECIFIC_DATA_LEN);
        }

        cJSON_Delete(json);
        free(value);
    }
    else
    {
        DEBUG_PRINT("xbbGetStatus: failed\n");
        return false;
    }

    return true;
}

bool xbbGetConfig(XbbConfiguration *config)
{
    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbGetConfig: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbGetConfig: battery is in communication failure\n");
        return false;
    }

    if(config == NULL)
    {
        DEBUG_PRINT("xbbGetConfig: invalid argument\n");
        return false;
    }

    memset(config, 0, sizeof(XbbConfiguration));

    char *value;
    if(deviceHelperReadEndpointResource(deviceId, XBB_ENDPOINT_ID, "config", &value))
    {
        cJSON *json = cJSON_Parse(value);
        cJSON *item;

        if((item = cJSON_GetObjectItem(json, "PoweredDeviceIdlePower1")) != NULL)
        {
            config->poweredDeviceIdlePower1 = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve PoweredDeviceIdlePower1\n");
        }

        if((item = cJSON_GetObjectItem(json, "PoweredDeviceIdlePower2")) != NULL)
        {
            config->poweredDeviceIdlePower2 = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve PoweredDeviceIdlePower2\n");
        }

        if((item = cJSON_GetObjectItem(json, "ConfigLowBatteryTime")) != NULL)
        {
            config->configLowBatteryMinutes = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve ConfigLowBatteryTime\n");
        }

        if((item = cJSON_GetObjectItem(json, "LowTempThreshold")) != NULL)
        {
            config->lowTempThreshold = (int16_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve LowTempThreshold\n");
        }

        if((item = cJSON_GetObjectItem(json, "HighTempThreshold")) != NULL)
        {
            config->highTempThreshold = (int16_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve HighTempThreshold\n");
        }

        if((item = cJSON_GetObjectItem(json, "LowTempDwellTripPoint")) != NULL)
        {
            config->lowTempDwellTripPointSeconds = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve LowTempDwellTripPoint\n");
        }

        if((item = cJSON_GetObjectItem(json, "HighTempDwellTripPoint")) != NULL)
        {
            config->highTempDwellTripPointSeconds = (uint32_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve HighTempDwellTripPoint\n");
        }

        if((item = cJSON_GetObjectItem(json, "DeviceTempAlarmMask")) != NULL)
        {
            config->deviceTempAlarmMask = (uint8_t) item->valueint;
        }
        else
        {
            DEBUG_PRINT("xbbGetConfig: Unable to retrieve DeviceTempAlarmMask\n");
        }

        return true;
    }
    else
    {
        return false;
    }
}

bool xbbSetConfig(XbbConfiguration *config)
{
    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbSetConfig: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbSetConfig: battery is in communication failure\n");
        return false;
    }

    if(config == NULL)
    {
        DEBUG_PRINT("xbbSetConfig: invalid argument\n");
        return false;
    }

    cJSON *json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "PoweredDeviceIdlePower1", config->poweredDeviceIdlePower1);
    cJSON_AddNumberToObject(json, "PoweredDeviceIdlePower2", config->poweredDeviceIdlePower2);
    cJSON_AddNumberToObject(json, "ConfigLowBatteryTime", config->configLowBatteryMinutes);
    cJSON_AddNumberToObject(json, "LowTempThreshold", config->lowTempThreshold);
    cJSON_AddNumberToObject(json, "HighTempThreshold", config->highTempThreshold);
    cJSON_AddNumberToObject(json, "LowTempDwellTripPoint", config->lowTempDwellTripPointSeconds);
    cJSON_AddNumberToObject(json, "HighTempDwellTripPoint", config->highTempDwellTripPointSeconds);
    cJSON_AddNumberToObject(json, "DeviceTempAlarmMask", config->deviceTempAlarmMask);

    char *configStr = cJSON_Print(json);
    if(deviceHelperWriteEndpointResource(deviceId, XBB_ENDPOINT_ID, "config", configStr) == false)
    {
        DEBUG_PRINT("xbbSetConfig: failed to write config resource\n");
    }
    free(configStr);
    cJSON_Delete(json);

    return true;
}

bool xbbDiscover(uint16_t timeoutSeconds)
{
    bool result = false;

    initialize();

    if(timeoutSeconds == 0)
    {
        DEBUG_PRINT("xbbDiscover: invalid argument\n");
        return false;
    }

    if(deviceId != NULL)
    {
        //removing the battery will also trigger rediscovery
        bool output;
        if(deviceService_request_REMOVE_DEVICE(deviceId, &output) == IPC_SUCCESS)
        {
            result = output;
        }
    }
    else
    {
        DSDiscoverDevicesByClassRequest *request = create_DSDiscoverDevicesByClassRequest();
        request->deviceClass = strdup("xbb");
        request->timeoutSeconds = timeoutSeconds;
        bool output;
        if(deviceService_request_DISCOVER_DEVICES_BY_CLASS(request, &output) == IPC_SUCCESS)
        {
            result = output;
        }
        destroy_DSDiscoverDevicesByClassRequest(request);
    }

    return result;
}

bool xbbGetAlarms(XbbAlarmInfo **alarmInfos, uint16_t *numAlarmInfos)
{
    bool result = false;

    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbGetAlarms: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbGetAlarms: battery is in communication failure\n");
        return false;
    }

    if(alarmInfos == NULL || numAlarmInfos == NULL)
    {
        DEBUG_PRINT("xbbGetAlarms: invalid argument\n");
        return false;
    }

    *alarmInfos = NULL;
    *numAlarmInfos = 0;

    char *value = NULL;
    if(deviceHelperReadEndpointResource(deviceId, XBB_ENDPOINT_ID, "alarms", &value) && value != NULL)
    {
        cJSON *json = cJSON_Parse(value);
        *numAlarmInfos = (uint16_t) cJSON_GetArraySize(json);
        *alarmInfos = (XbbAlarmInfo*)calloc(*numAlarmInfos, sizeof(XbbAlarmInfo));
        for(int i = 0; i < *numAlarmInfos; i++)
        {
            cJSON *entryJson = cJSON_GetArrayItem(json, i);
            if(entryJson != NULL)
            {
                cJSON *tmpJson = cJSON_GetObjectItem(entryJson, "type");
                if(strcmp(tmpJson->valuestring, "badBattery") == 0)
                {
                    (*alarmInfos)[i].type = alarmTypeBatteryBad;
                }
                else if(strcmp(tmpJson->valuestring, "lowBattery") == 0)
                {
                    (*alarmInfos)[i].type = alarmTypeBatteryLow;
                }
                else if(strcmp(tmpJson->valuestring, "chargingSystemBad") == 0)
                {
                    (*alarmInfos)[i].type = alarmTypeChargingSystemBad;
                }
                else if(strcmp(tmpJson->valuestring, "missingBattery") == 0)
                {
                    (*alarmInfos)[i].type = alarmTypeBatteryMissing;
                }
                else if(strcmp(tmpJson->valuestring, "lowTemp") == 0)
                {
                    (*alarmInfos)[i].type = alarmTypeLowTemp;
                }
                else if(strcmp(tmpJson->valuestring, "highTemp") == 0)
                {
                    (*alarmInfos)[i].type = alarmTypeHighTemp;
                }

                tmpJson = cJSON_GetObjectItem(entryJson, "timestamp");
                (*alarmInfos)[i].timestamp = (uint32_t) tmpJson->valueint;
            }
        }

        cJSON_Delete(json);
        result = true;
    }

    return result;
}

bool xbbSirenStart( uint16_t frequency,
                    uint8_t volumePercent,
                    uint16_t durationSeconds,
                    SirenTemporalPattern temporalPattern,
                    uint8_t numPulses,
                    uint16_t onPhaseDurationMillis,
                    uint16_t offPhaseDurationMillis,
                    uint16_t pauseDurationMillis)
{
    bool result = false;

    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbSirenStart: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbSirenStart: battery is in communication failure\n");
        return false;
    }

    cJSON *json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "Frequency", frequency);
    cJSON_AddNumberToObject(json, "Volume", volumePercent);
    cJSON_AddNumberToObject(json, "Duration", durationSeconds);

    char *pattern;
    switch(temporalPattern)
    {
        case sirenTemporalPatternNone:
            pattern = "none";
            break;
        case sirenTemporalPattern3:
            pattern = "3";
            break;
        case sirenTemporalPattern4:
            pattern = "4";
            break;
        case sirenTemporalPatternUserDefined:
            pattern = "user";
            break;

        default:
            pattern = "unknown";
            break;

    }
    cJSON_AddStringToObject(json, "TemporalPattern", pattern);

    cJSON_AddNumberToObject(json, "NumPulses", numPulses);
    cJSON_AddNumberToObject(json, "OnPhaseDuration", onPhaseDurationMillis);
    cJSON_AddNumberToObject(json, "OffPhaseDuration", offPhaseDurationMillis);
    cJSON_AddNumberToObject(json, "PauseDuration", pauseDurationMillis);

    DSExecuteResourceRequest *request = create_DSExecuteResourceRequest();
    DSExecuteResourceResponse *response = create_DSExecuteResourceResponse();

    request->arg = cJSON_Print(json);
    request->uri = createEndpointResourceUri(deviceId, XBB_ENDPOINT_ID, "sirenStart");

    if(deviceService_request_EXECUTE_RESOURCE(request, response) == IPC_SUCCESS && response->success == true)
    {
        result = true;
    }

    destroy_DSExecuteResourceRequest(request);
    destroy_DSExecuteResourceResponse(response);

    return result;
}

bool xbbSirenStop()
{
    bool result = false;

    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbSirenStop: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbSirenStop: battery is in communication failure\n");
        return false;
    }

    DSExecuteResourceRequest *request = create_DSExecuteResourceRequest();
    DSExecuteResourceResponse *response = create_DSExecuteResourceResponse();

    request->uri = createEndpointResourceUri(deviceId, XBB_ENDPOINT_ID, "sirenStop");

    if(deviceService_request_EXECUTE_RESOURCE(request, response) == IPC_SUCCESS && response->success == true)
    {
        result = true;
    }

    destroy_DSExecuteResourceRequest(request);
    destroy_DSExecuteResourceResponse(response);

    return result;
}

bool xbbSirenMute()
{
    bool result = false;

    initialize();

    if(!verifyBatteryPaired())
    {
        DEBUG_PRINT("xbbSirenMute: battery not paired\n");
        return false;
    }

    // Short circuit read attempts if the battery is in comm fail
    if(batteryInCommFail())
    {
        DEBUG_PRINT("xbbSirenMute: battery is in communication failure\n");
        return false;
    }

    DSExecuteResourceRequest *request = create_DSExecuteResourceRequest();
    DSExecuteResourceResponse *response = create_DSExecuteResourceResponse();

    request->uri = createEndpointResourceUri(deviceId, XBB_ENDPOINT_ID, "sirenMute");

    if(deviceService_request_EXECUTE_RESOURCE(request, response) == IPC_SUCCESS && response->success == true)
    {
        result = true;
    }

    destroy_DSExecuteResourceRequest(request);
    destroy_DSExecuteResourceResponse(response);

    return result;
}
