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
 * cameraDevice.c
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <uuid/uuid.h>
#include <curl/curl.h>
#include <pthread.h>

#include <icBuildtime.h>
#include <ssdp/ssdp.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icTime/timeTracker.h>
#include <icUtil/macAddrUtils.h>
#include <icSystem/hardwareCapabilities.h>
#include <icConcurrent/timedWait.h>
#include <urlHelper/urlHelper.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <icConcurrent/threadUtils.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/sslVerify.h>
#include <openHomeCamera/ohcm.h>
#include <versionUtils.h>
#include "deviceDriver.h"
#include "deviceServicePrivate.h"
#include "cameraDevice.h"
#include "cameraPrivate.h"

#ifdef CONFIG_SERVICE_NETWORK
#include <icIpc/ipcMessage.h>
#include <networkService/networkService_ipc.h>
#endif

// stream identifiers
#define VIDEO_UPLOAD_STREAM_ID          0       // video clips
#define VIDEO_REMOTE_H264_STREAM_ID     0       // mobile/SP high quality video
#define VIDEO_LOCAL_STREAM_ID           1       // local video
#define VIDEO_REMOTE_MJPEG_STREAM_ID    2       // mjpeg for some mobile

#define REMOTE_STREAM_FRAME_RATE        15
#define REMOTE_STREAM_BIT_RATE          768
#define REMOTE_STREAM_FRAME_INTERVAL    15
#define LOCAL_STREAM_FRAME_RATE         5
#define LOCAL_STREAM_BIT_RATE           512
#define LOCAL_STREAM_FRAME_INTERVAL     10

#define PRE_CAPTURE_LENGTH              5000 // 5 seconds
#define POST_CAPTURE_LENGTH             10000 // 10 seconds

typedef enum {
    VIDEO_QUALITY_CBR,
    VIDEO_QUALITY_VBR
} videoQualityType;

typedef struct _videoResolutionAndRatio {
    uint32_t width;
    uint32_t height;
    const char *ratio;
} videoResolutionAndRatio;

typedef enum
{
    /**
     * Network trust level unknown. The network should be treated
     * as untrusted until a determination can be made.
     */
    NW_TRUST_UNKNOWN = 0,

    /**
     * Network is not trusted (customer controls the network credentials).
     */
    NW_TRUST_UNTRUSTED,

    /**
     * Network is trusted (customer does not know the network
     * credentials and is considered sufficiently secure).
     */
    NW_TRUST_TRUSTED
} NetworkTrustLevel;

static videoResolutionAndRatio QVGA =   { 320, 240, "4:3" };
static videoResolutionAndRatio PAN240 = { 640, 240, "8:3" };
static videoResolutionAndRatio PAN360 = { 640, 360, "16:9" };
static videoResolutionAndRatio VGA =    { 640, 480, "4:3" };
static videoResolutionAndRatio D1 =     { 720, 480, "4:3" };
static videoResolutionAndRatio PAN480 = { 1280, 480, "8:3" };
static videoResolutionAndRatio HD720 =  { 1280, 720, "16:9" };
static videoResolutionAndRatio PAN720 = { 1920, 720, "8:3" };
static videoResolutionAndRatio HD1080 = { 1920, 1080, "16:9" };

static pthread_once_t initOnce = PTHREAD_ONCE_INIT;
static void oneTimeInit(void);

static pthread_mutex_t propsMtx = PTHREAD_MUTEX_INITIALIZER;
static uint32_t offlineErrorCount = ERROR_COUNT_COMM_FAIL_THRESHOLD;
static uint32_t onlineSuccessCount = SUCCESS_COUNT_COMM_RESTORE_THRESHOLD;
static uint32_t pingIntervalSecs = LONG_POLL_WAIT_SECONDS;

static pthread_mutex_t nwTrustMtx = PTHREAD_MUTEX_INITIALIZER;
static NetworkTrustLevel networkTrustLevel = NW_TRUST_UNKNOWN;
static NetworkTrustLevel getNetworkTrustLevel(void);

/*
 * private functions
 */
static ohcmCameraInfo *allocCameraInfo(cameraDevice *device);
static bool isCameraIntegratedPeripheral(const char *uuid);
static ohcmResultCode configureMotionDetectionMechanism(ohcmCameraInfo *info, uint32_t numRetries, bool usePush);
static char *getEventPushUrl(const char *macAddress);
static void configureSercommEventPushUrl(cameraDevice *device);
static bool findStreamChannelById(void *searchVal, void *item);
static void handlePropertyChange(const cpePropertyEvent *event);
static void setTLSVerifyLevel(sslVerify level);
static bool waitForCameraRestart(ohcmCameraInfo *cam, bool waitForDeath, uint32_t timeoutSeconds);

static const char *DEFAULT_ALLOWED_TLS_SUBJECTS[] =
{
    "*.xcal.tv",
    "*.xfinityhome.com",
    NULL
};

/**
 * Initialize once. Do not call directly; instead, pass to pthread_once().
 */
static void oneTimeInit(void)
{
    /* These are intentionally not unregistered: it is only registered once per process via pthread_once */
    register_cpePropertyEvent_eventListener((handleEvent_cpePropertyEvent) handlePropertyChange);

    uint32_t offlineThreshold = getPropertyAsUInt32(CPE_CAMERA_OFFLINE_DETECTION_THRESHOLD_CNT, ERROR_COUNT_COMM_FAIL_THRESHOLD);
    uint32_t onlineThreshold = getPropertyAsUInt32(CPE_CAMERA_ONLINE_DETECTION_THRESHOLD_CNT, SUCCESS_COUNT_COMM_RESTORE_THRESHOLD);
    uint32_t pingInterval = getPropertyAsUInt32(CPE_CAMERA_PING_INTERVAL_SEC, LONG_POLL_WAIT_SECONDS);

    pthread_mutex_lock(&propsMtx);

    offlineErrorCount = offlineThreshold;
    onlineSuccessCount = onlineThreshold;
    pingIntervalSecs = pingInterval;

    pthread_mutex_unlock(&propsMtx);

    setTLSVerifyLevel(getSslVerifyProperty(SSL_VERIFY_HTTP_FOR_DEVICE));
}

static void setTLSVerifyLevel(sslVerify level)
{
    /*
     * Legacy behavior on 'managed' networks is to always use SSL_VERIFY_NONE.
     * 'level' should only be set on 'unmanaged' networks where the camera server cert must be valid.
     * Older cameras (many more are supported on 'managed') can easily have invalid certs.
     *
     * TODO: icam2 and xcam1 cameras need some kind of certificate management API (or need to become OTT)
     */

    if (getNetworkTrustLevel() == NW_TRUST_TRUSTED)
    {
        level = SSL_VERIFY_NONE;
    }

    ohcmSetTLSVerify(level);
}

static void handlePropertyChange(const cpePropertyEvent *event)
{
    if (event != NULL && event->propKey != NULL)
    {
        if(strcmp(event->propKey, CPE_CAMERA_OFFLINE_DETECTION_THRESHOLD_CNT) == 0)
        {
            pthread_mutex_lock(&propsMtx);
            offlineErrorCount = getPropertyEventAsUInt32(event, ERROR_COUNT_COMM_FAIL_THRESHOLD);
            pthread_mutex_unlock(&propsMtx);
        }
        else if (strcmp(event->propKey, CPE_CAMERA_ONLINE_DETECTION_THRESHOLD_CNT) == 0)
        {
            pthread_mutex_lock(&propsMtx);
            onlineSuccessCount = getPropertyEventAsUInt32(event, SUCCESS_COUNT_COMM_RESTORE_THRESHOLD);
            pthread_mutex_unlock(&propsMtx);
        }
        else if (strcmp(event->propKey, CPE_CAMERA_PING_INTERVAL_SEC) == 0)
        {
            pthread_mutex_lock(&propsMtx);
            pingIntervalSecs = getPropertyEventAsUInt32(event, LONG_POLL_WAIT_SECONDS);
            pthread_mutex_unlock(&propsMtx);
        }

        sslVerify tmp = sslVerifyConvertCPEPropEvent((cpePropertyEvent *) event, SSL_VERIFY_HTTP_FOR_DEVICE);
        if (tmp != SSL_VERIFY_INVALID)
        {
            setTLSVerifyLevel(tmp);
        }
    }
}

static NetworkTrustLevel getNetworkTrustLevel(void)
{
    NetworkTrustLevel retVal;

    pthread_mutex_lock(&nwTrustMtx);

    if (networkTrustLevel == NW_TRUST_UNKNOWN)
    {
        // TODO: Needs implementation
        bool isGenericRouter = true;
        if (isGenericRouter == false)
        {
            networkTrustLevel = NW_TRUST_TRUSTED;
        }
        else
        {
            networkTrustLevel = NW_TRUST_UNTRUSTED;
        }
    }
    retVal = networkTrustLevel;

    pthread_mutex_unlock(&nwTrustMtx);

    icLogInfo(LOG_TAG, "Gateway is %s", retVal == NW_TRUST_TRUSTED ? "trusted" : "not trusted or unknown");

    return retVal;
}

/*--===================================================================================--*
 *
 * object create/destroy
 *
 *--===================================================================================--*/

/*
 * apply resolution & ratio to the cameraDevice (for local storage)
 */
static void applyVideoResolutionToStorage(cameraDevice *target, videoResolutionAndRatio *resolutionAndRatio)
{
    // create the container within the cameraDevice object (if necessary)
    //
    if (target->videoSettings == NULL)
    {
        target->videoSettings = (cameraVideoSettings *) calloc(1, sizeof(cameraVideoSettings));
    }

    // save the W:H string (we used to do WxH, but Converge uses ':' not 'x')
    //
    char temp[32];
    sprintf(temp, "%d:%d", resolutionAndRatio->width, resolutionAndRatio->height);
    free(target->videoSettings->videoResolution);
    target->videoSettings->videoResolution = strdup(temp);

    // save the ratio
    //
    free(target->videoSettings->aspectRatio);
    target->videoSettings->aspectRatio = strdup(resolutionAndRatio->ratio);
}

/*
 * extract values from 'channel video stream settings' and apply to the cameraDevice object (for local storage)
 */
static void applyCameraStreamingSettingsToStorage(cameraDevice *target, ohcmStreamChannel *channel)
{
    if (channel == NULL || channel->videoResolutionHeight <= 0 || channel->videoResolutionWidth <= 0)
    {
        // bogus
        return;
    }

    // look at each 'known' resolution so we can locate the ratio
    //
    if (channel->videoResolutionWidth == QVGA.width && channel->videoResolutionHeight == QVGA.height)
    {
        applyVideoResolutionToStorage(target, &QVGA);
    }
    else if (channel->videoResolutionWidth == PAN240.width && channel->videoResolutionHeight == PAN240.height)
    {
        applyVideoResolutionToStorage(target, &PAN240);
    }
    else if (channel->videoResolutionWidth == PAN360.width && channel->videoResolutionHeight == PAN360.height)
    {
        applyVideoResolutionToStorage(target, &PAN360);
    }
    else if (channel->videoResolutionWidth == VGA.width && channel->videoResolutionHeight == VGA.height)
    {
        applyVideoResolutionToStorage(target, &VGA);
    }
    else if (channel->videoResolutionWidth == D1.width && channel->videoResolutionHeight == D1.height)
    {
        applyVideoResolutionToStorage(target, &D1);
    }
    else if (channel->videoResolutionWidth == PAN480.width && channel->videoResolutionHeight == PAN480.height)
    {
        applyVideoResolutionToStorage(target, &PAN480);
    }
    else if (channel->videoResolutionWidth == HD720.width && channel->videoResolutionHeight == HD720.height)
    {
        applyVideoResolutionToStorage(target, &HD720);
    }
    else if (channel->videoResolutionWidth == PAN720.width && channel->videoResolutionHeight == PAN720.height)
    {
        applyVideoResolutionToStorage(target, &PAN720);
    }
    else if (channel->videoResolutionWidth == HD1080.width && channel->videoResolutionHeight == HD1080.height)
    {
        applyVideoResolutionToStorage(target, &HD1080);
    }
    else
    {
        // use what is in settings and determine the ratio
        //
        videoResolutionAndRatio temp;
        temp.width = channel->videoResolutionWidth;
        temp.height = channel->videoResolutionHeight;

        // see if 4:3 or 16:9
        //
        float test = (float)channel->videoResolutionWidth / (float)channel->videoResolutionHeight;
        if (test >= 1.7)
        {
            temp.ratio = "16:9";
        }
        else
        {
            temp.ratio = "4:3";
        }
        applyVideoResolutionToStorage(target, &temp);
    }
}

/*
 * obtain stream settings from the camera and apply to the cameraDevice object (for storage)
 */
static void loadCameraStreamingSettings(ohcmCameraInfo *camInfo, cameraDevice *target)
{
    // get the video settings information from the device
    //
    icLinkedList *streamList = linkedListCreate();
    ohcmResultCode rc = getOhcmStreamingChannels(camInfo, streamList, 2);
    if (rc == OHCM_SUCCESS)
    {
        // NOTE: the set of streams we just read is a linked list, which should have
        //       2 or 3 channels (depending on what the camera supports).
        //   Touchstone only uses channel #0
        //   Converge also sets #1 for local streaming to the local screen.
        //
        char streamId[16];
        sprintf(streamId, "%d", VIDEO_REMOTE_H264_STREAM_ID);
        ohcmStreamChannel *remoteStream = (ohcmStreamChannel *)linkedListFind(streamList, streamId, findStreamChannelById);
        if (remoteStream != NULL)
        {
            // save these settings into our local device object
            //
            applyCameraStreamingSettingsToStorage(target, remoteStream);
        }
    }

    // cleanup the list of channel info
    //
    linkedListDestroy(streamList, destroyOhcmStreamChannelFromList);
}

/*
 * a 'linkedListCompareFunc' to find the 'ohcmStreamChannel' for
 * a specific stream 'id'.  Assuming the 'searchVal' is the string
 * of the stream 'id' to locate.
 */
static bool findStreamChannelById(void *searchVal, void *item)
{
    char *streamId = (char *)searchVal;
    ohcmStreamChannel *channel = (ohcmStreamChannel *)item;

    if (channel != NULL && channel->id != NULL && strcasecmp(streamId, channel->id) == 0)
    {
        // found matching 'stream id'
        return true;
    }
    return false;
}

/*
 * see if the desired settings are applicable based on the 'stream capabilities'
 */
