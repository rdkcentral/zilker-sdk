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
 * eventListener.c
 *
 * Determine which system events to listen for, and
 * forward them along to the Engine when received.
 *
 * Author: jelderton - 8/26/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <commMgr/commService_eventAdapter.h>
#include <deviceService/deviceService_eventAdapter.h>
#include <commonDeviceDefs.h>
#include <propsMgr/commonProperties.h>

#include "alarm/alarmPanel.h"
#include "trouble/troubleState.h"
#include "eventListener.h"
#include "common.h"

/*
 * various callbacks for the event listeners we registered as
 */
static void deviceResourceUpdatedNotify(DeviceServiceResourceUpdatedEvent *event);
static void deviceRemovedNotify(DeviceServiceDeviceRemovedEvent *event);
static void deviceDiscoveryCompleteNotify(DeviceServiceDeviceDiscoveryCompletedEvent *event);
static void zigbeeNetworkInterferenceChangedNotify(DeviceServiceZigbeeNetworkInterferenceChangedEvent *event);
static void zigbeePanIdAttackChangedNotify(DeviceServiceZigbeePanIdAttackChangedEvent *event);
static void commTroubleNotify(commOnlineChangedEvent *event);
static void cloudAssociateNotify(cloudAssociationStateChangedEvent *event);

/*
 * register with various services for any event
 * that can be consumed by the zone, trouble,
 * or alarm sub-services.
 *
 * should be called once all of the services
 * are online and ready for processing
 */
void setupSecurityServiceEventListeners()
{
    // register for trouble notifications
    //
    register_DeviceServiceZigbeeNetworkInterferenceChangedEvent_eventListener(zigbeeNetworkInterferenceChangedNotify);
    register_DeviceServiceZigbeePanIdAttackChangedEvent_eventListener(zigbeePanIdAttackChangedNotify);
    register_DeviceServiceResourceUpdatedEvent_eventListener(deviceResourceUpdatedNotify);
    register_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedNotify);
    // For new devices, want to check them after they are fully in
    register_DeviceServiceDeviceDiscoveryCompletedEvent_eventListener(deviceDiscoveryCompleteNotify);

    // register for communication notifications
    register_cloudAssociationStateChangedEvent_eventListener(cloudAssociateNotify);
}

/*
 * called during shutdown to cleanup event listeners
 */
void removeSecurityServiceEventListeners()
{
    // trouble notifications
    //
    unregister_DeviceServiceZigbeeNetworkInterferenceChangedEvent_eventListener(zigbeeNetworkInterferenceChangedNotify);
    unregister_DeviceServiceZigbeePanIdAttackChangedEvent_eventListener(zigbeePanIdAttackChangedNotify);
    unregister_DeviceServiceResourceUpdatedEvent_eventListener(deviceResourceUpdatedNotify);
    unregister_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedNotify);

    // communication notifications
    unregister_cloudAssociationStateChangedEvent_eventListener(cloudAssociateNotify);
}

/*
 * callback from deviceService when a resource on a device changes.
 * We need to identify resources that represent troubles here and whether or not they are troubles vs. trouble cleared.
 */
static void deviceResourceUpdatedNotify(DeviceServiceResourceUpdatedEvent *event)
{
    // got a 'resource changed' event.  need to see if this represents a
    // trouble or trouble-clear
    //
    if (event != NULL && event->resource != NULL)
    {
        // look at the resource id, as that should be our indicator.
        // for example, a comm-fail trouble from a camera would look something like:
        //
        // {  "_evId":	1461184677559433,
        //    "_evCode":	303,
        //    "_evVal":	0,
        //    "_evTime":	1461184677,
        //    "DeviceServiceResourceUpdatedEvent":	{
        //        "rootDeviceId":	"000e8fe0bcac",
        //        "rootDeviceClass":	"camera",
        //        "resource":	{
        //	          "DSResource":	{
        //		        "id":	"communicationFailure",
        //		        "uri":	"/944a0c1c0ad2/r/communicationFailure",
        //		        "ownerId":	"944a0c1c0ad2",
        //		        "ownerClass":	"camera",
        //		        "value":	"true",
        //		        "type":	"com.icontrol.trouble",
        //		        "mode":	1,
        //		        "dateOfLastSyncMillis":	1481039347770
        //	          }
        //         }
        //      },
        //    "_svcIdNum":	19600 }
        // since this is from a device, use the trouble type of TROUBLE_TYPE_DEVICE,
        // however need to determine the contents for the 'payload'.  we'll do this
        // by examining the 'resource id':
        //	"id":"communicationFailure",
        //
        processTroubleForResource(event->resource, NULL, event->rootDeviceId, &event->baseEvent, true, true);
    }
}

/*
 * callback from deviceService when a device is removed/deleted
 */
static void deviceRemovedNotify(DeviceServiceDeviceRemovedEvent *event)
{
    if (event == NULL || event->deviceId == NULL)
    {
        return;
    }

    // find all troubles with a matching 'deviceId', and clear each
    //
    icLogDebug(SECURITY_LOG, "removing any troubles found for %s", event->deviceId);
    clearTroublesForDevicePublic(event->deviceId);
}

/*
 * callback from deviceService once a device is completely discovered/added
 */
static void deviceDiscoveryCompleteNotify(DeviceServiceDeviceDiscoveryCompletedEvent *event)
{
    // For sensors we have to wait for the zone to be created.  Once the zone is created a direct
    // call is made to check the sensor for the zone for troubles
    if (stringCompare(event->device->deviceClass, SENSOR_DC, false) != 0)
    {
        // Add any initial troubles for the device
        checkDeviceForInitialTroubles(event->device->id, false, true);
    }
}

/*
 * callback from deviceService when zigbee network interference is detected or cleared
 */
static void zigbeeNetworkInterferenceChangedNotify(DeviceServiceZigbeeNetworkInterferenceChangedEvent *event)
{
    if (event == NULL)
    {
        return;
    }

    processZigbeeNetworkInterferenceEvent(event);
}

/*
 * callback from deviceService when zigbee pan id attack is detected or cleared
 */
static void zigbeePanIdAttackChangedNotify(DeviceServiceZigbeePanIdAttackChangedEvent *event)
{
    if (event == NULL)
    {
        return;
    }

    processZigbeePanIdAttackEvent(event);
}

/*
 * callback from commService when our cloud association state changes
 */
static void cloudAssociateNotify(cloudAssociationStateChangedEvent *event)
{
    // forward to alarmPanel
    //
    icLogDebug(SECURITY_LOG, "received cloud association event; code=%"PRIi32" val=%"PRIi32, event->baseEvent.eventCode, event->baseEvent.eventValue);
    processCloudAssociationStateChangeEvent(event);
}

