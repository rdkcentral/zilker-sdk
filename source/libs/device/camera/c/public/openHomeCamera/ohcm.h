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
 * ohcm.h
 *
 * public function interface for the OpenHome Camera API.
 * each call will translate into an OpenHome call to a camera device.
 *
 * Author: jelderton (refectored from original one created by karan)
 *-----------------------------------------------*/

#ifndef IC_OPENHOME_CAMERA_H
#define IC_OPENHOME_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <libxml/tree.h>
#include <propsMgr/sslVerify.h>

#include <icBuildtime.h>        // possible 'verbose' flags set
#include <icTypes/icLinkedList.h>

// NOTE:
// NOTE: some of the functions in here are were not used, and are
// NOTE: currently disabled via an '#ifdef OHCM_SUPPORTED' block.
// NOTE:

/*-----------------------
 * init/de-init functions
 *-----------------------*/

/*
 * initialize the OpemHome Camera library.  must be called at least once
 * prior to other function calls.
 */
void initOhcm();

/*
 * cleanup internal resources created during the initOhcm() function
 */
void cleanupOhcm();

/*
 * enable/disable "mutual TLS" with the camera.  when supplying valid
 * filenames that exist, they will be leveraged by CURL as client encryption;
 * thus enabling mutual TLS.  If the files are NULL or missing, then mutual
 * will be disabled.  Note that these are only used if the camera supports
 * mutual TLS (as of now, only xCams with version V3.0.02.23 or higher)
 */
/**
 * Enable camera mTLS (legacy).
 * @param certFilename  A PEM formatted public certificate with any intermediates
 * @param privKeyFilename
 */
void setOhcmMutualTlsMode(const char *certFilename, const char *privKeyFilename);

/**
 * Determine if mutual TLS is possible.
 * @return true when a client key and certificate is available.
 */
bool ohcmIsMtlsCapable(void);

/**
 * Set the TLS verification level (to camera)
 * @note Only 'peer' and 'none' are supported. 'both' and 'host' will be effectively 'peer' and 'none', respectively.
 * @param level
 */
void ohcmSetTLSVerify(sslVerify level);

/**
 * Get The TLS verification level (to camera)
 * @return
 */
sslVerify ohcmGetTLSVerify(void);

/*-----------------------
 * common/base object definitions
 *-----------------------*/

/* set of possible return codes from most function invocations */
typedef enum {
    OHCM_SUCCESS,
    OHCM_REBOOT_REQ,        // success, however the camera needs to be rebooted
    OHCM_COMM_FAIL,         // unable to communicate with the camera
    OHCM_COMM_TIMEOUT,      // comm failure due to timeout
    OHCM_SSL_FAIL,          // SSL failure
    OHCM_LOGIN_FAIL,        // unable to login to the camera (invalid credentials)
    OHCM_INVALID_CONTENT,   //
    OHCM_GENERAL_FAIL,      // general failure when processing the request
    OHCM_NOT_SUPPORTED      // failure code for an unsupported operation
} ohcmResultCode;

/* string representations of ohcmResultCode (mainly used for debugging) */
static char *ohcmResultCodeLabels[] =
{
    "SUCCESS",          // OHCM_SUCCESS
    "REBOOT_REQUIRED",  // OHCM_REBOOT_REQ
    "COMM_FAILURE",     // OHCM_COMM_FAIL
    "COMM_TIMEOUT",     // OHCM_COMM_TIMEOUT
    "SSL_FAILURE",      // OHCM_SSL_FAIL
    "LOGIN_FAILURE",    // OHCM_LOGIN_FAIL
    "INVALID_CONTENT",  // OHCM_INVALID_CONTENT
    "GENERAL_FAILURE",  // OHCM_GENERAL_FAIL
    "NOT_SUPPORTED"     // OHCM_NOT_SUPPORTED
};

/* minimal amount of info required for performing the OpenHome operation */
typedef struct {
    char *cameraIP;         // IP of the camera to contact
    char *macAddress;       // optional
    char *userName;         // user to use when communicating with the camera
    char *password;         // password to use when communicating with the camera
} ohcmCameraInfo;

/*
 * helper function to create a new ohcmCameraInfo object (with NULL values)
 */
ohcmCameraInfo *createOhcmCameraInfo();

/*
 * helper function to destroy the ohcmCameraInfo object, along with it's contents
 */
void destroyOhcmCameraInfo(ohcmCameraInfo *device);


/* base 'device' information about a camera */
typedef struct {
    char *deviceName;       // current assigned label/name
    char *deviceID;         // Camera Device ID
    char *manufacturer;     // manufacturer identifier
    char *model;            // model identifier
    char *serialNumber;
    char *macAddress;       // MAC Address (as defined by the camera)
    char *firmwareVersion;  // current Firmware Version
    char *firmwareReleasedDate;
    char *bootVersion;
    char *bootReleasedDate;
    char *rescueVersion;    // no idea...
    char *hardwareVersion;
    char *apiVersion;       // OpenHome Camera Api Version
} ohcmDeviceInfo;

/*
 * helper function to create a new ohcmDeviceInfo object (with NULL values)
 */
ohcmDeviceInfo *createOhcmDeviceInfo();

/*
 * helper function to destroy the ohcmDeviceInfo object, along with it's contents
 */
void destroyOhcmDeviceInfo(ohcmDeviceInfo *device);

/*
 * debug print the contents
 */
void printDeviceInfo(ohcmDeviceInfo *device);

/*
 * obtain details about the camera
 *
 * @param cam - device to contact
 * @param info - object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmDeviceInfo(ohcmCameraInfo *cam, ohcmDeviceInfo *info, uint32_t retryCounts);

/*
 * attempts to ping the camera using OpenHome API (not fork the ping command)
 *
 * @param cam - device to contact
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode isOhcmAlive(ohcmCameraInfo *cam, uint32_t retryCounts);

/*-----------------------
 * security access functions
 *-----------------------*/

// possible ACL for the ohcmSecurityAccount object
typedef enum {
    OHCM_ACCESS_ADMIN,
    OHCM_ACCESS_USER
} ohcmAccessRights;

// used for get/set credentials on the camera
typedef struct {
    char *id;               // An unique alphanumeric id for the account (generally 0 for admin, 1 for user)
    char *userName;         // Username for relavant id
    char *password;         // only used during 'set' operation
    ohcmAccessRights accessRights;  // Admin or User
} ohcmSecurityAccount;

/* currently disabled as this isn't used yet */
#ifdef OHCM_SUPPORTED
/*
 * ask the camera for the current set of credentials (NOTE: the password will not be populated)
 * caller must provide the empty linked list, which will be populated with
 * 'ohcmSecurityAccount' objects.  the objects within the linked list can be released
 * via '
 *
 * @param cam - device to contact
 * @param output - linked list to fill in with ohcmSecurityAccount objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getSecurityAccounts(ohcmCameraInfo *cam, icLinkedList *output, int retryCounts);

ohcmResultCode addSecurityAccounts(CameraInfo *cam, SECURITYACCOUNTLIST *ptr, int curlRetryCounts);
ohcmResultCode deleteSecurityAccount(CameraInfo *cam, char *uid, int curlRetryCounts);
#endif  // OHCM_SUPPORTED

/*
 * helper function to create a blank ohcmSecurityAccount object
 */
ohcmSecurityAccount *createOhcmSecurityAccount();

/*
 * helper function to destroy the ohcmSecurityAccount object
 */
