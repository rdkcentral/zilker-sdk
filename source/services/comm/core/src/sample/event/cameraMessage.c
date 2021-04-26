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
 * cameraMessage.c
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a Camera to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include "sample/sampleChMessage.h"
#include "cameraMessage.h"

/*
 * formatSampleMessageFunc implementation to format our message
 * in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatCameraCrudMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the camera
    sampleMessage *internal = extractSampleMessage(msg);
    Camera *camera = (Camera *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply put a bunch of details from the Camera into a string buffer
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Camera:");
    stringBufferAppend(retVal, "\n  DeviceID: ");
    stringBufferAppend(retVal, stringCoalesce(camera->deviceId));
    stringBufferAppend(retVal, "\n  Label: ");
    stringBufferAppend(retVal, stringCoalesce(camera->label));
    stringBufferAppend(retVal, "\n  Manufacturer: ");
    stringBufferAppend(retVal, stringCoalesce(camera->manufacturer));
    stringBufferAppend(retVal, "\n  Model: ");
    stringBufferAppend(retVal, stringCoalesce(camera->model));
    stringBufferAppend(retVal, "\n  SerialNumber: ");
    stringBufferAppend(retVal, stringCoalesce(camera->serialNumber));
    stringBufferAppend(retVal, "\n  MACAddress: ");
    stringBufferAppend(retVal, stringCoalesce(camera->macAddress));
    stringBufferAppend(retVal, "\n  IPAddress: ");
    stringBufferAppend(retVal, stringCoalesce(camera->ipAddress));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'Camera' object provided at creation
 */
static void cameraCrudMessageDestroyFunc(message *msg)
{
    if (msg != NULL)
    {
        // extract our internal container
        sampleMessage *internal = extractSampleMessage(msg);

        // free the Camera
        Camera *camera = (Camera *)internal->sampleMsgData;
        destroyCamera(camera);
        internal->sampleMsgData = NULL;

        // since we replaced the default 'cleanup' function, need
        // to remove the sampleMsg specific mem in msg->userData
        //
        destroySampleMessage(internal);
        msg->userData = NULL;
    }
}

/*
 * formatSampleMessageFunc implementation to format our 'camera removed'
 * message in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatCameraRemoveMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the camera's endpointId
    sampleMessage *internal = extractSampleMessage(msg);
    char *endpointId = (char *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply show the endpoint of the camera that was removed
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Camera (removed):");
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(endpointId));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'deviceId' provided at creation
 */
static void cameraRemoveMessageDestroyFunc(message *msg)
{
    if (msg != NULL)
    {
        // extract our internal container
        sampleMessage *internal = extractSampleMessage(msg);

        // free the deviceId
        char *deviceId = (char *)internal->sampleMsgData;
        free(deviceId);
        internal->sampleMsgData = NULL;

        // since we replaced the default 'cleanup' function, need
        // to remove the sampleMsg specific mem in msg->userData
        //
        destroySampleMessage(internal);
        msg->userData = NULL;
    }
}

/*
 * create a message to inform the server that
 * a new camera has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createCameraAddedMessage(Camera *camera)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = cameraCrudMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Camera and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatCameraCrudMessageFormatter;
    internal->sampleMsgData = camera;

    return retVal;
}

/*
 * create a message to inform the server that
 * a camera was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createCameraRemovedMessage(char *cameraEndpointId)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = cameraRemoveMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Camera and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatCameraRemoveMessageFormatter;
    if (cameraEndpointId != NULL)
    {
        internal->sampleMsgData = strdup(cameraEndpointId);
    }

    return retVal;
}

/*
 * create a message to inform the server that
 * a camera was updated (label, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createCameraUpdatedMessage(Camera *camera)
{
    // for this sample channel, same as "add"
    return createCameraAddedMessage(camera);
}
