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
 * sampleChannel.c
 *
 * sample channel implementation 
 *
 * Author: jelderton - 9/10/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <deviceService/deviceService_eventAdapter.h>
#include <commonDeviceDefs.h>
#include <deviceService/deviceService_ipc.h>
#include <camera.h>
#include <sensor.h>
#include <light.h>
#include <doorlock.h>
#include <thermostat.h>
#include <sensorHelper.h>
#include "sample/event/sensorMessage.h"
#include "sample/event/lightMessage.h"
#include "sample/event/cameraMessage.h"
#include "sample/event/doorlockMessage.h"
#include "sample/event/thermostatMessage.h"
#include "sampleChMessage.h"
#include "../messageQueue.h"
#include "../channelManager.h"
#include "../commServiceCommon.h"
#include "../commServiceEventBroadcaster.h"

#define SAMPLE_CHANNEL_NAME             "sample"
#define MAX_CONCURRENT_MESSAGE_COUNT    3       // for messageQueue

/*
 * private function declarations
 */

// connectivity functions
//
static void sampleChannelShutdown();                  // channel->shutdownFunc
static bool sampleChannelIsEnabled();                 // channel->isEnabledFunc
static void sampleChannelSetEnabled(bool enabled);    // channel->setEnabledFunc
static void sampleChannelConnect(bool useCell);       // channel->connectFunc
static void sampleChannelDisconnect();                // channel->disconnectFunc

// message processing functions
//
static bool sampleChannelRequest(message *msg);       // channel->requestFunc
static bool sampleMessageMeetsFilter(message *msg);
static void sampleMessageNotify(message *msg, bool success, messageFailureReason reason);
static processMessageCode sampleMessageProcesses(message *msg);

// state functions
//
static channelState sampleChannelGetState();          // channel->getStateFunc
static channelConnectionState sampleChannelGetConnectionState();
static void sampleGetDetailStatus(commChannelStatus *output);
static void sampleGetRuntimeStatus(serviceStatusPojo *output);
static void sampleGetRuntimeStatistics(runtimeStatsPojo *container, bool thenClear);

// activation functions
//
static bool sampleChannelIsActivated();               // channel->isActivatedFunc

// event callback functions
//
static void propertyChangedNotify(cpePropertyEvent *event);
static void deviceAddedNotify(DeviceServiceDeviceAddedEvent *event);
static void endpointAddedNotify(DeviceServiceEndpointAddedEvent *event);
static void deviceRemovedNotify(DeviceServiceDeviceRemovedEvent *event);
static void endpointRemovedNotify(DeviceServiceEndpointRemovedEvent *event);
static void deviceResourceUpdatedNotify(DeviceServiceResourceUpdatedEvent *event);


/*
 * private variables
 */

// primary variables
static pthread_mutex_t SAMPLE_CHANNEL_MTX  = PTHREAD_MUTEX_INITIALIZER;
static messageQueueDelegate *queueDelegate = NULL;
static messageQueue *queue = NULL;          // SMAP/CSMAP messages to deliver
static bool         isEnabled  = true;
static bool         doShutdown = false;

/*
 * create the sample channel object.  will populate the
 * function pointers within the 'channel'
 */