static bool applyResolutionToConfig(ohcmStreamChannel *settings,
                                    ohcmStreamCapabilities *streamCaps,
                                    videoResolutionAndRatio *resolution,
                                    uint32_t desiredFrameRate,
                                    uint32_t desiredBitRate,
                                    videoQualityType qual,
                                    uint32_t keyFrameInterval)
{
    // first see if the resolution is within the  min/max capabilities
    //
    if (isOhcmValueInRange(streamCaps->videoCapabilities->minWidth,
                           streamCaps->videoCapabilities->maxWidth,
                           streamCaps->videoCapabilities->widthRange,
                           resolution->width) == false)
    {
        return false;
    }
    if (isOhcmValueInRange(streamCaps->videoCapabilities->minHeight,
                           streamCaps->videoCapabilities->maxHeight,
                           streamCaps->videoCapabilities->heightRange,
                           resolution->height) == false)
    {
        return false;
    }

/* NOTE: disabled this check because it doesn't really matter if we ask for CBR or VBR.
 *       the camera settings use the same variable names, so doing this check is useless
 *
    // check the quality is supported
    //
    if (qual == VIDEO_QUALITY_CBR)
    {
        if (ohcmContainsCapability(streamCaps->videoCapabilities->qualityTypes, "CBR") == false)
        {
            return false;
        }
    }
    else // VBR
    {
        if (ohcmContainsCapability(streamCaps->videoCapabilities->qualityTypes, "VBR") == false)
        {
            return false;
        }
    }
 */

    // check the quality bitrate is supported
    //
    if (isOhcmValueInRange(streamCaps->videoCapabilities->minCBR,
                           streamCaps->videoCapabilities->maxCBR,
                           streamCaps->videoCapabilities->cbrRange,
                           desiredBitRate) == false)
    {
        return false;
    }

    // check framerate is supported
    //
    if (isOhcmValueInRange(streamCaps->videoCapabilities->minFramerate,
                           streamCaps->videoCapabilities->maxFramerate,
                           streamCaps->videoCapabilities->framerateRange,
                           desiredFrameRate) == false)
    {
        return false;
    }

    // see if the camera supports RTSP and/or HTTP
    bool supportHTTP = ohcmContainsCapability(streamCaps->streamingTransports, "HTTP");
    bool supportRTSP = ohcmContainsCapability(streamCaps->streamingTransports, "RTSP");

    if (supportHTTP == true && supportRTSP == true)
    {
        // according to Java code, this is deprecated...but I see it used all of the time
        free(settings->streamingTransport);
        settings->streamingTransport = strdup("HTTP,RTSP");
    }
    else if (supportRTSP == true)
    {
        free(settings->streamingTransport);
        settings->streamingTransport = strdup("RTSP");
    }
    else if (supportHTTP == true)
    {
        free(settings->streamingTransport);
        settings->streamingTransport = strdup("HTTP");
    }

    if (qual == VIDEO_QUALITY_CBR)
    {
        free(settings->videoQualityControlType);
        settings->videoQualityControlType = strdup("CBR");
    }
    else if (qual == VIDEO_QUALITY_VBR)
    {
        free(settings->videoQualityControlType);
        settings->videoQualityControlType = strdup("VBR");
    }

    // within all ranges, so apply to the 'settings'
    //
    settings->videoResolutionWidth = resolution->width;
    settings->videoResolutionHeight = resolution->height;
    // Used if VBR
    settings->vbrMaxRate = desiredBitRate;
    // Used if CBR
    settings->constantBitRate = desiredBitRate;
    settings->maxFrameRate = desiredFrameRate;
    settings->keyFrameInterval = keyFrameInterval;

    return true;
}

/*
 * see if the desired settings are applicable based on the 'stream capabilities'
 */
static bool applyProfileToConfig(ohcmStreamChannel *settings,
                                 ohcmStreamCapabilities *streamCaps,
                                 h264ProfileEnum profile,
                                 h264LevelEnum level)
{
    bool retVal = false;

    // first, attempt to apply the requested profile
    //
    if (ohcmContainsCapability(streamCaps->videoCapabilities->h264Profiles, h264ProfileLabels[profile]) == true)
    {
        free(settings->h264Profile);
        settings->h264Profile = strdup(h264ProfileLabels[profile]);
        retVal = true;
    }
    else
    {
        // not there, so behave like Java and loop through all of them (in order) until
        // we find one that is acceptable.  this way we're using the highest quality supported
        //
        int i;
        for (i = H264_PROFILE_BASELINE ; i <= H264_PROFILE_EXTENDED ; i++)
        {
            // attempt this profile
            //
            if (ohcmContainsCapability(streamCaps->videoCapabilities->h264Profiles, h264ProfileLabels[i]) == true)
            {
                free(settings->h264Profile);
                settings->h264Profile = strdup(h264ProfileLabels[i]);
                retVal = true;
                break;
            }
        }
    }

    if (retVal == true)
    {
        // now the level.  again, see if the requested level is good
        //
        if (ohcmContainsCapability(streamCaps->videoCapabilities->h264Levels, h264LevelLabels[level]) == true)
        {
            free(settings->h264Level);
            settings->h264Level = strdup(h264LevelLabels[level]);
        }
        else
        {
            // loop through all levels to see what works
            //
            int i;
            for (i = H264_LEVEL_1; i <= H264_LEVEL_5_2; i++)
            {
                // attempt this profile
                //
                if (ohcmContainsCapability(streamCaps->videoCapabilities->h264Levels, h264LevelLabels[i]) == true)
                {
                    free(settings->h264Level);
                    settings->h264Level = strdup(h264LevelLabels[i]);
                    break;
                }
            }
        }
    }

    return retVal;
}

/*
 * Set streams for media capture(e.g. pre-roll and video duration)
 */
static bool applyMediaCaptureSettings(ohcmStreamChannel *stream, ohcmStreamCapabilities *streamCaps)
{
    bool mediaCaptureSettingsApplied = false;
    if (streamCaps != NULL && streamCaps->mediaCapabilities != NULL)
    {
        if (isOhcmValueInRange(streamCaps->mediaCapabilities->minPre, streamCaps->mediaCapabilities->maxPre,
                               streamCaps->mediaCapabilities->preRange, PRE_CAPTURE_LENGTH) &&
            isOhcmValueInRange(streamCaps->mediaCapabilities->minPost, streamCaps->mediaCapabilities->maxPost,
                               streamCaps->mediaCapabilities->postRange, POST_CAPTURE_LENGTH))
        {
            stream->preCaptureLength = PRE_CAPTURE_LENGTH;
            stream->postCaptureLength = POST_CAPTURE_LENGTH;
            mediaCaptureSettingsApplied = true;
        }
    }

    return mediaCaptureSettingsApplied;
}

/*
 * set "remote" stream settings
 */
