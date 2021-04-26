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
 * thermostatMessage.c
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a Thermostat to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include "sample/sampleChMessage.h"
#include "thermostatMessage.h"

/*
 * formatSampleMessageFunc implementation to format our message
 * in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatThermostatCrudMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the thermostat
    sampleMessage *internal = extractSampleMessage(msg);
    Thermostat *thermostat = (Thermostat *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply put a bunch of details from the Thermostat into a string buffer
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Thermostat:");
    stringBufferAppend(retVal, "\n  DeviceID: ");
    stringBufferAppend(retVal, stringCoalesce(thermostat->deviceId));
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(thermostat->endpointId));
    stringBufferAppend(retVal, "\n  Label: ");
    stringBufferAppend(retVal, stringCoalesce(thermostat->label));
    stringBufferAppend(retVal, "\n  System On: ");
    stringBufferAppend(retVal, stringValueOfBool(thermostat->systemOn));
    stringBufferAppend(retVal, "\n  Fan On: ");
    stringBufferAppend(retVal, stringValueOfBool(thermostat->fanOn));
    stringBufferAppend(retVal, "\n  Manufacturer: ");
    stringBufferAppend(retVal, stringCoalesce(thermostat->manufacturer));
    stringBufferAppend(retVal, "\n  Model: ");
    stringBufferAppend(retVal, stringCoalesce(thermostat->model));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'Thermostat' object provided at creation
 */
static void thermostatCrudMessageDestroyFunc(message *msg)
{
    if (msg != NULL)
    {
        // extract our internal container
        sampleMessage *internal = extractSampleMessage(msg);

        // free the Thermostat
        Thermostat *thermostat = (Thermostat *)internal->sampleMsgData;
        destroyThermostat(thermostat);
        internal->sampleMsgData = NULL;

        // since we replaced the default 'cleanup' function, need
        // to remove the sampleMsg specific mem in msg->userData
        //
        destroySampleMessage(internal);
        msg->userData = NULL;
    }
}

/*
 * formatSampleMessageFunc implementation to format our 'thermostat removed'
 * message in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatThermostatRemoveMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the thermostat's endpointId
    sampleMessage *internal = extractSampleMessage(msg);
    char *endpointId = (char *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply show the endpoint of the thermostat that was removed
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Thermostat (removed):");
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(endpointId));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'deviceId' provided at creation
 */
static void thermostatRemoveMessageDestroyFunc(message *msg)
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
 * a new thermostat has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createThermostatAddedMessage(Thermostat *thermostat)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = thermostatCrudMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Sensor and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatThermostatCrudMessageFormatter;
    internal->sampleMsgData = thermostat;

    return retVal;
}

/*
 * create a message to inform the server that
 * a thermostat was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createThermostatRemovedMessage(char *thermostatEndpointId)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = thermostatRemoveMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Sensor and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatThermostatRemoveMessageFormatter;
    if (thermostatEndpointId != NULL)
    {
        internal->sampleMsgData = strdup(thermostatEndpointId);
    }

    return retVal;
}

/*
 * create a message to inform the server that
 * a thermostat was updated (label, on/off, mode, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createThermostatUpdatedMessage(Thermostat *thermostat)
{
    // for this sample channel, same as "add"
    return createThermostatAddedMessage(thermostat);
}