channel *createSampleChannel()
{
    icLogDebug(COMM_LOG, "sample: creating channel");

    // allocate the channel object and assign function pointers so
    // that our object implements the abstract channel object
    //
    channel *retVal = (channel *)calloc(1, sizeof(channel));
    retVal->getStateFunc = sampleChannelGetState;
    retVal->getConnectStateFunc = sampleChannelGetConnectionState;
    retVal->isEnabledFunc = sampleChannelIsEnabled;
    retVal->setEnabledFunc = sampleChannelSetEnabled;
    retVal->connectFunc = sampleChannelConnect;
    retVal->disconnectFunc = sampleChannelDisconnect;
    retVal->shutdownFunc = sampleChannelShutdown;
    retVal->requestFunc = sampleChannelRequest;
    retVal->getStatusDetailsFunc = sampleGetDetailStatus;
    retVal->getRuntimeStatusFunc = sampleGetRuntimeStatus;
    retVal->getRuntimeStatisticsFunc = sampleGetRuntimeStatistics;

    // save our assigned identifier and set initial state
    //
    retVal->id = SAMPLE_CHANNEL_ID;

    // TODO: load configuration
    //

    // TODO: init local variables
    //

    // to create the message queue, need to provide function pointers
    // via the messageQueueDelegate object
    //
    queueDelegate = (messageQueueDelegate *)calloc(1, sizeof(messageQueueDelegate));
    queueDelegate->filterFunc = sampleMessageMeetsFilter;
    queueDelegate->processFunc = sampleMessageProcesses;
    queueDelegate->notifyFunc = sampleMessageNotify;
    queue = messageQueueCreate(queueDelegate, MAX_CONCURRENT_MESSAGE_COUNT, 30);

    pthread_mutex_lock(&SAMPLE_CHANNEL_MTX);

    // setup event listeners
    //
    register_cpePropertyEvent_eventListener(propertyChangedNotify);
    register_DeviceServiceDeviceAddedEvent_eventListener(deviceAddedNotify);
    register_DeviceServiceEndpointAddedEvent_eventListener(endpointAddedNotify);
    register_DeviceServiceEndpointRemovedEvent_eventListener(endpointRemovedNotify);
    register_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedNotify);
    register_DeviceServiceResourceUpdatedEvent_eventListener(deviceResourceUpdatedNotify);

    pthread_mutex_unlock(&SAMPLE_CHANNEL_MTX);
    return retVal;
}

/*-------------------------------*
 *
 * connectivity functions
 *
 *-------------------------------*/

/*
 * grabs the lock, then checks the shutdown state
 */
static bool doingShutdown()
{
    pthread_mutex_lock(&SAMPLE_CHANNEL_MTX);
    bool retVal = doShutdown;
    pthread_mutex_unlock(&SAMPLE_CHANNEL_MTX);

    return retVal;
}

/*
 * sampleChannel implementation of 'channelShutdownFunc'.
 * assume we're on the way out...so kill off all threads
 */
static void sampleChannelShutdown()
{
    icLogDebug(COMM_LOG, "Shutting down sample channel");

    // remove event listeners
    //
    unregister_cpePropertyEvent_eventListener(propertyChangedNotify);
    unregister_DeviceServiceDeviceAddedEvent_eventListener(deviceAddedNotify);
    unregister_DeviceServiceEndpointAddedEvent_eventListener(endpointAddedNotify);
    unregister_DeviceServiceEndpointRemovedEvent_eventListener(endpointRemovedNotify);
    unregister_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedNotify);
    unregister_DeviceServiceResourceUpdatedEvent_eventListener(deviceResourceUpdatedNotify);

    // TODO: disconnect from the server and cleanup.
    //

    // stop, then kill the message queue
    //
    pthread_mutex_lock(&SAMPLE_CHANNEL_MTX);
    doShutdown = true;
    messageQueueStopThread(queue, true);
    messageQueueDestroy(queue);
    free(queueDelegate);
    queueDelegate = NULL;
    queue = NULL;
    pthread_mutex_unlock(&SAMPLE_CHANNEL_MTX);
}

/*
 * sampleChannel implementation of 'channelIsEnabledFunc'
 */
static bool sampleChannelIsEnabled()
{
    pthread_mutex_lock(&SAMPLE_CHANNEL_MTX);
    bool retVal = isEnabled;
    pthread_mutex_unlock(&SAMPLE_CHANNEL_MTX);

   return retVal;
}

/*
 * sampleChannel implementation of 'channelSetEnabledFunc'
 */
static void sampleChannelSetEnabled(bool enabled)
{
    pthread_mutex_lock(&SAMPLE_CHANNEL_MTX);
    if (isEnabled != enabled)
    {
        // save new flag
        //
        isEnabled = enabled;
    }
    pthread_mutex_unlock(&SAMPLE_CHANNEL_MTX);
}

/*
 * sampleChannel implementation of 'channelConnectFunc'
 */
static void sampleChannelConnect(bool useCell)
{
    if (doingShutdown() == true)
    {
        return;
    }

    // TODO: connect to a server
}

/*
 * sampleChannel implementation of 'channelDisconnectFunc'
 */
static void sampleChannelDisconnect()
{
    if (doingShutdown() == true)
    {
        return;
    }

    // TODO: disconnect from the server
}

/*-------------------------------*
 *
 * message processing functions
 *
 *-------------------------------*/

