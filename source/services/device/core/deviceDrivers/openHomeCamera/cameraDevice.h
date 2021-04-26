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
 * cameraDevice.h
 *
 * Internal model of a Camera Device.  Contains
 * data provided from the deviceService database
 * as well as info collected from the device (via ohcm).
 *
 * Serves as a layer between the camera 'driver' and
 * the ohcm library (conduit to physical device).
 * The layers involved:
 *
 * | device- | camera- | camera- |  ohcm-  |
 * | service | device- | device  | library |
 * |         | driver  |         |         |
 *
 * Author: jelderton - 6/21/16
 *-----------------------------------------------*/

#ifndef ZILKER_CAMERADEVICE_H
#define ZILKER_CAMERADEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include <deviceDescriptor.h>
#include <openHomeCamera/ohcm.h>

typedef struct _cameraDevice cameraDevice;

/*
 * sent as part of the 'cameraDeviceChangedCallback'
 * so the camera-device-driver can be informed of changes
 * to the cameraDevice (physical and logical)
 */
typedef enum
{
    CAMERA_OFFLINE_CHANGE,      // camera is offline
    CAMERA_ONLINE_CHANGE,       // camera is online
    CAMERA_FIRMWARE_CHANGE,     // camera firmware version changed
    CAMERA_MOTION_FAULT_CHANGE, // motion detected in camera
    CAMERA_MOTION_CLEAR_CHANGE, // motion cleared from camera
    CAMERA_BUTTON_PRESSED,      // a user button on the camera was pressed
} cameraAttrChange;

/*
 * function prototype for a cameraDevice to invoke with
 * an attribute changes (ex: ipAddress, firmware version, etc)
 *
 * @param searchVal - the 'searchVal' supplied to the "find" or "delete" function call
 * @param item - the current item in the list being examined
 * @return TRUE if 'item' matches 'searchVal', terminating the loop
 */
typedef void (*cameraDeviceChangedCallback)(cameraDevice *device, cameraAttrChange reason);

/*
 * define the current operation occuring on the device.
 * primarily used to prevent running operations while
 * busy handling another (ex: don't check for motion while upgrading)
 */
typedef enum
{
    CAMERA_OP_STATE_READY,       // free to be interacted with
    CAMERA_OP_STATE_OFFLINE,     // device not responding (offline)
    CAMERA_OP_STATE_CONFIGURE,   // currently configuring the camera
    CAMERA_OP_STATE_UPGRADE,     // currently upgrading the camera
    CAMERA_OP_STATE_DESTROY,     // tagged for removal.  when set, the monitor thread needs to release the mem
} cameraOperateState;

/*
 * set the motion sesnsitivity.  only applicable if
 * motion is enabled.
 */
typedef enum
{
    CAMERA_MOTION_SENSITIVITY_LOW,
    CAMERA_MOTION_SENSITIVITY_MEDIUM,
    CAMERA_MOTION_SENSITIVITY_HIGH
} cameraMotionSensitivity;

/*
 * username and password container
 */
typedef struct
{
    char *username;
    char *password;
} cameraCredentials;

/*
 * video settings container
 */
typedef struct
{
    char *videoResolution;
    char *aspectRatio;
} cameraVideoSettings;

/*
 * base model of a Camera Device.  when accessing the information,
 * be sure to grab the mutex, except for the uuid and isIntegratedPeripheral
 */
struct _cameraDevice
{
    char                *uuid;                  // Camera unique ID,  never changes, so no need to mutex when accessing
    char                *ipAddress;             // Camera IP Address
    char                *macAddress;            // Camera MAC Address
    ohcmDeviceInfo      *details;               // detailed info (version, manufacturer, model, etc) - comes from ohcm.h
    cameraCredentials   *adminCredentials;      // administrator user/pass
    cameraCredentials   *userCredentials;       // access streams user/pass
    cameraVideoSettings *videoSettings;         // resolution and aspect ratio
    cameraOperateState  opState;                // current operation being executed
    bool                motionEnabled;          // if motion detection is enabled
    bool                motionPossible;         // if motion detection is allowed (via the whitelist)
    bool                hasUserButton;          // if the camera has a button (such as a doorbell camera).  Set via whitelist metadata
    bool                useSercommEventPush;    // if true, this will use sercomm's proprietary http event push to us instead of us polling the camera
    bool                hasSpeaker;             // if the camera has a speaker.  Set via whitelist metadata
    bool                isIntegratedPeripheral; // true if camera is also the hub (never changes, no need for mutex)
    pthread_t           monitorThread;          // monitor thread identifier
    bool                monitorRunning;         // signify if monitor thread is running
    pthread_mutex_t     mutex;                  // mutex for the object
    pthread_cond_t      cond;                   // condition for the object
    cameraDeviceChangedCallback notify;         // function to call when attribute changes
};

