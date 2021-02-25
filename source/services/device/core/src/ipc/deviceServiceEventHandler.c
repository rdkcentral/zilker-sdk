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

/*
 * Created by Thomas Lea on 10/23/15.
 */

#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/propsHelper.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <deviceDescriptorHandler.h>
#include <propsMgr/commonProperties.h>
#include <inttypes.h>
#include <securityService/securityService_eventAdapter.h>
#include <icConcurrent/threadPool.h>
#include <deviceDriverManager.h>
#include <deviceService.h>
#include <commonDeviceDefs.h>
#include <deviceService/zoneChanged.h>
#include <securityService/securityZoneHelper.h>
#include "subsystems/zigbee/zigbeeHealthCheck.h"
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "deviceServiceEventHandler.h"
#include <icSystem/hardwareCapabilities.h>

#define LOG_TAG "deviceServiceEventHandler"

#define EVENT_THREADS_MAX    10
#define EVENT_QUEUE_MAX      128

#define OTA_UPGRADE_DELAY_ARMED      1*60*60 // Wait for 1 hour (3600 seconds)
#define OTA_UPGRADE_DELAY_DISARMED   0

static void propsListener(cpePropertyEvent *event);

static void handleSecurityEvent(BaseEvent *event);

static void handleSecurityZoneEvent(securityZoneEvent *event);

static void handleSecurityZoneReorderEvent(securityZoneReorderEvent *event);

bool deviceServiceEventHandlerInit()
{
    register_cpePropertyEvent_eventListener(propsListener);

    register_armingEvent_eventListener((handleEvent_armingEvent) handleSecurityEvent);
    register_armedEvent_eventListener((handleEvent_armedEvent) handleSecurityEvent);
    register_disarmEvent_eventListener((handleEvent_disarmEvent) handleSecurityEvent);
    register_entryDelayEvent_eventListener((handleEvent_entryDelayEvent) handleSecurityEvent);
    register_alarmEvent_eventListener((handleEvent_alarmEvent) handleSecurityEvent);
    register_securityZoneEvent_eventListener(handleSecurityZoneEvent);
    register_securityZoneReorderEvent_eventListener(handleSecurityZoneReorderEvent);
    return true;
}

bool deviceServiceEventHandlerShutdown()
{
    unregister_cpePropertyEvent_eventListener(propsListener);

    unregister_armingEvent_eventListener((handleEvent_armingEvent) handleSecurityEvent);
    unregister_armedEvent_eventListener((handleEvent_armedEvent) handleSecurityEvent);
    unregister_disarmEvent_eventListener((handleEvent_disarmEvent) handleSecurityEvent);
    unregister_entryDelayEvent_eventListener((handleEvent_entryDelayEvent) handleSecurityEvent);
    unregister_alarmEvent_eventListener((handleEvent_alarmEvent) handleSecurityEvent);
    unregister_securityZoneEvent_eventListener(handleSecurityZoneEvent);
    unregister_securityZoneReorderEvent_eventListener(handleSecurityZoneReorderEvent);

    shutdownEventListener();

    return true;
}

