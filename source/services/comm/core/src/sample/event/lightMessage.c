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
 * lightMessage.c
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a Light to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include "sample/sampleChMessage.h"
#include "lightMessage.h"

/*
 * formatSampleMessageFunc implementation to format our message
 * in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatLightCrudMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the light
    sampleMessage *internal = extractSampleMessage(msg);
    Light *light = (Light *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply put a bunch of details from the Light into a string buffer
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Light:");
    stringBufferAppend(retVal, "\n  DeviceID: ");
    stringBufferAppend(retVal, stringCoalesce(light->deviceId));
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(light->endpointId));
    stringBufferAppend(retVal, "\n  Label: ");
    stringBufferAppend(retVal, stringCoalesce(light->label));
    stringBufferAppend(retVal, "\n  Is On: ");
    stringBufferAppend(retVal, stringValueOfBool(light->isOn));
    stringBufferAppend(retVal, "\n  Is Dimable: ");
    stringBufferAppend(retVal, stringValueOfBool(light->isDimable));
    stringBufferAppend(retVal, "\n  Manufacturer: ");
    stringBufferAppend(retVal, stringCoalesce(light->manufacturer));
    stringBufferAppend(retVal, "\n  Model: ");
    stringBufferAppend(retVal, stringCoalesce(light->model));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'Light' object provided at creation
 */
static void lightCrudMessageDestroyFunc(message *msg)
{
    if (msg != NULL)
    {
        // extract our internal container
        sampleMessage *internal = extractSampleMessage(msg);

        // free the Light
        Light *light = (Light *)internal->sampleMsgData;
        destroyLight(light);
        internal->sampleMsgData = NULL;

        // since we replaced the default 'cleanup' function, need
        // to remove the sampleMsg specific mem in msg->userData
        //
        destroySampleMessage(internal);
        msg->userData = NULL;
    }
}

/*
 * formatSampleMessageFunc implementation to format our 'light removed'
 * message in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatLightRemoveMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the light's endpointId
    sampleMessage *internal = extractSampleMessage(msg);
    char *endpointId = (char *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply show the endpoint of the light that was removed
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Light (removed):");
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(endpointId));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'deviceId' provided at creation
 */
static void lightRemoveMessageDestroyFunc(message *msg)
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
 * a new light has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createLightAddedMessage(Light *light)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = lightCrudMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Light and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatLightCrudMessageFormatter;
    internal->sampleMsgData = light;

    return retVal;
}

/*
 * create a message to inform the server that
 * a light was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createLightRemovedMessage(char *lightEndpointId)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = lightRemoveMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Light and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatLightRemoveMessageFormatter;
    if (lightEndpointId != NULL)
    {
        internal->sampleMsgData = strdup(lightEndpointId);
    }

    return retVal;
}

/*
 * create a message to inform the server that
 * a light was updated (label, on/off, level, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createLightUpdatedMessage(Light *light)
{
    // for this sample channel, same as "add"
    return createLightAddedMessage(light);
}