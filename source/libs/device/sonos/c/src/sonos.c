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
* sonos.c
*
* Author: tlea
*-----------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include <sonos/sonos.h>
#include <ssdp/ssdp.h>
#include <icLog/logging.h>
#include <pthread.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <cjson/cJSON.h>
#include <icBuildtime.h>

#define LOG_TAG "SonosLib"

#define MONITOR_INTERVAL_SECS 5
#define RECOVERY_TIMEOUT_SECONDS 10

static void localSpeakerDiscoveredCallback(SsdpDevice *device);

static pthread_mutex_t discoverMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    pthread_t thread;
    bool running;
    char *macAddress;
    char *ipAddress;
    SonosIpAddressChangedCallback ipChangedCallback;
} SpeakerMonitoringInfo;

static icHashMap *monitors = NULL;
static pthread_mutex_t monitorsMutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t ssdpHandle = 0;

static SonosDiscoverCallback clientSpeakerDiscoveredCallback = NULL;

bool sonosStartDiscovery(SonosDiscoverCallback callback)
{
    bool result = false;

    pthread_mutex_lock(&discoverMutex);
    if (clientSpeakerDiscoveredCallback == NULL)
    {
        clientSpeakerDiscoveredCallback = callback;
        ssdpHandle = ssdpDiscoverStart(SONOS, localSpeakerDiscoveredCallback);
        if (ssdpHandle != 0)
        {
            result = true;
        }

        if (result == false)
        {
            clientSpeakerDiscoveredCallback = NULL;
        }
    }
    pthread_mutex_unlock(&discoverMutex);

    return result;
}

bool sonosStopDiscovery()
{
    ssdpDiscoverStop(ssdpHandle);
    ssdpHandle = 0;
    pthread_mutex_lock(&discoverMutex);
    clientSpeakerDiscoveredCallback = NULL;
    pthread_mutex_unlock(&discoverMutex);
    return true;
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

static void localSpeakerDiscoveredCallback(SsdpDevice *device)
{
    icLogInfo(LOG_TAG, "Speaker found: ip=%s, st=%s, url=%s", device->ipAddress, device->upnpST, device->upnpURL);

    pthread_mutex_lock(&discoverMutex);
    if (clientSpeakerDiscoveredCallback != NULL)
    {
        clientSpeakerDiscoveredCallback(device->macAddress, device->ipAddress);
    }
    pthread_mutex_unlock(&discoverMutex);
}

bool sonosPlayClip(const char *ipAddress, const char *clipUrl)
{
    bool result = false;
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    if (curl)
    {
        char url[128];
        sprintf(url, "http://%s/api", ipAddress);

        char *body = strdup("{\"on\":true}");

        struct curl_slist *header = NULL;

        header = curl_slist_append(header, "Accept: application/json");
        header = curl_slist_append(header, "Content-Type: application/json");
        header = curl_slist_append(header, "charsets: utf-8");

        res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // prevent curl from calling SIGABRT or SIGPIPE

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s", __func__, curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            free(body);
            curl_slist_free_all(header);
            return false;
        }
        else
        {
            result = true;
        }

        curl_easy_cleanup(curl);

        free(body);
        curl_slist_free_all(header);
    }

    return result;
}

void sonosSpeakerDestroy(SonosSpeaker *speaker)
{
    free(speaker->id);
    speaker->id = NULL;
    free(speaker);
}

static void destroySpeakerMonitoringInfo(SpeakerMonitoringInfo *info)
{
    free(info->macAddress);
    info->macAddress = NULL;
    free(info->ipAddress);
    info->ipAddress = NULL;

    free(info);
}

static bool pingSpeaker(const char *ipAddress)
{
    return false;
}

/*
 * thread to monitor a single speaker
 */
static void *speakerMonitoringThreadProc(void *arg)
{
    SpeakerMonitoringInfo *info = (SpeakerMonitoringInfo*)arg;
    while(info->running == true)
    {
        if(pingSpeaker(info->ipAddress) == false)
        {
            // we failed to talk to the speaker.  That most likely means its IP address changed and we have
            // to find it again
            icLogInfo(LOG_TAG, "speakerMonitoringThreadProc: cant ping speaker %s, its ip address probably changed from %s... attempting recovery", info->macAddress, info->ipAddress);
            char *recoveredIpAddress = NULL;
            if(ssdpRecoverIpAddress(SONOS, info->macAddress, &recoveredIpAddress, RECOVERY_TIMEOUT_SECONDS) == true)
            {
                icLogInfo(LOG_TAG, "speakerMonitoringThreadProc: found %s at %s", info->macAddress, recoveredIpAddress);
                info->ipChangedCallback(info->macAddress, recoveredIpAddress);
                free(info->ipAddress);
                info->ipAddress = recoveredIpAddress;
            }
        }

        usleep(MONITOR_INTERVAL_SECS * 1000000);
    }

    return NULL;
}

bool sonosStartMonitoring(const char *macAddress,
                          const char *ipAddress,
                          SonosIpAddressChangedCallback ipAddressChangedCallback)
{
    bool result = true;

    icLogInfo(LOG_TAG, "Monitoring of speaker %s at %s starting", macAddress, ipAddress);

    pthread_mutex_lock(&monitorsMutex);
    if(monitors == NULL)
    {
        // create hash of 'ipAddresses' to monitor
        //
        monitors = hashMapCreate();
    }
    else
    {
        // skip if this ipAddress already being monitored
        //
        if(hashMapGet(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1)) != NULL)
        {
            icLogError(LOG_TAG, "duplicate attempt to watch speaker at %s ignored", ipAddress);
            result = false;
        }
    }

    if(result == true)
    {
        SpeakerMonitoringInfo *info = (SpeakerMonitoringInfo*)calloc(1, sizeof(SpeakerMonitoringInfo));
        info->macAddress = strdup(macAddress);
        info->ipAddress = strdup(ipAddress);
        info->ipChangedCallback = ipAddressChangedCallback;
        info->running = true;

        hashMapPut(monitors, info->ipAddress, (uint16_t) (strlen(ipAddress) + 1), info);
        pthread_create(&info->thread, NULL, speakerMonitoringThreadProc, info);
    }
    pthread_mutex_unlock(&monitorsMutex);

    return result;
}

bool sonosStopMonitoring(const char *ipAddress)
{
    bool result = false;

    icLogInfo(LOG_TAG, "Monitoring of the speaker at %s stopping", ipAddress);

    if(ipAddress == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&monitorsMutex);
    if(monitors != NULL)
    {
        SpeakerMonitoringInfo *info = (SpeakerMonitoringInfo*)hashMapGet(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1));
        if(info != NULL)
        {
            info->running = false;
            pthread_join(info->thread, NULL);
            hashMapDelete(monitors, (void *) ipAddress, (uint16_t) (strlen(ipAddress) + 1), standardDoNotFreeHashMapFunc);
            destroySpeakerMonitoringInfo(info);
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