static void applyRemoteStreamSettings(ohcmStreamChannel *remoteStream, ohcmStreamCapabilities *streamCaps)
{
    // enable video, disable audio
    //
    remoteStream->enabled = true;
    remoteStream->audioEnabled = false;

    // apply "main" profile
    //
    if (applyProfileToConfig(remoteStream, streamCaps, H264_PROFILE_MAIN, H264_LEVEL_3_1) == false)
    {
        icLogWarn(LOG_TAG, "error setting H264 profile");
    }

    // Set up our timings for video length
    //
    if (applyMediaCaptureSettings(remoteStream, streamCaps) == false)
    {
        icLogWarn(LOG_TAG, "failed setting media capture settings");
    }

    // following the Java code, attempt to apply each resolution in a specific order
    // until we find one that works on this camera
    //
    if (applyResolutionToConfig(remoteStream, streamCaps, &HD1080, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' HD1080 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &PAN720, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' PAN720 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &HD720, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' HD720 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &PAN480, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' PAN480 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &D1, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' D1 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &VGA, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' VGA resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &PAN360, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' PAN360 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &PAN240, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' PAN240 resolution");
    }
    else if(applyResolutionToConfig(remoteStream, streamCaps, &QVGA, REMOTE_STREAM_FRAME_RATE, REMOTE_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, REMOTE_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'remote' QVGA resolution");
    }
    else
    {
        // apply the defaults (as defined by CVR4xi group):
        //   Resolution: 1280x720
        //   bit rate: 768 kbps
        //   frame rate: 15 fps
        //   GOP: 15
        //
        remoteStream->videoResolutionWidth = 1280;
        remoteStream->videoResolutionHeight = 720;
        remoteStream->vbrMaxRate = REMOTE_STREAM_BIT_RATE;
        remoteStream->constantBitRate = REMOTE_STREAM_BIT_RATE;
        remoteStream->maxFrameRate = REMOTE_STREAM_FRAME_RATE;
        remoteStream->keyFrameInterval = REMOTE_STREAM_FRAME_INTERVAL;
    }
}

/*j
 * set "local" stream settings.  only applicable for devices with a screen
 */
static void applyLocalStreamSettings(ohcmStreamChannel *localStream, ohcmStreamCapabilities *streamCaps)
{
    // enable video, disable audio
    //
    localStream->enabled = true;
    localStream->audioEnabled = false;

    // apply "baseline" profile
    //
    if (applyProfileToConfig(localStream, streamCaps, H264_PROFILE_BASELINE, H264_LEVEL_3_1) == false)
    {
        icLogWarn(LOG_TAG, "error setting H264 profile");
    }

    // following the Java code, attempt to apply each resolution in a specific order
    // until we find one that works on this camera
    //
    if(applyResolutionToConfig(localStream, streamCaps, &HD720, LOCAL_STREAM_FRAME_RATE, LOCAL_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, LOCAL_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'local' HD720 resolution");
    }
    else if(applyResolutionToConfig(localStream, streamCaps, &PAN480, LOCAL_STREAM_FRAME_RATE, LOCAL_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, LOCAL_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'local' PAN480 resolution");
    }
    else if(applyResolutionToConfig(localStream, streamCaps, &D1, LOCAL_STREAM_FRAME_RATE, LOCAL_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, LOCAL_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'local' D1 resolution");
    }
    else if(applyResolutionToConfig(localStream, streamCaps, &VGA, LOCAL_STREAM_FRAME_RATE, LOCAL_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, LOCAL_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'local' VGA resolution");
    }
    else if(applyResolutionToConfig(localStream, streamCaps, &PAN360, LOCAL_STREAM_FRAME_RATE, LOCAL_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, LOCAL_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'local' PAN360 resolution");
    }
    else if(applyResolutionToConfig(localStream, streamCaps, &PAN240, LOCAL_STREAM_FRAME_RATE, LOCAL_STREAM_BIT_RATE, VIDEO_QUALITY_CBR, LOCAL_STREAM_FRAME_INTERVAL) == true)
    {
        icLogDebug(LOG_TAG,"Configuring camera for 'local' PAN240 resolution");
    }
}

/*
 * set default stream settings on both the camera and the cameraDeviceObject
 */
static ohcmResultCode applyDefaultCameraStreamingSettings(ohcmCameraInfo *camInfo, cameraDevice *target)
{
    // first get the video settings information from the device
    //
    icLinkedList *streamList = linkedListCreate();
    ohcmResultCode rc = getOhcmStreamingChannels(camInfo, streamList, 2);
    if (rc == OHCM_SUCCESS)
    {
        // NOTE: the set of streams we just read is a linked list, which should have
        //       2 or 3 channels (depending on what the camera supports).
        //   Touchstone only uses channel #0
        //   Converge also sets #1 for local streaming to the local display.
        //
        bool applyChanges = false;
        char streamId[16];
        sprintf(streamId, "%d", VIDEO_REMOTE_H264_STREAM_ID);

        //We're gonna apply some defaults to each stream channel. Also, we're going to configure the remote stream
        icLinkedListIterator *streamIter = linkedListIteratorCreate(streamList);
        while (linkedListIteratorHasNext(streamIter))
        {
            ohcmStreamChannel *streamChannel = linkedListIteratorGetNext(streamIter);
            if (streamChannel != NULL)
            {
                streamChannel->securityEnabled = true;
                streamChannel->audioEnabled = false;

                //This one is also the remote stream. Let's configure it a bit more.
                if (stringCompare(streamChannel->id, streamId, true) == 0)
                {
                    // get the stream 'capabilities' from the camera so we can apply settings
                    // based on what the camera supports (keeps us from making assumptions)
                    //
                    ohcmStreamCapabilities* streamCaps = createOhcmStreamCapabilities();
                    if ((rc = getOhcmStreamCapabilities(camInfo, streamId, streamCaps, 2)) == OHCM_SUCCESS)
                    {
                        // apply the 'remote' settings
                        //
                        applyRemoteStreamSettings(streamChannel, streamCaps);

                        // update our in-memory copies of these settings in the cameraDevice
                        //
                        applyCameraStreamingSettingsToStorage(target, streamChannel);
                        applyChanges = true;
                    }
                    else
                    {
                        icLogWarn(LOG_TAG, "error obtaining stream 0 capabilities about camera device %s %s; rc=%d %s",
                                  camInfo->macAddress, camInfo->cameraIP, rc, ohcmResultCodeLabels[rc]);
                    }

                    // cleanup
                    //
                    destroyOhcmStreamCapabilites(streamCaps);
                }
            }
        }
        linkedListIteratorDestroy(streamIter);

        if (applyChanges == true && hasDisplayScreen() == true)
        {
            // local display, so set the second stream
            //
            sprintf(streamId, "%d", VIDEO_LOCAL_STREAM_ID);
            ohcmStreamChannel *localStream = (ohcmStreamChannel *) linkedListFind(streamList, streamId, findStreamChannelById);
            if (localStream != NULL)
            {
                // get the stream 'capabilities' from the camera so we can apply settings
                // based on what the camera supports (keeps us from making assumptions)
                //
                ohcmStreamCapabilities* streamCaps = createOhcmStreamCapabilities();
                if ((rc = getOhcmStreamCapabilities(camInfo, streamId, streamCaps, 2)) == OHCM_SUCCESS)
                {
                    // apply the 'local' settings
                    //
                    applyLocalStreamSettings(localStream, streamCaps);
                }
                else
                {
                    icLogWarn(LOG_TAG, "error obtaining stream 1 capabilities about camera device %s %s; rc=%d %s",
                              camInfo->macAddress, camInfo->cameraIP, rc, ohcmResultCodeLabels[rc]);
                }

                // cleanup
                //
                destroyOhcmStreamCapabilites(streamCaps);
            }
            else
            {
                icLogWarn(LOG_TAG, "error obtaining stream 1 from camera device %s %s", camInfo->macAddress, camInfo->cameraIP);
            }
        }

        if (applyChanges == true)
        {
            // finally, apply the stream settings on the camera
            //
            rc = setOhcmStreamingChannels(camInfo, streamList, 2);
        }
        else
        {
            // missing stream #0
            icLogWarn(LOG_TAG, "camera did not have any streams configured.  unable to apply stream settings");
            rc = OHCM_INVALID_CONTENT;
        }
    }

    // cleanup the list of channel info
    //
    linkedListDestroy(streamList, destroyOhcmStreamChannelFromList);
    return rc;
}

/*---------------------------------------------------------------------------------------
 *
 * create a new cameraDevice object.  if 'gatherRest' is true, will probe
 * the physical device to obtain any missing information (i.e. new device would
 * only supply the 'ipAddress).  otherwise relies on the caller to populate
 * each of the pieces of information.
 *
 *---------------------------------------------------------------------------------------*/
cameraDevice *createCameraDevice(const char *uuid,
                                 const char *ipAddress, const char *macAddress,
                                 const char *adminUser, const char *adminPass,
                                 cameraDeviceChangedCallback callback, bool gatherRest,
                                 ohcmResultCode *result)
{
    // must have a callback
    //
    if (callback == NULL)
    {
        icLogWarn(LOG_TAG, "unable to create cameraDevice, null 'callback'");
        return NULL;
    }

    // make the container (and the sub-containers)
    //
    cameraDevice *retVal = (cameraDevice *)calloc(1, sizeof(cameraDevice));
    retVal->adminCredentials = (cameraCredentials *)calloc(1, sizeof(cameraCredentials));
    retVal->userCredentials = (cameraCredentials *)calloc(1, sizeof(cameraCredentials));
    retVal->details = createOhcmDeviceInfo();
    retVal->videoSettings = (cameraVideoSettings *)calloc(1, sizeof(cameraVideoSettings));
    retVal->opState = CAMERA_OP_STATE_READY;
    retVal->isIntegratedPeripheral = isCameraIntegratedPeripheral(uuid);
    retVal->monitorRunning = false;
    retVal->notify = callback;

    // initialize the mutex so it can be used
    //
    pthread_mutex_init(&retVal->mutex, NULL);
    initTimedWaitCond(&retVal->cond);

    // fill in what we can based on the input parms
    //
    if (uuid != NULL)
    {
        retVal->uuid = strdup(uuid);
    }
    if (ipAddress != NULL)
    {
        retVal->ipAddress = strdup(ipAddress);
        if (strcmp(ipAddress, "127.0.0.1") == 0)
        {
            retVal->isIntegratedPeripheral = true;
        }
    }
    if (macAddress != NULL)
    {
        /*
         * Load the address into the ARP cache.
         * Some devices (temporarily) fail to respond to ARP for no good reason but are otherwise functional.
         */
        retVal->macAddress = strdup(macAddress);
        uint8_t hwAddr[ETHER_ADDR_LEN];
        if (macAddrToBytes(macAddress, hwAddr, true) == true)
        {
            setMacAddressForIP(ipAddress, hwAddr, NULL);
        }
        else
        {
            icLogWarn(LOG_TAG, "Unable to convert camera '%s' MAC to byte array", uuid);
        }
    }
    if (adminUser != NULL)
    {
        retVal->adminCredentials->username = strdup(adminUser);
    }
    else
    {
        retVal->adminCredentials->username = strdup(DEFAULTED_ADMIN_USERNAME);
    }
    if (adminPass != NULL)
    {
        retVal->adminCredentials->password = strdup(adminPass);
    }
    else
    {
        retVal->adminCredentials->password = strdup(DEFAULTED_ADMIN_PASSWORD);
    }

    // can only get remaining information if the IP is set
    //
    ohcmResultCode rc = OHCM_GENERAL_FAIL;
    if (gatherRest == true && ipAddress != NULL)
    {
        // get the device info from camera
        //
        icLogDebug(LOG_TAG, "probing camera device %s %s", uuid, ipAddress);
        ohcmCameraInfo *camInfo = allocCameraInfo(retVal);
        if ((rc = getOhcmDeviceInfo(camInfo, retVal->details, CONNECTION_RETRY_COUNT)) == OHCM_SUCCESS)
        {
            // see if we need to copy the details->macAddress into retVal->macAddress
            // (possible we just discovered this camera and only have an ipAddress)
            //
            if (retVal->macAddress == NULL && retVal->details->macAddress != NULL && strlen(retVal->details->macAddress) > 0)
            {
                retVal->macAddress = strdup(retVal->details->macAddress);
                icLogDebug(LOG_TAG, "Populated mac address %s from device info", retVal->macAddress);
            }

            if (retVal->uuid == NULL && retVal->macAddress != NULL)
            {
                retVal->uuid = (char *)calloc(1, MAC_ADDR_BYTES+1);
                macAddrToUUID(retVal->uuid, retVal->macAddress);
                icLogDebug(LOG_TAG, "Populated uuid %s from device info", retVal->uuid);
            }

            // get the video settings information from the device
            //
            loadCameraStreamingSettings(camInfo, retVal);
        }
        else
        {
            // TODO: probably bad user/pass - cycle through all known camera devices to see if this is already known to us

            icLogWarn(LOG_TAG, "error obtaining information about camera device %s %s; rc=%d %s",
                      uuid, (ipAddress != NULL) ? ipAddress : "unknown IP", rc, ohcmResultCodeLabels[rc]);
        }

        // cleanup the CameraInfo container
        //
        destroyOhcmCameraInfo(camInfo);
    }
    else if (gatherRest == false)
    {
        // nothing gathered, so successful return code
        //
        rc = OHCM_SUCCESS;
    }

    // see if we need to assign the uuid
    //
    if (retVal->uuid == NULL && retVal->macAddress != NULL && strlen(retVal->macAddress) != 0)
    {
        // for cameras, UUID is the MAC address with the colons stripped out
        //
        retVal->uuid = malloc(MAC_ADDR_BYTES + 1);
        macAddrToUUID(retVal->uuid, retVal->macAddress);
    }

    // return values
    //
    *result = rc;
    return retVal;
}

/*
 * internal function - destroy the cameraCredentials object
 */
static void destroyCameraCredentials(cameraCredentials *cred)
{
    if (cred != NULL)
    {
        if (cred->username != NULL)
        {
            free(cred->username);
            cred->username = NULL;
        }
        if (cred->password != NULL)
        {
            free(cred->password);
            cred->password = NULL;
        }
        free(cred);
    }
}

/*---------------------------------------------------------------------------------------
 *
 * destroy a cameraDevice object. will stop the
 * monitor thread if running.
 *
 *---------------------------------------------------------------------------------------*/
void destroyCameraDevice(cameraDevice *device)
{
    if (device == NULL)
    {
        return;
    }

    // stop monitor thread if running
    //
    pthread_mutex_lock(&device->mutex);
    if (device->monitorRunning == true)
    {
        // stop monitoring thread and wait for it to
        // complete.  otherwise not safe to destroy mem
        //
        cameraDeviceStopMonitorThread(device, true);
        device->monitorRunning = false;
    }

    // release strings
    //
    if (device->uuid != NULL)
    {
        free(device->uuid);
        device->uuid = NULL;
    }
    if (device->ipAddress != NULL)
    {
        free(device->ipAddress);
        device->ipAddress = NULL;
    }
    if (device->macAddress != NULL)
    {
        free(device->macAddress);
        device->macAddress = NULL;
    }
    if (device->adminCredentials != NULL)
    {
        destroyCameraCredentials(device->adminCredentials);
        device->adminCredentials = NULL;
    }
    if (device->userCredentials != NULL)
    {
        destroyCameraCredentials(device->userCredentials);
        device->userCredentials = NULL;
    }

    // release details from OHCM
    //
    if (device->details != NULL)
    {
        destroyOhcmDeviceInfo(device->details);
        device->details = NULL;
    }

    // release video settings
    //
    if (device->videoSettings != NULL)
    {
        if (device->videoSettings->aspectRatio != NULL)
        {
            free(device->videoSettings->aspectRatio);
            device->videoSettings->aspectRatio = NULL;
        }
        if (device->videoSettings->videoResolution != NULL)
        {
            free(device->videoSettings->videoResolution);
            device->videoSettings->videoResolution = NULL;
        }
        free(device->videoSettings);
        device->videoSettings = NULL;
    }

    // finally the container
    // device->mutex is NOT unlocked by cameraDeviceStopMonitorThread when called from this function
    // coverity[double_unlock]
    //
    pthread_mutex_unlock(&device->mutex);
    pthread_mutex_destroy(&device->mutex);
    pthread_cond_destroy(&device->cond);
    device->notify = NULL;
    free(device);
}

/*--===================================================================================--*
 *
 *  configuration of the device
 *
 *--===================================================================================--*/

/*
 * -- internal function --
 *
 * make a CameraInfo object that points to the same information
 * we have in the cameraDevice object.  needed for the calls
 * to the OHCM library.
 *
 * when releasing, use freeCameraInfo()
 * assumes caller has the mutex on 'device'
 */
static ohcmCameraInfo *allocCameraInfo(cameraDevice *device)
{
    ohcmCameraInfo *retVal = createOhcmCameraInfo();

    if (device->ipAddress != NULL)
    {
        retVal->cameraIP = strdup(device->ipAddress);
    }
    if (device->macAddress != NULL)
    {
        retVal->macAddress = strdup(device->macAddress);
    }
    if (device->adminCredentials != NULL && device->adminCredentials->username != NULL)
    {
        retVal->userName = strdup(device->adminCredentials->username);
    }
    if (device->adminCredentials != NULL && device->adminCredentials->password != NULL)
    {
        retVal->password = strdup(device->adminCredentials->password);
    }
    return retVal;
}

#ifdef CONFIG_SERVICE_NETWORK
/*
 * if we're on a managed network, this will return a clone
 * of the wifi credentials to use when configuration a camera device.
 * if not on a managed network, this returns NULL
 *
 * caller is responsible for releasing the object (of not NULL)
 */
static wifiInfo *getManagedWifiCredentials()
{
    // first see if this is a managed network
    //
    bool isManaged = false;
    IPCCode rc = networkService_request_IS_MANAGED_NETWORK(&isManaged);
    if (rc == IPC_SUCCESS)
    {
        // ask networkService for the wifi credentials
        //
        wifiInfo *credentials = create_wifiInfo();
        if (networkService_request_GET_WIFI_CONFIG_INFO(true, credentials) == IPC_SUCCESS)
        {
            icLogDebug(LOG_TAG, "%s: retrieved managed network credentials", __FUNCTION__);
            return credentials;
        }
        destroy_wifiInfo(credentials);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: error checking 'is managed network' - %s", __FUNCTION__, IPCCodeLabels[rc]);
    }

    // not managed, or unable to get the credentials
    //
    return NULL;
}

/*
 * if on a managed network, get the wifi credentials and populate an
 * ohcmNetworkInterface object for use in the configration
 */
static ohcmNetworkInterface *makeNetworkConfigObject()
{
    ohcmNetworkInterface *retVal = NULL;

    // if we're on a managed network, fill in the wifi information so the camera can
    // bail from ethernet and join the managed wifi network.
    // we can find out if this is managed by getting the wifi credentials from
    // networkService.  if defined, then we're managed and need to apply these to the camera
    //
    wifiInfo *credentials = getManagedWifiCredentials();
    if (credentials != NULL && credentials->ssid != NULL && credentials->passPhrase != NULL)
    {
        // fill in the 'networkInterface' with the info we have
        //
        retVal = createOhcmNetworkInterface();
        retVal->id = 0;
        retVal->enabled = true;
        retVal->addressingType = OHCM_NET_ADDRESS_DYNAMIC;
        retVal->wirelessEnabled = true;
        retVal->wirelessNetworkMode = strdup("infrastructure");
        retVal->profileWmmEnabled = true;
        if (credentials->channel <= 0)
        {
            retVal->profileChannel = strdup("auto");
        }
        else
        {
            retVal->profileChannel = stringBuilder("%"PRIi32, credentials->channel);
        }
        retVal->profileSsid = strdup(credentials->ssid);
        retVal->profileSharedKey = strdup(credentials->passPhrase);

        // since this is a managed network, hard-code the security & encryption
        // values to match what the Java legacy code used.
        //
        retVal->profileSecurityMode = OHCM_SEC_WPA_WPA2_PERSONAL;
        retVal->profileAlgorithmType = OHCM_WPA_ENCR_ALGO_TKIP_AES;
    }
    destroy_wifiInfo(credentials);

    return retVal;
}
#endif

/*
 * -- internal function --
 *
 * This function will clear out the ConfigFile before building it up.
 * It will only fill in the sections that we want to configure.
 *
 * CALLER must free ConfigFile and all its contents.
 *
 * assumes caller has the mutex on 'device'
 *
 * All elements in the config file are optional.
 * From the OpenHome Camera Interface Spec, they are:
 *      <xs:element ref="ConfigTimers" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="DeviceInfo" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="Time" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="NTPServerList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="LoggingConfig" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="HostServer" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="HistoryConfiguration" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="NetworkInterfaceList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="AudioChannelList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="VideoInput" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="UserList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="StreamingChannelList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="MotionDetectionList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="SoundDetectionList" minOccurs="0" maxOccurs="1"/>
 *      <xs:element ref="EventNotification" minOccurs="0" maxOccurs="1"/>
 *
 *  For our implementation, we will configure:
 *      ConfigTimers                UserList
 *      HostServer                  StreamingChannelList
 *      NetworkInterfaceList        MotionDetectionList
 *      AudioChannelList            SoundDetectionList
 *      VideoInput                  EventNotification
 */
static ohcmConfigFile *createConfFile(cameraDevice *device, bool isReconfig)
{
    ohcmConfigFile *conf = createOhcmConfigFile();

    /* For now, we are not configuring any of these settings:
     *       - Device Info settings
     *       - Time settings
     *       - NTP Server settings
     *       - Logging settings
     *       - History settings
     *       - Network Interface List settings
     *              x configure the cameras network capabilities (wired & wireless)
     *              x Since UpNP is enabled by default, and we are an unmanaged network - do nothing here
     *       - Video Input settings
     *       - Sound Detection List settings
     *       - Motion Detection List settings - these cannot be configured when setting config file
     */

    // ConfigTimers
    //
    conf->timers.maxMediaTunnelReadyWait = MEDIA_TUNNEL_READY_MAX_WAIT;
    conf->timers.mediaTunnelReadyTimersMinWait = MEDIA_TUNNEL_READY_MIN_RETRY_WAIT;
    conf->timers.mediaTunnelReadyTimersMaxWait = MEDIA_TUNNEL_READY_MAX_RETRY_WAIT;
    conf->timers.mediaTunnelReadyTimersStepsizeWait = MEDIA_TUNNEL_READY_STEPSIZE_WAIT;
    conf->timers.mediaTunnelReadyTimersRetries = MEDIA_TUNNEL_READY_RETRIES;
    conf->timers.mediaUploadTimersMinWait = MEDIA_TUNNEL_UPLOAD_MIN_RETRY_WAIT;
    conf->timers.mediaUploadTimersMaxWait = MEDIA_TUNNEL_UPLOAD_MAX_RETRY_WAIT;
    conf->timers.mediaUploadTimersStepsizeWait = MEDIA_TUNNEL_UPLOAD_STEPSIZE_WAIT;
    conf->timers.mediaUploadTimersRetries = MEDIA_TUNNEL_UPLOAD_RETRIES;
    conf->timers.mediaUploadTimersUploadTimeout = MEDIA_TUNNEL_UPLOAD_TIMEOUT;

// test if we can adjust the timezone
//    conf->time.timeMode = strdup("NTP");
//    conf->time.timeZone = strdup("CST6CDT,M3.2.0,M11.1.0");

    // Host Server settings
    //
    conf->hostServer.httpsPort = HOST_SERVER_HTTPS_PORT;
    conf->hostServer.httpPort = HOST_SERVER_HTTP_PORT;
    conf->hostServer.httpsEnabled = HOST_SERVER_HTTPS_ENABLED;
    conf->hostServer.httpsValidateCerts = HOST_SERVER_HTTPS_VALIDATE_CERTS;
    conf->hostServer.httpEnabled = HOST_SERVER_HTTP_ENABLED;    // http = false
    conf->hostServer.httpPort = HOST_SERVER_HTTP_PORT;
    conf->hostServer.pollEnabled = HOST_SERVER_POLL_ENABLED;
    conf->hostServer.pollDefaultLinger = HOST_SERVER_POLL_DEFAULT_LINGER;

    // Audio Channel List settings (disable audio for now)
    //
    ohcmAudioChannel *audio = createOhcmAudioChannel();
    audio->id = strdup("0");
    audio->enabled = false;
    audio->audioMode = OHCM_AUDIO_LISTENONLY;
    audio->microphoneEnabled = false;
    linkedListAppend(conf->audioChannelList, audio);

    // User List settings (create 2 users: Admin & Viewer)
    //
    ohcmSecurityAccount *admin = createOhcmSecurityAccount();
    admin->id = strdup("0");
    admin->accessRights = OHCM_ACCESS_ADMIN;
    ohcmSecurityAccount *viewer = createOhcmSecurityAccount();
    viewer->id = strdup("1");
    viewer->accessRights = OHCM_ACCESS_USER;

    if (isReconfig == true)
    {
        // apply username & passwords assigned to the cameraDevice
        //
        admin->userName = strdup(device->adminCredentials->username);
        admin->password = strdup(device->adminCredentials->password);
        viewer->userName = strdup(device->userCredentials->username);
        viewer->password = strdup(device->userCredentials->password);
    }
    else
    {
        // randomly-generate new usernames and passwords
        //
        admin->userName = generateRandomToken(MIN_PASSWORD_TOKEN_LENGTH, MAX_PASSWORD_TOKEN_LENGTH, 1);
        admin->password = generateRandomToken(MIN_PASSWORD_TOKEN_LENGTH, MAX_PASSWORD_TOKEN_LENGTH, 5);
        viewer->userName = generateRandomToken(MIN_PASSWORD_TOKEN_LENGTH, MAX_PASSWORD_TOKEN_LENGTH, 7);
        viewer->password = generateRandomToken(MIN_PASSWORD_TOKEN_LENGTH, MAX_PASSWORD_TOKEN_LENGTH, 9);
    }

#ifdef CONFIG_SERVICE_NETWORK
    // if we're on a managed network, fill in the wifi information so the camera can
    // bail from ethernet and join the managed wifi network.
    // we can find out if this is managed by getting the wifi credentials from
    // networkService.  if defined, then we're managed and need to apply these to the camera
    //
    ohcmNetworkInterface *net = makeNetworkConfigObject();
    if (net != NULL)
    {
        // add to the config
        //
        linkedListAppend(conf->networkInterfaceList, net);
    }
#endif

    // clear previous users and add our 2 new ones
    //
    linkedListClear(conf->securityAccountList, destroyOhcmSecurityAccountFromList);
    linkedListAppend(conf->securityAccountList, admin);
    linkedListAppend(conf->securityAccountList, viewer);

    return conf;
}

/*---------------------------------------------------------------------------------------
 *
 * Configure the device, using the descriptor as a guide.  If successful,
 * the admin and user credentials will be randomized.  the caller will need to
 * save those newly generated credentials.
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceConfigure(cameraDevice *device, CameraDeviceDescriptor *descriptor, bool isReconfig)
{
    bool retVal = false;
    bool wasOffline = false;
    NetworkTrustLevel localTrustLevel = getNetworkTrustLevel();

    // look at current operation
    //
    pthread_mutex_lock(&device->mutex);
    if (device->opState == CAMERA_OP_STATE_UPGRADE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is being upgraded", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    else if (device->opState == CAMERA_OP_STATE_OFFLINE)
    {
        // allow if 'reconfig', but need to save the fact it was offline
        //
        if (isReconfig == true)
        {
            wasOffline = true;
        }
        else
        {
            icLogWarn(LOG_TAG, "unable to configure camera %s; it is offline", device->uuid);
            pthread_mutex_unlock(&device->mutex);
            return false;
        }
    }
    device->opState = CAMERA_OP_STATE_CONFIGURE;

    if (localTrustLevel == NW_TRUST_UNKNOWN)
    {
        icLogError(LOG_TAG, "Cannot configure camera %s with indeterminate network trust", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }

#ifdef CONFIG_PRODUCT_TCA203
    if (localTrustLevel == NW_TRUST_UNTRUSTED)
    {
        icLogError(LOG_TAG, "Cameras not supported on this device when configured for customer operated network");
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
#endif

    // create a CameraInfo for authentication via ohcm
    //
    icLogInfo(LOG_TAG, "storing configuration of camera %s %s", device->uuid, device->ipAddress);
    ohcmCameraInfo *cam = allocCameraInfo(device);

    // create the config file structure, and populate with
    // the base settings.
    //
    ohcmConfigFile *conf = createConfFile(device, isReconfig);
    if (isReconfig == true)
    {
        // now that the config file is created with old user/pass,
        // update the 'cam' to use the default user/pass
        //
        free(cam->userName);
        cam->userName = strdup(DEFAULTED_ADMIN_USERNAME);
        free(cam->password);
        cam->password = strdup(DEFAULTED_ADMIN_PASSWORD);
    }

    // safe to release lock
    //
    pthread_mutex_unlock(&device->mutex);

    // push the config file to the camera
    //
    icLogDebug(LOG_TAG,"setConfigFile...");
    ohcmResultCode rc = setOhcmConfigFile(cam, conf, CONFIG_CONNECTION_RETRY_COUNT);
    if (rc == OHCM_SUCCESS || rc == OHCM_REBOOT_REQ)
    {
        if (isReconfig == false)
        {
            // success, so save the admin/user credentials we just pushed
            //
            ohcmSecurityAccount *adminCred = (ohcmSecurityAccount *)linkedListGetElementAt(conf->securityAccountList, 0);
            ohcmSecurityAccount *viewerCred = (ohcmSecurityAccount *)linkedListGetElementAt(conf->securityAccountList, 1);

            pthread_mutex_lock(&device->mutex);
            free(device->adminCredentials->username);
            device->adminCredentials->username = strdup(adminCred->userName);
            free(device->adminCredentials->password);
            device->adminCredentials->password = strdup(adminCred->password);

            free(device->userCredentials->username);
            device->userCredentials->username = strdup(viewerCred->userName);
            free(device->userCredentials->password);
            device->userCredentials->password = strdup(viewerCred->password);
            pthread_mutex_unlock(&device->mutex);

            icLogDebug(LOG_TAG, "setConfigFile() success with value %d; internally updated credentials", rc);
        }
        else
        {
            icLogDebug(LOG_TAG, "setConfigFile() success with value %d", rc);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "setConfigFile() failed rc=%d %s", rc, ohcmResultCodeLabels[rc]);
    }

    // done applying config, cleanup the memory inside
    //
    destroyOhcmConfigFile(conf);
    conf = NULL;

    // use the device descriptor to enable/disable features
    //
    if (descriptor != NULL)
    {
        char *userButtonPresent = stringHashMapGet(descriptor->baseDescriptor.metadata, USER_BUTTON_PRESENT_PROPNAME);
        if (userButtonPresent != NULL && strcmp(userButtonPresent, "true") == 0)
        {
            icLogDebug(LOG_TAG, "user button supported");
            device->hasUserButton = true;
        }

/* no longer supported as it required listening on port 80
        char *useSercommEventPush = stringHashMapGet(descriptor->baseDescriptor.metadata, USE_SERCOMM_PUSH_EVENT_PROPNAME);
        if (useSercommEventPush != NULL && strcmp(useSercommEventPush, "true") == 0)
        {
            icLogDebug(LOG_TAG, "should use sercomm event push");
            device->useSercommEventPush = true;
        }
 */

        char *speakerAvailable = stringHashMapGet(descriptor->baseDescriptor.metadata, SPEAKER_PRESENT_PROPNAME);
        if (speakerAvailable != NULL && strcmp(speakerAvailable, "true") == 0)
        {
            icLogDebug(LOG_TAG, "camera has a speaker");
            device->hasSpeaker = true;
        }
    }

    if (rc == OHCM_SUCCESS || rc == OHCM_REBOOT_REQ)
    {
        // re-create a CamInfo using the device's new admin user/pass
        //
        ohcmCameraInfo *newCam = allocCameraInfo(device);

        // setup default streaming values in our device
        //
        ohcmResultCode streamRC = applyDefaultCameraStreamingSettings(newCam, device);
        if (streamRC == OHCM_LOGIN_FAIL)
        {
            // perhaps admin user/pass not applied yet, so try with original
            //
            streamRC = applyDefaultCameraStreamingSettings(cam, device);
            destroyOhcmCameraInfo(newCam);
        }
        else
        {
            // The new user/pass was applied already, so use it for the rest of this session
            destroyOhcmCameraInfo(cam);
            cam = newCam;
        }

        ohcmResultCode mtlsRC = ohcmConfigSetMutualTLS(cam, DEFAULT_ALLOWED_TLS_SUBJECTS);
        if (mtlsRC == OHCM_SUCCESS)
        {
            icLogInfo(LOG_TAG, "Successfully enabled mTLS on camera %s", cam->macAddress);

            // Setting mTLS up will reset the https server, let it come back up so configuration can continue
            //
            waitForCameraRestart(cam, false, CAMERA_SERVER_RESTART_TIMEOUT_SECONDS);
        }
        else
        {
            icLogWarn(LOG_TAG,
                      "Failed to set up mTLS on camera %s: %s",
                      cam->macAddress,
                      ohcmResultCodeLabels[mtlsRC]);

            if (localTrustLevel != NW_TRUST_TRUSTED)
            {
                icLogError(LOG_TAG,
                           "mTLS could not be enabled but is required for unmanaged network;"
                               "cannot configure camera %s",
                           cam->macAddress);
                rc = OHCM_GENERAL_FAIL;
            }
        }

        if (streamRC == OHCM_SUCCESS || streamRC == OHCM_REBOOT_REQ)
        {
            icLogDebug(LOG_TAG, "successfully applied default stream channel settings on camera %s", cam->macAddress);

            // transfer reboot flag to rc if needed
            if (streamRC == OHCM_REBOOT_REQ && rc != OHCM_REBOOT_REQ)
            {
                rc = OHCM_REBOOT_REQ;
            }
        }
        else
        {
            icLogWarn(LOG_TAG, "problem setting stream channel defaults on camera %s - %d %s", cam->macAddress, streamRC, ohcmResultCodeLabels[streamRC]);
        }
    }

    if (rc == OHCM_SUCCESS || rc == OHCM_REBOOT_REQ)
    {
        // success applying the config.  now setup motion if allowed via DeviceDescriptor
        //
        bool setupMotion = false;
        if (isReconfig == true)
        {
            // use motion settings assigned to the device object
            //
            icLogDebug(LOG_TAG, "setConfigFile(), enabling motion as part of re-configuration...");
            setupMotion = device->motionEnabled;
        }
        else if (descriptor != NULL && descriptor->defaultMotionSettings.enabled == true)
        {
            // device descriptor allows
            //
            icLogDebug(LOG_TAG, "setConfigFile(), enabling motion because told to via descriptor...");
            setupMotion = true;
        }
        else if (descriptor == NULL)
        {
            // check our 'defaults', specifically the camera.local.motion.default value
            //
            char *defaultMotion = getPropertyAsString("camera.local.motion.default", NULL);
            if (defaultMotion != NULL)
            {
                // check for string != 'off'
                if (strcasecmp(defaultMotion, "off") != 0)
                {
                    // not set to "off", so enable motion
                    //
                    icLogDebug(LOG_TAG, "setConfigFile(), enabling motion because of default properties...");
                    setupMotion = true;
                }

                // cleanup
                free(defaultMotion);
            }
        }

        if (setupMotion == true)
        {
            bool didMotion = false;
            icLogDebug(LOG_TAG, "setConfigFile(), enabling motion");
            ohcmResultCode motionRC = configureMotionDetectionMechanism(cam, 2, device->useSercommEventPush);
            if (motionRC == OHCM_SUCCESS || motionRC == OHCM_REBOOT_REQ)
            {
                // good to go
                //
                didMotion = true;
            }

            // transfer reboot flag to rc if needed
            if (motionRC == OHCM_REBOOT_REQ && rc != OHCM_REBOOT_REQ)
            {
                rc = OHCM_REBOOT_REQ;
            }

            // save motion detection flag
            //
            pthread_mutex_lock(&device->mutex);
            icLogDebug(LOG_TAG, "setConfigFile(), motion enabled = %s", (didMotion == true) ? "true" : "false");
            device->motionEnabled = didMotion;
            device->motionPossible = true;
            pthread_mutex_unlock(&device->mutex);
        }
        else
        {
            // save the fact that motion detection is off and not-possible
            //
            pthread_mutex_lock(&device->mutex);
            icLogDebug(LOG_TAG, "setConfigFile(), not configuring motion");
            device->motionEnabled = false;
            device->motionPossible = false;
            pthread_mutex_unlock(&device->mutex);
        }

        if(device->useSercommEventPush)
        {
            configureSercommEventPushUrl(device);
        }

        // good to go, see if we need to reboot the camera to complete the config
        //
        retVal = true;
        icLogDebug(LOG_TAG, "Successfully updated config on camera %s", device->uuid);
        if (rc == OHCM_REBOOT_REQ)
        {
            if (device->isIntegratedPeripheral == false)
            {
                // Reboot the external camera, block until it is back up
                //
                icLogDebug(LOG_TAG, "Successfully updated config on camera %s, need to reboot the device...", device->uuid);
                bool rebootRetVal = cameraDeviceReboot(device, true, CAMERA_REBOOT_TIMEOUT_SECONDS);
                if (rebootRetVal == true)
                {
                    icLogDebug(LOG_TAG, "Camera rebooted");
                    retVal = true;
                }
                else
                {
                    icLogWarn(LOG_TAG, "Camera failed to reboot");
                    /* TODO - return true here? the reboot either failed or the camera failed to come alive */
                }
            }
            else
            {
                /* Restart the necessary process on the camera hub */
                /* TODO ... restart the camera process rather than rebooting the host */
                icLogDebug(LOG_TAG, "Successfully updated config on camera %s, skipping reboot since we are running on the camera", device->uuid);
                retVal = true;
            }
        }
    }
    else
    {
        // log the return code & label
        icLogWarn(LOG_TAG, "Could not update camera config: rc=%d %s", rc, ohcmResultCodeLabels[rc]);
        retVal = false;
    }

    // put state to 'ready'
    //
    pthread_mutex_lock(&device->mutex);
    if (wasOffline == true)
    {
        // restore so monitor thread can clear the trouble
        //
        device->opState = CAMERA_OP_STATE_OFFLINE;
    }
    else
    {
        device->opState = CAMERA_OP_STATE_READY;
    }
    icLogInfo(LOG_TAG, "done configuring camera %s %s", device->uuid, device->ipAddress);
    pthread_mutex_unlock(&device->mutex);

    // cleanup & return
    //
    destroyOhcmCameraInfo(cam);

    return retVal;
}

bool cameraDeviceSetWiFiNetworkCredentials(cameraDevice *device, const char* ssid, const char* passphrase)
{
    if (device == NULL)
    {
        icLogError(LOG_TAG, "Invalid camera device specified.");
        return false;
    }

    if ((ssid == NULL) || (ssid[0] == '\0'))
    {
        icLogError(LOG_TAG, "Invalid SSID specified.");
        return false;
    }

    if ((passphrase == NULL) || (passphrase[0] == '\0'))
    {
        icLogError(LOG_TAG, "Invalid passphrase specified.");
        return false;
    }

    ohcmCameraInfo *camInfo = NULL;

    // look at current operation
    //
    pthread_mutex_lock(&device->mutex);
    if (device->opState == CAMERA_OP_STATE_UPGRADE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is being upgraded", device->uuid);
    }
    else if (device->opState == CAMERA_OP_STATE_OFFLINE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is offline", device->uuid);
    }
    else
    {
        device->opState = CAMERA_OP_STATE_CONFIGURE;
        camInfo = allocCameraInfo(device);
    }
    pthread_mutex_unlock(&device->mutex);

    bool retVal = false;

    if (camInfo)
    {
        icLinkedList* networkList = linkedListCreate();

        ohcmResultCode rc = getOhcmNetworkInterfaceList(camInfo, networkList);
        if ((rc == OHCM_SUCCESS) && linkedListCount(networkList))
        {
            ohcmNetworkInterface* networkInterface = linkedListGetElementAt(networkList, 0);

            free(networkInterface->profileSsid);
            free(networkInterface->profileSharedKey);

            networkInterface->profileSsid = strdup(ssid);
            networkInterface->profileSharedKey = strdup(passphrase);

            rc = setOhcmNetworkInterface(camInfo, networkInterface);

            retVal = ((rc == OHCM_SUCCESS) || (rc == OHCM_REBOOT_REQ));
            if (!retVal)
            {
                icLogWarn(LOG_TAG, "Unable to set network credentials; rc=%d %s",
                          rc, ohcmResultCodeLabels[rc]);
            }
        }
        else
        {
            icLogWarn(LOG_TAG, "Unable to get network interface list from device. rc=%d %s",
                      rc, ohcmResultCodeLabels[rc]);
        }

        destroyOhcmCameraInfo(camInfo);
        linkedListDestroy(networkList, destroyOhcmNetworkInterfaceFromList);

        // set state to READY
        //
        pthread_mutex_lock(&device->mutex);
        device->opState = CAMERA_OP_STATE_READY;
        pthread_mutex_unlock(&device->mutex);
    }

    return retVal;
}

/*
 * -- internal function --
 *
 * for the given cameraMotionSensitivity, convert it to
 * the ohcm sensitivity percentage and detection threshold
 */
static void getMotionDetectionValues(cameraMotionSensitivity setting, int *percentage, int *threshold)
{
    switch(setting)
    {
        case CAMERA_MOTION_SENSITIVITY_LOW:
            *percentage = DEFAULT_LOW_SENSITIVITY_PERCENTAGE;
            *threshold  = DEFAULT_LOW_DETECTION_THRESHOLD;
            break;

        case CAMERA_MOTION_SENSITIVITY_MEDIUM:
            *percentage = DEFAULT_MED_SENSITIVITY_PERCENTAGE;
            *threshold  = DEFAULT_MED_DETECTION_THRESHOLD;
            break;

        case CAMERA_MOTION_SENSITIVITY_HIGH:
            *percentage = DEFAULT_HIGH_SENSITIVITY_PERCENTAGE;
            *threshold  = DEFUALT_HIGH_DETECTION_THRESHOLD;
            break;
    }
}
/*
 * -- internal function --
 *
 * informs the camera that we want to perform motion detection
 * via a 'polling' mechanism.  needs to be done prior to setting
 * the 'motion detection sensitivity'
 */
static ohcmResultCode configureMotionDetectionMechanism(ohcmCameraInfo *info, uint32_t numRetries, bool usePush)
{
    char *motionId = MOTION_ID;

    if (numRetries < 1)
    {
        numRetries = CURL_RETRY_COUNT;
    }

    // NOTE:
    // NOTE: seems odd, but have to do this in 3 steps or else Sercom cameras
    // NOTE: do not apply the motion detection settings
    // NOTE:

    // Step 1:
    // create the config object and populate with our default motion settings
    //
    ohcmMotionDetection *detect = createOhcmMotionDetection();
    detect->id = strdup(MOTION_ID);
    detect->enabled = true;
    detect->inputID = strdup("0");
    detect->directionSensitivity = OHCM_MOTION_DIR_ANY;
    detect->regionType = OHCM_MOTION_REGION_ROI;
    /* TODO : these motion detection values should be determined based on camera settings */
    detect->minHorizontalResolution = MOTION_DETECTION_MIN_HORIZONTAL_RESOLUTION;
    detect->minVerticalResolution = MOTION_DETECTION_MIN_VERTICAL_RESOLUTION;
    detect->sourceHorizontalResolution = MOTION_DETECTION_SOURCE_HORIZONTAL_RESOLUTION;
    detect->sourceVerticalResolution = MOTION_DETECTION_SOURCE_VERTICAL_RESOLUTION;

    // apply Step 1
    //
    ohcmResultCode rc = setOhcmMotionDetectionForUid(info, detect, numRetries);
    if ((rc != OHCM_SUCCESS) && (rc != OHCM_REBOOT_REQ))
    {
        icLogWarn(LOG_TAG, "Unable to set motion detection video UID for motionId=%s; rc=%d %s", motionId, rc, ohcmResultCodeLabels[rc]);
        destroyOhcmMotionDetection(detect);
        return rc;
    }
    icLogDebug(LOG_TAG, "Successfully set motion detection video UID for motionId=%s", motionId);

    // Step 2:
    // Set the event trigger
    //
    ohcmEventTrigger *trigger = createOhcmEventTrigger();
    trigger->id = strdup(EVENT_ID);
    trigger->eventType = OHCM_EVENT_TRIGGER_VMD;
    trigger->intervalBetweenEvents = EVENT_TRIGGER_MINIMUM_INTERVAL_BETWEEN_EVENTS;
    trigger->eventTypeInputID = strdup(MOTION_ID);
    trigger->notif->notificationID = strdup(NOTIFICATION_ID);
    if (usePush == true)
    {
        trigger->notif->notificationMethod = strdup("HTTP");
    }
    else
    {
        trigger->notif->notificationMethod = strdup("POLL");
    }
    trigger->notif->notificationRecurrence = strdup("beginning");

    // notification mechanism
    ohcmEventNotifMethods *methods = createOhcmEventNotifMethods();
    ohcmHostNotif *hostNotif = createOhcmHostNotif();
    hostNotif->id = strdup(NOTIFICATION_LIST_ID);
    if (usePush == false)
    {
        hostNotif->url = strdup("poll://eventalertsystem");
    }
    linkedListAppend(methods->hostNotifList, hostNotif);
    methods->nonMediaEvent = true;

    // apply Step 2 and cleanup right away
    //
    rc = setOhcmMotionEvent(info, trigger, methods, numRetries);
    destroyOhcmEventTrigger(trigger);
    trigger = NULL;
    destroyOhcmEventNotifMethods(methods);
    methods = NULL;
    if ((rc != OHCM_SUCCESS) && (rc != OHCM_REBOOT_REQ))
    {
        icLogWarn(LOG_TAG, "Unable to set motion event notification; rc=%d %s", rc, ohcmResultCodeLabels[rc]);
        destroyOhcmMotionDetection(detect);
        return rc;
    }
    icLogDebug(LOG_TAG, "Successfully set motion event notification");

    // Step 3:
    // apply the region of interest
    //
    ohcmMotionDetectRegion *region = createOhcmMotionDetectRegion();
    region->id = strdup(MOTION_DETECTION_REGION_ID);
    region->enabled = true;
    region->sensitivityLevel = MOTION_DETECTION_REGION_LIST_SENSITIVITY_LEVEL;
    region->detectionThreshold = MOTION_DETECTION_REGION_LIST_DETECTION_THRESHOLD;
    linkedListAppend(detect->regionList, region);

    ohcmRegionCoordinate *coord1 = createOhcmRegionCoordinate();
    ohcmRegionCoordinate *coord2 = createOhcmRegionCoordinate();
    coord1->positionX = MOTION_DETECTION_REGION_UPPER_LEFT_X;
    coord1->positionY = MOTION_DETECTION_REGION_UPPER_LEFT_Y;
    coord2->positionX = MOTION_DETECTION_REGION_LOWER_RIGHT_X;
    coord2->positionY = MOTION_DETECTION_REGION_LOWER_RIGHT_Y;
    linkedListAppend(region->coordinatesList, coord1);
    linkedListAppend(region->coordinatesList, coord2);

    // apply Step 3
    //
    rc = setOhcmMotionDetectionForUid(info, detect, numRetries);
    destroyOhcmMotionDetection(detect);
    if ((rc != OHCM_SUCCESS) && (rc != OHCM_REBOOT_REQ))
    {
        icLogWarn(LOG_TAG, "Unable to set motion detection region for motionId=%s; rc=%d %s", motionId, rc, ohcmResultCodeLabels[rc]);
        return rc;
    }
    icLogDebug(LOG_TAG, "Successfully set motion detection region for motionId=%s", motionId);

    return rc;
}


/*---------------------------------------------------------------------------------------
 *
 * update the local flag and informs the camera to turn motion on/off
 *
 *---------------------------------------------------------------------------------------*/
void cameraDeviceEnableMotionDetection(cameraDevice *device, bool enabled)
{
    // look at current operation
    //
    pthread_mutex_lock(&device->mutex);
    if (device->opState == CAMERA_OP_STATE_UPGRADE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is being upgraded", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return;
    }
    else if (device->opState == CAMERA_OP_STATE_OFFLINE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is offline", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return;
    }
    device->opState = CAMERA_OP_STATE_CONFIGURE;

    // create OHCM camera access container
    //
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    // get current motion setting
    //
    icLinkedList *motionList = linkedListCreate();
    ohcmMotionDetection *detect = NULL;
    ohcmResultCode rc = getOhcmMotionDetection(camInfo, motionList, CURL_RETRY_COUNT);
    if (rc == OHCM_SUCCESS && linkedListCount(motionList) > 0)
    {
        // motion settings are applied
        //
        detect = (ohcmMotionDetection *)linkedListGetElementAt(motionList, 0);
    }

    // see if being asked to enable or disable
    //
    if (enabled == true)
    {
        if (detect == NULL)
        {
            // the motion mechanism was never setup.  need to do that first
            // (which will also enable the motion detection)
            //
            icLogDebug(LOG_TAG, "Enabling motion detection on camera");
            configureMotionDetectionMechanism(camInfo, CURL_RETRY_COUNT, device->useSercommEventPush);

            // clean up and bail
            //
            destroyOhcmCameraInfo(camInfo);
            linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);

            // set state to READY
            //
            pthread_mutex_lock(&device->mutex);
            device->opState = CAMERA_OP_STATE_READY;
            device->motionEnabled = true;
            pthread_mutex_unlock(&device->mutex);
            return;
        }

        // set enabled flag within the config to 'true'
        //
        detect->enabled = true;
    }
    else
    {
        icLogDebug(LOG_TAG, "Disabling motion detection on camera");
        if (detect == NULL)
        {
            // the motion mechanism was never setup.  nothing to do
            //
            destroyOhcmCameraInfo(camInfo);
            linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);

            // set state to READY
            //
            pthread_mutex_lock(&device->mutex);
            device->opState = CAMERA_OP_STATE_READY;
            device->motionEnabled = false;
            pthread_mutex_unlock(&device->mutex);
            return;
        }

        // set enabled to false
        //
        detect->enabled = false;
    }

    // if we got here, then eed to apply the flag change
    //
    rc = setOhcmMotionDetectionForUid(camInfo, detect, CURL_RETRY_COUNT);
    if ((rc != OHCM_SUCCESS) && (rc != OHCM_REBOOT_REQ))
    {
        icLogWarn(LOG_TAG, "Unable to enable/disable motion detection for motionId=%s; rc=%d %s", detect->id, rc, ohcmResultCodeLabels[rc]);
    }
    else
    {
        icLogDebug(LOG_TAG, "Success enable/disable motion detection for motionId=%s", detect->id);
    }

    // clean up
    //
    destroyOhcmCameraInfo(camInfo);
    linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);

    // set state to READY
    //
    pthread_mutex_lock(&device->mutex);
    device->opState = CAMERA_OP_STATE_READY;
    device->motionEnabled = enabled;
    pthread_mutex_unlock(&device->mutex);
}

/*
 * -- internal function --
 */
static const char *sensitivityToString(cameraMotionSensitivity sensitivity)
{
    switch(sensitivity)
    {
        case CAMERA_MOTION_SENSITIVITY_LOW:
            return "low";

        case CAMERA_MOTION_SENSITIVITY_MEDIUM:
            return "medium";

        case CAMERA_MOTION_SENSITIVITY_HIGH:
            return "high";
    }
    return "off";
}

/*---------------------------------------------------------------------------------------
 *
 * updates the sensitivity of motion detection within the camera
 * only applicable if motion detection is enabled.
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceSetMotionDetectionSensitivity(cameraDevice *device, cameraMotionSensitivity sensitivity)
{
    // look at current operation
    //
    pthread_mutex_lock(&device->mutex);
    if (device->opState == CAMERA_OP_STATE_UPGRADE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is being upgraded", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    else if (device->opState == CAMERA_OP_STATE_OFFLINE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is offline", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    device->opState = CAMERA_OP_STATE_CONFIGURE;

    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    /* Convert the sensitivity to ohcm values */
    int percentage = 0;
    int threshold = 0;
    getMotionDetectionValues(sensitivity, &percentage, &threshold);

    /* get the current config from camera */
    // get current motion setting
    //
    icLinkedList *motionList = linkedListCreate();
    ohcmMotionDetection *detect = NULL;
    ohcmResultCode rc = getOhcmMotionDetection(camInfo, motionList, CURL_RETRY_COUNT);
    if (rc == OHCM_SUCCESS && linkedListCount(motionList) > 0)
    {
        // motion settings are applied
        //
        detect = (ohcmMotionDetection *)linkedListGetElementAt(motionList, 0);
    }

    if (detect == NULL)
    {
        // not enabled yet, so do that now
        //
        icLogDebug(LOG_TAG, "Motion not set. Need to configure.");
        if (configureMotionDetectionMechanism(camInfo, CURL_RETRY_COUNT, device->useSercommEventPush) == false)
        {
            // error enabling motion
            //
            icLogWarn(LOG_TAG, "Error setting up motion detection on %s", device->uuid);
            destroyOhcmCameraInfo(camInfo);
            linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);
            pthread_mutex_lock(&device->mutex);
            device->opState = CAMERA_OP_STATE_READY;
            pthread_mutex_unlock(&device->mutex);
            return false;
        }
        else
        {
            // pull current config again (now that it's enabled)
            //
            rc = getOhcmMotionDetection(camInfo, motionList, CURL_RETRY_COUNT);
            if (rc == OHCM_SUCCESS && linkedListCount(motionList) > 0)
            {
                // motion settings are applied
                //
                detect = (ohcmMotionDetection *)linkedListGetElementAt(motionList, 0);
            }
            else
            {
                // still boned...
                icLogWarn(LOG_TAG, "Error setting up motion detection on %s", device->uuid);
                destroyOhcmCameraInfo(camInfo);
                linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);
                pthread_mutex_lock(&device->mutex);
                device->opState = CAMERA_OP_STATE_READY;
                pthread_mutex_unlock(&device->mutex);
                return false;
            }
        }
    }

    // sanity check
    //
    if (detect == NULL)
    {
        icLogWarn(LOG_TAG, "Error setting up motion detection on %s", device->uuid);
        destroyOhcmCameraInfo(camInfo);
        linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);
        pthread_mutex_lock(&device->mutex);
        device->opState = CAMERA_OP_STATE_READY;
        pthread_mutex_unlock(&device->mutex);
        return false;
    }

    /* Now set the sensitivity */
    icLogDebug(LOG_TAG, "Update motion sensitivity to %s (percentage = %d, threshold = %d)",
               sensitivityToString(sensitivity), percentage, threshold);

    // enable motion and set the sensitivity values */
    detect->enabled = true;
    ohcmMotionDetectRegion *region = NULL;
    if (linkedListCount(detect->regionList) > 0)
    {
        region = (ohcmMotionDetectRegion *)linkedListGetElementAt(detect->regionList, 0);
    }
    if (region == NULL)
    {
        region = createOhcmMotionDetectRegion();
        linkedListAppend(detect->regionList, region);
    }
    region->sensitivityLevel = percentage;
    region->detectionThreshold = threshold;

    // write the config to camera
    //
    bool retVal = true;
    rc = setOhcmMotionDetectionForUid(camInfo, detect, CURL_RETRY_COUNT);
    if ((rc != OHCM_SUCCESS) && (rc != OHCM_REBOOT_REQ))
    {
        icLogWarn(LOG_TAG, "Unable to set motion sensitivity; rc=%d %s", rc, ohcmResultCodeLabels[rc]);
        retVal = false;
    }

    // clean up
    //
    destroyOhcmCameraInfo(camInfo);
    linkedListDestroy(motionList, destroyOhcmMotionDetectionFromList);

    // set state to READY
    //
    pthread_mutex_lock(&device->mutex);
    device->opState = CAMERA_OP_STATE_READY;
    pthread_mutex_unlock(&device->mutex);

    return retVal;
}