void destroyOhcmSecurityAccount(ohcmSecurityAccount *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmSecurityAccount from a linked list
 */
void destroyOhcmSecurityAccountFromList(void *item);

/* currently disabled as this isn't used */
#ifdef OHCM_SUPPORTED

/*-----------------------
 * api (wtf?)
 *-----------------------*/

struct Api{					/* /OpenHome/api */ /* Linked List */
    char *command;				/* List the APILIST, every node points to next availble APILIST */
    struct Api *Next;			/* Linked list pointer points to next element */
};
typedef struct Api APILIST;

RET_TYPE getSupportedAPI(CameraInfo *cam, APILIST **api, int curlRetryCounts);
RET_TYPE destroySupportedAPIStruct(APILIST *api);

#endif  // OHCM_SUPPORTED


/*-----------------------
 * stream functions
 *-----------------------*/

typedef enum {
    H264_PROFILE_BASELINE = 0,
    H264_PROFILE_MAIN,
    H264_PROFILE_HIGH,
    H264_PROFILE_EXTENDED,
} h264ProfileEnum;

// string representations of the h264ProfileEnum
static const char *h264ProfileLabels[] = {
        "baseline",     // H264_PROFILE_BASELINE
        "main",         // H264_PROFILE_MAIN
        "high",         // H264_PROFILE_HIGH
        "extended"      // H264_PROFILE_EXTENDED
};

typedef enum
{
    H264_LEVEL_1,
    H264_LEVEL_1B,
    H264_LEVEL_1_1,
    H264_LEVEL_1_2,
    H264_LEVEL_1_3,
    H264_LEVEL_2,
    H264_LEVEL_2_1,
    H264_LEVEL_2_2,
    H264_LEVEL_3,
    H264_LEVEL_3_1,
    H264_LEVEL_3_2,
    H264_LEVEL_4,
    H264_LEVEL_4_1,
    H264_LEVEL_4_2,
    H264_LEVEL_5,
    H264_LEVEL_5_1,
    H264_LEVEL_5_2,
} h264LevelEnum;

// string representations of the h264LevelEnum
static const char *h264LevelLabels[] = {
    "1",    // H264_LEVEL_1
    "1b",   // H264_LEVEL_1B
    "1.1",  // H264_LEVEL_1_1
    "1.2",  // H264_LEVEL_1_2
    "1.3",  // H264_LEVEL_1_3
    "2",    // H264_LEVEL_2
    "2.1",  // H264_LEVEL_2_1
    "2.2",  // H264_LEVEL_2_2
    "3",    // H264_LEVEL_3
    "3.1",  // H264_LEVEL_3_1
    "3.2",  // H264_LEVEL_3_2
    "4",    // H264_LEVEL_4
    "4.1",  // H264_LEVEL_4_1
    "4.2",  // H264_LEVEL_4_2
    "5",    // H264_LEVEL_5
    "5.1",  // H264_LEVEL_5_1
    "5.2"   // H264_LEVEL_5_2
};

typedef char* OptRange;

typedef struct _ohcmVideoStreamCapabilities {
    int inputChannelID;

    icLinkedList* h264Profiles;
    icLinkedList* h264Levels;
    icLinkedList* mpeg4Profiles;

    bool supportsMJPEG;

    icLinkedList* scanTypes;

    int maxWidth;
    int minWidth;
    OptRange widthRange;

    int maxHeight;
    int minHeight;
    OptRange heightRange;

    icLinkedList* qualityTypes;

    int maxCBR;
    int minCBR;
    OptRange cbrRange;

    int maxFramerate;
    int minFramerate;
    OptRange framerateRange;

    icLinkedList* snapshotTypes;
} ohcmVideoStreamCapabilities;

typedef struct _ohcmAudioStreamCapabilities {
    int inputChannelID;

    icLinkedList* compressionTypes;

    int maxBitrate;
    int minBitrate;
    OptRange bitrateRange;
} ohcmAudioStreamCapabilities;

typedef struct _ohcmMediaStreamCapabilities {
    int maxPre;
    int minPre;
    OptRange preRange;

    int maxPost;
    int minPost;
    OptRange postRange;
} ohcmMediaStreamCapabilities;

typedef struct _ohcmStreamCapabilities {
    char* id;
    char* name;

    icLinkedList* streamingTransports;

    ohcmVideoStreamCapabilities* videoCapabilities;
    ohcmAudioStreamCapabilities* audioCapabilities;
    ohcmMediaStreamCapabilities* mediaCapabilities;
} ohcmStreamCapabilities;

ohcmStreamCapabilities* createOhcmStreamCapabilities();
void destroyOhcmStreamCapabilites(ohcmStreamCapabilities* obj);

ohcmResultCode getOhcmStreamCapabilities(ohcmCameraInfo *cam,
                                         const char* id,
                                         ohcmStreamCapabilities* obj,
                                         uint32_t retryCounts);

bool isOhcmValueInRange(int minValue, int maxValue, OptRange range, int value);
bool ohcmContainsCapability(icLinkedList* list, const char* item);

// used for get/set stream channels
typedef struct {
    char *id;                   // An unique alphanumeric id
    char *name;                 // name of the channel
    bool enabled;               // true if channel is enabled
    uint32_t rtspPortNo;        // RTSP port number.  only used if > 0
    char *streamingTransport;   // Streaming Transport (HTTP or RTSP)
    bool unicastEnabled;        // true if Unicast is enabled
    bool multicastEnabled;      // true if Multicast is enabled
    char *destIPAddress;        // Destination IP Address
    uint32_t  videoDestPortNo;  // Destination Video Port Number.  only used if > 0
    uint32_t  audioDestPortNo;  // Destination Audio Port Number.  only used if > 0
    uint32_t  ttl;              // only used if > 0.            TODO: what is this, time-to-live????
    bool securityEnabled;       // true if Security is enabled  TODO: what is this?
    bool videoEnabled;			// true if Video is enabled
    char *videoInputChannelID;  // An unique alphanumeric video input channel id
    char *h264Profile;          // Supported H264 profile
    char *h264Level;            // Supported H264 level
    char *mpeg4Profile;         // Supported MPEG4 profile
    char *mjpegProfile;         // Supported MJPEG profile
    char *videoScanType;        // Supported Video Scan Type
    uint32_t videoResolutionWidth;     // Video Width Resolution.  only used if > 0
    uint32_t videoResolutionHeight;    // Video Height Resolution.  only used if > 0
    char *videoQualityControlType;  // Video Quality Control Type
    uint32_t fixedQuality;      // Fixed Quality.  only used if > 0     TODO: define min/max
    uint32_t maxFrameRate;      // Maximum Frame Rate Supported. only use if > 0
    uint32_t keyFrameInterval;  // Maximum Key Frame Interval Supported.  only used if > 0
    uint32_t vbrMinRate;        // Minimum bitrate for VBR.  only used if videoQualityControlType == VBR
    uint32_t vbrMaxRate;        // Maximum bitrate for VBR.  only used if videoQualityControlType == VBR
    uint32_t constantBitRate;   // Bitrate used when videoQualityControlType == CBR
    bool mirrorEnabled;         // true if Mirror is enabled
    char *snapShotImageType;    // Type of Snapshot Image
    bool audioEnabled;          // true if Audio is enabled
    char *audioInputChannelID;  // An unique alphanumeric Audio input channel id
    char *audioCompressionType; // Audio Compression Type
    uint32_t preCaptureLength;  // Pre Capture Length.  only used if > 0
    uint32_t postCaptureLength; // Post Capture Length.  only used if > 0
} ohcmStreamChannel;

/*
 * helper function to create a blank ohcmStreamChannel object
 */
ohcmStreamChannel *createOhcmStreamChannel();

/*
 * helper function to destroy the ohcmStreamAccount object
 */
void destroyOhcmStreamChannel(ohcmStreamChannel *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmStreamChannel from a linked list
 */
void destroyOhcmStreamChannelFromList(void *item);

/*
 * query the camera for the current 'streaming channel configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmStreamChannel
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmStreamChannel objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmStreamingChannels(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts);

/*
 * query the camera for a specific 'streaming channel configuration'.
 * if successful, will populate 'target' with details about the channel.
 *
 * @param cam - device to contact
 * @param streamUid - the name of the stream to get config for
 * @param target - ohcmStreamChannel object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmStreamingChannelByID(ohcmCameraInfo *cam, char *streamUid, ohcmStreamChannel *target, uint32_t retryCounts);

/*
 * apply new 'streaming channel configuration' to a camera.
 *
 * @param cam - device to contact
 * @param inputList - linked likst of ohcmStreamChannel objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmStreamingChannels(ohcmCameraInfo *cam, icLinkedList *inputList, uint32_t retryCounts);


/* currently disabled as this isn't used */
#ifdef OHCM_SUPPORTED

/*-----------------------
 * upload picture functions
 *-----------------------*/

typedef struct {				/* /OpenHome/Streaming/channels/[UID]/picture/upload */
    char *id;				/* An unique alphanumeric id */
    char *snapShotImageType;		/* SnapShot Type e.g JPEG */
    char *blockUploadComplete;		/* Block Upload Complete */
    char *gatewayUrl;			/* Gateway Server IP where to Upload Picture */
    char *eventUrl;				/* Event Url */
} UploadPictureStruct;

RET_TYPE uploadPicture(CameraInfo *cam, UploadPictureStruct *upicture, char *uid, int curlRetryCounts);
RET_TYPE destroyUploadPictureStruct(UploadPictureStruct *upicture);

#endif  // OHCM_SUPPORTED


/*-----------------------
 * audio functions
 *-----------------------*/

typedef enum {
    OHCM_AUDIO_LISTENONLY,
    OHCM_AUDIO_TALKONLY,
    OHCM_AUDIO_TALKORLISTEN,
    OHCM_AUDIO_TALKANDLISTEN
} ohcmAudioModeType;

/* used to get/set the audio channel settings */
typedef struct {
    char *id;                   // An unique alphanumeric id
    bool enabled;               // true if Audio Channels is enabled
    ohcmAudioModeType audioMode;// currently only supports LISTENONLY
    bool microphoneEnabled;     // true if Microphone is enabled
} ohcmAudioChannel;

/*
 * helper function to create a blank ohcmAudioChannel object
 */
ohcmAudioChannel *createOhcmAudioChannel();

/*
 * helper function to destroy the ohcmAudioChannel object
 */
void destroyOhcmAudioChannel(ohcmAudioChannel *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmAudioChannel from a linked list
 */
void destroyOhcmAudioChannelFromList(void *item);

/*
 * query the camera for the current 'audio channel configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmAudioChannel
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmAudioChannel objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmAudioChannels(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts);

/*
 * query the camera for a specific 'audio channel configuration'.
 * if successful, will populate 'target' with details about the channel.
 *
 * @param cam - device to contact
 * @param audioUid - the name of the audio channel to get config for
 * @param target - ohcmAudioChannel object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmAudioChannelByID(ohcmCameraInfo *cam, char *audioUid, ohcmAudioChannel *target, uint32_t retryCounts);

/*
 * apply new 'audio channel configuration' to a camera.
 *
 * @param cam - device to contact
 * @param settings - channel settings to use
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmAudioChannel(ohcmCameraInfo *cam, ohcmAudioChannel *settings, uint32_t retryCounts);


/*-----------------------
 * video functions
 *-----------------------*/

typedef enum {
    OHCM_POWERLINE_FREQ_50hz,
    OHCM_POWERLINE_FREQ_60hz
} ohcmPowerlineFrequencyType;

typedef enum {
    OHCM_WHITEBALANCE_MANUAL,
    OHCM_WHITEBALANCE_AUTO
} ohcmWhiteBalanceType;

typedef enum {
    OHCM_DAYNIGHT_FILTER_DAY,
    OHCM_DAYNIGHT_FILTER_NIGHT,
    OHCM_DAYNIGHT_FILTER_AUTO,
} ohcmDayNightFilterType;

/* used to get/set the 'video input channel' settings */
typedef struct {
    char *id;                       // An unique alphanumeric video input channel id
    ohcmPowerlineFrequencyType powerLineFrequencyMode;
    ohcmWhiteBalanceType whiteBalanceMode;
    uint32_t brightnessLevel;      // percentage
    uint32_t contrastLevel;            // percentage
    uint32_t sharpnessLevel;           // percentage
    uint32_t saturationLevel;          // percentage
    ohcmDayNightFilterType dayNightFilterType;
    bool mirrorEnabled;             // true if Mirror is enabled
} ohcmVideoInput;

/*
 * helper function to create a blank ohcmVideoInput object
 */
ohcmVideoInput *createOhcmVideoInput();

/*
 * helper function to destroy the ohcmVideoInput object
 */
void destroyOhcmVideoInput(ohcmVideoInput *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmVideoInput from a linked list
 */
void destroyOhcmVideoInputFromList(void *item);

/*
 * query the camera for the current 'video input configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmVideoInput
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmVideoInput objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmVideoInputs(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts);

/*
 * query the camera for a specific 'video input configuration'.
 * if successful, will populate 'target' with details about the input.
 *
 * @param cam - device to contact
 * @param videoUid - the name of the video input to get config for
 * @param target - ohcmVideoInput object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmVideoInputByID(ohcmCameraInfo *cam, char *videoUid, ohcmVideoInput *target, uint32_t retryCounts);

/*
 * apply new 'video input configuration' to a camera.
 *
 * @param cam - device to contact
 * @param inputList - linked likst of ohcmVideoInput objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmVideoInputs(ohcmCameraInfo *cam, icLinkedList *inputList, uint32_t retryCounts);

// seems silly, but spec only allows JPEG right now
typedef enum {
    OHCM_SNAPSHOT_IMAGE_JPEG
} ohcmSnapShotImageType;

typedef enum {
    OHCM_VIDEO_FORMAT_MP4,
    OHCM_VIDEO_FORMAT_FLV
} ohcmVideoClipFormatType;

/* used for 'upload video clips' requests to the camera */
typedef struct {                // /OpenHome/Streaming/channels/[UID]/video/upload
    char *id;                   // An unique alphanumeric id
    ohcmSnapShotImageType   snapShotImageType;
    ohcmVideoClipFormatType videoClipFormatType;
    bool blockUploadComplete;   // if true, will block until the upload is complete (or errored)
    char *gatewayUrl;           // Gateway Server IP where to Upload Video
    char *eventUrl;             // Event Url
} ohcmUploadVideo;

/*
 * helper function to create a blank ohcmUploadVideo object
 */
ohcmUploadVideo *createOhcmUploadVideo();

/*
 * helper function to destroy the ohcmUploadVideo object
 */
void destroyOhcmUploadVideo(ohcmUploadVideo *obj);

/*
 * query the camera to upload a video clip.
 *
 * @param cam - device to contact
 * @param details - hints about the clip to upload (where, format, etc)
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode uploadOhcmVideo(ohcmCameraInfo *cam, ohcmUploadVideo *details, uint32_t retryCounts);

/*
 * query the camera to upload a video clip.
 *
 * @param cam - device to contact
 * @param videoUid - UID of the video channel to use for the picture
 * @param outputFilename - local file to save the picture to
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode downloadOhcmPicture(ohcmCameraInfo *cam, const char *videoUid, const char *outputFilename, uint32_t retryCounts);


/*-----------------------
 * general config functions
 *-----------------------*/

typedef struct {
    uint32_t  maxMediaTunnelReadyWait;              // if > 0, define the Max Media Tunnel Ready Wait in milliseconds
    uint32_t  mediaTunnelReadyTimersMinWait;        // if > 0, define the minimum time to wait in milliseconds before the next retry
    uint32_t  mediaTunnelReadyTimersMaxWait;        // if > 0, define the maximum time to wait in milliseconds before the next retry
    uint32_t  mediaTunnelReadyTimersStepsizeWait;   // if > 0, define is the stepsize used in exponential backoff
    uint32_t  mediaTunnelReadyTimersRetries;        // if > 0, define is the maximum number of retries. If <retries> equals 0, the number of retries is infinite
    uint32_t  mediaUploadTimersMinWait;             // if > 0, define the minimum time to wait in milliseconds before the next retry
    uint32_t  mediaUploadTimersMaxWait;             // if > 0, define the maximum time to wait in milliseconds before the next retry
    uint32_t  mediaUploadTimersStepsizeWait;        // if > 0, define is the stepsize used in exponential backoff
    uint32_t  mediaUploadTimersRetries;             // if > 0, define is the maximum number of retries. If <retries> equals 0, the number of retries is infinite
    uint64_t  mediaUploadTimersUploadTimeout;       // if > 0, define Max upload timeout in milliseconds
} ohcmConfigTimers;

typedef struct {
    char *timeMode;         // Time Mode
    struct tm *localTime;   // Structure Time will fill values of year,month,day and time
    char *timeZone;         // Time Zone
} ohcmTimeConfig;

typedef struct {
    bool     httpsEnabled;          // true if https is enabled
    uint32_t httpsPort;             // if > 0, define the https port number to use
    bool     httpsValidateCerts;    // true if it needs Certificate validation
    bool     httpEnabled;           // true if http is enabled
    uint32_t httpPort;              // if > 0, define the http port number
    bool     pollEnabled;           // true if poll is enabled
    uint32_t pollDefaultLinger;     // if > 0, define the poll Default Linger
} ohcmHostServer;

typedef struct {
    char *severity;				/* LogTrigger severity*/
    char *maxEntries;			/* LogTrigger Max Entries*/
    char *xmppEnabled;			/* value true if xmpp is enabled or else false */
    char *xmppUrl;				/* xmpp url*/
    char *httpsEnabled;			/* value true if https is enabled or else false */
    char *httpsUrl;				/* https url */
    char *pollEnabled;			/* value true if poll is enabled or else false */
    char *pollUrl;				/* poll url */
} ohcmLoggingConfig;

typedef struct {
    char *id;				/* An unique alphanumeric id */
    char *addressingFormatType;		/* Addressing Format Type*/
    char *hostName;				/* Host Name */
} ohcmNtpServer;

typedef struct {
    int *commandHistorySize;		/* Command History Size */
    int *notificationHistorySize;		/* Notification History Size */
} ohcmHistoryConfig;

/*
 * helper function to create a blank ohcmTimeConfig object
 */
ohcmTimeConfig *createOhcmTimeConfig();

/*
 * helper function to destroy the ohcmTimeConfig object
 */
void destroyOhcmTimeConfig(ohcmTimeConfig *obj);

/*
 * queries the camera for the current timezone
 *
 * @param cam - device to contact
 * @param timezone - string to store the timezone setting
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmTimeZoneInfo(ohcmCameraInfo *cam, char *timezone, uint32_t retryCounts);

 /**
  * Set up mutual TLS for a camera. This should only be done while pairing.
  * When configured, this module, which acts as a client, signs its TLS
  * request with a client certificate.
  *
  * @note cURL doesn't support TLS "Certificate Request," so the client will
  *       always offer up a signed request and certificate.
  *
  * @param camInfo A camera that is being configured
  *
  * @param allowedSubjects A NULL terminated list of subject common names (may be wildcard) the camera should accept.
  *                        If this list is NULL or empty, mTLS will be disabled.
  *
  * @warning if allowedSubjects does not match the client certificate given to setOhcmMutualTlsMode, camera access
  *          will be locked out until defaulted.
  *
  * @return An error/success code.
  */
ohcmResultCode ohcmConfigSetMutualTLS(ohcmCameraInfo *camInfo, const char *allowedSubjects[]);

/*
 * requests the camera set timezone
 *
 * @param cam - device to contact
 * @param timezone - timezone to use
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmTimeZoneInfo(ohcmCameraInfo *cam, const char *timezone, uint32_t retryCounts);


/*-----------------------
 * network config functions
 *-----------------------*/

typedef enum {
    OHCM_IPV4,
    OHCM_IPV6
} ohcmIpVersion;

typedef enum {
    OHCM_NET_ADDRESS_STATIC,
    OHCM_NET_ADDRESS_DYNAMIC
} ohcmNetAddType;

typedef enum {
    OHCM_SEC_DISABLED,
    OHCM_SEC_WEP,
    OHCM_SEC_WPA_PERSONAL,
    OHCM_SEC_WPA2_PERSONAL,
    OHCM_SEC_WPA_RADIUS,
    OHCM_SEC_WPA_ENTRERPRISE,
    OHCM_SEC_WPA2_ENTERPRISE,
    OHCM_SEC_WPA_WPA2_PERSONAL
} ohcmWifiSecurityType;

typedef enum {
    OHCM_WPA_ENCR_ALGO_NONE,
    OHCM_WPA_ENCR_ALGO_TKIP,
    OHCM_WPA_ENCR_ALGO_AES,
    OHCM_WPA_ENCR_ALGO_TKIP_AES
} ohcmWpaEncrAlgoType;

typedef struct {
    int32_t id;                     // the network unique id
    bool enabled;                   // value true if Network Interface List is enabled or else false
    ohcmIpVersion ipVersion;        // IPv4 or IPv6
    ohcmNetAddType addressingType;  // Static or Dynamic
    char *ipAddress;                // IPAddress
    char *subnetMask;               // IPAddress Subnet Mask
    char *gatewayIpAddress;         // DefaultGateway ipAddress
    char *primaryDNSIpAddress;      // PrimaryDNS ipAddress
    char *secondaryDNSIpAddress;    // secondaryDNS ipAddress
    bool wirelessEnabled;           // value true if Wireless mode is enabled or else false
    char *wirelessNetworkMode;      // Wireless Network Mode
    char *profileChannel;           // profile channel (auto or manual)
    char *profileSsid;              // profile ssid
    bool profileWmmEnabled;         // profile value true if WMM is enabled or else false
    ohcmWifiSecurityType profileSecurityMode;     // profile WirelessSecurity securityMode
    ohcmWpaEncrAlgoType profileAlgorithmType;     // profile WPA algorithmType
    char *profileSharedKey;         // profile WPA sharedKey
    int32_t statusRefreshInterval;    // Status Refresh Interval
    bool aggressiveRoamingEnabled; // value true if Aggressive Roaming is enabled or else false
    bool UPnPEnabled;               // value true if UPnP is enabled or else false
} ohcmNetworkInterface;


/*
 * helper function to create a blank ohcmNetworkInterface object
 */
ohcmNetworkInterface *createOhcmNetworkInterface();

/*
 * helper function to destroy the ohcmNetworkInterface object
 */
void destroyOhcmNetworkInterface(ohcmNetworkInterface *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmNetworkInterface from a linked list
 */
void destroyOhcmNetworkInterfaceFromList(void *item);

/**
 * Retrieve the list of configured network interfaces from a camera.
 *
 * @param cam The camera to pull the network information from.
 * @param output A linked list of all camera network interfaces.
 * @return The success/failure code for the access call.
 */
ohcmResultCode getOhcmNetworkInterfaceList(const ohcmCameraInfo *cam, icLinkedList* output);

/**
 * Configure a network interface.
 *
 * @param cam The camera to pull the network information from.
 * @param network The network configuration information.
 * @return The success/failure code for the access call.
 */
ohcmResultCode setOhcmNetworkInterface(const ohcmCameraInfo* cam,
                                       const ohcmNetworkInterface* network);

typedef struct {
    int interfaceId;
    bool enabled;
    char *ssid;
    char *bssid;
    char *channel;
    int rssidB;
    int signalStrength;
    int noiseIndB;
    int numAPs;
    //icLinkedList *accessPoints;   //TODO Wireless APs not added/implemented yet
} ohcmWirelessStatus;

/*
 * Creates an ohcmWirelessStatus
 */
ohcmWirelessStatus *createOhcmWirelessStatus();

/*
 * Cleans up an ohcmWirelessStatus
 */
void destroyOhcmWirelessStatus(ohcmWirelessStatus *status);

/*
 * attempts to get the wireless status of a camera
 */
ohcmResultCode getWirlessStatusOhcmCamera(ohcmCameraInfo *cam, const char* ifaceUid, ohcmWirelessStatus *target, uint32_t retryCounts);

/* currently disabled as this isn't used */
#ifdef OHCM_SUPPORTED

typedef struct {
    ohcmIpVersion ipVersion;        // IPv4 or IPv6
    ohcmNetAddType addressingType;  // Static or Dynamic
    char *ipAddress;                // IPAddress
    char *subnetMask;               // IPAddress Subnet Mask
    char *gatewayIpAddress;         // DefaultGateway ipAddress
    char *primaryDNSIpAddress;      // PrimaryDNS ipAddress
    char *secondaryDNSIpAddress;    // secondaryDNS ipAddress
} ohcmNetworkInterfaceIPAddress;

typedef enum {
    OHCM_WIFI_PROFILE_AUTO,
    OHCM_WIFI_PROFILE_MANUAL
} ohcmWifiProfile;

typedef struct {
    bool wirelessEnabled;           // true if Wireless mode is enabled
    char *wirelessNetworkMode;      // Wireless Network Mode
    ohcmWifiProfile profileChannel; // auto or manual
    char *profileSsid;              // profile ssid
    bool profileWmmEnabled;         // value true if WMM is enabled
    char *profileSecurityMode;      // profile WirelessSecurity securityMode
    char *profileAlgorithmType;     // profile WPA algorithmType
    char *profileSharedKey;         // profile WPA sharedKey
    char *statusRefreshInterval;    // Status Refresh Interval
    bool aggressiveRoamingEnabled;  // true if Aggressive Roaming is enabled
} ohcmNetworkInterfaceWireless;

RET_TYPE getNetworkInterfaceList(CameraInfo *cam, NETWORKINTERFACELIST **ptr, int curlRetryCounts);
RET_TYPE destroyNetworkInterfaceListStruct(NETWORKINTERFACELIST **networkInterfacePtr);
RET_TYPE getNetworkInterfaceListByID(CameraInfo *cam, NETWORKINTERFACELIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE addNetworkInterfaceListByID(CameraInfo *cam, NETWORKINTERFACELIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE getNetworkInterfaceIPAddress(CameraInfo *cam, NetworkInterfaceIPAddress *ipAddress, char *uid, int curlRetryCounts);
RET_TYPE addNetworkInterfaceIPAddress(CameraInfo *cam, NetworkInterfaceIPAddress *ipAddress, char *uid, int curlRetryCounts);
RET_TYPE destroyNetworkInterfaceIPAddressStruct(NetworkInterfaceIPAddress *ipAddress);
RET_TYPE getNetworkInterfaceWireless(CameraInfo *cam, NetworkInterfaceWireless *wireless, char *uid, int curlRetryCounts);
RET_TYPE addNetworkInterfaceWireless(CameraInfo *cam, NetworkInterfaceWireless *wireless, char *uid, int curlRetryCounts);
RET_TYPE destroyNetworkInterfaceWirelessStruct(NetworkInterfaceWireless *wireless);
RET_TYPE getNetworkInterfaceWirelessStatus(CameraInfo *cam, NetworkInterfaceWirelessStatus *wirelessStatus, char *uid, int curlRetryCounts);
RET_TYPE destroyNetworkInterfaceWirelessStatusStruct(NetworkInterfaceWirelessStatus *wirelessStatus);
RET_TYPE getNetworkInterfaceDiscovery(CameraInfo *cam, NetworkInterfaceDiscovery *discovery, char *uid, int curlRetryCounts);
RET_TYPE addNetworkInterfaceDiscovery(CameraInfo *cam, NetworkInterfaceDiscovery *discovery, char *uid, int curlRetryCounts);
RET_TYPE destroyNetworkInterfaceDiscoveryStruct(NetworkInterfaceDiscovery *discovery);

#endif  // OHCM_SUPPORTED

/* currently disabled as this isn't used */
#ifdef OHCM_SUPPORTED
struct AvailableAPList{
    char *ssid;				/* ssid */
    char *bssid;				/* bssid */
    char *rssidB;				/* rssidB */
    char *securityMode;			/* Security Mode*/
    struct AvailableAPList *Next;		/* Linked list pointer points to next element */
};
typedef struct AvailableAPList AVAILABLEAPLIST;

typedef struct {
    char *enabled;				/* value true if Network Interface Wireless Status is enabled or else false */
    char *channelNo;			/* Channel No*/
    char *ssid;				/* ssid */
    char *bssid;				/* bssid */
    char *rssidB;				/* rssidB */
    int  *signalStrength;			/* Signal Strength */
    int  *noiseIndB;			/* Noise in dB */
    int  *numOfAPs;				/* Number of APs Available */
    AVAILABLEAPLIST *availableAPListPtr;	/* AvailableAPList Pointer */
}NetworkInterfaceWirelessStatus;

typedef struct {
    char *upnpEnabled;			/* UPnP Enabled */
    char *zeorconfEnabled;			/* Zero Config Enabled */
    char *multicastEnabled;			/* Multicast Enabled */
    char *ipAddress;			/* ip Address */
    char *ipv6Address;			/* ipv6 Address*/
    int  *portNo;				/* Port Number */
    int  *ttl;				/* ttl */
}NetworkInterfaceDiscovery;

typedef struct {
    char *siteID;				/* authorizationInfo siteID */
    char *sharedSecret;			/* authorizationInfo Shared Secret */
    char *pendingKey;			/* authorizationInfo Pending Key */
    char *credentialGWURL;			/* authorizationInfo CredentialGWURL */
}AuthorizationInfo;
#endif  // OHCM_SUPPORTED


/*-----------------------
 * motion detect functions
 *-----------------------*/

/* currently disabled as this isn't used */
#ifdef OHCM_SUPPORTED

typedef struct {
    int  *samplingInterval;
    char *motionTypes;
    int  *maxRegions;
    int  *minHorizontalResolution;
    int  *minVerticalResolution;
}MotionDetectionCapabilities;

RET_TYPE setMotionDetectionVideoUID(CameraInfo *cam, MOTIONDETECTIONLIST *ptr, char *uid, int curlRetryCounts);
RET_TYPE destroyMotionDetectionVideoStruct(MOTIONDETECTIONLIST *ptr);
RET_TYPE setEvent(CameraInfo *cam, CreateEventNotification *setEventNotification, int curlRetryCounts);
RET_TYPE destroyEventStruct(CreateEventNotification *setEventNotification);
RET_TYPE getMotionDetection(CameraInfo *cam, MOTIONDETECTIONLIST **ptr, int curlRetryCounts);
RET_TYPE destroyMotionDetectionListStruct(MOTIONDETECTIONLIST **motionDetectionPtr);
RET_TYPE getMotionDetectionByID(CameraInfo *cam, MOTIONDETECTIONLIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE getMotionDetectionCapabilities(CameraInfo *cam, MotionDetectionCapabilities *mdCapabilities, char *uid, int curlRetryCounts);
RET_TYPE destroyMotionDetectionCapabilitiesStruct(MotionDetectionCapabilities *mdCapabilities);

struct SoundDetectionList{			/* Linked List */
    char *id;				/* An unique alphanumeric id */
    char *enabled;				/* value true if Sound Detection is enabled or else false */
    int  *inputID;				/* Sound Detection input ID */
    char *triggeringType;			/* Triggering Type (Rising or falling edge) */
    int  *detectionThreshold;		/* Detection Threshold */
    struct SoundDetectionList *Next;	/* Linked list pointer points to next element */
};
typedef struct SoundDetectionList SOUNDDETECTIONLIST;

RET_TYPE getEventTriggers(CameraInfo *cam, EVENTTRIGGERLIST **ptr, int curlRetryCounts);
RET_TYPE addEventTriggers(CameraInfo *cam, EVENTTRIGGERLIST **ptr, int curlRetryCounts);
RET_TYPE getEventNotificationMethods(CameraInfo *cam, EventNotificationMethods *eNotificationMethods, int curlRetryCounts);
RET_TYPE addEventNotificationMethods(CameraInfo *cam, EventNotificationMethods *eNotificationMethods, int curlRetryCounts);
RET_TYPE getEventTriggersByID(CameraInfo *cam, EVENTTRIGGERLIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE addEventTriggersByID(CameraInfo *cam, EVENTTRIGGERLIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE deleteEventTriggers(CameraInfo *cam, char *uid, int curlRetryCounts);
RET_TYPE getEventTriggerNotifications(CameraInfo *cam, EVENTTRIGGERNOTOFICATIONLIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE addEventTriggerNotifications(CameraInfo *cam, EVENTTRIGGERNOTOFICATIONLIST **ptr, char *uid, int curlRetryCounts);
RET_TYPE getEventTriggerNotificationsByID(CameraInfo *cam, EVENTTRIGGERNOTOFICATIONLIST **ptr, char *uid, char *notifyID, int curlRetryCounts);
RET_TYPE addEventTriggerNotificationsByID(CameraInfo *cam, EVENTTRIGGERNOTOFICATIONLIST **ptr, char *uid, char *notifyID, int curlRetryCounts);
RET_TYPE deleteEventTriggerNotifications(CameraInfo *cam, char *uid, char *notifyID, int curlRetryCounts);
#endif  // OHCM_SUPPORTED


typedef enum {
    OHCM_MOTION_DIR_LEFT_RIGHT,
    OHCM_MOTION_DIR_RIGHT_LEFT,
    OHCM_MOTION_DIR_UP_DOWN,
    OHCM_MOTION_DIR_DOWN_UP,
    OHCM_MOTION_DIR_ANY,
} ohcmMotionDirection;

typedef enum {
    OHCM_MOTION_REGION_GRID,
    OHCM_MOTION_REGION_ROI
} ohcmMotionRegionType;

typedef struct  {
    char *id;                           // An unique alphanumeric id
    bool enabled;                       // value true if Motion Detection is enabled or else false
    char *inputID;                      // Motion Detection input ID
    uint32_t samplingInterval;          // Sampling Interval
    uint32_t startTriggerTime;          // Start Trigger Time
    uint32_t endTriggerTime;            // End Trigger Time
    ohcmMotionDirection  directionSensitivity;  // Direction Sensitivity
    ohcmMotionRegionType regionType;    // Region Type e.g Region of Intrest
    uint32_t minObjectSize;             // Min Object Size
    uint32_t maxObjectSize;             // Min Object Size
    uint32_t rowGranularity;            // Row Granularity
    uint32_t columnGranularity;         // Column Granularity
    uint32_t minHorizontalResolution;   // Min Horizontal Resolution
    uint32_t minVerticalResolution;     // Min Vertical Resolution
    uint32_t sourceHorizontalResolution;// Source Horizontal Resolution
    uint32_t sourceVerticalResolution;  // Source Vertical Resolution
    icLinkedList *regionList;           // list of ohcmMotionDetectRegion objects
} ohcmMotionDetection;

/*
 * helper function to create a blank ohcmMotionDetection object
 */
ohcmMotionDetection *createOhcmMotionDetection();

/*
 * helper function to destroy the ohcmMotionDetection object
 */
void destroyOhcmMotionDetection(ohcmMotionDetection *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmMotionDetection from a linked list
 */
void destroyOhcmMotionDetectionFromList(void *item);

typedef struct {
    char *id;                       // motionDetectionRegionList An unique alphanumeric id
    bool enabled;                   // true if Motion Detection Region List is enabled
    bool maskEnabled;               // true if Mask is enabled
    int32_t  sensitivityLevel;      // motionDetectionRegionList Sensitivity Level
    int32_t  detectionThreshold;    // motionDetectionRegionList Detection Threshold
    icLinkedList *coordinatesList;  // list of ohcmRegionCoordinate objects
} ohcmMotionDetectRegion;

/*
 * helper function to create a blank ohcmMotionDetectRegion object
 */
ohcmMotionDetectRegion *createOhcmMotionDetectRegion();

/*
 * helper function to destroy the ohcmMotionDetectRegion object
 */
void destroyOhcmMotionDetectRegion(ohcmMotionDetectRegion *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmMotionDetectRegion from a linked list
 */
void destroyOhcmMotionDetectRegionFromList(void *item);

typedef struct {
    int32_t  positionX;             // regionCoordinatesList positionX
    int32_t  positionY;             // regionCoordinatesList positionY
} ohcmRegionCoordinate;

/*
 * helper function to create a blank ohcmRegionCoordinate object
 */
ohcmRegionCoordinate *createOhcmRegionCoordinate();

/*
 * helper function to destroy the ohcmRegionCoordinate object
 */
void destroyOhcmRegionCoordinate(ohcmRegionCoordinate *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmRegionCoordinate from a linked list
 */
void destroyOhcmRegionCoordinateFromList(void *item);

/*
 * query the camera for the current 'motion detection configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmMotionDetection
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmMotionDetection objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmMotionDetection(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts);

/*
 * request the camera apply a 'motion detection configuration' for a UID
 * (uses the settings->uid).
 *
 * @param cam - device to contact
 * @param settings - the motion detection settings to apply
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmMotionDetectionForUid(ohcmCameraInfo *cam, ohcmMotionDetection *settings, uint32_t retryCounts);


typedef struct {                    // /OpenHome/Event/notification/host
    char *id;                       // An unique alphanumeric id
    char *url;                      // URL to post to
    char *httpAuthenticationMethod; // http Authentication Method
} ohcmHostNotif;

/*
 * helper function to create a blank ohcmHostNotif object
 */
ohcmHostNotif *createOhcmHostNotif();

/*
 * helper function to destroy the ohcmHostNotif object
 */
void destroyOhcmHostNotif(ohcmHostNotif *obj);

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmHostNotif from a linked list
 */
void destroyOhcmHostNotifFromList(void *item);

typedef struct {                    // /OpenHome/Event/notification
    icLinkedList *hostNotifList;    // list of ohcmHostNotif objects
    bool         nonMediaEvent;     // include non-media events
} ohcmEventNotifMethods;

/*
 * helper function to create a blank ohcmEventNotifMethods object
 */
ohcmEventNotifMethods *createOhcmEventNotifMethods();

/*
 * helper function to destroy the ohcmEventNotifMethods object
 */
void destroyOhcmEventNotifMethods(ohcmEventNotifMethods *obj);

typedef struct {                    // /OpenHome/Event/triggers/[UID]/notifications
    char *notificationID;           // Notification ID
    char *notificationMethod;       // Notification Mathod
    char *notificationRecurrence;   // Notification Recurrence
    uint32_t notificationInterval;  // Notification Interval
} ohcmEventTriggerNotif;

/*
 * helper function to create a blank ohcmEventTriggerNotif object
 */
ohcmEventTriggerNotif *createOhcmEventTriggerNotif();

/*
 * helper function to destroy the ohcmEventTriggerNotif object
 */
void destroyOhcmEventTriggerNotif(ohcmEventTriggerNotif *obj);

typedef enum {
    OHCM_EVENT_TRIGGER_PIRMD,
    OHCM_EVENT_TRIGGER_VMD,
    OHCM_EVENT_TRIGGER_SND,
    OHCM_EVENT_TRIGGER_TMPD,
} ohcmEventTriggerType;

typedef struct {
    char *id;                       // An unique alphanumeric id
    ohcmEventTriggerType eventType; // Event Type
    char *eventTypeInputID;         // Event Type InputID
    char *eventDescription;         // Event Description
    char *inputIOPortID;            // Input IO Port ID
    uint32_t intervalBetweenEvents;     // Interval Between Two Events
    ohcmEventTriggerNotif *notif;   // list of ohcmEventTriggerNotif objects
} ohcmEventTrigger;

/*
 * helper function to create a blank ohcmEventTrigger object
 */
ohcmEventTrigger *createOhcmEventTrigger();

/*
 * helper function to destroy the ohcmEventTrigger object
 */
void destroyOhcmEventTrigger(ohcmEventTrigger *obj);

/*
 * request the camera apply a 'motion event delivery' mechanism
 *
 * @param cam - device to contact
 * @param trigger - ???
 * @param method - mechanism to use for event delivery
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmMotionEvent(ohcmCameraInfo *cam, ohcmEventTrigger *trigger, ohcmEventNotifMethods *method, uint32_t retryCounts);


/* currently disabled as this isn't used */
#ifdef OHCM_SUPPORTED
RET_TYPE getNotificationHost(CameraInfo *cam, HOSTNOTIFICATIONLIST **host, int curlRetryCounts);
RET_TYPE addNotificationHost(CameraInfo *cam, HOSTNOTIFICATIONLIST **host, int curlRetryCounts);
RET_TYPE getNotificationHostByID(CameraInfo *cam, HOSTNOTIFICATIONLIST **host, char *uid, int curlRetryCounts);
RET_TYPE addNotificationHostByID(CameraInfo *cam, HOSTNOTIFICATIONLIST **host, char *uid, int curlRetryCounts);
#endif  // OHCM_SUPPORTED


// result of a "pollNotification" request
typedef enum {
    POLL_NO_EVENT,      // no new events to report
    POLL_MOTION_EVENT,  // a new motion event was discovered
    POLL_BUTTON_EVENT,  // a button event (like from a doorbell camera)
    POLL_COMM_ERROR,    // unable to ask the device due to I/O error
    POLL_RESULT_ERROR   // device reported an error in the response
} ohcmPollNotifResult;

/*
 * Perform a blocking 'poll' of the camera to see if there are motion events to report.
 * This synchronous call will block for 'waitSecs' seconds for an event to occur.
 *
 * @param cam : The camera to poll
 * @param waitSecs : number of seconds to wait for an event before disconnecting
 *
 * @return POLL_NO_EVENT if 'waitSecs' elapsed with nothing to report;
 *         POLL_MOTION_EVENT if a motion event occurs;
 *         or POLL_ERROR if unable to connect to the camera device
 */
ohcmPollNotifResult getOhcmPollNotification(ohcmCameraInfo *cam, uint8_t waitSecs);

//RET_TYPE getOhcmMotionDetectionConfiguration(CameraInfo *cam, MOTIONDETECTIONLIST **mdList, int curlRetryCounts);


/*-----------------------
 *  config functions
 *-----------------------*/

typedef struct {
    ohcmDeviceInfo *device;                     // the device to configure
    icLinkedList *streamChannelsList;           // list of ohcmStreamChannel objects
    icLinkedList *securityAccountList;          // list of ohcmSecurityAccount objects
    bool eventNotification;                     // variable of  structure
    icLinkedList *audioChannelList;             // list of ohcmAudioChannel objects
    icLinkedList *videoInputList;               // list of ohcmVideoInput objects
    icLinkedList *configTimerList;              // list of ohcmConfigTimers objects
    ohcmTimeConfig *time;
    ohcmConfigTimers timers;
    ohcmHostServer hostServer;                  // hostServer settings
    ohcmLoggingConfig loggingConfig;            // logging settings
    ohcmNtpServer ntpServerPtr;                 // NTP settings
    ohcmHistoryConfig historyConfig;            // history settings
    icLinkedList *networkInterfaceList;         // list of ohcmNetworkInterface objects
    icLinkedList *motionDetectionList;          // list of ohcmMotionDetection objects
//    AuthorizationInfo authorizationInfo;      // variable of authorizationInfo structure  TODO: is this used?
//    SOUNDDETECTIONLIST *soundDetectionPtr;    // list of sound detection                  TODO: support this someday
} ohcmConfigFile;

/*
 * helper function to create a blank ohcmConfigFile object
 */
ohcmConfigFile *createOhcmConfigFile();

/*
 * helper function to destroy the ohcmConfigFile object
 */
void destroyOhcmConfigFile(ohcmConfigFile *obj);

/*
 * query the camera for the 'massive configuration' from the device
 *
 * @param cam - device to contact
 * @param conf - object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmConfigFile(ohcmCameraInfo *cam, ohcmConfigFile *conf, uint32_t retryCounts);

/*
 * apply the 'massive configuration' settings on the camera
 *
 * @param cam - device to contact
 * @param conf - settings to apply
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmConfigFile(ohcmCameraInfo *cam, ohcmConfigFile *conf, uint32_t retryCounts);


/*-----------------------
 * media tunnel functions
 *-----------------------*/

typedef struct {
    char *sessionID;            // session ID
    char *gatewayURL;           // gateway URL
    char *failureURL;           // failure URL
} ohcmMediaTunnelRequest;

typedef struct {                // /openhome/streaming/mediatunnel/create */ /* Linked List */
    char *sessionID;            // session ID
    char *transportSecurity;    // Transport Security
    struct tm *startTime;       // Structure Time will fill values of year,month,day and time
    int  *elapsedTime;          // Elapsed Time
    char *state;                // State (start or stop)
} ohcmMediaTunnelStatus;

/*
 * helper function to create a blank ohcmMediaTunnelRequest object
 */
ohcmMediaTunnelRequest *createOhcmMediaTunnelRequest();

/*
 * helper function to destroy the ohcmMediaTunnelRequest object
 */
void destroyOhcmMediaTunnelRequest(ohcmMediaTunnelRequest *obj);

/*
 * ask the camera to start a media tunnel session
 *
 * @param cam - device to contact
 * @param conf - details for the media tunnel request
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode startOhcmMediaTunnelRequest(ohcmCameraInfo *cam, ohcmMediaTunnelRequest *conf, uint32_t retryCounts);

/*
 * ask the camera to stop a media tunnel session
 *
 * @param cam - device to contact
 * @param sessionId - media tunnel session to stop
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode stopOhcmMediaTunnelRequest(ohcmCameraInfo *cam, char *sessionId, uint32_t retryCounts);


/*-----------------------
 * upgrade firmware functions
 *-----------------------*/

typedef struct {            // /OpenHome/System/updateFirmware
    char *url;              // URL where the firmware is
    char *fwVersion;        // Firmware Version
    char *md5checksum;      // md5checksum
} ohcmUpdateFirmwareRequest;

/*
 * helper function to create a blank ohcmUpdateFirmwareRequest object
 */
ohcmUpdateFirmwareRequest *createOhcmUpdateFirmwareRequest();

/*
 * helper function to destroy the ohcmUpdateFirmwareRequest object
 */
void destroyOhcmUpdateFirmwareRequest(ohcmUpdateFirmwareRequest *obj);

/*
 * ask the camera to start a firmware update.  if successful, it will be
 * possible to get update state via ohcmUpdateFirmareStatus
 *
 * @param cam - device to contact
 * @param req - details for the firmware update
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode startOhcmUpdateFirmwareRequest(ohcmCameraInfo *cam, ohcmUpdateFirmwareRequest *conf, uint32_t retryCounts);


typedef struct {                    // /OpenHome/System/updateFirmware/status
    bool updateSuccess;             // update success
    char *updateState;              // update state ("failure" or "success")
    struct tm *updateTime;          // Structure Time will fill values of year,month,day and time
    char *url;                      // url
    uint32_t downloadPercentage;    // percentage of download
} ohcmUpdateFirmwareStatus;

/*
 * helper function to create a blank ohcmMediaTunnelRequest object
 */
ohcmUpdateFirmwareStatus *createOhcmUpdateFirmwareStatus();

/*
 * helper function to destroy the ohcmUpdateFirmwareStatus object
 */
void destroyOhcmUpdateFirmwareStatus(ohcmUpdateFirmwareStatus *obj);

/*
 * ask the camera to retrieve the status of the 'update firmware' request
 *
 * @param cam - device to contact
 * @param status - target object to populate with details
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmUpdateFirmwareStatus(ohcmCameraInfo *cam, ohcmUpdateFirmwareStatus *status, uint32_t retryCounts);


/*-----------------------
 * reboot/reset functions
 *-----------------------*/

/*
 * attempts to reboot the camera using OpenHome API
 */
ohcmResultCode rebootOhcmCamera(ohcmCameraInfo *cam, uint32_t retryCounts);

/*
 * attempts to reset the camera to factory
 */
ohcmResultCode factoryResetOhcmCamera(ohcmCameraInfo *cam, uint32_t retryCounts);


/** TODO: debug print

//Print Functions
void printDeviceInfo(DeviceInfo *device);
void printAccountInfo(SECURITYACCOUNTLIST **acc);
void printSupportedAPI(APILIST **api);
void printStreamingChannels(STREAMINGCHANNELSLIST **ptr);
void printStreamingChannelsByID(STREAMINGCHANNELSLIST **ptr, char *uid);
void printStreamingCapabilities(StreamingCapabilities *scap);
void printStreamingStatus(StreamingStatus *sstatus);
void printStreamingChannelStatus(STREAMINGSTATUSLIST **ss_ptr);
void printEventNotification(EventNotification *enotification);
void printAudioChannelList(AUDIOCHANNELSLIST **ptr);
void printVideoInput(VIDEOINPUTSLIST **ptr);
void printConfigTimers(ConfigTimers *ctimers);
void printTime(TimeStructure *time);
void printHostServer(HostServer *hserver);
void printLoggingConfig(LoggingConfig *lconfig);
void printNTPServerList(NTPSERVERLIST **ptr);
void printHistoryConfiguration(HistoryConfiguration *hconfiguration);
void printNetworkInterfaceList(NETWORKINTERFACELIST **ptr);
void printAuthorizationInfo(AuthorizationInfo *ainfo);
void printMotionDetectionList(MOTIONDETECTIONLIST **ptr);
void printfConfigFile(ConfigFile *conf);
void printMediaTunnel(MEDIATUNNELSTATUSLIST **ptr);
void printMediaTunnelStatus(MEDIATUNNELSTATUSLIST **ptr, char *uid);
void printUpdateFirmwareStatus(UpdateFirmwareStatus *ufstatus);
void printEventTriggers(EVENTTRIGGERLIST **ptr);
void printEventNotificationMethods(EventNotificationMethods *eNotificationMethods);
void printEventTriggersByID(EVENTTRIGGERLIST **ptr, char *uid);
void printEventTriggerNotifications(EVENTTRIGGERNOTOFICATIONLIST **ptr);
void printEventTriggerNotificationsByID(EVENTTRIGGERNOTOFICATIONLIST **ptr, char *notifyID);
void printNotificationHost(HOSTNOTIFICATIONLIST **host);
void printNotificationHostByID(HOSTNOTIFICATIONLIST **host, char *uid);
void printNetworkInterfaceList(NETWORKINTERFACELIST **ptr);
void printNetworkInterfaceListByID(NETWORKINTERFACELIST **ptr, char *uid);
void printNetworkInterfaceIPAddress(NetworkInterfaceIPAddress *ipAddress);
void printNetworkInterfaceWireless(NetworkInterfaceWireless *wireless);
void printNetworkInterfaceWirelessStatus(NetworkInterfaceWirelessStatus *wirelessStatus);
void printNetworkInterfaceDiscovery(NetworkInterfaceDiscovery *discovery);
void printAudioChannelList(AUDIOCHANNELSLIST **audioChannelsPtr);
void printAudioChannelsByID(AUDIOCHANNELSLIST **audioChannelsPtr, char *uid);
void printVideoInput(VIDEOINPUTSLIST **videoInputsPtr);
void printVideoInputsChannelsByID(VIDEOINPUTSLIST **videoInputsPtr, char *uid);
void printMotionDetectionList(MOTIONDETECTIONLIST **ptr);
void printMotionDetectionByID(MOTIONDETECTIONLIST **ptr, char *uid);
void printMotionDetectionCapabilities(MotionDetectionCapabilities *mdCapabilities);

 **/

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // IC_OPENHOME_CAMERA_H
