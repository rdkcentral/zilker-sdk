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
* cameraTest.c
*
* Tests the camera functions:
*       - discover cameras using SSDP
 *      - TODO add more as they become available
*
* Author: jgleason
*-----------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <ssdp/ssdp.h>
#include <openHomeCamera/ohcm.h>
#include <openHomeCamera/ohcmDiscover.h>


/* Define the nodes in XML responses */
#define ROOT_NODE           "root"
#define DEVICE_NODE         "device"
#define FRIENDLY_NAME_NODE  "friendlyName"
#define MANUFACTURER_NODE   "manufacturer"
#define MODEL_NAME_NODE     "modelName"
#define MODEL_NUMBER_NODE   "modelNumber"
#define UUID_NODE           "UDN"

// local variables
static char user[64];
static char password[64];
static char certFile[1024];
static char keyFile[1024];

// private functions
static void discoverCallback(SsdpDevice *device);
static bool testGetCameraDeviceInfo(char *camHost);
static bool testIsCameraAlive(char *camHost);
static bool testRebootCamera(char *camHost);
static bool testResetCamera(char *camHost);
static bool testConfigureCamera(char *camHost);
static bool testGetCameraTimezone(char *camHost);
static bool testDownloadCameraPic(char *camHost);

/*
 * Usage function
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  cameraTest <-s|-r|-g|-w|-f> -n [hostname] -p [priv-key-file] -c [cert-file]>\n");
    fprintf(stderr, "    -s : discovery cameras via SSDP\n");
    fprintf(stderr, "    -r : reboot the camera                        (requires -n)\n");
    fprintf(stderr, "    -g : get device info                          (requires -n)\n");
    fprintf(stderr, "    -w : factory reset (wipe) the camera          (requires -n)\n");
    fprintf(stderr, "    -f : configure camera similar to Touchstone   (requires -n)\n");
	fprintf(stderr, "    -n [hostname]     : define device hostname/ip\n");
	fprintf(stderr, "    -u [user]         : default = administrator\n");
	fprintf(stderr, "    -p [password]     : default = \n");
	fprintf(stderr, "    -k [privkey-file] : use TLS private key\n");
	fprintf(stderr, "    -c [cert-file]    : use TLS certificate\n");
    fprintf(stderr, "\n");
}

/*
 * Main - CAMERA BOOTSTRAP TEST
 *      1. Camera Discovery using SSDP scan
 *      2. Get Camera Capabilities
 *      3. Configure Camera (TODO)
 *      4. Reboot camera
 *      5. Upgrade Camera (TODO)
 */
int main(int argc, char *argv[])
{
    int c;
    bool doCameraScan = false;
    bool doCameraReboot = false;
	bool doCameraReset = false;
    bool doCameraConfig = false;
    bool doCameraInfo = false;
	char host[64];

	initIcLogger();
    initOhcm();

	// set defaults
    //
	strcpy(host, "localhost");
	strcpy(user, "administrator");
	strcpy(password, "");
    strcpy(certFile, "");
    strcpy(keyFile, "");

    // process cli args
    while ((c = getopt(argc, argv, "srgwfhn:u:p:k:c:")) != -1)
    {
        switch (c)
        {
            case 's':
                doCameraScan = true;
                break;

            case 'r':
                doCameraReboot = true;
                break;

            case 'g':
                doCameraInfo = true;
                break;

            case 'w':
                doCameraReset = true;
                break;

            case 'f':
                doCameraConfig = true;
                break;

            case 'n':
                strcpy(host, optarg);
                break;

            case 'u':
                strcpy(user, optarg);
                break;

            case 'p':
                strcpy(password, optarg);
                break;

            case 'c':
                strcpy(certFile, optarg);
                break;

            case 'k':
                strcpy(keyFile, optarg);
                break;

            case 'h':
            default:
            {
                printUsage();
                closeIcLogger();
                return -1;
            }
        }
    }

    // init camera lib and set TLS info (if defined)
    //
    if (strlen(certFile) > 0 && strlen(keyFile) > 0)
    {
        setOhcmMutualTlsMode((const char *)certFile, (const char *)keyFile);
    }

    if (doCameraScan == true)
    {
        printf("\n\ntest: Starting SSDP Discovery Test: Scan for Cameras\n\n");

        uint32_t handle = 0;
        if ((handle = ssdpDiscoverStart(CAMERA, discoverCallback)) != 0)
        {
            // wait 10 seconds, then stop the discovery
            //
            printf("test: Started discovery");
            sleep(10);
            ssdpDiscoverStop(handle);
        }
        else
        {
            printf("test: Failed to start discovery");
        }
    }

    if (doCameraInfo == true)
    {
        testGetCameraDeviceInfo(host);
    }

    if (doCameraConfig == true)
    {
        testConfigureCamera(host);
    }

    if (doCameraReset == true)
    {
        testResetCamera(host);
    }

    if (doCameraReboot == true)
    {
        testRebootCamera(host);
    }

    // cleanup and bail
    //
    closeIcLogger();
    cleanupOhcm();
    return 0;
}