/*--===================================================================================--*
 *
 *  monitor the device
 *
 *--===================================================================================--*/

/*
 * -- internal function --
 *
 * Poll the camera - internal call from event thread process
 */
static bool handleCameraPoll(cameraDevice *device, timeTracker *motionBlackoutTracker, bool *faulted)
{
    bool goodResponse = false;

    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *info = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    // perform a blocking call to the device - waiting for a motion event to occur
    //
    ohcmPollNotifResult result = getOhcmPollNotification(info, LONG_POLL_WAIT_SECONDS);
    if (result == POLL_MOTION_EVENT)
    {
        // set toggle to 'faulted' then send the event
        //
        icLogDebug(LOG_TAG, "Got a motion event from camera %s", device->uuid);
        goodResponse = true;

        if (*faulted == false)
        {
            // switch to faulted
            //
            *faulted = true;
            timeTrackerStart(motionBlackoutTracker, getMotionBlackoutSeconds());
            device->notify(device, CAMERA_MOTION_FAULT_CHANGE);
        }
    }
    else if (result == POLL_BUTTON_EVENT)
    {
        // set toggle to 'faulted' then send the event
        //
        icLogDebug(LOG_TAG, "Got a button pressed event from camera %s", device->uuid);
        goodResponse = true;
        device->notify(device, CAMERA_BUTTON_PRESSED);
    }
    else if (result == POLL_NO_EVENT)
    {
        // we got a valid response, but no event... clear the error counter and trouble if it exists
        //
        goodResponse = true;
        icLogDebug(LOG_TAG, "Long poll from camera %s returned with NO MOTION EVENT", device->uuid);
    }
    else if (result == POLL_RESULT_ERROR)
    {
        //       able to talk to the camera, but error checking for motion.
        //       need to wait before looping back around or else we'll be
        //       hammering the device too hard (in other words, check every
        //       few seconds,  not dozens of times per-second)
        //
        goodResponse = true;
        icLogDebug(LOG_TAG, "Long poll from camera %s returned with POLL_RESULT_ERROR (device error)", device->uuid);
    }
    else if (result == POLL_COMM_ERROR)
    {
        // unable to connect to the camera.
        //
        icLogDebug(LOG_TAG, "Poll got comm error for camera %s", device->uuid);
    }

    // cleanup and return
    //
    destroyOhcmCameraInfo(info);
    return goodResponse;
}