static void propsListener(cpePropertyEvent *event)
{
    if (strcmp(event->propKey, DEVICE_DESCRIPTOR_LIST) == 0)
    {
        if (!hasProperty(DEVICE_DESC_WHITELIST_URL_OVERRIDE))
        {
            deviceDescriptorsUpdateWhitelist(event->propValue);
        }
        else
        {
            icLogInfo(LOG_TAG, "Ignoring new DDL URL %s as there is an override set", event->propValue);
        }
    }
    if (strcmp(event->propKey, DEVICE_DESC_WHITELIST_URL_OVERRIDE) == 0)
    {
        // Check if the override was deleted
        if (event->propValue != NULL)
        {
            deviceDescriptorsUpdateWhitelist(event->propValue);
        }
        else
        {
            // Restore the regular whitelist
            char *whitelistUrl = getPropertyAsString(DEVICE_DESCRIPTOR_LIST, NULL);
            if (whitelistUrl != NULL)
            {
                deviceDescriptorsUpdateWhitelist(whitelistUrl);
                free(whitelistUrl);
            }
        }
    }
    else if (strcmp(event->propKey, DEVICE_DESC_BLACKLIST) == 0)
    {
        deviceDescriptorsUpdateBlacklist(event->propValue);
    }
    else if (strcmp(event->propKey, POSIX_TIME_ZONE_PROP) == 0)
    {
        icLogDebug(LOG_TAG, "Got new time zone: %s", event->propValue);
        timeZoneChanged(event->propValue);
    }
    else if (strcmp(event->propKey, DEVICE_FIRMWARE_URL_NODE) == 0 ||
             strcmp(event->propKey, CAMERA_FIRMWARE_URL_NODE) == 0)
    {
        icLogDebug(LOG_TAG, "Got new firmware url: %s", event->propValue);
        // Got a new URL, its possible this could trigger new downloads that failed before, so
        // process device descriptors and attempt to download any needed firmware
        deviceServiceProcessDeviceDescriptors();
    }
    else if (strcmp(event->propKey, CPE_BLACKLISTED_DEVICES_PROPERTY_NAME) == 0)
    {
        icLogDebug(LOG_TAG, "Blacklisted devices property set to : %s", event->propValue);
        processBlacklistedDevices(event->propValue);
    }
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    else if (strncmp(event->propKey, ZIGBEE_PROPS_PREFIX, strlen(ZIGBEE_PROPS_PREFIX)) == 0 ||
             strncmp(event->propKey, TELEMETRY_PROPS_PREFIX, strlen(TELEMETRY_PROPS_PREFIX)) == 0)
    {
        zigbeeSubsystemHandlePropertyChange(event->propKey, event->propValue);
    }
#endif

    // Finally, give device service a chance to handle the event
    deviceServiceNotifyPropertyChange(event);

}

static void freeDeviceList(icLinkedList **list)
{
    linkedListDestroy(*list, standardDoNotFreeFunc);
}

/**
 * @note event is disposed of by caller
 * TODO: remove this once security service deals with writing securityState
 */