/*
 * callback when the camera device is discovered
 */
static void discoverCallback(SsdpDevice *device)
{
    printf("test: Found camera! IP=%s, MAC=%s\n", device->ipAddress, device->macAddress);

    // attempt to get the details from this camera.
    //
    printf("test: gathering information about camera IP=%s\n", device->ipAddress);
    if (testGetCameraDeviceInfo(device->ipAddress) == true)
    {
        // run 'isAlive'
        printf("test: running 'is alive' on camera IP=%s\n", device->ipAddress);
        testIsCameraAlive(device->ipAddress);

        // get the streaming channel list
//        icLinkedList *list = linkedListCreate();
        ohcmCameraInfo cam;
        cam.cameraIP = device->ipAddress;
        cam.macAddress = NULL;
        cam.userName = user;
        cam.password = password;
//        getOhcmStreamingChannels(&cam, list, 1);

        // get massive config file
        printf("test: running 'get config file' on camera IP=%s\n", device->ipAddress);
        ohcmConfigFile *configFile = createOhcmConfigFile();
        getOhcmConfigFile(&cam, configFile, 1);
        destroyOhcmConfigFile(configFile);

        // get timezone
        printf("test: running 'get timezone' from camera IP=%s\n", device->ipAddress);
        testGetCameraTimezone(device->ipAddress);

        // get a picture
        printf("test: running 'download pic' from camera IP=%s\n", device->ipAddress);
        testDownloadCameraPic(device->ipAddress);

        // reboot it so we don't discover it again during this execution...
        printf("test: running 'reboot' on camera IP=%s\n", device->ipAddress);
        testRebootCamera(device->ipAddress);
    }
}

/*
 * ask the camera for the 'device info', and if successful
 * print the results.  uses the global user/pass/cert/key values
 */
static bool testGetCameraDeviceInfo(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // gather information about the device from the camera via OHCM
    //
    ohcmDeviceInfo *device = createOhcmDeviceInfo();
    ohcmResultCode rc = getOhcmDeviceInfo(&cam, device, 10);
    if(rc == OHCM_SUCCESS)
    {
        printDeviceInfo(device);
        retVal = true;
    }
    else
    {
        printf("test: Fail to get Device Information, rc=%d %s\n", rc, ohcmResultCodeLabels[rc]);
    }
    destroyOhcmDeviceInfo(device);

    return retVal;
}

/*
 * see if a particular camera is alive.  use the global variables
 * for user/pass/cert/key
 */