/* Overview of message processing:
 *
 * 1. a message is created and added to our messageQueue:
 *    sampleChannelReqest(msg)
 *
 * 2. the messageQueue will ask if the message meets the 'filter':
 *    sampleMessageMeetsFilter(msg)
 *
 * 3. when ready for delivery, the messageQueue will pass the message over for processing:
 *    sampleMessageProcesses(msg)
 *
 * 4. once the server response is received, subChannel will pass that response back:
 *    messageResponseReceived(payload)
 *
 * 5. locate the message that correlates to the payload, parse the payload, then delete the message
 */

/*
 * adds a message to our messageQueue.
 * sampleChannel implementation of 'channelRequestFunc'.
 */
static bool sampleChannelRequest(message *msg)
{
    // ignore if shutting down or disabled due to account suspension/deactivation
    //
    if (doingShutdown() == true)
    {
        icLogDebug(COMM_LOG, "sample: ignoring 'process message' request; shutting down");
        return false;
    }

    // add this message to our queue for processing.  let
    // the filtering of the queue deal with allowing this
    // to go now or cache for later.
    //
    if (msg != NULL && queue != NULL)
    {
        messageQueueAppend(queue, msg);
        return true;
    }
    return false;
}

/*
 * sampleChannel implementation of 'messageMeetsFilter' (callback from messageQueue).
 * used to filter message objects within our 'queue' based on our current connection status
 */
static bool sampleMessageMeetsFilter(message *msg)
{
    // normally this would examine the message and determine if it's
    // allowed to be sent to the server at this time.  for now just
    // always return 'yes'
    // TODO: Needs implementation
    //
    return true;
}

/*
 * sampleChannel implementation of 'messageNotifyFunc' (callback from messageQueue).
 */
static void sampleMessageNotify(message *msg, bool success, messageFailureReason reason)
{
    if (msg == NULL)
    {
        return;
    }

    // called by messageQueue when it's done with this object.  need to call the
    // message callback (if there), then delete the object
    //
    if (success == true)
    {
        // notify callback (if applicable)
        //
        if (msg->successCallback != NULL)
        {
            msg->successCallback(msg);
        }
    }
    else
    {
        if (msg->failureCallback != NULL)
        {
            msg->failureCallback(msg);
        }
    }

    // Safe to destroy message; it is no longer in the queue
    //
    destroyMessage(msg);
    msg = NULL;
}

/*
 * sampleChannel implementation of 'messageProcesses' (callback from messageQueue).
 * used to dispatch message objects to the server over the appropriate subchannel.
 */
static processMessageCode sampleMessageProcesses(message *msg)
{
    // bail early if on the way out the door
    //
    if (doingShutdown() == true)
    {
        return PROCESS_MSG_SEND_FAILURE;
    }

    // normally this is where one would marshall message and send to server.
    // for now we will just log the fact that the message was processed.
    // TODO: Needs implementation
    //
    sampleMessage *internal = extractSampleMessage(msg);
    if (internal != NULL && internal->encodeMessageFunc != NULL)
    {
        // format the message then log it
        //
        icStringBuffer *payload = internal->encodeMessageFunc(msg, PAYLOAD_FORMAT_JSON);
        if (payload != NULL)
        {
            char *data = stringBufferToString(payload);
            icLogInfo(COMM_LOG, "formatted message into '%s'", data);
            free(data);
            stringBufferDestroy(payload);
        }
    }

    // at this time all of our messages do NOT expect a reply, so no need to
    // inform the messageQueue that the message was received by the server.
    //

    return true;
}

/*-------------------------------*
 *
 * state functions
 *
 *-------------------------------*/

/*
 * sampleChannel implementation of 'channelGetStateFunc'
 */
static channelState sampleChannelGetState()
{
    channelState retVal = CHANNEL_STATE_DOWN;

    if (doingShutdown() == true)
    {
        return retVal;
    }

    // TODO: Needs implementation
    return retVal;
}

/*
 * channel->getConnectStateFunc
 */
static channelConnectionState sampleChannelGetConnectionState()
{
    channelConnectionState retVal = CHANNEL_CONNECT_INTERNAL_ERROR;
    if (doingShutdown() == true)
    {
        return retVal;
    }

    // TODO: Needs implementation
    return retVal;
}

/*
 * obtain detailed status info for the IPC call 
 */