static void handleSecurityEvent(BaseEvent *event)
{
    PanelStatus panelStatus;
    indicationType panelIndication = INDICATION_NONE;
    uint32_t timeLeft = 0;
    bool bypassActive = 0;

    switch (event->eventCode)
    {
        case ALARM_EVENT_ARMED:
        {
            armingEvent *armedEvent = (armingEvent *) event;
            panelStatus = deviceServiceConvertSystemPanelStatus(armedEvent->panelStatus, ALARM_REASON_NONE,
                                                                PANIC_ALARM_TYPE_NONE);
            timeLeft = armedEvent->exitDelay;
            bypassActive = armedEvent->panelStatus->bypassActive;
            panelIndication = armedEvent->indication;

            deviceServiceSetOtaUpgradeDelay(OTA_UPGRADE_DELAY_ARMED);

            break;
        }
        case ALARM_EVENT_DISARMED:
        {
            disarmEvent *sysDisarmEvent = (disarmEvent *) event;
            panelStatus = deviceServiceConvertSystemPanelStatus(sysDisarmEvent->panelStatus, ALARM_REASON_NONE,
                                                                PANIC_ALARM_TYPE_NONE);
            timeLeft = deviceServiceGetSecurityExitDelay();
            bypassActive = sysDisarmEvent->panelStatus->bypassActive;
            panelIndication = sysDisarmEvent->indication;

            deviceServiceSetOtaUpgradeDelay(OTA_UPGRADE_DELAY_DISARMED);

            break;
        }
        case ALARM_EVENT_ENTRY_DELAY_REMAINING:
        case ALARM_EVENT_ENTRY_DELAY:
        {
            entryDelayEvent *delayEvent = (entryDelayEvent *) event;
            timeLeft = delayEvent->entryDelay;
            panelIndication = delayEvent->indication;
            panelStatus = event->eventCode == ALARM_EVENT_ENTRY_DELAY ? PANEL_STATUS_ENTRY_DELAY_ONESHOT
                                                                      : PANEL_STATUS_ENTRY_DELAY;
            bypassActive = delayEvent->panelStatus->bypassActive;
            break;
        }
        case ALARM_EVENT_EXIT_DELAY_REMAINING:
        {
            armingEvent *armEvent = (armingEvent *) event;
            panelStatus = PANEL_STATUS_EXIT_DELAY;
            panelIndication = armEvent->indication;
            timeLeft = armEvent->exitDelay;
            bypassActive = armEvent->panelStatus->bypassActive;
            break;
        }
        case ALARM_EVENT_ARMING:
        {
            armingEvent *armEvent = (armingEvent *) event;
            timeLeft = armEvent->exitDelay;
            panelIndication = armEvent->indication;
            panelStatus = deviceServiceConvertSystemPanelStatus(armEvent->panelStatus, ALARM_REASON_NONE,
                                                                PANIC_ALARM_TYPE_NONE);
            bypassActive = armEvent->panelStatus->bypassActive;
            break;
        }
        case ALARM_EVENT_ALARM:
        case ALARM_EVENT_ALARM_CANCELLED:
        case ALARM_EVENT_ALARM_RESET:
        case ALARM_EVENT_STATE_NOT_READY:
        case ALARM_EVENT_STATE_READY:
        case ALARM_EVENT_PANIC:
        {
            alarmEvent *systemAlarmEvent = (alarmEvent *) event;
            panelStatus = deviceServiceConvertSystemPanelStatus(systemAlarmEvent->panelStatus,
                                                                systemAlarmEvent->alarm->alarmReason,
                                                                systemAlarmEvent->panicType);
            panelIndication = systemAlarmEvent->indication;
            bypassActive = systemAlarmEvent->panelStatus->bypassActive;
            break;
        }
        default:
            panelStatus = PANEL_STATUS_INVALID;
            icLogWarn(LOG_TAG, "Unsupported security service event code [%d]", event->eventCode);
            break;
    }

    if (panelStatus != PANEL_STATUS_INVALID && panelIndication != INDICATION_NONE)
    {
        SecurityIndication indication = deviceServiceConvertSystemIndication(panelIndication);
        AUTO_CLEAN(securityStateDestroy__auto) SecurityState *state = securityStateCreate(panelStatus,
                                                                                          timeLeft,
                                                                                          indication,
                                                                                          bypassActive);
        AUTO_CLEAN(free_generic__auto) const char *stateJson = securityStateToJSON(state);
        deviceServiceWriteResource("^.*/r/" SECURITY_CONTROLLER_PROFILE_RESOURCE_SECURITY_STATE "$",
                                   stateJson);
    }
}