/*
 * Ping the camera to see  if it is reachable
 */
static bool handleCameraIsAlive(cameraDevice *device)
{
    bool goodResponse = false;

    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *info = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    // ping the camera to see if alive
    //
    ohcmResultCode rc = isOhcmAlive(info, IS_ALIVE_RETRY_COUNT);
    if (rc == OHCM_SUCCESS)
    {
        goodResponse = true;
        icLogDebug(LOG_TAG, "isAlive() from camera %s returned Success", device->uuid);
    }
    else
    {
        icLogDebug(LOG_TAG, "isAlive() from camera %s returned %d %s, ", device->uuid, rc, ohcmResultCodeLabels[rc]);
    }

    // cleanup and return
    //
    destroyOhcmCameraInfo(info);
    return goodResponse;
}

/*
 * -- internal function --
 *
 * thread to monitor the cameraDevice that serves 2 purposes:
 * Polls for motion events and checks for camera offline
 * - If motion is enabled, the motion polling will be used to determine both motion events and camera offline events.
 * - If motion is disabled, the isAlive() ping will be used to determine camera offline events.
 */
static void *cameraDeviceMonitorThread(void *arg)
{
    cameraDevice *device = (cameraDevice*)arg;
    bool faulted = false;
    bool keepGoing = true;
    bool inCommFail = false;
    bool didInitialCheck = false;
    bool deleteMe = false;      // set when state --> CAMERA_OP_STATE_DESTROY
    timeTracker *motionBlackoutTracker = timeTrackerCreate();
    timeTracker *pollTracker = timeTrackerCreate();
    uint32_t errorCount = 0;
    uint32_t successCount = 0;

    icLogDebug(LOG_TAG, "Starting event listener for camera %s", device->uuid);

    // Check to see if we are starting out in a comm fail state.
    //
    pthread_mutex_lock(&device->mutex);
    if (device->opState == CAMERA_OP_STATE_OFFLINE)
    {
        inCommFail = true;
    }
    pthread_mutex_unlock(&device->mutex);

    while (keepGoing == true)
    {
        bool goodResponse = false;
        uint32_t pingInterval;
        uint32_t maxErrorCount;
        uint32_t minSuccessCount;

        // quick check to see if the monitoring should continue
        //
        pthread_mutex_lock(&device->mutex);
        keepGoing = device->monitorRunning;
        pthread_mutex_unlock(&device->mutex);
        if (keepGoing == false)
        {
            icLogDebug(LOG_TAG, "bailing from eventThread for camera %s", device->uuid);
            break;
        }

        pthread_mutex_lock(&propsMtx);

        pingInterval = pingIntervalSecs;
        maxErrorCount = offlineErrorCount;
        minSuccessCount = onlineSuccessCount;

        pthread_mutex_unlock(&propsMtx);

        // start the time tracker - use the long poll time
        //
        timeTrackerStart(pollTracker, pingInterval);

        // first see if the device is in maintenance mode OR performing an upgrade
        //
        pthread_mutex_lock(&device->mutex);
        cameraOperateState currState = device->opState;
        pthread_mutex_unlock(&device->mutex);
        if (currState == CAMERA_OP_STATE_CONFIGURE ||
            currState == CAMERA_OP_STATE_UPGRADE)
        {
            // configuring or upgrading the device. wait for the poll time tracker, then loop around again
            //
            icLogDebug(LOG_TAG, "temporarily ignoring monitoring of camera %s, currently being configured or upgraded", device->uuid);
            while (timeTrackerRunning(pollTracker) == true && timeTrackerExpired(pollTracker) == false)
            {
                sleep(1);
            }
            timeTrackerStop(pollTracker);
            continue;
        }

        if (currState == CAMERA_OP_STATE_DESTROY)
        {
            // tagged for removal.  bail from this loop then destroy the camera object
            //
            icLogDebug(LOG_TAG, "camera %s is tagged for removal; exiting monitor thread", device->uuid);
            deleteMe = true;
            keepGoing = false;
            continue;
        }

        // see if the device is online (check motion or isAlive)
        //
        pthread_mutex_lock(&device->mutex);
        icLogTrace(LOG_TAG, "checking camera %s", device->uuid);
        bool motionEnabled = device->motionEnabled;
        bool useSercommEventPush = device->useSercommEventPush;
        pthread_mutex_unlock(&device->mutex);
        if (motionEnabled == true && useSercommEventPush == false)
        {
            // MOTION IS ENABLED and we are not using sercomm event push
            //
            uint32_t blackoutSecs = getMotionBlackoutSeconds();

            // log the elapsed time
            //
            uint32_t elapsedSeconds = timeTrackerElapsedSeconds(motionBlackoutTracker);
            if (timeTrackerRunning(motionBlackoutTracker))
            {
                icLogDebug(LOG_TAG, "Motion blackout period %"
                        PRIu32
                        " seconds (Elapsed seconds = %d, Faulted = %s)", blackoutSecs, elapsedSeconds,
                           faulted ? "true" : "false");
            }

            // see if we should send the "still" event
            //
            if (faulted == true && elapsedSeconds >= blackoutSecs)
            {
                // update flag and stop the timer
                //
                icLogDebug(LOG_TAG, "Sensor has been faulted long enough, clearing");
                faulted = false;
                timeTrackerStop(motionBlackoutTracker);

                // inform our callback that the motion state changed
                //
                device->notify(device, CAMERA_MOTION_CLEAR_CHANGE);
            }

            // see if there is a motion event to process (also serves as an 'isAlive' check)
            //
            goodResponse = handleCameraPoll(device, motionBlackoutTracker, &faulted);
        }
        else
        {
            // MOTION IS DISABLED or we are using sercomm event push
            // reset motion variables (motion is off, it may have been on previously)
            //
            timeTrackerStop(motionBlackoutTracker);
            faulted = false;

            // see if the device is alive
            //
            goodResponse = handleCameraIsAlive(device);
        }

        // check the result from polling or pinging the camera
        //
        if (goodResponse == true)
        {
            // camera is online.  see if we need to do anything due to the previous state
            //
            errorCount = 0;
            if (inCommFail == true)
            {
                successCount++;
                if (successCount >= minSuccessCount)
                {
                    // clear the comm fail trouble by setting the 'comFailTrouble' value to NULL
                    //
                    icLogInfo(LOG_TAG, "Camera %s is up, clearing 'commFail' trouble", device->uuid);
                    inCommFail = false;
                    pthread_mutex_lock(&device->mutex);
                    device->opState = CAMERA_OP_STATE_READY;
                    pthread_mutex_unlock(&device->mutex);
                    device->notify(device, CAMERA_ONLINE_CHANGE);
                }
                else
                {
                    int32_t untilRestore = minSuccessCount - successCount;
                    if (untilRestore < 0)
                    {
                        untilRestore = 0;
                    }
                    icLogInfo(LOG_TAG,
                              "Camera %s is up, waiting for %"PRId32 " polls to restore trouble",
                              device->uuid,
                              untilRestore);
                }

                // see if the firmware changed.  quite possible the upgrade timed out waiting
                // for the camera to come back, and this is when it came back
                //
                cameraDeviceCheckFirmwareValue(device, "after comm restore;", true);
                didInitialCheck = true;
            }
            else if (didInitialCheck == false)
            {
                // first successful time talking to the camera (since bootup).
                // go ahead and get the version to see if it's different
                //
                if (cameraDeviceCheckFirmwareValue(device, "initial query;", true) == true)
                {
                    // able to query the camera.  go ahead and mark it online (handle scenarios where we think
                    // it's offline initially due to caching)
                    //
                    inCommFail = false;
                    pthread_mutex_lock(&device->mutex);
                    device->opState = CAMERA_OP_STATE_READY;
                    pthread_mutex_unlock(&device->mutex);
                    device->notify(device, CAMERA_ONLINE_CHANGE);
                }
                didInitialCheck = true;
            }

            updateDeviceDateLastContacted(device->uuid);
        }
        else
        {
            successCount = 0;

            if (++errorCount >= maxErrorCount && inCommFail == false)
            {
                // too many errors, so create a 'comFailTrouble' attribute with some details.  The camera is now
                // in communication failure mode.  We will continue to attempt to find it at its current IP address
                // via this monitor thread, but there is a chance it simply changed IP addresses due to DHCP.
                // the main openHomeCameraDeviceDriver will perform SSDP discovery of cameras if any of them are
                // in comm fail.  If it finds that this device has a new IP address, it will update it there which
                // causes this monitor thread to pick it up again at its new IP address and clear the trouble.
                //
                icLogInfo(LOG_TAG, "Camera %s is down after %"PRIu32 " attempts, creating 'commFail' trouble", device->uuid, errorCount);
                inCommFail = true;
                pthread_mutex_lock(&device->mutex);
                device->opState = CAMERA_OP_STATE_OFFLINE;
                pthread_mutex_unlock(&device->mutex);
                device->notify(device, CAMERA_OFFLINE_CHANGE);
            }
            else if (inCommFail == true)
            {
                icLogWarn(LOG_TAG, "Camera %s still in 'commFail' ", device->uuid);
            }
            else
            {
                icLogWarn(LOG_TAG,
                          "Camera '%s' (%s) did not respond. Pushing device into ARP cache to recover",
                          device->uuid,
                          device->ipAddress);

                uint8_t macAddr[ETHER_ADDR_LEN];
                pthread_mutex_lock(&device->mutex);

                if (macAddrToBytes(device->macAddress, macAddr, true) == true)
                {
                    setMacAddressForIP(device->ipAddress, macAddr, NULL);
                }
                else
                {
                    icLogWarn(LOG_TAG, "Unable to convert camera '%s' MAC to byte array", device->uuid);
                }

                pthread_mutex_unlock(&device->mutex);
            }
        }

        // wait for the poll time tracker to complete so we don't hammer the camera
        //
        pthread_mutex_lock(&device->mutex);
        while (device->monitorRunning == true && timeTrackerRunning(pollTracker) == true && timeTrackerExpired(pollTracker) == false)
        {
            incrementalCondTimedWait(&device->cond, &device->mutex, timeTrackerSecondsUntilExpiration(pollTracker));
        }
        timeTrackerStop(pollTracker);
        pthread_mutex_unlock(&device->mutex);
    }

    // exit the thread (shutting down or deleted the device)
    //
    icLogDebug(LOG_TAG, "Event listener for camera %s exiting", device->uuid);
    timeTrackerDestroy(motionBlackoutTracker);
    timeTrackerDestroy(pollTracker);

    // reset monitor running flag
    //
    pthread_mutex_lock(&device->mutex);
    device->monitorRunning = false;
    pthread_mutex_unlock(&device->mutex);

    if (deleteMe == true)
    {
        icLogDebug(LOG_TAG, "destroying camera device %s", device->uuid);
        destroyCameraDevice(device);
    }

    return NULL;
}

