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
* philipsHue.c
*
* Author: tlea
*-----------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include <ssdp/ssdp.h>
#include <icLog/logging.h>
#include <pthread.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <rtcoaWifi/rtcoaWifi.h>
#include <math.h>
#include <icBuildtime.h>

#define LOG_TAG "RTCoAWifiLib"

#define MONITOR_INTERVAL_SECS 5
#define RECOVERY_TIMEOUT_SECONDS 10

static void thermostatDiscoveredCallback(SsdpDevice *device);

static pthread_mutex_t discoverMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    pthread_t thread;
    bool running;
    char *macAddress;
    char *ipAddress;
    RtcoaWifiThermostatStateChangedCallback stateChangedCallback;
    RtcoaWifiThermostatIpChangedCallback ipChangedCallback;
} ThermostatMonitoringInfo;

static icHashMap *monitors = NULL;
static pthread_mutex_t monitorsMutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t ssdpHandle = 0;

static RtcoaWifiThermostatDiscoverCallback clientCallback = NULL;

bool rtcoaWifiThermostatStartDiscovery(RtcoaWifiThermostatDiscoverCallback callback)
{
    bool result = false;

    pthread_mutex_lock(&discoverMutex);
    if (clientCallback == NULL)
    {
        clientCallback = callback;
        ssdpHandle = ssdpDiscoverStart(RTCOA, thermostatDiscoveredCallback);
        if (ssdpHandle != 0)
        {
            result = true;
        }

        if (result == false)
        {
            clientCallback = NULL;
        }
    }
    pthread_mutex_unlock(&discoverMutex);

    return result;
}

bool rtcoaWifiThermostatStopDiscovery()
{
    ssdpDiscoverStop(ssdpHandle);
    ssdpHandle = 0;
    pthread_mutex_lock(&discoverMutex);
    clientCallback = NULL;
    pthread_mutex_unlock(&discoverMutex);
    return true;
}


static size_t readCallback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    memcpy(ptr, userp, size * nmemb);
    return size * nmemb;
}

struct MemoryBlock
{
    char *memory;
    size_t size;
};

static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryBlock *mem = (struct MemoryBlock *) userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL)
    {
        icLogError(LOG_TAG, "Not enough memory in %s (realloc returned NULL) . . .", __func__);
        return (size_t) -1;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static void thermostatDiscoveredCallback(SsdpDevice *device)
{
    icLogInfo(LOG_TAG, "Thermostat found: ip=%s, url=%s", device->ipAddress, device->upnpURL);

    pthread_mutex_lock(&discoverMutex);
    if (clientCallback != NULL)
    {
        clientCallback(device->macAddress, device->ipAddress);
    }
    pthread_mutex_unlock(&discoverMutex);
}

RtcoaWifiThermostatState* rtcoaWifiThermostatStateGetState(const char *ipAddress)
{
    RtcoaWifiThermostatState *result = NULL;

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        struct MemoryBlock responseBlock;
        responseBlock.memory = malloc(1);
        responseBlock.size = 0;

        char url[128];
        sprintf(url, "http://%s/tstat", ipAddress);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &responseBlock);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(responseBlock.memory);
            return NULL;
        }

        curl_easy_cleanup(curl);

        icLogDebug(LOG_TAG, "got response %s", responseBlock.memory);

        cJSON *responseObj = cJSON_Parse(responseBlock.memory);

        if(responseObj != NULL)
        {
            result = (RtcoaWifiThermostatState*)calloc(1, sizeof(RtcoaWifiThermostatState));
            cJSON *tmp;

            if((tmp = cJSON_GetObjectItem(responseObj, "temp")) != NULL)
            {
                result->temp = (float) tmp->valuedouble;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "t_cool")) != NULL)
            {
                result->t_cool = (float) tmp->valuedouble;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "t_heat")) != NULL)
            {
                result->t_heat = (float) tmp->valuedouble;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "tmode")) != NULL)
            {
                result->tmode = tmp->valueint;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "fmode")) != NULL)
            {
                result->fmode = tmp->valueint;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "override")) != NULL)
            {
                result->override = tmp->valueint;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "hold")) != NULL)
            {
                result->hold = tmp->valueint;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "program_mode")) != NULL)
            {
                result->program_mode = tmp->valueint;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "tstate")) != NULL)
            {
                result->tstate = tmp->valueint;
            }

            if((tmp = cJSON_GetObjectItem(responseObj, "fstate")) != NULL)
            {
                result->fstate = tmp->valueint;
            }

            cJSON_Delete(responseObj);
        }

        free(responseBlock.memory);
    }

    return result;
}