static bool testIsCameraAlive(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // perform 'is alive'
    //
    ohcmResultCode rc = isOhcmAlive(&cam, 3);
    if(rc == OHCM_SUCCESS)
    {
        printf("test: Success 'isAlive' of %s\n", camHost);
        retVal = true;
    }
    else
    {
        printf("test: Failed requesting 'isAlive' of %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
    }

    return retVal;
}

/*
 * reboot a particular camera.  use the global variables for user/pass/cert/key
 */
static bool testRebootCamera(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // perform reboot request
    //
    ohcmResultCode rc = rebootOhcmCamera(&cam, 3);
    if(rc == OHCM_SUCCESS)
    {
        printf("test: Success requesting reboot of %s\n", camHost);
        retVal = true;
    }
    else
    {
        printf("test: Failed requesting reboot of %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
    }

    return retVal;
}

/*
 * reset a particular camera "to factory defaults".  use the global variables for user/pass/cert/key
 */
static bool testResetCamera(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // perform reboot request
    //
    ohcmResultCode rc = factoryResetOhcmCamera(&cam, 3);
    if(rc == OHCM_SUCCESS)
    {
        printf("test: Success requesting reset of %s\n", camHost);
        retVal = true;
    }
    else
    {
        printf("test: Failed requesting reset of %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
    }

    return retVal;
}

/*
 * reconfigure a particular camera.  use the global variables for user/pass/cert/key
 */
static bool testConfigureCamera(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // first get the massive config file
    //
    ohcmConfigFile *configFile = createOhcmConfigFile();
    ohcmResultCode rc = getOhcmConfigFile(&cam, configFile, 1);
    if (rc == OHCM_SUCCESS)
    {
        printf("test: Success requesting config of %s\n", camHost);

        // create random user account (leave admin the same)
        //
        ohcmSecurityAccount *viewer = createOhcmSecurityAccount();
        viewer->id = strdup("1");
        viewer->userName = generateRandomToken(8, 8, 7);
        viewer->password = generateRandomToken(8, 8, 9);
        viewer->accessRights = OHCM_ACCESS_USER;
        linkedListAppend(configFile->securityAccountList, viewer);

        // disable http & the microphone
        //
        configFile->hostServer.httpEnabled = false;
        configFile->hostServer.httpsValidateCerts = false;
        ohcmAudioChannel *first = (ohcmAudioChannel *)linkedListGetElementAt(configFile->audioChannelList, 0);
        if (first != NULL)
        {
            first->enabled = false;
            first->microphoneEnabled = false;
        }

        // remove video input channel
        //
        linkedListClear(configFile->videoInputList, destroyOhcmVideoInputFromList);

        // now apply the configuration
        //
        rc = setOhcmConfigFile(&cam, configFile, 1);
        if(rc == OHCM_SUCCESS || rc == OHCM_REBOOT_REQ)
        {
            printf("test: Success setting config on %s\n", camHost);
            retVal = true;
            if (rc == OHCM_REBOOT_REQ)
            {
                // perform the reboot
                //
                printf("test: rebooting %s...\n", camHost);
                rebootOhcmCamera(&cam, 3);
            }
        }
        else
        {
            printf("test: Failed setting config on %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
        }
    }
    else
    {
        printf("test: Failed requesting config of %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
    }

    // cleanup
    //
    destroyOhcmConfigFile(configFile);
    return retVal;
}

/*
 * reboot a particular camera.  use the global variables for user/pass/cert/key
 */
static bool testGetCameraTimezone(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // get the timezone
    //
    char tzone[1024];
    memset(tzone, 0, 1023);
    ohcmResultCode rc = getOhcmTimeZoneInfo(&cam, tzone, 1);
    if(rc == OHCM_SUCCESS)
    {
        printf("test: Success requesting timezone from %s: %s\n", camHost, tzone);
        retVal = true;
    }
    else
    {
        printf("test: Failed requesting timezone from %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
    }

    return retVal;
}

/*
 * reboot a particular camera.  use the global variables for user/pass/cert/key
 */
static bool testDownloadCameraPic(char *camHost)
{
    bool retVal = false;

    // fill in camera object with supplied arguments
    //
    ohcmCameraInfo cam;
    cam.cameraIP = camHost;
    cam.macAddress = NULL;
    cam.userName = user;
    cam.password = password;

    // get a snapshot from the camera and save locally
    //
    ohcmResultCode rc = downloadOhcmPicture(&cam, "0", "/tmp/pic.jpg", 1);
    if(rc == OHCM_SUCCESS)
    {
        printf("test: Success downloading pic from %s;  file saved to '/tmp/pic.jpg'\n", camHost);
        retVal = true;
    }
    else
    {
        printf("test: Failed downloading pic from %s: rc=%d %s\n", camHost, rc, ohcmResultCodeLabels[rc]);
    }

    return retVal;
}

