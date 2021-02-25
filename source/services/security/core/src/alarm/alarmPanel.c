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
 * alarmPanel.c
 *
 * Central interface to the "alarm state machine".
 *
 * NOTE: should only be referenced if "supportAlarms() == true"
 *       (with the one exception of getSystemPanelStatus)
 *
 * Author: jelderton -  7/27/18.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <icLog/logging.h>
#include <icSystem/softwareCapabilities.h>
#include <securityService/deviceTroubleEventHelper.h>
#include <trouble/troubleContainer.h>
#include "alarm/alarmPanel.h"
#include "alarm/systemMode.h"
#include "trouble/troubleStatePrivate.h"
#include "zone/securityZonePrivate.h"
#include "broadcastEvent.h"
#include "common.h"
#include "internal.h"

/*
 * one-time init to load the config.  since this makes
 * requests to propsService, should not be called until
 * all of the services are available.
 *
 * expects a follow-up call once the rest of the service is
 * loaded (i.e. zones & troubles are read/loaded) so this can
 * determine the overall panel status.
 *
 * NOTE: should only be called if "supportAlarms() == true"
 *
 * @see finishInitAlarmPanel();
 */
void initAlarmPanelPublic()
{
    // TODO: Needs implementation
    icLogDebug(SECURITY_LOG, "alarm: in init");
}

/*
 * generally called at the end of service startup,
 * this will examine zones, troubles, cloud association, etc
 * to determine if the system is READY or NOT_READY
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
void finishInitAlarmPanelPublic()
{
    // TODO: Needs implementation
    icLogDebug(SECURITY_LOG, "alarm: in finishInit");
}

/*
 * called during shutdown
 */
void shutdownAlarmPanelPublic()
{
    // TODO: Needs implementation
}

/*------------------------------------
 *
 *  Status Functions
 *
 *------------------------------------*/

/*
 * return the 'status' of the alarm state machine
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
alarmStatusType getAlarmPanelStatusPublic()
{
    // TODO: Needs implementation
    return ALARM_STATUS_UNCHANGED;
}

/*
 * return the 'arm mode' of the alarm state machine
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
armModeType getAlarmPanelArmModePublic()
{
    // TODO: Needs implementation
    return ARM_METHOD_NONE;
}

/*
 * repeating task that is called every second.
 * this should ONLY be active when the currentStateOperator
 * wants to be notified of time-tick events
 */
void timerTickEvent(void *arg)
{
    // TODO: Needs implementation
}

/*
 * return true if the alarm state machine is in test mode
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
bool isAlarmPanelInTestModePublic()
{
    // TODO: Needs implementation
    return false;
}

/*
 * Set the alarm panel into test mode
 * @param secondsInTestMode number of seconds in test mode.
 * NOTE: this is a blocking call until we get the 'ack' from
 *       the central station.  see alarmPanelAckTestModeMessage()
 *
 * @return true if successful, false if failure(already armed, etc).
 */
alarmTestModeType alarmPanelStartTestModePublic(uint32_t secondsInTestMode)
{
    // TODO: Needs implementation
    return ALARM_TEST_MODE_TIMEOUT;
}

/*
 * Sets the alarm panel out of test mode
 */
void alarmPanelEndTestModePublic()
{
    // TODO: Needs implementation
}

/*
 * return true if motion sensors should be considered "armed"
 */
bool areMotionSensorsArmedPublic()
{
    // TODO: Needs implementation
    return false;
}

/*
 * return true if motion sensors should be considered "armed"
 */
bool areMotionSensorsArmedPrivate()
{
    // TODO: Needs implementation
    return false;
}

/**
 * populate the supplied object with the current states of
 * the alarm state machine.
 *
 * NOTE: works for both supportAlarm() and supportSystemMode()
 **/
void populateSystemPanelStatusPublic(systemPanelStatus *output)
{
    lockSecurityMutex();
    if (supportAlarms() == true)
    {
        // TODO: Needs implementation
    }

    // set system trouble indicator
    //
    output->trouble = (getTroubleCountPrivate(true) > 0);
    unlockSecurityMutex();

    // add system mode
    //
    if (supportSystemMode() == true)
    {
        systemModeSet mode = getCurrentSystemMode();
        free(output->systemMode);
        output->systemMode = strdup(systemModeNames[mode]);
    }
}