static void sampleGetDetailStatus(commChannelStatus *output)
{
    // fill in the easy stuff
    //
    output->enabled = sampleChannelIsEnabled();
    output->channelId = strdup(SAMPLE_CHANNEL_NAME);

    if (doingShutdown() == true)
    {
        return;
    }

    // TODO: Needs implementation
    // fill in more data
}

/*
 * obtain current status, and shove into the commStatusPojo
 * for external processes to gather details about our state
 */
void sampleGetRuntimeStatus(serviceStatusPojo *output)
{
    if (doingShutdown() == true)
    {
        return;
    }

    // queue size
    //
    put_int_in_serviceStatusPojo(output, "SampleQueueSize", messageQueueAllSetCount(queue));

    // TODO: Needs implementation
    // fill in more data
}

/*
 * sampleChannel implementation of 'channelGetStatisticsFunc'.
 * collect statistics about the SMAP messages to/from the server,
 * and populate them into the supplied runtimeStatsPojo container
 */
static void sampleGetRuntimeStatistics(runtimeStatsPojo *container, bool thenClear)
{
    if (doingShutdown() == true)
    {
        return;
    }

    // add how many message are in the queue
    //
    put_int_in_runtimeStatsPojo(container, "SampleQueueSize", messageQueueAllSetCount(queue));

    // TODO: Needs implementation
    // fill in more data
}

/*-------------------------------*
 *
 * activation functions
 *
 *-------------------------------*/

/*
 * sampleChannel implementation of 'channelIsActivatedFunc'
 */
static bool sampleChannelIsActivated()
{
    return true;
}

/*-------------------------------*
 *
 * event callback functions
 *
 *-------------------------------*/


/*
 * callback from PropsService when a CPE property is added/edited/deleted
 */
static void propertyChangedNotify(cpePropertyEvent *event)
{
    if (event == NULL || event->propKey == NULL)
    {
        return;
    }

    // TODO: look for special properties that could dictate behavior
}


/*
 * callback from deviceService when a new device (physical device) is added
 */
static void deviceAddedNotify(DeviceServiceDeviceAddedEvent *event)
{
    // sanity check
    if (event == NULL || event->details == NULL || event->details->deviceId == NULL || event->details->deviceClass == NULL)
    {
        return;
    }

    // some devices are captured here, but the bulk occur with the endpoint is added
    //
    if (stringCompare(CAMERA_DC, event->details->deviceClass, false) == 0)
    {
        // added a camera, so convert to a Camera simplistic object then queue
        // the message so we can send it to a server
        //
        icLogDebug(COMM_LOG, "received 'camera added' event; id=%s", event->details->deviceId);
        DSDevice *camDevice = create_DSDevice();
        if (deviceService_request_GET_DEVICE_BY_ID(event->details->deviceId, camDevice) == IPC_SUCCESS)
        {
            // create camera message
            Camera *cam = createCameraFromDevice(camDevice);
            message *msg = createCameraAddedMessage(cam);
            if (sampleChannelRequest(msg) == false)
            {
                icLogWarn(COMM_LOG, "unable to queue 'new camera' message");
                destroyMessage(msg);
            }
        }
        destroy_DSDevice(camDevice);
    }
}

/*
 * callback from deviceService when a new endpoint (logical device) is added
 */