/*---------------------------------------------------------------------------------------
 *
 * start the thread to monitor the device.  will look for
 * "offline" and/or "motion", as well as rediscover if the
 * IP Address changes.  any changes (motion, ip, offline)
 * will be communicated via the cameraDeviceChangedCallback
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceStartMonitorThread(cameraDevice *device)
{
    // sanity check.  silly, but need at least the uuid
    //
    if (device == NULL || device->uuid == NULL)
    {
        icLogWarn(LOG_TAG, "unable to start camera monitor thread, device and/or uuid is missing");
        return false;
    }

    // Not the best place for this
    if(device->useSercommEventPush)
    {
        // although this configuration is stored persistently in the camera, our IP address could have changed
        configureSercommEventPushUrl(device);
    }

    // don't run 2 of them
    //
    pthread_mutex_lock(&device->mutex);
    if (device->monitorRunning == true)
    {
        icLogWarn(LOG_TAG, "unable to start camera monitor for %s;  already have a thread running", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    // make a detached thread, saving the thread-id
    //
    device->monitorRunning = true;

    char *name = stringBuilder("camMon:%s", device->uuid);
    createThread(&device->monitorThread, cameraDeviceMonitorThread, device, name);
    free(name);

    pthread_mutex_unlock(&device->mutex);

    return true;
}

/*---------------------------------------------------------------------------------------
 *
 * stops the monitoring of this device.  if 'waitForIt' is true,
 * will block until the thread dies
 *---------------------------------------------------------------------------------------*/