/*
 * if the system is currently in alarm, will populate the details about
 * the alarm into the provided container.  Only applicable if supportAlarm() is true
 */
void populateSystemCurrentAlarmStatusPublic(currentAlarmStatus *output)
{
    // TODO: Needs implementation
}

/*
 * if the system is currently in alarm, will populate the details about
 * the alarm into the provided container.  Only applicable if supportAlarm() is true
 */
void populateSystemCurrentAlarmStatusPrivate(alarmDetails *output)
{
    // TODO: Needs implementation
}

/**
 * populate the supplied object with the current states of
 * the alarm state machine (status, mode, test).
 *
 * NOTE: does not lock the mutex
 **/
void populateSystemPanelStatusPrivate(systemPanelStatus *output)
{
    // since internal, assume we only care about alarms & system trouble (not system mode)
    //
    if (supportAlarms() == true)
    {
        // TODO: Needs implementation
    }
    else
    {
        output->alarmStatus = ALARM_STATUS_READY;
        output->armMode = ARM_METHOD_NONE;
    }

    if (supportSystemMode() == true)
    {
        systemModeSet mode = getCurrentSystemMode();
        free(output->systemMode);
        output->systemMode = strdup(systemModeNames[mode]);
    }
    output->trouble = (getTroubleCountPrivate(true) > 0);
}

/*
 * used to get service status (via IPC)
 */
void getAlarmPanelStatusDetailsPublic(serviceStatusPojo *output)
{
    systemPanelStatus *panelStatus = create_systemPanelStatus();
    populateSystemPanelStatusPublic(panelStatus);
    if (supportAlarms() == true)
    {
        put_string_in_serviceStatusPojo(output, "ALARM_STATUS", (char *) alarmStatusTypeLabels[panelStatus->alarmStatus]);
        put_string_in_serviceStatusPojo(output, "ALARM_ARM_MODE", (char *) armModeTypeLabels[panelStatus->armMode]);
        put_string_in_serviceStatusPojo(output, "IN_TEST_MODE", (panelStatus->testModeSecsRemaining > 0) ? "true" : "false");
    }
    if (supportSystemMode() == true)
    {
        put_string_in_serviceStatusPojo(output, "SYSTEM_MODE", (panelStatus->systemMode != NULL) ? panelStatus->systemMode : "");
    }
    put_string_in_serviceStatusPojo(output, "SYSTEM_TROUBLE", (panelStatus->trouble == true) ? "true" : "false");
    destroy_systemPanelStatus(panelStatus);
}

/*
 * used to get runtime stats (via IPC)
 */
void getAlarmPanelStatsDetailsPublic(runtimeStatsPojo *output)
{
    // get the systemPanelStatus and add to output
    //
    systemPanelStatus *panelStatus = create_systemPanelStatus();
    populateSystemPanelStatusPublic(panelStatus);
    if (supportAlarms() == true)
    {
        // get the system armed state
        //
        put_string_in_runtimeStatsPojo(output, "secSysState", panelStatus->armMode == ARM_METHOD_NONE ? "disarmed" : "armed");
    }

    destroy_systemPanelStatus(panelStatus);
}

/*------------------------------------
 *
 *  Arm Functions
 *
 *------------------------------------*/

/*
 * request that the alarmPanel move to "armed" state.  this function takes
 * all of the input arguments needed for the variety of ways to arm the system.
 *
 * @param type - mechanism to use for the arm.  dictates which parms are required or optional
 * @param userCode - code user entered to perform the 'disarm'
 * @param armSource - who is asking for the arm to occur
 * @param mode - mode to arm in (away, stay, night)
 * @param overrideSeconds - override the default delay.  <= 0 keeps it at default
 * @param token - specific to ARM_TYPE_FROM_RULE
 */