static void endpointAddedNotify(DeviceServiceEndpointAddedEvent *event)
{
    // sanity check
    if (event == NULL || event->details == NULL || event->details->uri == NULL || event->details->profile == NULL)
    {
        return;
    }

    // Note that the memory for 'event' is owned by the event delivery thread, so if
    // we need any of this information it will need to be cloned.  Assume that we are interested
    // in informing the server of this new Endpoint, and gather the Endpoint details up-front
    //
    DSEndpoint *endpoint = create_DSEndpoint();
    if (deviceService_request_GET_ENDPOINT_BY_URI(event->details->uri, endpoint) != IPC_SUCCESS)
    {
        icLogWarn(COMM_LOG, "Unable to get DSEndpoint for uri %s", event->details->uri);
        destroy_DSEndpoint(endpoint);
        return;
    }

    // we need to peek into the endpoint profile so we know
    // what type of endpoint was added.
    //
    if (stringCompare(SENSOR_PROFILE, event->details->profile, false) == 0)
    {
        // got a "sensor added" event, so convert to sensor object and stuff into a message
        // so it can be sent to our server.
        //
        icLogDebug(COMM_LOG, "received 'sensor added' event; uri=%s", event->details->uri);
        Sensor *sensor = createSensorFromEndpoint(endpoint);
        message *msg = createSensorAddedMessage(sensor);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'new sensor' message");
            destroyMessage(msg);
        }
    }
    else if (stringCompare(LIGHT_PROFILE, event->details->profile, false) == 0)
    {
        // got a "light added" event
        //
        icLogDebug(COMM_LOG, "received 'light added' event; uri=%s", event->details->uri);
        Light *light = createLightFromEndpoint(endpoint);
        message *msg = createLightAddedMessage(light);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'new light' message");
            destroyMessage(msg);
        }
    }
    else if (stringCompare(DOORLOCK_PROFILE, event->details->profile, false) == 0)
    {
        // got a "door lock added" event
        //
        icLogDebug(COMM_LOG, "received 'door lock added' event; uri=%s", event->details->uri);
        DoorLock *lock = createDoorLockFromEndpoint(endpoint);
        message *msg = createDoorLockAddedMessage(lock);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'new door lock' message");
            destroyMessage(msg);
        }
    }
    else if (stringCompare(THERMOSTAT_PROFILE, event->details->profile, false) == 0)
    {
        // got a "thermostat added" event
        //
        icLogDebug(COMM_LOG, "received 'thermostat added' event; uri=%s", event->details->uri);
        Thermostat *tstat = createThermostatFromEndpoint(endpoint);
        message *msg = createThermostatAddedMessage(tstat);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'new thermostat' message");
            destroyMessage(msg);
        }
    }
    else
    {
        icLogWarn(COMM_LOG, "Ignoring endpoint added event for profile %s; class not yet implemented", event->details->profile);
    }

    // mem cleanup
    destroy_DSEndpoint(endpoint);
}

/*
 * callback from deviceService when a device is removed/deleted
 */
static void deviceRemovedNotify(DeviceServiceDeviceRemovedEvent *event)
{
    // sanity check
    if (event == NULL || event->deviceId == NULL || event->deviceClass == NULL)
    {
        return;
    }

    // some devices are captured here, but the bulk occur with the endpoint is added
    //
    if (stringCompare(CAMERA_DC, event->deviceClass, false) == 0)
    {
        // removed a camera.  all of the information is gone, so all we can do
        // is report the device identifier to the server
        //
        icLogDebug(COMM_LOG, "received 'camera deleted' event; id=%s", event->deviceId);
        message *msg = createCameraRemovedMessage(event->deviceId);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'deleted camera' message");
            destroyMessage(msg);
        }
    }
}

/*
 * callback from deviceService when a new endpoint (logical device) is removed
 */
static void endpointRemovedNotify(DeviceServiceEndpointRemovedEvent *event)
{
    if (event == NULL || event->endpoint == NULL || event->endpoint->id == NULL || event->endpoint->profile == NULL)
    {
        return;
    }

    // note that all of the information is gone about the endpoint, so all we can do
    // is report the device identifier to the server
    //
    // unfortunately we need to peek into the endpoint profile so we know
    // what type of endpoint was removed.
    //
    if (stringCompare(SENSOR_PROFILE, event->endpoint->profile, false) == 0)
    {
        icLogDebug(COMM_LOG, "received 'sensor removed' event; id=%s", event->endpoint->id);
        message *msg = createSensorRemovedMessage(event->endpoint->id);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'deleted sensor' message");
            destroyMessage(msg);
        }
    }
    else if (strcmp(LIGHT_PROFILE, event->endpoint->profile) == 0)
    {
        icLogDebug(COMM_LOG, "received 'light removed' event; id=%s", event->endpoint->id);
        message *msg = createLightRemovedMessage(event->endpoint->id);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'deleted light' message");
            destroyMessage(msg);
        }
    }
    else if (strcmp(DOORLOCK_PROFILE, event->endpoint->profile) == 0)
    {
        icLogDebug(COMM_LOG, "received 'door lock removed' event; id=%s", event->endpoint->id);
        message *msg = createDoorLockRemovedMessage(event->endpoint->id);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'deleted door lock' message");
            destroyMessage(msg);
        }
    }
    else if (strcmp(THERMOSTAT_PROFILE, event->endpoint->profile) == 0)
    {
        icLogDebug(COMM_LOG, "received 'thermostat removed' event; id=%s", event->endpoint->id);
        message *msg = createThermostatRemovedMessage(event->endpoint->id);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'deleted thermostat' message");
            destroyMessage(msg);
        }
    }
    else
    {
        icLogWarn(COMM_LOG, "Ignoring endpoint removed event for profile %s; class not yet implemented",
                  event->endpoint->profile);
    }
}

