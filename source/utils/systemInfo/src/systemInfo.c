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
 * systemInfo.c
 *
 * Command line utility to get or set basic system information
 *
 * Author: jelderton - 12/1/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <icBuildtime.h>
#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include <icSystem/runtimeAttributes.h>
#include <sysinfo/sysinfoHAL.h>

#ifdef CONFIG_SERVICE_NETWORK
#include <networkService/networkService_ipc.h>
#endif

/*
 * private function declarations
 */
void printUsage();

typedef enum
{
    NO_ACTION,
    VERSION_ACTION,
    MANUF_ACTION,
    MODEL_ACTION,
    CPE_ACTION,
    SERIAL_ACTION,
    HW_VER_ACTION,
    MAC_ADDR_ACTION,
#ifdef CONFIG_SERVICE_NETWORK
    GET_SIM_ACTION,
    GET_IMEI_ACTION,
#endif
} actionMode;

/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int c = 0;
    actionMode action = NO_ACTION;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"vmMucshwie")) != -1)
    {
        switch (c)
        {
            case 'v':       // get system "version"
            {
                action = VERSION_ACTION;
                break;
            }
            case 'm':       // model
            {
                action = MODEL_ACTION;
                break;
            }
            case 'u':       // manufacturer
            {
                action = MANUF_ACTION;
                break;
            }
            case 'c':       // CPE Id
            {
                action = CPE_ACTION;
                break;
            }
            case 's':       // serial number
            {
                action = SERIAL_ACTION;
                break;
            }
            case 'w':       // hardware version
            {
                action = HW_VER_ACTION;
                break;
            }
            case 'M':       // MAC Address
            {
                action = MAC_ADDR_ACTION;
                break;
            }
            case 'i':       // SIM
            {
#ifdef CONFIG_SERVICE_NETWORK
                action = GET_SIM_ACTION;
#endif
                break;
            }
            case 'e':       // IMEI
            {
#ifdef CONFIG_SERVICE_NETWORK
                action = GET_IMEI_ACTION;
#endif
                break;
            }

            case 'h':       // help option
            {
                printUsage();
                closeIcLogger();
                return EXIT_SUCCESS;
            }
                        
            default:
            {
                fprintf(stderr,"Unknown option '%c'.  Use '-h' for help.\n",c);
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    // look to see that we have a mode set
    //
    switch(action)
    {
        case NO_ACTION:
            fprintf(stderr, "No operation defined.  Use -h option for usage\n");
            closeIcLogger();
            return EXIT_FAILURE;

        case VERSION_ACTION:
        {
            systemVersion ver;
            if (getSystemVersion(&ver) == true)
            {
                printf("%s\n", ver.serverVersionString);
            }
            else
            {
                fprintf(stderr, "unable to obtain the system version\n");
            }
            break;
        }

        case MODEL_ACTION:
        {
            const char *label = getSystemModelLabel();
            if (label != NULL)
            {
                printf("%s\n", label);
            }
            else
            {
                fprintf(stderr, "unable to obtain the system model number\n");
            }
            break;
        }

        case MANUF_ACTION:
        {
            const char *label = getSystemManufacturerLabel();
            if (label != NULL)
            {
                printf("%s\n", label);
            }
            else
            {
                fprintf(stderr, "unable to obtain the system manufacturer\n");
            }
            break;
        }

        case SERIAL_ACTION:
        {
            const char *label = getSystemSerialLabel();
            if (label != NULL)
            {
                printf("%s\n", label);
            }
            else
            {
                fprintf(stderr, "unable to obtain the system serial number\n");
            }
            break;
        }

        case HW_VER_ACTION:
        {
            const char *label = getSystemHardwareVersionLabel();
            if (label != NULL)
            {
                printf("%s\n", label);
            }
            else
            {
                fprintf(stderr, "unable to obtain the hardware version number\n");
            }
            break;
        }

        case MAC_ADDR_ACTION:
        {
            // get the mac from the HAL
            //
            char *label = NULL;
            uint8_t halMacAddress[6];
            int rc = hal_sysinfo_get_macaddr(&halMacAddress[0]);
            if (rc == 0)
            {
                label = stringBuilder("%02x:%02x:%02x:%02x:%02x:%02x",
                                           halMacAddress[0],halMacAddress[1],halMacAddress[2],
                                           halMacAddress[3],halMacAddress[4],halMacAddress[5]);
            }

            if (label != NULL)
            {
                printf("%s\n", label);
                free(label);
            }
            else
            {
                fprintf(stderr, "unable to obtain the hardware MAC Address\n");
            }
            break;
        }

        case CPE_ACTION:
        {
            const char *label = getSystemCpeId();
            if (label != NULL)
            {
                printf("%s\n", label);
            }
            else
            {
                fprintf(stderr, "unable to obtain the CPE Id\n");
            }
            break;
        }
#ifdef CONFIG_SERVICE_NETWORK
        case GET_SIM_ACTION:
        {
            char *simId = NULL;
            if (networkService_request_GET_GPRS_SIM_ID(&simId) == IPC_SUCCESS && simId != NULL)
            {
                printf("%s\n", simId);
            }
            else
            {
                fprintf(stderr, "unable to get the SIM Id\n");
            }
            free(simId);
            break;
        }

        case GET_IMEI_ACTION:
        {
            char *imeiId = NULL;
            if (networkService_request_GET_GPRS_IMEI(&imeiId) == IPC_SUCCESS && imeiId != NULL)
            {
                printf("%s\n", imeiId);
            }
            else
            {
                fprintf(stderr, "unable to get the IMEI Id\n");
            }
            free(imeiId);
            break;
        }
#endif
        default:
            break;
    }

    // cleanup
    //
    closeIcLogger();
    return EXIT_SUCCESS;
}

/*
 * show user available options
 */
void printUsage()
{
    fprintf(stderr, "iControl System Information\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  systemInfo [-v | -m | -u | -c | -s | -M | -w | -i | -e]\n");
    fprintf(stderr, "    -v : print system 'version'\n");
    fprintf(stderr, "    -m : print system 'model number'\n");
    fprintf(stderr, "    -u : print system 'manufacturer'\n");
    fprintf(stderr, "    -s : print system 'serial number'\n");
    fprintf(stderr, "    -M : print system 'MAC Address'\n");
    fprintf(stderr, "    -w : print system 'harware version'\n");
    fprintf(stderr, "    -c : print 'CPE Id'\n");
    fprintf(stderr, "    -i : print 'SIM Id'  (if supported)\n");
    fprintf(stderr, "    -e : print 'IMEI Id' (if supported)\n\n");
}