static void handleSecurityZoneEvent(securityZoneEvent *event)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(zoneChangedDestroy__auto) ZoneChanged *zoneChanged = NULL;

    switch (event->baseEvent.eventCode)
    {
        case ZONE_EVENT_FAULT_CODE:
        case ZONE_EVENT_RESTORE_CODE:
        {
            SecurityIndication indication = deviceServiceConvertSystemIndication(event->indication);

            /*
             * PIMs count open zones to set the panel readiness instead of using the actual status from
             * SecurityState. Do not notify them of zone faults that have no effect on panel readiness.
             * Restores will always be passed (e.g., to clear after a function change).
             */
            if (securityZoneFaultPreventsArming(event->zone) == false)
            {
                indication = SECURITY_INDICATION_NONE;
            }

            zoneChanged = zoneChangedCreate((uint8_t) event->zone->displayIndex,
                                            event->zone->label,
                                            event->zone->isFaulted,
                                            event->zone->isBypassed,
                                            event->panelStatus->bypassActive,
                                            indication,
                                            event->baseEvent.eventId,
                                            ZONE_CHANGED_REASON_FAULT_CHANGED);
            break;
        }

        case ZONE_EVENT_REMOVED_CODE:
            zoneChanged = zoneChangedCreate((uint8_t) event->zone->displayIndex,
                                            event->zone->label,
                                            false,
                                            false,
                                            event->panelStatus->bypassActive,
                                            deviceServiceConvertSystemIndication(event->indication),
                                            event->baseEvent.eventId,
                                            ZONE_CHANGED_REASON_CRUD);
            break;

        case ZONE_EVENT_UPDATED_CODE:
        case ZONE_EVENT_ADDED_CODE:
        {
            SecurityIndication indication = deviceServiceConvertSystemIndication(event->indication);

            bool faulted = event->zone->isFaulted;
            if (securityZoneFaultPreventsArming(event->zone) == false)
            {
                /*
                 * PIMs count open zones for panel readiness. If adding or updating a zone to a function that can not
                 * affect panel readiness, fake a restore to maintain correct PIM state without having to manually
                 * restore those zones.
                 */
                indication = SECURITY_INDICATION_NONE;
                faulted = false;
            }

            zoneChanged = zoneChangedCreate((uint8_t) event->zone->displayIndex,
                                            event->zone->label,
                                            faulted,
                                            event->zone->isBypassed,
                                            event->panelStatus->bypassActive,
                                            indication,
                                            event->baseEvent.eventId,
                                            ZONE_CHANGED_REASON_CRUD);
            break;
        }

        case ZONE_EVENT_BYPASSED_CODE:
        case ZONE_EVENT_UNBYPASSED_CODE:
            zoneChanged = zoneChangedCreate((uint8_t) event->zone->displayIndex,
                                            event->zone->label,
                                            event->zone->isFaulted,
                                            event->zone->isBypassed,
                                            event->panelStatus->bypassActive,
                                            deviceServiceConvertSystemIndication(event->indication),
                                            event->baseEvent.eventId,
                                            ZONE_CHANGED_REASON_BYPASS_CHANGED);
            break;

        default:
            //just ignore the other events
            break;
    }

    /*
     * Ignore zone faults for silent events, but allow restores to propagate to
     * devices in case a zone function becomes silent while faulted.
     */
    if(zoneChanged != NULL &&
       (zoneChanged->reason != ZONE_CHANGED_REASON_FAULT_CHANGED ||
        !zoneChanged->faulted ||
        zoneChanged->indication != SECURITY_INDICATION_NONE))
    {
        AUTO_CLEAN(free_generic__auto) const char *json = zoneChangedToJSON(zoneChanged);
        deviceServiceWriteResource("^.*/r/" SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED "$", json);
    }
}

static void handleSecurityZoneReorderEvent(securityZoneReorderEvent *reorderEvent)
{
    if (reorderEvent->baseEvent.eventCode == ZONE_EVENT_REORDER_CODE)
    {
        sbIcLinkedListIterator *it = linkedListIteratorCreate(reorderEvent->zoneList->zoneArray);

        while(linkedListIteratorHasNext(it))
        {
            const securityZone *zone = linkedListIteratorGetNext(it);
            AUTO_CLEAN(zoneChangedDestroy__auto) ZoneChanged *zoneChanged = zoneChangedCreate(zone->displayIndex,
                                                                                              zone->label,
                                                                                              zone->isFaulted,
                                                                                              zone->isBypassed,
                                                                                              reorderEvent->panelStatus->bypassActive,
                                                                                              SECURITY_INDICATION_VISUAL,
                                                                                              reorderEvent->baseEvent.eventId,
                                                                                              ZONE_CHANGED_REASON_REORDER);

            AUTO_CLEAN(free_generic__auto) const char *json = zoneChangedToJSON(zoneChanged);
            deviceServiceWriteResource("^.*/r/" SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED "$", json);
        }
    }
}

