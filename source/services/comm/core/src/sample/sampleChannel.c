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

    // TODO: establish event listeners
    //

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

    // TODO: remove event listeners
    //

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
    // to go vs caching for later.
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
    // TODO: Needs implementation
    // example message to see if it's allowed to be sent to the server at this time
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

    // TODO: Needs implementation
    // marshall message and send to server
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