static bool postTstatRequest(const char *ipAddress, const char *urlPattern, const char *body)
{
    bool result = false;
    CURL *curl;
    CURLcode res;

    char url[128];
    sprintf(url, urlPattern, ipAddress);

    icLogDebug(LOG_TAG, "posting '%s' to %s", body, url);

    curl = curl_easy_init();
    if (curl)
    {
        struct curl_slist *header = NULL;

        header = curl_slist_append(header, "Accept: application/json");
        header = curl_slist_append(header, "Content-Type: application/json");
        header = curl_slist_append(header, "charsets: utf-8");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_slist_free_all(header);
            return false;
        }
        else
        {
            result = true;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(header);
    }

    return result;
}

char* rtcoaWifiThermostatGetModel(const char *ipAddress)
{
    char *result = NULL;

    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        struct MemoryBlock responseBlock;
        responseBlock.memory = malloc(1);
        responseBlock.size = 0;

        char url[128];
        sprintf(url, "http://%s/tstat/model", ipAddress);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &responseBlock);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(responseBlock.memory);
            return NULL;
        }

        curl_easy_cleanup(curl);

        icLogDebug(LOG_TAG, "got response %s", responseBlock.memory);

        cJSON *responseObj = cJSON_Parse(responseBlock.memory);

        if(responseObj != NULL)
        {
            cJSON *modelObj = cJSON_GetObjectItem(responseObj, "Model");
            if(modelObj != NULL && modelObj->valuestring != NULL)
            {
                result = strdup(modelObj->valuestring);
            }
            cJSON_Delete(responseObj);
        }

        free(responseBlock.memory);
    }

    return result;
}

bool rtcoaWifiThermostatSetMode(const char *ipAddress, RtcoaWifiThermostatOperatingMode mode)
{
    bool result = false;

    char *body = NULL;

    switch(mode)
    {
        case RTCOA_WIFI_TSTAT_OP_MODE_OFF:
            body = strdup("{\"tmode\":0}\r\n");
            break;

        case RTCOA_WIFI_TSTAT_OP_MODE_HEAT:
            body = strdup("{\"tmode\":1}\r\n");
            break;

        case RTCOA_WIFI_TSTAT_OP_MODE_COOL:
            body = strdup("{\"tmode\":2}\r\n");
            break;

        default:
            icLogError(LOG_TAG, "rtcoaWifiThermostatSetMode: invalid mode %d", mode);
            return false;
    }

    result = postTstatRequest(ipAddress, "http://%s/tstat", body);

    free(body);

    return result;
}

void rtcoaWifiThermostatStateDestroyState(RtcoaWifiThermostatState *thermostatState)
{
    free(thermostatState);
}

static void destroyThermostatMonitoringInfo(ThermostatMonitoringInfo *info)
{
    if(info->macAddress != NULL)
    {
        free(info->macAddress);
    }
    if(info->ipAddress != NULL)
    {
        free(info->ipAddress);
    }

    free(info);
}

static void *thermostatMonitoringThreadProc(void *arg)
{
    ThermostatMonitoringInfo *info = (ThermostatMonitoringInfo*)arg;

    RtcoaWifiThermostatState *previousState = (RtcoaWifiThermostatState*)calloc(1, sizeof(RtcoaWifiThermostatState));

    while(info->running == true)
    {
        usleep(MONITOR_INTERVAL_SECS * 1000000);

        RtcoaWifiThermostatState *tstatState = rtcoaWifiThermostatStateGetState(info->ipAddress);
        if(tstatState != NULL)
        {
            //compare this state with the previous to see if anything changed.
            if (memcmp(previousState, tstatState, sizeof(RtcoaWifiThermostatState)) != 0)
            {
                info->stateChangedCallback(info->macAddress, info->ipAddress);
                memcpy(previousState, tstatState, sizeof(RtcoaWifiThermostatState));
            }

            rtcoaWifiThermostatStateDestroyState(tstatState);
        }
        else
        {
            // we failed to get any lights from the device.  That most likely means its IP address changed and we have
            // to find it again
            icLogInfo(LOG_TAG,
                      "thermostatMonitoringThreadProc: failed to get state from %s, its ip address probably changed from %s... attempting recovery",
                      info->macAddress, info->ipAddress);
            char *recoveredIpAddress = NULL;
            if (ssdpRecoverIpAddress(RTCOA, info->macAddress, &recoveredIpAddress, RECOVERY_TIMEOUT_SECONDS) == true)
            {
                icLogInfo(LOG_TAG, "thermostatMonitoringThreadProc: found %s at %s", info->macAddress, recoveredIpAddress);
                info->ipChangedCallback(info->macAddress, recoveredIpAddress);
                free(info->ipAddress);
                info->ipAddress = recoveredIpAddress;
            }
        }
    }

    rtcoaWifiThermostatStateDestroyState(previousState);

    return NULL;
}

