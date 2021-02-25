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
// Created by wboyd747 on 6/7/18.
//

#include <pthread.h>
#include <securityService/securityService_event.h>
#include <memory.h>
#include <malloc.h>
#include <icSystem/softwareCapabilities.h>
#include <securityService/securityService_eventAdapter.h>
#include <securityService/securityService_ipc.h>
#include "automationService.h"
#include "automationSystemStatus.h"

#ifndef sizeof_array
#define sizeof_array(a) (sizeof(a) / sizeof((a)[0]))
#endif

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static automationSystemStatus systemStatus = AUTOMATION_SYSTEMSTATUS_HOME;

static const char* systemStatus_enum2str[] = {
    "home",
    "away",
    "stay",
    "night",
    "vacation",
    "arming",
    "alarm"
};

static void setSystemStatus(const char* value)
{
    if (value) {
        int i;

        pthread_mutex_lock(&mutex);
        for (i = 0; i < sizeof_array(systemStatus_enum2str); i++) {
            if (strcmp(value, systemStatus_enum2str[i]) == 0) {
                systemStatus = (automationSystemStatus) i;
            }
        }
        pthread_mutex_unlock(&mutex);
    }
}

static void setSystemStatusForPanelStatus(systemPanelStatus *panelStatus)
{
    if (panelStatus != NULL) {
        pthread_mutex_lock(&mutex);
        switch(panelStatus->alarmStatus) {
            case ALARM_STATUS_READY:
            case ALARM_STATUS_NOTREADY:
                systemStatus = AUTOMATION_SYSTEMSTATUS_HOME;
                break;
                // Nothing distinct for entry delay, treat it like we are still armed
            case ALARM_STATUS_ENTRY_DELAY:
            case ALARM_STATUS_ARMED:
                switch (panelStatus->armMode) {
                    case ARM_METHOD_AWAY:
                        systemStatus = AUTOMATION_SYSTEMSTATUS_AWAY;
                        break;
                    case ARM_METHOD_STAY:
                        systemStatus = AUTOMATION_SYSTEMSTATUS_STAY;
                        break;
                    case ARM_METHOD_NIGHT:
                        systemStatus = AUTOMATION_SYSTEMSTATUS_NIGHT;
                        break;
                    default:
                        break;
                }
                break;
            case ALARM_STATUS_ALARM:
                systemStatus = AUTOMATION_SYSTEMSTATUS_ALARM;
                break;
            case ALARM_STATUS_ARMING:
                systemStatus = AUTOMATION_SYSTEMSTATUS_ARMING;
                break;
            case ALARM_STATUS_UNCHANGED:
            default:
                break;
        }
        pthread_mutex_unlock(&mutex);
    }
}

static void systemModeChangedListener(systemModeChangedEvent* event)
{
    if (event) {
        setSystemStatus(event->currentSystemMode);
    }
}

static void alarmEventListener(alarmEvent *event)
{
    if (event != NULL) {
        setSystemStatusForPanelStatus(event->panelStatus);
    }
}

static void armedEventListener(armedEvent *event)
{
    if (event != NULL) {
        setSystemStatusForPanelStatus(event->panelStatus);
    }
}

static void armingEventListener(armingEvent *event)
{
    if (event != NULL) {
        setSystemStatusForPanelStatus(event->panelStatus);
    }
}

static void disarmedEventListener(disarmEvent *event)
{
    if (event != NULL) {
        setSystemStatusForPanelStatus(event->panelStatus);
    }
}

void automationRegisterSystemStatus(void)
{
    if (supportSystemMode()) {
        char* status = NULL;

        register_systemModeChangedEvent_eventListener(systemModeChangedListener);

        if (securityService_request_GET_CURRENT_SYSTEM_MODE(&status) == IPC_SUCCESS) {
            setSystemStatus(status);
        }
    }

    if (supportAlarms()) {
        register_alarmEvent_eventListener(alarmEventListener);
        register_armedEvent_eventListener(armedEventListener);
        register_armingEvent_eventListener(armingEventListener);
        register_disarmEvent_eventListener(disarmedEventListener);

        systemPanelStatus *panelStatus = create_systemPanelStatus();
        if (securityService_request_GET_SYSTEM_PANEL_STATUS(panelStatus) == IPC_SUCCESS) {
            setSystemStatusForPanelStatus(panelStatus);
        }
        destroy_systemPanelStatus(panelStatus);
    }
}

void automationUnregisterSystemStatus(void)
{
    if (supportSystemMode()) {
        unregister_systemModeChangedEvent_eventListener(systemModeChangedListener);
    }

    if (supportAlarms()) {
        unregister_alarmEvent_eventListener(alarmEventListener);
        unregister_armedEvent_eventListener(armedEventListener);
        unregister_armingEvent_eventListener(armingEventListener);
        unregister_disarmEvent_eventListener(disarmedEventListener);
    }
}

automationSystemStatus automationGetSystemStatus(void)
{
    automationSystemStatus ret;

    pthread_mutex_lock(&mutex);
    ret = systemStatus;
    pthread_mutex_unlock(&mutex);

    return ret;
}

const char* automationGetSystemStatusLabel(void)
{
    const char* ret;

    pthread_mutex_lock(&mutex);
    ret = systemStatus_enum2str[systemStatus];
    pthread_mutex_unlock(&mutex);

    return ret;
}
