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
 * cameraPrivate.h
 *
 * Set of internal functions that can be called from
 * the handful of .c files within this driver
 *
 * Author: jelderton - 6/21/16
 *-----------------------------------------------*/

#ifndef ZILKER_CAMERAPRIVATE_H
#define ZILKER_CAMERAPRIVATE_H


/*---------------------------------------------------------------------------------------
 * Miscellaneous Defines
 *---------------------------------------------------------------------------------------*/
#define LOG_TAG                         "openHomeCamera-DD"
#define DEVICE_CLASS_VERSION            1
#define DEVICE_DRIVER_NAME              "openHomeCameraDeviceDriver"
#define DEFAULTED_ADMIN_USERNAME        "administrator"
#define DEFAULTED_ADMIN_PASSWORD        ""
#define MIN_PASSWORD_TOKEN_LENGTH       8
#define MAX_PASSWORD_TOKEN_LENGTH       8
#define DEFAULT_MOTION_BLACKOUT_SECONDS 60*3        // 3 minute for a "camera motion blackout period"
#define HTTPS_PORT                      443
#define CURL_RETRY_COUNT                5
#define CURL_RETRY_SLEEP_MICROSECONDS   500000      // half second
#define CAMERA_ISALIVE_WAIT             5000000     // 5 seconds, wait for camera to respond to isAlive
#define CAMERA_REBOOT_TIMEOUT_SECONDS   360         // max time to wait for camera to reboot
#define CAMERA_SERVER_RESTART_TIMEOUT_SECONDS   60  // max time to wait for camera https service to restart
#define DETAULT_FW_UPDATE_TIMEOUT_SECS  280         // default time to wait for camera upgrade
#define CONNECTION_RETRY_COUNT          3           // number of retries to use for some APIs
#define CONFIG_CONNECTION_RETRY_COUNT   5           // number of retries to use for initial configuration api
#define IS_ALIVE_RETRY_COUNT            1           // do not retry on isAlive()
#define IS_ALIVE_SUCCESS_COUNT          2           // number of consecutive successes to confirm camera is alive
#define RECOVERY_TIMEOUT_SECONDS        10
#define USER_BUTTON_PRESENT_PROPNAME    "userButtonPresent"
#define SPEAKER_PRESENT_PROPNAME        "speakerPresent"
#define USE_SERCOMM_PUSH_EVENT_PROPNAME "useSercommEventPush"
#define SERCOMM_EVENT_HANDLER_PORT      5555
/*---------------------------------------------------------------------------------------
 * Configuration Settings for Open Home Cameras
 *---------------------------------------------------------------------------------------*/
/* Data Timer defines */
#define MEDIA_TUNNEL_READY_MAX_WAIT         60000   // max time to wait in milliseconds
#define MEDIA_TUNNEL_READY_MIN_RETRY_WAIT   0       // min time to wait in milliseconds before retry
#define MEDIA_TUNNEL_READY_MAX_RETRY_WAIT   5000    // max time to wait in milliseconds before retry
#define MEDIA_TUNNEL_READY_STEPSIZE_WAIT    500     // stepsize in milliseconds to wait, for exponential backup
#define MEDIA_TUNNEL_READY_RETRIES          10      // max number of retries
#define MEDIA_TUNNEL_UPLOAD_MIN_RETRY_WAIT  1000    // min time to wait in milliseconds before retry
#define MEDIA_TUNNEL_UPLOAD_MAX_RETRY_WAIT  5000    // max time to wait in milliseconds before retry
#define MEDIA_TUNNEL_UPLOAD_STEPSIZE_WAIT   500     // stepsize in milliseconds to wait, for exponential backup
#define MEDIA_TUNNEL_UPLOAD_RETRIES         10      // max number of retries
#define MEDIA_TUNNEL_UPLOAD_TIMEOUT         1800000 // upload timeout in milliseconds
/* Host Server defines */
#define HOST_SERVER_HTTPS_ENABLED           true    // true if https is enabled
#define HOST_SERVER_HTTPS_PORT              443     // https port number
#define HOST_SERVER_HTTPS_VALIDATE_CERTS    false   // true if it needs certificate
#define HOST_SERVER_HTTP_ENABLED            false   // true if http is enabled
#define HOST_SERVER_HTTP_PORT               80      // http port number
#define HOST_SERVER_POLL_ENABLED            true    // true if poll is enabled
#define HOST_SERVER_POLL_DEFAULT_LINGER     10      // default linger in seconds
/*---------------------------------------------------------------------------------------
 * Motion Settings for Open Home Cameras
 *---------------------------------------------------------------------------------------*/
#define MOTION_DETECTION_LOW    "low"
#define MOTION_DETECTION_MEDIUM "medium"
#define MOTION_DETECTION_HIGH   "high"
#define DEFAULT_LOW_SENSITIVITY_PERCENTAGE  25
#define DEFAULT_MED_SENSITIVITY_PERCENTAGE  50
#define DEFAULT_HIGH_SENSITIVITY_PERCENTAGE 75
#define DEFAULT_LOW_DETECTION_THRESHOLD     10
#define DEFAULT_MED_DETECTION_THRESHOLD      8
#define DEFUALT_HIGH_DETECTION_THRESHOLD     6
/* Define the motion IDs to be unique per camera (i.e., we will re-use them across cameras) */
#define MOTION_ID                   "57df9d68-4759-4622-ade2-88f330c7a261"
#define EVENT_ID                    "007ce98c-6e7f-4831-a1c6-01b0a3e98a96"
#define NOTIFICATION_ID             "095ebc8c-4b13-49c4-a8b7-bc469f3c1170"
#define NOTIFICATION_LIST_ID        "768e043b-e2d9-49d1-af85-b23a7f8648a4"
#define MOTION_DETECTION_REGION_ID  "f555f551-15e1-45a3-af9f-8fe856339c2c"
/* Define the motion settings - these should be configurable based on camera / customer */
/* TODO : These should be configurable */
#define MOTION_DETECTION_MIN_HORIZONTAL_RESOLUTION      16
#define MOTION_DETECTION_MIN_VERTICAL_RESOLUTION        16
#define MOTION_DETECTION_SOURCE_HORIZONTAL_RESOLUTION   1280
#define MOTION_DETECTION_SOURCE_VERTICAL_RESOLUTION     720
#define EVENT_TRIGGER_MINIMUM_INTERVAL_BETWEEN_EVENTS   15 // in seconds
#define MOTION_DETECTION_REGION_LIST_SENSITIVITY_LEVEL  60
#define MOTION_DETECTION_REGION_LIST_DETECTION_THRESHOLD 10
#define MOTION_DETECTION_REGION_UPPER_LEFT_X            96
#define MOTION_DETECTION_REGION_UPPER_LEFT_Y            479
#define MOTION_DETECTION_REGION_LOWER_RIGHT_X           543
#define MOTION_DETECTION_REGION_LOWER_RIGHT_Y           240
#define LONG_POLL_WAIT_SECONDS                          60

//The number of consecutive failures to a camera's event polling interface before declaring it in comm failure
#define ERROR_COUNT_COMM_FAIL_THRESHOLD 4
// The number of consecutive successes required to transition out of commFail
//
#define SUCCESS_COUNT_COMM_RESTORE_THRESHOLD 1


/*
 * return the number of seconds for the "motion blackout period"
 */
uint32_t getMotionBlackoutSeconds();

#endif //ZILKER_CAMERAPRIVATE_H