/*
 * create a new cameraDevice object.  if 'gatherRest' is true, will probe
 * the physical device to obtain any missing information (i.e. new device would
 * only supply the 'ipAddress).  otherwise relies on the caller to populate
 * each of the required pieces of information.
 */
cameraDevice *createCameraDevice(const char *uuid,
                                 const char *ipAddress, const char *macAddress,
                                 const char *adminUser, const char *adminPass,
                                 cameraDeviceChangedCallback callback, bool gatherRest,
                                 ohcmResultCode *result);

/*
 * destroy a cameraDevice object. will stop the
 * monitor thread if running.
 */
void destroyCameraDevice(cameraDevice *device);

/*
 * Configure the device, using the descriptor as a guide.  If successful,
 * the admin and user credentials will be randomized.  the caller will need to
 * save those newly generated credentials.
 */
bool cameraDeviceConfigure(cameraDevice *device, CameraDeviceDescriptor *descriptor, bool isReconfig);

/**
 * The a camera devices SSID and WPA2 passphrase directly.
 *
 * @param device A device that represents a specific camera.
 * @param ssid The new SSID to give to the camera.
 * @param passphrase The new WPA2 passphrase to provide to the camera.
 * @return True if the camera was successfully configured with the new SSID and passphrase.
 */
bool cameraDeviceSetWiFiNetworkCredentials(cameraDevice *device, const char* ssid, const char* passphrase);

/*
 * update the local flag and informs the camera to turn motion on/off
 */
void cameraDeviceEnableMotionDetection(cameraDevice *device, bool enabled);

/*
 * updates the sensitivity of motion detection within the camera.
 * only applicable if motion detection is enabled.
 */
bool cameraDeviceSetMotionDetectionSensitivity(cameraDevice *device, cameraMotionSensitivity sensitivity);

/*
 * start the thread to monitor the device.  will look for
 * "offline" and/or "motion", as well as rediscover if the
 * IP Address changes.  any changes (motion, ip, offline)
 * will be communicated via the cameraDeviceChangedCallback
 */
bool cameraDeviceStartMonitorThread(cameraDevice *device);

/*
 * stops the monitoring of this device.  if 'waitForIt' is true,
 * will block until the thread dies
 */
void cameraDeviceStopMonitorThread(cameraDevice *device, bool waitForIt);

/*
 * attempts to reset the device to factory defaults.
 */
void cameraDeviceResetToFactory(cameraDevice *device);

/*
 * checks to see if this device needs an upgrade by comparing
 * the camera firmwareVersion to ones defined in the device descriptor.
 * if 'checkMinimum' is true, then the comparison is against the 'min fw version',
 * otherwise compared to the 'latest fw version'
 */
bool cameraDeviceCheckForUpgrade(cameraDevice *device, DeviceDescriptor *descriptor, bool checkMinimum);

/*
 * ask the camera to begin the firmware upgrade process.
 * this will block until the upgrade is complete (or fails).
 * on success, the device->details->firmwareVersion should
 * reflect the new version requested.
 */
bool cameraDevicePerformUpgrade(cameraDevice *device, char *firmwareFilename, char *firmwareVersion, uint16_t timeoutSecs);

/*
 * asks the camera for it's firmware version, and compare to
 * what we believe the version is.  if the version changed,
 * will update the device->details->firmwareVersion.  will
 * return 'true' if the firmware version string changed.
 */
bool cameraDeviceCheckFirmwareValue(cameraDevice *device, const char *logPrefix, bool notifyCallback);

/*
 * reboot the device, and wait for it to come back online
 * (if 'waitForReturn' is set)
 */
bool cameraDeviceReboot(cameraDevice *device, bool waitForReturn, uint16_t timeoutSeconds);

/*
 * ping the device to see if it's online
 */
bool cameraDevicePing(cameraDevice *device, uint16_t timeoutSeconds);

/*
 * establish a new media tunnel with the camera.
 * returns the 'media session id'.
 * caller must free returned string (if not NULL)
 */
char *cameraDeviceCreateMediaTunnel(cameraDevice *device, char *url);

/*
 * destroy a media tunnel
 */
bool cameraDeviceDestroyMediaTunnel(cameraDevice *device, char *session);

/*
 * NOT IMPLEMENTED YET
 */
void cameraDeviceGetMediaTunnelStatus(cameraDevice *device, void *wtf);

/*
 * take a picture and save to the provided 'localFilename'
 */
bool cameraDeviceTakePicture(cameraDevice *device, char *localFilename);

/*
 * grab and upload a video clip
 */
bool cameraDeviceTakeVideoClip(cameraDevice *device, char *postUrl, uint8_t durationSecs);



#endif //ZILKER_CAMERADEVICE_H