void cameraDeviceStopMonitorThread(cameraDevice *device, bool waitForIt)
{
    // set the running flag to false, but don't block on it
    //
    int worked = pthread_mutex_trylock(&device->mutex);
    device->monitorRunning = false;
    if (worked == 0)
    {
        pthread_cond_broadcast(&device->cond);
        pthread_mutex_unlock(&device->mutex);
    }

    // if told to wait, join on the thread until it completes
    //
    if (waitForIt == true)
    {
        pthread_join(device->monitorThread, NULL);
    }
}

/*---------------------------------------------------------------------------------------
 *
 * attempts to reset the device to factory defaults.
 *
 *---------------------------------------------------------------------------------------*/
void cameraDeviceResetToFactory(cameraDevice *device)
{
    // ignore state, probably about to be destroyed
    //
    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    // calling ohcm, so need a camInfo
    //
    icLogDebug(LOG_TAG, "resetting %s to factory", device->uuid);
    factoryResetOhcmCamera(camInfo, CONNECTION_RETRY_COUNT);
    destroyOhcmCameraInfo(camInfo);
}

/*--===================================================================================--*
 *
 *  upgrade the device
 *
 *--===================================================================================--*/

/*
 * -- internal function --
 *
 * wait for a camera to complete a reboot
 */
static bool waitForCameraRestart(ohcmCameraInfo *cam, bool waitForDeath, uint32_t timeoutSeconds)
{
    bool retVal = false;
    ohcmResultCode rc;

    /* start time tracker to timeout after timeoutSeconds */
    timeTracker *timer = timeTrackerCreate();
    timeTrackerStart(timer, timeoutSeconds);

    if (waitForDeath == true)
    {
        /* wait for camera to shut down */
        do
        {
            rc = isOhcmAlive(cam, IS_ALIVE_RETRY_COUNT);
            icLogDebug(LOG_TAG, "Waiting for camera %s to shutdown, isAlive() returned %d", cam->macAddress, rc);
            usleep(CAMERA_ISALIVE_WAIT);
        } while (rc == OHCM_SUCCESS);
    }

    uint8_t successes = 0;
    /* Now wait for the camera to be ready again */
    do
    {
        rc = isOhcmAlive(cam, IS_ALIVE_RETRY_COUNT);
        icLogDebug(LOG_TAG, "Waiting for camera %s to boot up, isAlive() returned %d", cam->macAddress, rc);
        usleep(CAMERA_ISALIVE_WAIT);

        if (rc == OHCM_SUCCESS)
        {
            successes++;
        }
        else
        {
            successes = 0;
        }

    } while (successes < IS_ALIVE_SUCCESS_COUNT && (timeTrackerExpired(timer)!= true));

    if (rc == OHCM_SUCCESS)
    {
        icLogDebug(LOG_TAG, "Camera %s is now alive, continue.", cam->macAddress);
        retVal = true;
    }
    else
    {
        icLogDebug(LOG_TAG, "Timed out waiting for camera %s to reboot", cam->macAddress);
    }

    // cleanup and return
    //
    timeTrackerDestroy(timer);
    return retVal;
}

/*---------------------------------------------------------------------------------------
 *
 * checks to see if this device needs an upgrade by comparing
 * the camera firmwareVersion to ones defined in the device descriptor.
 * if 'checkMinimum' is true, then the comparison is against the 'min fw version',
 * otherwise compared to the 'latest fw version'
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceCheckForUpgrade(cameraDevice *device, DeviceDescriptor *descriptor, bool checkMinimum)
{
    bool retVal = false;

    // get the current camera firmware version as an array of integers
    //
    pthread_mutex_lock(&device->mutex);
    if (device->details == NULL || device->details->firmwareVersion == NULL)
    {
        // unable to compare, unknown camera firmware version
        //
        icLogWarn(LOG_TAG, "unable to check camera upgrade for device %s; missing firmware version", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return retVal;
    }

    // while we have the lock, get the current firmware version
    // represented in an array of integers
    //
    uint16_t camLen = 0;
    uint32_t *camVersion = versionStringToInt(device->details->firmwareVersion, &camLen);
    pthread_mutex_unlock(&device->mutex);

    // see which version within the device descriptor to compare against (min or latest)
    //
    char *compareVersion = NULL;
    if (checkMinimum == true)
    {
        // minimum
        //
        if (descriptor == NULL ||
            descriptor->minSupportedFirmwareVersion == NULL ||
            descriptor->latestFirmware == NULL ||
            descriptor->latestFirmware->filenames == NULL ||
            linkedListCount(descriptor->latestFirmware->filenames) == 0)
        {
            // unable to obtain the minimum version (or missing the filename)
            //
            icLogWarn(LOG_TAG, "unable to check camera upgrade for device %s; missing descriptor 'minimum' firmware version ", device->uuid);
            free(camVersion);
            return retVal;
        }

        // use 'minimum' version for the array
        //
        compareVersion = descriptor->minSupportedFirmwareVersion;
    }
    else
    {
        // latest
        //
        if (descriptor == NULL ||
            descriptor->latestFirmware == NULL ||
            descriptor->latestFirmware->version == NULL ||
            descriptor->latestFirmware->filenames == NULL ||
            linkedListCount(descriptor->latestFirmware->filenames) == 0)
        {
            // unable to obtain the latest version (or missing the filename)
            //
            icLogWarn(LOG_TAG, "unable to check camera upgrade for device %s; missing descriptor 'latest' firmware version ", device->uuid);
            free(camVersion);
            return retVal;
        }

        // use 'latest' version for the array
        //
        compareVersion = descriptor->latestFirmware->version;
    }

    // convert version strings to an array so we can compare
    //
    uint16_t descLen = 0;
    uint32_t *descVersion = versionStringToInt(compareVersion, &descLen);

    icLogDebug(LOG_TAG, "checking if camera firmware version '%s' is less then '%s'", device->details->firmwareVersion, compareVersion);
    int8_t cmp = compareVersionArrays(camVersion, camLen, descVersion, descLen);
    if (cmp > 0)
    {
        // descriptor version is greater-than the camera version.  therefore
        // need to upgrade the camera prior to configuring it.
        //
        icLogDebug(LOG_TAG, "need to upgrade camera since the fw version is below minimum/latest!");
        retVal = true;
    }
    else
    {
        icLogDebug(LOG_TAG, "camera meets minimum fw version.  not upgrading at this time");
        retVal = false;
    }

    // cleanup
    //
    free(camVersion);
    free(descVersion);

    return retVal;
}

/*---------------------------------------------------------------------------------------
 *
 * ask the camera to begin the firmware upgrade process.
 * this will block until the upgrade is complete (or fails).
 * on success, the device->details->firmwareVersion should
 * reflect the new version requested.
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDevicePerformUpgrade(cameraDevice *device, char *firmwareFilename, char *firmwareVersion, uint16_t timeoutSecs)
{
    if (firmwareFilename == NULL)
    {
        // unable to continue
        //
        icLogWarn(LOG_TAG, "unable to upgrade camera firmware, missing 'firmware filename'");
        return false;
    }

    pthread_mutex_lock(&device->mutex);

    // ignore if this is the camera we're running on
    //
    if (device->isIntegratedPeripheral == true)
    {
        icLogWarn(LOG_TAG, "unable to upgrade camera %s via standard mechanisms; this is a Zilker/Touchstone device", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }

    // look at current operation
    //
    if (device->opState == CAMERA_OP_STATE_CONFIGURE)
    {
        icLogWarn(LOG_TAG, "unable to upgrade camera %s; it is being configured", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    else if (device->opState == CAMERA_OP_STATE_OFFLINE)
    {
        icLogWarn(LOG_TAG, "unable to configure camera %s; it is offline", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    device->opState = CAMERA_OP_STATE_UPGRADE;
    pthread_mutex_unlock(&device->mutex);

    // need the base URL of where camera firmware is kept
    //
    char *baseURL = getPropertyAsString(CAMERA_FIRMWARE_URL_NODE, NULL);
    if (baseURL == NULL)
    {
        icLogWarn(LOG_TAG, "unable to upgrade camera firmware, missing property %s", CAMERA_FIRMWARE_URL_NODE);
        return false;
    }

    // combine URL with 'firmwareFilename' to get the full path of where the firmware is located
    //
    size_t len = strlen(baseURL) + strlen(firmwareFilename) + 4;  // room for slash and NULL
    char *fullURL = (char *)malloc(sizeof(char) * len);
    sprintf(fullURL, "%s/%s", baseURL, firmwareFilename);
    free(baseURL);

    // create the request
    //
    icLogInfo(LOG_TAG, "starting camera firmware upgrade.  device=%s url=%s", device->uuid, fullURL);
    ohcmUpdateFirmwareRequest *req = createOhcmUpdateFirmwareRequest();
    if (firmwareVersion != NULL)
    {
        req->fwVersion = strdup(firmwareVersion);
    }
    req->url = fullURL;

    // create ohcm object
    //
    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    // start the upgrade
    //
    ohcmResultCode rc = startOhcmUpdateFirmwareRequest(camInfo, req, CURL_RETRY_COUNT);
    if (rc != OHCM_SUCCESS && rc != OHCM_REBOOT_REQ)
    {
        // log and cleanup (NOTE also releases fullURL)
        //
        icLogWarn(LOG_TAG, "error upgrading camera firmware.  device=%s url=%s rc=%d %s", device->uuid, fullURL, rc, ohcmResultCodeLabels[rc]);
        destroyOhcmUpdateFirmwareRequest(req);
        destroyOhcmCameraInfo(camInfo);

        // set state to READY
        //
        pthread_mutex_lock(&device->mutex);
        device->opState = CAMERA_OP_STATE_READY;
        pthread_mutex_unlock(&device->mutex);
        return false;
    }

    // cleanup the request (and fullURL)
    //
    destroyOhcmUpdateFirmwareRequest(req);
    fullURL = NULL;
    req = NULL;

    // upgrade started, need to keep probing to see when it's done
    //
    bool sawProgress = false;
    bool doneUpgrade = false;
    timeTracker *tracker = timeTrackerCreate();
    timeTrackerStart(tracker, timeoutSecs);
    while (doneUpgrade == false && timeTrackerExpired(tracker) == false)
    {
        // get the upgrade status
        //
        icLogDebug(LOG_TAG, "checking camera firmware upgrade progress on device=%s", device->uuid);
        ohcmUpdateFirmwareStatus *state = createOhcmUpdateFirmwareStatus();
        rc = getOhcmUpdateFirmwareStatus(camInfo, state, 1);
        if (rc == OHCM_SUCCESS)
        {
            // see if a failure has been detected and abort if so
            //
            if(state->updateState != NULL && strcmp("failure", state->updateState) == 0)
            {
                icLogError(LOG_TAG, "camera firmware upgrade failed.  Invalid url?");
                break;
            }

            // see if complete
            //
            if (state->updateSuccess == true)
            {
                // done!
                //
                icLogDebug(LOG_TAG, "completed camera firmware upgrade.  device=%s", device->uuid);
                doneUpgrade = true;
            }

            // see if we progressed forward
            //
            if (state->downloadPercentage > 0)
            {
                // % download is > 0
                //
                icLogDebug(LOG_TAG, "camera firmware upgrade progress=%d for device=%s", state->downloadPercentage, device->uuid);
                sawProgress = true;
            }

        }
        else
        {
            // possible that the upgrade is in progress
            //
            if (sawProgress == true)
            {
                icLogDebug(LOG_TAG, "unable to communicate with camera, however it appears to have started the upgrade...so assuming it is rebooting.  device=%s", device->uuid);
                doneUpgrade = true;
            }
        }
        destroyOhcmUpdateFirmwareStatus(state);

        // wait a few seconds before checking again
        //
        sleep(5);
    }

    // cleanup
    //
    timeTrackerDestroy(tracker);

    // if success, wait for the device to complete the reboot
    //
    if (doneUpgrade == true)
    {
        bool restartedSuccessfully = waitForCameraRestart(camInfo, false, CAMERA_REBOOT_TIMEOUT_SECONDS);
        if (restartedSuccessfully == true)
        {
            icLogDebug(LOG_TAG, "after camera upgrade; device appears to be online now; device=%s", device->uuid);

            // now that it's done, ask for the firmware version, and update our local vars
            //
            cameraDeviceCheckFirmwareValue(device, "after camera upgrade;", false);
        }
        else
        {
            icLogWarn(LOG_TAG, "Device=%s appears to still be offline.", device->uuid);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "camera upgrade failed. timed out? device=%s", device->uuid);
    }

    // restore state and cleaup.
    // NOTE: set state to 'ready' regardless if the upgrade failed or didn't see the camera reboot
    //       we're relying on the "monitor thread" to determine if the device is online or not (so
    //       it can create/clear the trouble)
    //
    pthread_mutex_lock(&device->mutex);
    device->opState = CAMERA_OP_STATE_READY;
    pthread_mutex_unlock(&device->mutex);
    destroyOhcmCameraInfo(camInfo);
    return doneUpgrade;
}

/*
 * asks the camera for it's firmware version, and compare to
 * what we believe the version is.  if the version changed,
 * will update the device->details->firmwareVersion.  will
 * return 'true' if the firmware version string changed.
 */
