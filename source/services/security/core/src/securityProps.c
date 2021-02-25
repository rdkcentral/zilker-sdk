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
 * securityProps.c
 *
 * common location for properties that pertain to the
 * security service.  it will initially load the values
 * and keep the cached values up-to-date via property change events.
 * this is intended for on-demand query use.  if the code needs
 * to react to the property change event, it is recommended that it
 * also register itself for those updates.
 *
 * Author: jelderton -  3/23/19.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include "securityProps.h"
#include "common.h"

#define MIN_COMFAIL_TROUBLE_DELAY_MINUTES   56      // two missed checkins - about an hour
#define MIN_COMFAIL_ALARM_DELAY_MINUTES     60      // two missed checkins + 5 minutes
#define DEFAULT_COMFAIL_ALARM_DELAY_MINUTES (6 * MIN_COMFAIL_ALARM_DELAY_MINUTES)
#define DEFAULT_NO_ALARM_ON_COMM_FAILURE    false
#define DEFAULT_DURESSCODE_DISABLED         false

// private functions
static void propertyChangedNotify(cpePropertyEvent *event);

// private variables
static pthread_mutex_t PROP_MTX = PTHREAD_MUTEX_INITIALIZER;
static uint32_t sensorCommFailTroubleMinProp = 0;
static uint32_t sensorCommFailAlarmMinProp = 0;
static bool noAlarmOnCommFailEnabled = true;
static bool systemTamperEnabled = true;
static bool systemBatteryEnabled = true;
static bool duressCodeDisabled = true;
static char *alarmCancelCustomContactId = NULL;
static setupWizardState activationSetupState = ACTIVATION_NOT_STARTED;

/*
 * load initial property values and register for changes
 */
void initSecurityProps()
{
    // load the initial values of the properties we care about
    //
    sensorCommFailTroubleMinProp = getPropertyAsUInt32(TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY, MIN_COMFAIL_TROUBLE_DELAY_MINUTES);
    if (sensorCommFailTroubleMinProp < MIN_COMFAIL_TROUBLE_DELAY_MINUTES)
    {
        sensorCommFailTroubleMinProp = MIN_COMFAIL_TROUBLE_DELAY_MINUTES;
    }
    sensorCommFailAlarmMinProp = getPropertyAsUInt32(TOUCHSCREEN_SENSOR_COMMFAIL_ALARM_DELAY, DEFAULT_COMFAIL_ALARM_DELAY_MINUTES);
    if (sensorCommFailAlarmMinProp < MIN_COMFAIL_ALARM_DELAY_MINUTES)
    {
        sensorCommFailAlarmMinProp = MIN_COMFAIL_ALARM_DELAY_MINUTES;
    }
    activationSetupState = (setupWizardState)getPropertyAsInt32(PERSIST_CPE_SETUPWIZARD_STATE, ACTIVATION_NOT_STARTED);
    icLogTrace(SECURITY_LOG, "secProps: initial 'activation' = %"PRIi32, activationSetupState);

    // runtime flags
    noAlarmOnCommFailEnabled = getPropertyAsBool(NO_ALARM_ON_COMM_FAILURE, DEFAULT_NO_ALARM_ON_COMM_FAILURE);
    duressCodeDisabled = getPropertyAsBool(DURESSCODE_DISABLED, DEFAULT_DURESSCODE_DISABLED);
    alarmCancelCustomContactId = NULL;
    systemTamperEnabled = false;
    systemBatteryEnabled = false;

    // property change envents
    register_cpePropertyEvent_eventListener(propertyChangedNotify);
}

/*
 * cleanup during shutdown
 */
void cleanupSecurityProps()
{
    // cleanup event registration
    unregister_cpePropertyEvent_eventListener(propertyChangedNotify);

    // cleanup memory
    free(alarmCancelCustomContactId);
}

/*
 * return the minute duration of a sensor in comm fail
 * before declaring it a "comm fail" trouble
 */
