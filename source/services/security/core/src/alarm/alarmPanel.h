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
 * alarmPanel.h
 *
 * Central interface to the "alarm state machine".
 *
 * NOTE: should only be referenced if "supportAlarms() == true"
 *       (with the one exception of getSystemPanelStatus)
 *
 * Author: jelderton -  7/27/18.
 *-----------------------------------------------*/

#ifndef ZILKER_ALARMPANEL_H
#define ZILKER_ALARMPANEL_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <icIpc/ipcStockMessagesPojo.h>
#include <securityService/securityService_pojo.h>
#include <securityService/securityService_event.h>
#include <commMgr/commService_pojo.h>
#include <commMgr/commService_event.h>
#include "../trouble/troubleContainer.h"

typedef enum {
    ARM_TYPE_DELAY,
    ARM_TYPE_QUICK_FOR_TEST,
    ARM_TYPE_FROM_RULE
} armType;

typedef enum {
    DISARM_TYPE_STANDARD,
    DISARM_TYPE_FROM_RULE,
    DISARM_TYPE_FROM_KEYFOB,
    DISARM_TYPE_FOR_TEST
} disarmType;

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
void initAlarmPanelPublic();

/*
 * generally called at the end of service startup,
 * this will examine zones, troubles, cloud association, etc
 * to determine if the system is READY or NOT_READY
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
void finishInitAlarmPanelPublic();

/*
 * called during shutdown
 */
void shutdownAlarmPanelPublic();

/*
 * return the 'status' of the alarm state machine
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
alarmStatusType getAlarmPanelStatusPublic();

/*
 * return the 'arm mode' of the alarm state machine
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
armModeType getAlarmPanelArmModePublic();

/*
 * return true if the alarm state machine is in test mode
 *
 * NOTE: should only be called if "supportAlarms() == true"
 */
bool isAlarmPanelInTestModePublic();

/*
 * Set the alarm panel into test mode
 * @param secondsInTestMode number of seconds in test mode.
 * NOTE: this is a blocking call until we get the 'ack' from
 *       the central station.  see alarmPanelAckTestModeMessage()
 *
 * @return true if successful, false if failure(already armed, etc).
 */
alarmTestModeType alarmPanelStartTestModePublic(uint32_t secondsInTestMode);

/*
 * Sets the alarm panel out of test mode
 */
void alarmPanelEndTestModePublic();

/*
 * called via IPC when the test mode message was acknowledged by the central station
 */
void alarmPanelAckTestModeMessage();

/*
 * return true if motion sensors should be considered "armed"
 */
bool areMotionSensorsArmedPublic();

/**
 * populate the supplied object with the current states of
 * the alarm state machine.
 *
 * NOTE: works for both supportAlarm() and supportSystemMode()
 **/
void populateSystemPanelStatusPublic(systemPanelStatus *output);

/*
 * if the system is currently in alarm, will populate the details about
 * the alarm into the provided container.  Only applicable if supportAlarm() is true
 */
void populateSystemCurrentAlarmStatusPublic(currentAlarmStatus *output);

/*
 * if the system is currently in alarm, will populate the details about
 * the alarm into the provided container.  Only applicable if supportAlarm() is true
 */
void populateSystemCurrentAlarmStatusPrivate(alarmDetails *output);

/**
 * populate the supplied object with the current states of
 * the alarm state machine (status, mode, test).
 *
 * NOTE: does not lock the mutex
 **/
void populateSystemPanelStatusPrivate(systemPanelStatus *output);

/*
 * used to get service status (via IPC)
 */
void getAlarmPanelStatusDetailsPublic(serviceStatusPojo *output);

/*
 * used to get runtime stats (via IPC)
 */
void getAlarmPanelStatsDetailsPublic(runtimeStatsPojo *output);

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
                                      armModeType mode, int16_t overrideSeconds, char *token);

/*
 * Get details about all zones that are preventing arming
 * @return linked list of securityZoneArmStatusDetails
 */
icLinkedList *getAllZoneArmStatusPublic();

/*
 * examine security zones (zones that can cause alarms) for faults or troubles
 * returns true if some zone is faulted or troubled.
 */
bool areAnyZonesFaultedOrTroubledPublic();

/*
 * request that the alarmPanel move to "disarmed" state.  this function takes
 * all of the input arguments needed for the variety of ways to disarm the system.
 *
 * @param type - mechanism to use for the disarm.  dictates which parms are required or optional
 * @param userCode - code user entered to perform the 'disarm'
 * @param disarmSource - who is asking for the disarm to occur
 * @param token - specific to DISARM_TYPE_FROM_RULE
 */
disarmResultType performDisarmRequestPublic(disarmType type, const char *userCode, armSourceType disarmSource,
                                            char *token);

/*
 * starts a panic alarm (generally initiated from IPC).
 * returns the alarmSessionId that was created for this alarm
 */
uint64_t startPanicAlarmPublic(alarmPanicType panicType, armSourceType panicSource);

/*
 * internally called when adding/clearing a trouble.  this is called BEFORE
 * the trouble is broadcasted to the system, and serves two purposes:
 * 1 - potentially update the panel status based on the trouble (i.e. system tampered, so state == NOT_READY)
 * 2 - populate the event->systemPanelStatus data
 *
 * NOTE: when called, assumed the SECURITY_MTX is held and that it is
 *       safe to make internal calls into zone/trouble.
 */
void processTroubleContainerForAlarmPanel(troubleContainer *container);

/*
 * internally called when a zone event occurs.  like troubles, this
 * should be called BEFORE broadcasting the zoneEvent so the alarm
 * state machine can react and potentially alter arm/alarm states.
 *
 * NOTE: when called, assumed the SECURITY_MTX is held and that it is
 *       safe to make internal calls into zone/trouble.
 */
void processZoneEventForAlarmPanel(securityZoneEvent *event);

/*
 * called when the service's event listener receives notification
 * that the cloud association state changes.
 */
void processCloudAssociationStateChangeEvent(cloudAssociationStateChangedEvent *event);

/*
 * returns the number of dormant (completed) alarmSessions (instances waiting to be acknowledged)
 */
uint16_t getDormantAlarmSessionCountPublic();

/*
 * acknowledge the dormant (completed) AlarmSessions.  this is a public method,
 * and will request the global security mutex lock.
 */
void acknowledgeDormantAlarmSessionsPublic();

#endif // ZILKER_ALARMPANEL_H
