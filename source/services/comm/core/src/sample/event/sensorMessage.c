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
 * sensorMessage.c
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a Sensor to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include "sample/sampleChMessage.h"
#include "sensorMessage.h"

/*
 * formatSampleMessageFunc implementation to format our message
 * in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatSensorCrudMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the sensor
    sampleMessage *internal = extractSampleMessage(msg);
    Sensor *sensor = (Sensor *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply put a bunch of details from the Sensor into a string buffer
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Sensor:");
    stringBufferAppend(retVal, "\n  DeviceID: ");
    stringBufferAppend(retVal, stringCoalesce(sensor->deviceId));
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(sensor->endpointId));
    stringBufferAppend(retVal, "\n  Label: ");
    stringBufferAppend(retVal, stringCoalesce(sensor->label));
    stringBufferAppend(retVal, "\n  Type: ");
    stringBufferAppend(retVal, sensorTypeLabels[sensor->type]);
    stringBufferAppend(retVal, "\n  Faulted: ");
    stringBufferAppend(retVal, stringValueOfBool(sensor->isFaulted));
    stringBufferAppend(retVal, "\n  Troubled: ");
    stringBufferAppend(retVal, stringValueOfBool(sensor->isTroubled));
    stringBufferAppend(retVal, "\n  Manufacturer: ");
    stringBufferAppend(retVal, stringCoalesce(sensor->manufacturer));
    stringBufferAppend(retVal, "\n  Model: ");
    stringBufferAppend(retVal, stringCoalesce(sensor->model));
    stringBufferAppend(retVal, "\n  SerialNumber: ");
    stringBufferAppend(retVal, stringCoalesce(sensor->serialNumber));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'Sensor' object provided at creation
 */
static void sensorCrudMessageDestroyFunc(message *msg)
{
    if (msg != NULL)
    {
        // extract our internal container
        sampleMessage *internal = extractSampleMessage(msg);

        // free the Sensor
        Sensor *sensor = (Sensor *)internal->sampleMsgData;
        destroySensor(sensor);
        internal->sampleMsgData = NULL;

        // since we replaced the default 'cleanup' function, need
        // to remove the sampleMsg specific mem in msg->userData
        //
        destroySampleMessage(internal);
        msg->userData = NULL;
    }
}

/*
 * formatSampleMessageFunc implementation to format our 'sensor removed'
 * message in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatSensorRemoveMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the sensor's endpointId
    sampleMessage *internal = extractSampleMessage(msg);
    char *endpointId = (char *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply show the endpoint of the sensor that was removed
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "Sensor (removed):");
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(endpointId));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'deviceId' provided at creation
 */
static void sensorRemoveMessageDestroyFunc(message *msg)
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
 * a new sensor has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorAddedMessage(Sensor *sensor)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = sensorCrudMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Sensor and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatSensorCrudMessageFormatter;
    internal->sampleMsgData = sensor;

    return retVal;
}

/*
 * create a message to inform the server that
 * a sensor was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorRemovedMessage(char *sensorEndpointId)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = sensorRemoveMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the Sensor and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatSensorRemoveMessageFormatter;
    if (sensorEndpointId != NULL)
    {
        internal->sampleMsgData = strdup(sensorEndpointId);
    }

    return retVal;
}

/*
 * create a message to inform the server that
 * a sensor was updated (label, type/function, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorUpdatedMessage(Sensor *sensor)
{
    // for this sample channel, same as "add"
    return createSensorAddedMessage(sensor);
}

/*
 * create a message to inform the server that
 * a sensor reported a fault/restore.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createSensorFaultRestoreMessage(Sensor *sensor)
{
    // for this sample channel, same as "add"
    return createSensorAddedMessage(sensor);
}