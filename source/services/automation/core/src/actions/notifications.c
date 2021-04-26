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
// Created by wboyd747 on 6/21/18.
//

#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <commMgr/commService_eventAdapter.h>
#include <commMgr/commService_ipc.h>
#include <icConcurrent/timedWait.h>
#include <icTime/timeUtils.h>

#include "automationAction.h"
#include "automationService.h"
#include "notifications.h"

#define MEDIA_EVENT_AGE (10 * 60) // 10 Minutes
#define MEDIA_WAIT_TIMEOUT (5 * 60) // 5 Minutes

#define JSON_EVENTID_KEY "eventId"

static pthread_mutex_t videoMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t imageMutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t videoCond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t imageCond = PTHREAD_COND_INITIALIZER;

static icLinkedList* videoList;
static icLinkedList* imageList;

static void mediaLinkedListDestroy(void* data)
{
    destroy_mediaUploadedEvent(data);
}

/**
 * Remove all media events of a specific event type older than
 * (or equal to) a given age.
 *
 * Thus if an age of 24hrs is specified then any message older
 * than (or equal to) 24hrs will be removed.
 *
 * @param list The list of media events.
 * @param age The age in seconds to prune.
 */
static void removeOldEvents(icLinkedList* list, time_t age)
{
    time_t now = time(NULL);
    icLinkedListIterator* iterator = linkedListIteratorCreate(list);

    while (linkedListIteratorHasNext(iterator)) {
        mediaUploadedEvent* event = linkedListIteratorGetNext(iterator);

        if ((now - event->baseEvent.eventTime.tv_sec) >= age) {
            linkedListIteratorDeleteCurrent(iterator, mediaLinkedListDestroy);
        }
    }
    linkedListIteratorDestroy(iterator);

}

/**
 * Listen for media uploaded events and place the event
 * in the appropriate media type queue. A signal will
 * be broadcast internally that an event has been received.
 *
 * Old events will be pruned from the list at this time.
 *
 * @param event The media upload event.
 */
static void mediaUploadEventListener(mediaUploadedEvent *event)
{
    switch (event->mediaType) {
        case MEDIA_VIDEO_UPLOAD_EVENT:
            pthread_mutex_lock(&videoMutex);
            linkedListAppend(videoList, clone_mediaUploadedEvent(event));
            removeOldEvents(videoList, MEDIA_EVENT_AGE);
            pthread_cond_signal(&videoCond);
            pthread_mutex_unlock(&videoMutex);
            break;
        case MEDIA_IMAGE_UPLOAD_EVENT:
            pthread_mutex_lock(&imageMutex);
            linkedListAppend(imageList, clone_mediaUploadedEvent(event));
            removeOldEvents(imageList, MEDIA_EVENT_AGE);
            pthread_cond_signal(&imageCond);
            pthread_mutex_unlock(&imageMutex);
            break;
    }
}

/**
 * Wait for a signal, or timeout, for a specific media event type
 * and ID.
 *
 * @param mutex The mutex of the media type.
 * @param cond The conditional of the media type.
 * @param list The list of media uploaded events of a specific type.
 * @param ruleId The original rule ID to compare against.
 * @param eventId The original event ID to compare against.
 * @param event The received matching media uploaded event, or NULL if
 * none found before the timeout condition.
 * @param timeout The number of seconds to wait for a match.
 * @return Zero on success, otherwise ETIMEDOUT.
 */
static int waitForNotification(pthread_mutex_t* mutex,
                               pthread_cond_t* cond,
                               icLinkedList* list,
                               uint64_t ruleId,
                               uint64_t eventId,
                               mediaUploadedEvent** event,
                               int timeout)
{
    time_t start_time;

    *event = NULL;

    start_time = time(NULL);

    pthread_mutex_lock(mutex);
    do {
        icLinkedListIterator *iter = linkedListIteratorCreate(list);
        while(linkedListIteratorHasNext(iter)) {
            mediaUploadedEvent *item = (mediaUploadedEvent *)linkedListIteratorGetNext(iter);
            if ((item->ruleId == ruleId) && (item->requestEventId == eventId)) {
                *event = item;
                // Don't delete the item, we are passing it back to the caller
                linkedListIteratorDeleteCurrent(iter, standardDoNotFreeFunc);
                break;
            }
        }
        linkedListIteratorDestroy(iter);

        if ((*event == NULL) && (timeout > 0)) {
            incrementalCondTimedWait(cond, mutex, timeout);
            timeout -= (time(NULL) - start_time);
        }
    } while ((*event == NULL) && (timeout > 0));
    pthread_mutex_unlock(mutex);

    return ((*event == NULL) && (timeout <= 0)) ? ETIMEDOUT : 0;
}