bool cameraDeviceCheckFirmwareValue(cameraDevice *device, const char *logPrefix, bool notifyCallback)
{
    // create ohcm object
    //
    bool retVal = false;
    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);




    // now that it's done, ask for the firmware version, and update our local vars
    //
    ohcmDeviceInfo *info = createOhcmDeviceInfo();
    ohcmResultCode rc = getOhcmDeviceInfo(camInfo, info, CONNECTION_RETRY_COUNT);
    if (rc == OHCM_SUCCESS)
    {
        if (info->firmwareVersion != NULL)
        {
            bool doSave = false;

            // save off the new firmware version (if different)
            //
            icLogInfo(LOG_TAG, "%s known camera firmware for device %s is %s", logPrefix, device->uuid, device->details->firmwareVersion);
            pthread_mutex_lock(&device->mutex);
            if (device->details->firmwareVersion != NULL && strcmp(info->firmwareVersion, device->details->firmwareVersion) != 0)
            {
                // firmware version changed
                //
                icLogDebug(LOG_TAG, "%s camera firmware for device %s changed from %s to %s", logPrefix, device->uuid,
                           device->details->firmwareVersion, info->firmwareVersion);
                doSave = true;
            }
            else if (device->details->firmwareVersion == NULL || strlen(device->details->firmwareVersion) == 0)
            {
                // do not have firmware version cached
                //
                doSave = true;
            }

            if (doSave == true)
            {
                if (device->details->firmwareVersion != NULL)
                {
                    free(device->details->firmwareVersion);
                }
                device->details->firmwareVersion = strdup(info->firmwareVersion);
            }
            pthread_mutex_unlock(&device->mutex);
            retVal = true;

            if (doSave == true && notifyCallback == true)
            {
                // notify our callback that the firmware version changed
                //
                device->notify(device, CAMERA_FIRMWARE_CHANGE);
            }
        }
        else
        {
            icLogWarn(LOG_TAG, "%s unable to obtain firmware version for device=%s", logPrefix, device->uuid);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s unable to get camera info for device=%s; rc=%d %s", logPrefix, device->uuid, rc, ohcmResultCodeLabels[rc]);
    }

    // cleanup
    //
    destroyOhcmDeviceInfo(info);
    destroyOhcmCameraInfo(camInfo);
    return retVal;
}

/*--===================================================================================--*
 *
 *  reboot the device
 *
 *--===================================================================================--*/

/*---------------------------------------------------------------------------------------
 *
 * reboot the device, and wait for it to come back online
 * (if 'waitForReturn' is set)
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceReboot(cameraDevice *device, bool waitForReturn, uint16_t timeoutSeconds)
{
    bool retVal = false;

    // use OHCM to reboot the camera
    //
    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);
    ohcmResultCode rc = rebootOhcmCamera(camInfo, CONNECTION_RETRY_COUNT);

    // look at different results as the camera may be booting already
    //
    if (rc == OHCM_LOGIN_FAIL || rc == OHCM_GENERAL_FAIL || rc == OHCM_INVALID_CONTENT)
    {
        // unable to ask the camera to reboot
        //
        icLogWarn(LOG_TAG, "rebootCamera failed with return value %d %s", rc, ohcmResultCodeLabels[rc]);
    }
    else if (waitForReturn == true)
    {
        // wait for the camera to come back online
        // TODO: delete the relay session since it will no longer be valid.  right now that's cleanedup via 'commService'
        //
        retVal = waitForCameraRestart(camInfo, true, timeoutSeconds);
    }
    else
    {
        retVal = true;
    }

    // cleanup and return
    //
    destroyOhcmCameraInfo(camInfo);
    return retVal;
}

/*---------------------------------------------------------------------------------------
 *
 * ping the device to see if it's online
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDevicePing(cameraDevice *device, uint16_t timeoutSeconds)
{
    bool retVal = false;

    // ping the camera to see if alive
    //
    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);
    ohcmResultCode rc = isOhcmAlive(camInfo, IS_ALIVE_RETRY_COUNT);
    if (rc == OHCM_SUCCESS)
    {
        // Log line used for Telemetry... DO NOT CHANGE
        //
        icLogDebug(LOG_TAG, "OHCM: isAlive() returned Success: from camera %s", device->ipAddress);
        retVal = true;
    }
    else
    {
        // Log line used for Telemetry... DO NOT CHANGE
        //
        icLogWarn(LOG_TAG, "OHCM: isAlive() returned Failure: from camera %s", device->ipAddress);
    }

    // cleanup & return
    //
    destroyOhcmCameraInfo(camInfo);
    return retVal;
}

/*--===================================================================================--*
 *
 *  media operations
 *
 *--===================================================================================--*/

static char *generateSessionUuid()
{
    uuid_t uuid = { '\0' };

    uuid_generate(uuid);
    char *uuidString = (char*)calloc(1, 64);
    uuid_unparse(uuid, uuidString);

    return uuidString;
}

/*---------------------------------------------------------------------------------------
 *
 * establish a new media tunnel with the camera.
 * returns the 'media session id'.
 * caller must free returned string (if not NULL)
 *
 *---------------------------------------------------------------------------------------*/
char *cameraDeviceCreateMediaTunnel(cameraDevice *device, char *url)
{
    char *retVal = NULL;

    // look at current operation
    //
    pthread_mutex_lock(&device->mutex);
    if (device->opState == CAMERA_OP_STATE_CONFIGURE)
    {
        icLogWarn(LOG_TAG, "unable to setup media tunnel for camera %s; it is being configured", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    else if (device->opState == CAMERA_OP_STATE_UPGRADE)
    {
        icLogWarn(LOG_TAG, "unable to setup media tunnel for camera %s; it is uprading", device->uuid);
        pthread_mutex_unlock(&device->mutex);
        return false;
    }
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    ohcmMediaTunnelRequest *req = createOhcmMediaTunnelRequest();
    req->sessionID = generateSessionUuid();
    req->gatewayURL = strdup(url);
    req->failureURL = strdup("poll://relayserversetupfailure");

    // Log line used for Telemetry... DO NOT CHANGE
    //
    icLogDebug(LOG_TAG, "creating media tunnel with session id %s and gatewayURL %s", req->sessionID, req->gatewayURL);

    ohcmResultCode rc = startOhcmMediaTunnelRequest(camInfo, req, CONNECTION_RETRY_COUNT);
    icLogDebug(LOG_TAG, "createMediaTunnel returned %d %s", rc, ohcmResultCodeLabels[rc]);
    if (rc == OHCM_SUCCESS)
    {
        retVal = req->sessionID; //caller frees
        req->sessionID = NULL;
    }
    destroyOhcmMediaTunnelRequest(req);
    destroyOhcmCameraInfo(camInfo);

    return retVal;
}

//static icLinkedList *getAllMediaTunnelSessions(CameraInfo *info)
//{
//    icLinkedList *retVal = linkedListCreate();
//    MEDIATUNNELSTATUSLIST *status;
//
//    RET_TYPE ret = getMediaTunnel(info, &status, CONNECTION_RETRY_COUNT);
//    if (ret == SUCCESS && status != NULL)
//    {
//        MEDIATUNNELSTATUSLIST *stat = status;
//        while (stat != NULL)
//        {
//            if (stat->sessionID != NULL)
//            {
//                linkedListAppend(retVal, strdup(stat->sessionID));
//            }
//            stat = stat->Next;
//        }
//    }
//    if (status != NULL)
//    {
//        destroyMediaTunnelStruct(status);
//    }
//
//    return retVal;
//}

/*---------------------------------------------------------------------------------------
 *
 * destroy a media tunnel
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceDestroyMediaTunnel(cameraDevice *device, char *session)
{
    bool retVal = false;

    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

//    // if session is not defined, destroy all of them
//    //
//    if (session == NULL)
//    {
//        // get all of the sessions
//        //
//        icLinkedList *sessions = getAllMediaTunnelSessions(camInfo);
//        icLinkedListIterator *loop = linkedListIteratorCreate(sessions);
//        while (linkedListIteratorHasNext(loop) == true)
//        {
//            session = linkedListIteratorGetNext(loop);
//            icLogDebug(LOG_TAG, "destroying media tunnel with session id %s", session);
//            RET_TYPE ret = deleteMediaTunnel(camInfo, session, CONNECTION_RETRY_COUNT);
//            icLogDebug(LOG_TAG, "deleteMediaTunnel returned %d", ret);
//            if (ret == SUCCESS) // why not also look for RESPONSE_OK?
//            {
//                retVal = true;
//            }
//        }
//
//        // destroy the list and the strings within it
//        //
//        linkedListIteratorDestroy(loop);
//        linkedListDestroy(sessions, NULL);
//    }
//    else
    if (session != NULL)
    {
        // Log line used for Telemetry... DO NOT CHANGE
        //
        icLogDebug(LOG_TAG, "destroying media tunnel with session id %s", session);

        ohcmResultCode rc = stopOhcmMediaTunnelRequest(camInfo, session, CONNECTION_RETRY_COUNT);
        icLogDebug(LOG_TAG, "deleteMediaTunnel returned %d %s", rc, ohcmResultCodeLabels[rc]);
        if (rc == OHCM_SUCCESS)
        {
            retVal = true;
        }
    }

    destroyOhcmCameraInfo(camInfo);
    return retVal;
}

/*---------------------------------------------------------------------------------------
 *
 *
 *
 *---------------------------------------------------------------------------------------*/
void cameraDeviceGetMediaTunnelStatus(cameraDevice *device, void *wtf)
{
    // TODO:
}

/*---------------------------------------------------------------------------------------
 *
 * take a picture and save to the provided 'localFilename'
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceTakePicture(cameraDevice *device, char *localFilename)
{
    bool retVal = false;

    // TODO: check state?

    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    // start the upload, using the pre-defined stream VIDEO_UPLOAD_STREAM_ID
    // (done this way on purpose so that it matches the video upload format) - CVGD-5590
    //
    char streamId[32];
    sprintf(streamId, "%d", VIDEO_UPLOAD_STREAM_ID);
    ohcmResultCode rc = downloadOhcmPicture(camInfo, streamId, localFilename, CONNECTION_RETRY_COUNT);
    if (rc == OHCM_SUCCESS)
    {
        retVal = true;
    }
    else
    {
        icLogWarn(LOG_TAG, "unable to take pic from camera %s: rc=%d %s", device->macAddress, rc, ohcmResultCodeLabels[rc]);
    }
    destroyOhcmCameraInfo(camInfo);
    return retVal;
}

/*---------------------------------------------------------------------------------------
 *
 * grab and upload a video clip
 *
 *---------------------------------------------------------------------------------------*/
bool cameraDeviceTakeVideoClip(cameraDevice *device, char *postUrl, uint8_t durationSecs)
{
    bool retVal = false;

    // TODO: check state?

    pthread_mutex_lock(&device->mutex);
    ohcmCameraInfo *camInfo = allocCameraInfo(device);
    pthread_mutex_unlock(&device->mutex);

    ohcmUploadVideo *req = createOhcmUploadVideo();
    req->id = strdup("0");
    req->videoClipFormatType = OHCM_VIDEO_FORMAT_MP4;
    req->blockUploadComplete = true;
    req->gatewayUrl = strdup(postUrl);
    req->eventUrl = strdup("poll://videouploadevent");
    // TODO: duration?

    // start the upload, using the pre-defined stream VIDEO_UPLOAD_STREAM_ID
    //
    char streamId[32];
    sprintf(streamId, "%d", VIDEO_UPLOAD_STREAM_ID);
    ohcmResultCode rc = uploadOhcmVideo(camInfo, req, CONNECTION_RETRY_COUNT);
    if (rc == OHCM_SUCCESS)
    {
        retVal = true;
    }
    else
    {
        icLogWarn(LOG_TAG, "unable to upload video from camera %s: rc=%d %s", device->macAddress, rc, ohcmResultCodeLabels[rc]);
    }

    // cleanup & return
    //
    destroyOhcmUploadVideo(req);
    destroyOhcmCameraInfo(camInfo);

    return retVal;
}


/*--===================================================================================--*
 *
 *  misc functions
 *
 *--===================================================================================--*/

/*
 * -- internal function --
 *
 * determine if camera is the integrated peripheral (hub)
 *
 * NOTE: ifdef is set for Sercomm cameras only. If we add other cameras
 *       as hubs, this will need to be updated.
 */
static bool isCameraIntegratedPeripheral(const char *uuid)
{
    return false;
}

/*
 * return the URL that a camera should use to post events to us.  Only used if useSercommEventPush is enabled.
 *
 * The URL will contain the camera's mac so we can tell which camera posted the event.  It should look similar to this:
 *
 * http://172.16.12.2:5555/b4a5efecf3df
 *
 * Caller frees result
 */
static char *getEventPushUrl(const char *macAddress)
{
    char *result = NULL;
    char uuid[13]; //12 for mac address without colons plus \0

    if(macAddress == NULL)
    {
        return NULL;
    }

    macAddrToUUID(uuid, macAddress);

    char *ourIpAddress = getPropertyAsString("localIpAddress", NULL);

    if(ourIpAddress != NULL)
    {
        result = (char*)malloc(10 /* http://:/\0 */ + 5 /* max uint16_t for port */ + strlen(ourIpAddress) + strlen(uuid));
        sprintf(result, "http://%s:%d/%s", ourIpAddress, SERCOMM_EVENT_HANDLER_PORT, uuid);
        free(ourIpAddress);
    }
    else
    {
        icLogError(LOG_TAG, "Attempt to get event push URL failed since localIpAddress property is not set!");
    }

    return result;
}

static void configureSercommEventPushUrl(cameraDevice *device)
{
    CURL *curl;

    if(device == NULL ||
            device->macAddress == NULL ||
            device->adminCredentials == NULL ||
            device->adminCredentials->username == NULL ||
            device->adminCredentials->password == NULL)
    {
        icLogError(LOG_TAG, "configureSercommEventPushUrl: invalid arguments");
        return;
    }

#define HTTP_NOTIFY_URI "/adm/set_group.cgi?group=HTTP_NOTIFY&http_url="

    char *urlArg = getEventPushUrl(device->macAddress);

    curl = curl_easy_init();
    if(curl != NULL && urlArg != NULL)
    {
        sslVerify tlsVerify = ohcmGetTLSVerify();

        // set standard curl options
        applyStandardCurlOptions(curl, NULL, 60, tlsVerify, false);

        //set the HTTP_NOTIFY group via a GET to something like:
        //    http://wgOw7300:JsB3b4Vk@172.16.12.3/adm/set_group.cgi?group=HTTP_NOTIFY&http_url=http%3A%2F%2F172.16.12.2%3A5555/b4a5efecf3df

        char *url = (char*)malloc(10 /* http://:@\0 */ +
                                  strlen(device->adminCredentials->username) +
                                  strlen(device->adminCredentials->password) +
                                  strlen(device->ipAddress) +
                                  strlen(HTTP_NOTIFY_URI) +
                                  strlen(urlArg));

        sprintf(url, "http://%s:%s@%s%s%s",
                device->adminCredentials->username,
                device->adminCredentials->password,
                device->ipAddress,
                HTTP_NOTIFY_URI,
                urlArg);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        CURLcode res = curl_easy_perform(curl);
        if(res == CURLE_OK)
        {
            icLogDebug(LOG_TAG, "Enabled sercomm event push to %s on camera %s", urlArg, device->macAddress);
        }
        else
        {
            icLogError(LOG_TAG, "Failed to enable sercomm event push: %s", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        free(url);
    }

    free(urlArg);
}