/*
 * callback from deviceService when a device has a change to one of its resources
 */
static void deviceResourceUpdatedNotify(DeviceServiceResourceUpdatedEvent *event)
{
    if (event == NULL || event->resource == NULL || event->resource->uri == NULL)
    {
        return;
    }

    // Note that the memory for 'event' is owned by the event delivery thread, so if
    // we need any of this information it will need to be cloned.  Assume that we are interested
    // in informing the server of this update Endpoint/Device.
    //
    // handle special-case of Camera first since it's a Device not an Endpoint
    //
    if (stringCompare(event->rootDeviceClass, CAMERA_DC, false) == 0)
    {
        // TODO: get the camera device
        icLogDebug(COMM_LOG, "received 'camera updated' event; id=%s", event->rootDeviceId);
        DSDevice *camDevice = create_DSDevice();
        if (deviceService_request_GET_DEVICE_BY_ID(event->rootDeviceId, camDevice) == IPC_SUCCESS)
        {
            // create camera message
            Camera *cam = createCameraFromDevice(camDevice);
            message *msg = createCameraUpdatedMessage(cam);
            if (sampleChannelRequest(msg) == false)
            {
                icLogWarn(COMM_LOG, "unable to queue 'updated camera' message");
                destroyMessage(msg);
            }
        }
        destroy_DSDevice(camDevice);
        return;
    }

    // all the others need the Endpoint
    //
    DSEndpoint *endpoint = create_DSEndpoint();
    if (deviceService_request_GET_ENDPOINT_BY_URI(event->resource->uri, endpoint) != IPC_SUCCESS)
    {
        icLogWarn(COMM_LOG, "Unable to get DSEndpoint for uri %s", event->resource->uri);
        destroy_DSEndpoint(endpoint);
        return;
    }

    // look at the profile to determine what type of device was modified
    //
    if (stringCompare(SENSOR_PROFILE, endpoint->profile, false) == 0)
    {
        // got a "sensor updated" event, so convert to sensor object and stuff into a message
        // so it can be sent to our server.
        //
        icLogDebug(COMM_LOG, "received 'sensor updated' event; uri=%s", event->resource->uri);
        Sensor *sensor = createSensorFromEndpoint(endpoint);

        // check for fault/restore
        message *msg = NULL;
        if (isEndpointFaultedViaEvent(event) == true)
        {
            msg = createSensorFaultRestoreMessage(sensor);
        }
        else
        {
            msg = createSensorUpdatedMessage(sensor);
        }
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'updated sensor' message");
            destroyMessage(msg);
        }
    }
    else if (stringCompare(LIGHT_PROFILE, endpoint->profile, false) == 0)
    {
        // got a "light added" event
        //
        icLogDebug(COMM_LOG, "received 'light updated' event; uri=%s", event->resource->uri);
        Light *light = createLightFromEndpoint(endpoint);
        message *msg = createLightUpdatedMessage(light);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'updated light' message");
            destroyMessage(msg);
        }
    }
    else if (stringCompare(DOORLOCK_PROFILE, endpoint->profile, false) == 0)
    {
        // got a "door lock added" event
        //
        icLogDebug(COMM_LOG, "received 'door lock updated' event; uri=%s", event->resource->uri);
        DoorLock *lock = createDoorLockFromEndpoint(endpoint);
        message *msg = createDoorLockUpdatedMessage(lock);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'updated door lock' message");
            destroyMessage(msg);
        }
    }
    else if (stringCompare(THERMOSTAT_PROFILE, endpoint->profile, false) == 0)
    {
        // got a "thermostat added" event
        //
        icLogDebug(COMM_LOG, "received 'thermostat updated' event; uri=%s", event->resource->uri);
        Thermostat *tstat = createThermostatFromEndpoint(endpoint);
        message *msg = createThermostatUpdatedMessage(tstat);
        if (sampleChannelRequest(msg) == false)
        {
            icLogWarn(COMM_LOG, "unable to queue 'updated thermostat' message");
            destroyMessage(msg);
        }
    }

    // cleanup
    destroy_DSEndpoint(endpoint);
}