armResultType performArmRequestPublic(armType type, const char *userCode, armSourceType armSource,
                                      armModeType mode, int16_t overrideSeconds, char *token)
{
    // TODO: Needs implementation
    return SYSTEM_ARM_INVALID_ARGS;
}

/**
 * Get details about all zones that are preventing arming
 * @return linked list of securityZoneArmStatusDetails
 */
icLinkedList *getAllZoneArmStatusPublic()
{
    // TODO: Needs implementation
    return NULL;
}

/*
 * examine security zones (zones that can cause alarms) for faults or troubles
 * returns true if some zone is faulted or troubled.
 */
bool areAnyZonesFaultedOrTroubledPublic()
{
    // TODO: Needs implementation
    return false;
}

/*------------------------------------
 *
 *  Disarm Functions
 *
 *------------------------------------*/

/*
 * request that the alarmPanel move to "disarmed" state.  this function takes
 * all of the input arguments needed for the variety of ways to disarm the system.
 *
 * @param *userCode - code user entered to perform the 'disarm'
 * @param disarmSource
 * @param token - specific to DISARM_FROM_RULE )
 */
disarmResultType performDisarmRequestPublic(disarmType type, const char *userCode, armSourceType disarmSource,
                                            char *token)
{
    // TODO: Needs implementation
    return SYSTEM_DISARM_INVALID_ARGS;
}

/*
 * starts a panic alarm (generally initiated from IPC).
 * returns the alarmSessionId that was created for this alarm
 */
uint64_t startPanicAlarmPublic(alarmPanicType panicType, armSourceType panicSource)
{
    // TODO: Needs implementation
    return 0;
}

/*------------------------------------
 *
 *  Event Processing
 *
 *------------------------------------*/

/*
 * internally called when adding/clearing a trouble.  this is called BEFORE
 * the trouble is broadcasted to the system, and serves two purposes:
 * 1 - potentially update the panel status based on the trouble (i.e. system tampered, so state == NOT_READY)
 * 2 - populate the event->systemPanelStatus data
 *
 * NOTE: when called, assumed the SECURITY_MTX is held and that it is
 *       safe to make internal calls into zone/trouble.
 */
void processTroubleContainerForAlarmPanel(troubleContainer *container)
{
    // sanity check (may not be necessary)
    //
    if (container == NULL || container->event == NULL)
    {
        return;
    }

    if (supportSystemMode() == true)
    {
        // populate status into the event, then return
        //
        populateSystemPanelStatusPrivate(container->event->panelStatus);
    }
}

/*
 * internally called when a zone event occurs.  like troubles, this
 * should be called BEFORE broadcasting the zoneEvent so the alarm
 * state machine can react and potentially alter arm/alarm states.
 *
 * NOTE: when called, assumed the SECURITY_MTX is held and that it is
 *       safe to make internal calls into zone/trouble.
 */
void processZoneEventForAlarmPanel(securityZoneEvent *event)
{
    // TODO: Needs implementation
}

/*
 * Does the second portion of the processZoneEventForAlarmPanel call. In particular, updates our alarm context and calls
 * the context's securityZoneEventFunc. Finally, updates the event's panel status.
 */
void zoneEventNotifyStateContext(securityZoneEvent* event)
{
    // TODO: Needs implementation
}

/*
 * internal called when the service's event listener receives notification
 * that the cloud association state changes.
 */
void processCloudAssociationStateChangeEvent(cloudAssociationStateChangedEvent *event)
{
    // TODO: Needs implementation
}

/*------------------------------------
 *
 *  Alarm Session Functions
 *
 *------------------------------------*/

/*
 * returns the number of dormant (completed) alarmSessions (instances waiting to be acknowledged)
 */
uint16_t getDormantAlarmSessionCountPublic()
{
    // TODO: Needs implementation
    return 0;
}

/*
 * acknowledge the dormant (completed) AlarmSessions.  this is a public method,
 * and will request the global security mutex lock.
 */
void acknowledgeDormantAlarmSessionsPublic()
{
    // TODO: Needs implementation
}