bool rtcoaWifiThermostatStartMonitoring(const char *macAddress,
                                             const char *ipAddress,
                                             RtcoaWifiThermostatStateChangedCallback stateChangedCallback,
                                             RtcoaWifiThermostatIpChangedCallback ipChangedCallback)
{
    bool result = true;

    icLogInfo(LOG_TAG, "Monitoring of the thermostat %s at %s starting", macAddress, ipAddress);

    ThermostatMonitoringInfo *info = (ThermostatMonitoringInfo*)calloc(1, sizeof(ThermostatMonitoringInfo));
    info->macAddress = strdup(macAddress);
    info->ipAddress = strdup(ipAddress);
    info->stateChangedCallback = stateChangedCallback;
    info->ipChangedCallback = ipChangedCallback;
    info->running = true;

    pthread_mutex_lock(&monitorsMutex);

    if(monitors == NULL)
    {
        monitors = hashMapCreate();
    }
    else
    {
        if(hashMapGet(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1)) != NULL)
        {
            icLogError(LOG_TAG, "duplicate attempt to watch thermostat at %s ignored", ipAddress);
            result = false;
        }
    }

    if(result == true)
    {
        hashMapPut(monitors, info->ipAddress, (uint16_t) (strlen(ipAddress) + 1), info);
        pthread_create(&info->thread, NULL, thermostatMonitoringThreadProc, info);
    }
    else
    {
        destroyThermostatMonitoringInfo(info);
    }

    pthread_mutex_unlock(&monitorsMutex);

    return result;
}

bool rtcoaWifiThermostatStopMonitoring(const char *ipAddress)
{
    bool result = false;

    icLogInfo(LOG_TAG, "Monitoring of the bridge at %s stopping", ipAddress);

    if(ipAddress == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&monitorsMutex);
    if(monitors != NULL)
    {
        ThermostatMonitoringInfo *info = (ThermostatMonitoringInfo*)hashMapGet(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1));
        if(info != NULL)
        {
            info->running = false;
            pthread_join(info->thread, NULL);
            hashMapDelete(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1), standardDoNotFreeHashMapFunc);
            destroyThermostatMonitoringInfo(info);
        }

        if(hashMapCount(monitors) == 0)
        {
            hashMapDestroy(monitors, standardDoNotFreeHashMapFunc);
            monitors = NULL;
        }
    }
    pthread_mutex_unlock(&monitorsMutex);

    return result;
}

bool rtcoaWifiThermostatSetCoolSetpoint(const char *ipAddress, float newTemp)
{
    bool result = true;
    char *body = (char*)calloc(1, 18); // {"a_cool":xxx}\0
    sprintf(body, "{\"a_cool\":%3.0f}\r\n", roundf(newTemp));
    result = postTstatRequest(ipAddress, "http://%s/tstat", body);
    free(body);
    return result;
}

bool rtcoaWifiThermostatSetHeatSetpoint(const char *ipAddress, float newTemp)
{
    bool result = true;
    char *body = (char*)calloc(1, 18); // {"a_heat":xxx}\0
    sprintf(body, "{\"a_heat\":%3.0f}\r\n", roundf(newTemp));
    result = postTstatRequest(ipAddress, "http://%s/tstat", body);
    free(body);
    return result;
}

bool rtcoaWifiThermostatSetSimpleMode(const char *ipAddress, bool enabled)
{
    bool result = true;
    result = postTstatRequest(ipAddress, "http://%s/tstat/simple_mode", (enabled == true) ? "{\"simple_mode\":2}\r\n" : "{\"simple_mode\":1}\r\n");
    return result;
}
