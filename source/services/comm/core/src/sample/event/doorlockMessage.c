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
 * doorlockMessage.c
 *
 * construct message objects to report CrUD (create, update, delete)
 * events about a DoorLock to the server.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include "sample/sampleChMessage.h"
#include "doorlockMessage.h"

/*
 * formatSampleMessageFunc implementation to format our message
 * in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatDoorLockCrudMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the doorLock
    sampleMessage *internal = extractSampleMessage(msg);
    DoorLock *doorLock = (DoorLock *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply put a bunch of details from the DoorLock into a string buffer
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "DoorLock:");
    stringBufferAppend(retVal, "\n  DeviceID: ");
    stringBufferAppend(retVal, stringCoalesce(doorLock->deviceId));
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(doorLock->endpointId));
    stringBufferAppend(retVal, "\n  Label: ");
    stringBufferAppend(retVal, stringCoalesce(doorLock->label));
    stringBufferAppend(retVal, "\n  Locked: ");
    stringBufferAppend(retVal, stringValueOfBool(doorLock->isLocked));
    stringBufferAppend(retVal, "\n  Manufacturer: ");
    stringBufferAppend(retVal, stringCoalesce(doorLock->manufacturer));
    stringBufferAppend(retVal, "\n  Model: ");
    stringBufferAppend(retVal, stringCoalesce(doorLock->model));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'DoorLock' object provided at creation
 */
static void doorLockCrudMessageDestroyFunc(message *msg)
{
    if (msg != NULL)
    {
        // extract our internal container
        sampleMessage *internal = extractSampleMessage(msg);

        // free the DoorLock
        DoorLock *doorLock = (DoorLock *)internal->sampleMsgData;
        destroyDoorLock(doorLock);
        internal->sampleMsgData = NULL;

        // since we replaced the default 'cleanup' function, need
        // to remove the sampleMsg specific mem in msg->userData
        //
        destroySampleMessage(internal);
        msg->userData = NULL;
    }
}

/*
 * formatSampleMessageFunc implementation to format our 'doorLock removed'
 * message in accordance with the sampleChannel requirements.  called by
 * sampleChannel prior to delivering the message.
 */
icStringBuffer *formatDoorLockRemoveMessageFormatter(message *msg, samplePayloadFormat format)
{
    // get the doorLock's endpointId
    sampleMessage *internal = extractSampleMessage(msg);
    char *endpointId = (char *)internal->sampleMsgData;

    // NOTE: we are going to ignore the format since this is for illustration purposes
    // and simply show the endpoint of the doorLock that was removed
    //
    icStringBuffer *retVal = stringBufferCreate(64);
    stringBufferAppend(retVal, "DoorLock (removed):");
    stringBufferAppend(retVal, "\n  EndpointID: ");
    stringBufferAppend(retVal, stringCoalesce(endpointId));
    stringBufferAppend(retVal, "\n");

    return retVal;
}

/*
 * 'messageDestroyFunc' implementation specific to our message
 * so it can cleanup the 'deviceId' provided at creation
 */
static void doorLockRemoveMessageDestroyFunc(message *msg)
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
 * a new door-lock has been added to the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createDoorLockAddedMessage(DoorLock *doorLock)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = doorLockCrudMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the DoorLock and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatDoorLockCrudMessageFormatter;
    internal->sampleMsgData = doorLock;

    return retVal;
}

/*
 * create a message to inform the server that
 * a door-lock was deleted from the system.
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createDoorLockRemovedMessage(char *doorLockEndpointId)
{
    // create the 'message wrapper' that represents
    // our event.  need to add a few function pointers
    // to ensure we properly generate SMAP and perform cleanup
    //
    message *retVal = createSampleMessage(0);
    retVal->destroyUserDataFunc = doorLockRemoveMessageDestroyFunc;

    // get our sampleMessage container that was created for us
    // then assign the DoorLock and our format function
    //
    sampleMessage *internal = extractSampleMessage(retVal);
    internal->encodeMessageFunc = formatDoorLockRemoveMessageFormatter;
    if (doorLockEndpointId != NULL)
    {
        internal->sampleMsgData = strdup(doorLockEndpointId);
    }

    return retVal;
}

/*
 * create a message to inform the server that
 * a door-lock was updated (label, locked/unlocked, etc).
 * can be released via the standard destroyMessageWrapper()
 *
 * @see destroyMessageWrapper()
 */
message *createDoorLockUpdatedMessage(DoorLock *doorLock)
{
    // for this sample channel, same as "add"
    return createDoorLockAddedMessage(doorLock);
}