/**
 * Send a request to the communication service forcing it to send
 * a notification (email/sms) from the server to the user. If there
 * is an attachment then the target handler will have to wait for
 * notification that an image or video was transmitted to the
 * server.
 *
 * @param id The ID of the request caller.
 * @param params JSON request
 * @return JSON response
 */
static cJSON* notificationActionHandler(const cJSON* id, const cJSON* params)
{
    cJSON* response = NULL;
    bool success = false;

    if (params) {
        cJSON* json;

        ruleSendMessage* cmd;
        ruleSendMessageType messageType = MESSAGE_TEXT;

        uint64_t eventId = 0;
        uint64_t ruleId;

        json = cJSON_GetObjectItem(params, "ruleId");
        ruleId = (uint64_t) json->valuedouble;

        json = cJSON_GetObjectItem(params, JSON_EVENTID_KEY);
        if (json && !cJSON_IsNull(json)) {
            /* Only set the event ID if there was one passed in.
             * It will be up to the communication service to determine
             * if it wants to create one or not later.
             */
            eventId = (uint64_t) json->valuedouble;
        }

        json = cJSON_GetObjectItem(params, "attachment");

        if (json && !cJSON_IsNull(json)) {
            mediaUploadedEvent* event = NULL;

            /* This notification has an attachment. Thus we must wait for
             * a video/picture to be uploaded to the server.
             */
            if (strcmp(json->valuestring, "video") == 0) {
                if (waitForNotification(&videoMutex,
                                        &videoCond,
                                        videoList,
                                        ruleId,
                                        eventId,
                                        &event,
                                        MEDIA_WAIT_TIMEOUT) == 0) {
                    messageType = MESSAGE_WITH_VIDEO_TYPE;
                }
            } else if (strcmp(json->valuestring, "picture") == 0) {
                if (waitForNotification(&imageMutex,
                                        &imageCond,
                                        imageList,
                                        ruleId,
                                        eventId,
                                        &event,
                                        MEDIA_WAIT_TIMEOUT) == 0) {
                    messageType = MESSAGE_WITH_IMAGE_TYPE;
                }
            }

            if (event) {
                eventId = (uint64_t) event->uploadEventId;
                mediaLinkedListDestroy(event);
            }
        }

        /* No attachment so this is just a straight up notification.
         * Go ahead and let the communication service know.
         */
        cmd = create_ruleSendMessage();

        if (cmd) {
            cmd->type = messageType;
            cmd->eventId = eventId;

            json = cJSON_GetObjectItem(params, "time");
            if (json && !cJSON_IsNull(json)) {
                cmd->eventTime = json->valuedouble;
            } else {
                cmd->eventTime = getCurrentUnixTimeMillis();
            }

            cmd->ruleId = ruleId;

            success = (commService_request_SEND_MESSAGE_TO_SUBSCRIBER(cmd) == IPC_SUCCESS);

            destroy_ruleSendMessage(cmd);
        }
    }

    if (id) {
        if (success) {
            response = jsonrpc_create_response_success(id, NULL);
        } else {
            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle notification action.",
                                                     NULL);
        }
    }

    return response;
}

void notificationMessageTargetInit(void)
{
    videoList = linkedListCreate();
    imageList = linkedListCreate();

    register_mediaUploadedEvent_eventListener(mediaUploadEventListener);

    automationActionRegisterOps("sendSmsAction", notificationActionHandler);
    automationActionRegisterOps("sendEmailAction", notificationActionHandler);
}

void notificationMessageTargetDestroy(void)
{
    unregister_mediaUploadedEvent_eventListener(mediaUploadEventListener);

    linkedListDestroy(videoList, mediaLinkedListDestroy);
    linkedListDestroy(imageList, mediaLinkedListDestroy);
}
