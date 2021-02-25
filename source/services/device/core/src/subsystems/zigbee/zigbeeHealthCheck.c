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
// Created by tlea on 7/9/19.
//

#include <stdint.h>
#include <pthread.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <icLog/logging.h>
#include <zhal/zhal.h>
#include <ipc/deviceEventProducer.h>
#include "zigbeeHealthCheck.h"

#define LOG_TAG "zigbeeHealthCheck"

// dont allow health checking faster than this
#define MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS 1000

//default to off
#define NETWORK_HEALTH_CHECK_INTERVAL_MILLIS_DEFAULT  0

// positive values dont make sense and are used to disable adjusting the CCA threshold
#define NETWORK_HEALTH_CHECK_CCA_THRESHOLD_DEFAULT 1

#define NETWORK_HEALTH_CHECK_CCA_FAILURE_THRESHOLD_DEFAULT 10
#define NETWORK_HEALTH_CHECK_RESTORE_THRESHOLD_DEFAULT 600
#define NETWORK_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS_DEFAULT 1000

static pthread_mutex_t interferenceDetectedMtx = PTHREAD_MUTEX_INITIALIZER;
static bool interferenceDetected = false;

void zigbeeHealthCheckStart()
{
    uint32_t intervalMillis = getPropertyAsUInt32(ZIGBEE_HEALTH_CHECK_INTERVAL_MILLIS,
                                                  NETWORK_HEALTH_CHECK_INTERVAL_MILLIS_DEFAULT);
    if(intervalMillis == 0)
    {
        icLogDebug(LOG_TAG, "%s: not monitoring, feature disabled", __FUNCTION__);

        zigbeeHealthCheckStop();

        //if there was interference before, we need to send a clear event since we are stopping monitoring
        pthread_mutex_lock(&interferenceDetectedMtx);
        if(interferenceDetected)
        {
            interferenceDetected = false;
            sendZigbeeNetworkInterferenceEvent(false);
        }
        pthread_mutex_unlock(&interferenceDetectedMtx);
    }
    else
    {
        if(intervalMillis < MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS)
        {
            icLogWarn(LOG_TAG, "%s: Attempt to set network health check intervalMillis to %"PRIu32" is below minimum, using %"PRIu32,
                    __FUNCTION__, intervalMillis, MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS);

            intervalMillis = MIN_NETWORK_HEALTH_CHECK_INTERVAL_MILLIS;
        }

        int32_t ccaThreshold = getPropertyAsInt32(ZIGBEE_HEALTH_CHECK_CCA_THRESHOLD,
                                                  NETWORK_HEALTH_CHECK_CCA_THRESHOLD_DEFAULT);
        uint32_t ccaFailureThreshold = getPropertyAsUInt32(ZIGBEE_HEALTH_CHECK_CCA_FAILURE_THRESHOLD,
                                                           NETWORK_HEALTH_CHECK_CCA_FAILURE_THRESHOLD_DEFAULT);
        uint32_t restoreThreshold = getPropertyAsUInt32(ZIGBEE_HEALTH_CHECK_RESTORE_THRESHOLD,
                                                        NETWORK_HEALTH_CHECK_RESTORE_THRESHOLD_DEFAULT);
        uint32_t delayBetweenRetriesMillis = getPropertyAsUInt32(ZIGBEE_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS,
                                                                 NETWORK_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS_DEFAULT);

        if(zhalConfigureNetworkHealthCheck(intervalMillis,
                                           ccaThreshold,
                                           ccaFailureThreshold,
                                           restoreThreshold,
                                           delayBetweenRetriesMillis) == false)
        {
            icLogError(LOG_TAG, "%s: failed to start network health checking", __FUNCTION__);
        }
    }
}

void zigbeeHealthCheckStop()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if(zhalConfigureNetworkHealthCheck(0, 0, 0, 0, 0) == false)
    {
        icLogError(LOG_TAG, "%s: failed to stop network health checking", __FUNCTION__);
    }
}

void zigbeeHealthCheckSetProblem(bool problemExists)
{
    icLogDebug(LOG_TAG, "%s: problemExists = %s", __FUNCTION__, problemExists ? "true" : "false");

    pthread_mutex_lock(&interferenceDetectedMtx);
    interferenceDetected = problemExists;
    pthread_mutex_unlock(&interferenceDetectedMtx);

    sendZigbeeNetworkInterferenceEvent(problemExists);
}