uint32_t getDeviceOfflineCommFailTroubleMinutesProp()
{
    pthread_mutex_lock(&PROP_MTX);
    uint32_t retVal = sensorCommFailTroubleMinProp;
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the minute duration of a sensor in comm fail
 * before declaring it in "comm fail alarm"
 */
uint32_t getDeviceOfflineCommFailAlarmTroubleMinutesProp()
{
    pthread_mutex_lock(&PROP_MTX);
    uint32_t retVal = sensorCommFailAlarmMinProp;
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the cached value for NO_ALARM_ON_COMM_FAILURE
 */
bool getNoAlarmOnCommFailProp()
{
    pthread_mutex_lock(&PROP_MTX);
    bool retVal = noAlarmOnCommFailEnabled;
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the cached value for TAMPER_ENABLED_BOOL_PROP
 */
bool getSystemTamperEnabledProp()
{
    pthread_mutex_lock(&PROP_MTX);
    bool retVal = systemTamperEnabled;
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the cached (negated) value for IGNORE_BATTERY_BOOL_PROPERTY
 */
bool getSystemBatteryEnabledProp()
{
    pthread_mutex_lock(&PROP_MTX);
    bool retVal = systemBatteryEnabled;
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the cached value for DURESSCODE_DISABLED
 */
bool getDuressCodeDisabledProp()
{
    pthread_mutex_lock(&PROP_MTX);
    bool retVal = duressCodeDisabled;
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the cached value for ALARM_CANCEL_CONTACT_ID
 * caller should free the returned value (if not NULL)
 */
char *getAlarmCancelCustomContactIdProp()
{
    char *retVal = NULL;

    pthread_mutex_lock(&PROP_MTX);
    if (alarmCancelCustomContactId != NULL)
    {
        retVal = strdup(alarmCancelCustomContactId);
    }
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * return the "setup wizard" (activation) state
 */
setupWizardState getSetupWizardStateProp()
{
    pthread_mutex_lock(&PROP_MTX);
    setupWizardState retVal = activationSetupState;
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/*
 * callback from PropsService when a CPE property is added/edited/deleted
 */
static void propertyChangedNotify(cpePropertyEvent *event)
{
    if (event == NULL || event->propKey == NULL)
    {
        return;
    }

    // look for certain properties that we need to react to
    //
    if (strcmp(event->propKey, TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY) == 0)
    {
        // got "device trouble comm fail" property
        pthread_mutex_lock(&PROP_MTX);
        if (event->baseEvent.eventValue == GENERIC_PROP_DELETED)
        {
            // use default value
            sensorCommFailTroubleMinProp = MIN_COMFAIL_TROUBLE_DELAY_MINUTES;
        }
        else
        {
            // add/edit
            sensorCommFailTroubleMinProp = getPropertyEventAsUInt32(event, MIN_COMFAIL_TROUBLE_DELAY_MINUTES);
            if (sensorCommFailTroubleMinProp < MIN_COMFAIL_TROUBLE_DELAY_MINUTES)
            {
                sensorCommFailTroubleMinProp = MIN_COMFAIL_TROUBLE_DELAY_MINUTES;
            }
        }
        pthread_mutex_unlock(&PROP_MTX);
    }
    if (strcmp(event->propKey, TOUCHSCREEN_SENSOR_COMMFAIL_ALARM_DELAY) == 0)
    {
        // got "device trouble comm fail" property
        pthread_mutex_lock(&PROP_MTX);
        if (event->baseEvent.eventValue == GENERIC_PROP_DELETED)
        {
            // use default value
            sensorCommFailAlarmMinProp = DEFAULT_COMFAIL_ALARM_DELAY_MINUTES;
        }
        else
        {
            // add/edit
            sensorCommFailAlarmMinProp = getPropertyEventAsUInt32(event, DEFAULT_COMFAIL_ALARM_DELAY_MINUTES);
            if (sensorCommFailAlarmMinProp < MIN_COMFAIL_ALARM_DELAY_MINUTES)
            {
                sensorCommFailAlarmMinProp = MIN_COMFAIL_ALARM_DELAY_MINUTES;
            }
        }
        pthread_mutex_unlock(&PROP_MTX);
    }

    else if (strcmp(event->propKey, NO_ALARM_ON_COMM_FAILURE) == 0)
    {
        // got "alarm on comm fail" property
        pthread_mutex_lock(&PROP_MTX);
        if (event->baseEvent.eventValue == GENERIC_PROP_DELETED)
        {
            // use default value
            noAlarmOnCommFailEnabled = DEFAULT_NO_ALARM_ON_COMM_FAILURE;
        }
        else
        {
            // add/edit
            noAlarmOnCommFailEnabled = getPropertyEventAsBool(event, DEFAULT_NO_ALARM_ON_COMM_FAILURE);
        }
        pthread_mutex_unlock(&PROP_MTX);
    }

    else if (strcmp(event->propKey, DURESSCODE_DISABLED) == 0)
    {
        // got "duress code disabled" property
        pthread_mutex_lock(&PROP_MTX);
        if (event->baseEvent.eventValue == GENERIC_PROP_DELETED)
        {
            // use default value
            duressCodeDisabled = DEFAULT_DURESSCODE_DISABLED;
        }
        else
        {
            // add/edit
            duressCodeDisabled = getPropertyEventAsBool(event, DEFAULT_DURESSCODE_DISABLED);
        }
        pthread_mutex_unlock(&PROP_MTX);
    }

    else if (strcmp(event->propKey, PERSIST_CPE_SETUPWIZARD_STATE) == 0)
    {
        // update activation state
        pthread_mutex_lock(&PROP_MTX);
        activationSetupState = ACTIVATION_NOT_STARTED;
        if (event->baseEvent.eventValue != GENERIC_PROP_DELETED && event->propValue != NULL)
        {
            activationSetupState = (setupWizardState)getPropertyEventAsInt32(event, ACTIVATION_NOT_STARTED);
            icLogTrace(SECURITY_LOG, "secProps: updated 'activation' = %"PRIi32, activationSetupState);
        }
        pthread_mutex_unlock(&PROP_MTX);
    }
}

