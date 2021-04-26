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

/*
 * configParser.h
 *
 *  Created on: Apr 4, 2011
 *      Author: gfaulkner
 */

#ifndef CONFIGPARSER_H_
#define CONFIGPARSER_H_

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#include <icTypes/icLinkedList.h>
#include <watchdog/watchdogService_pojo.h>

/*
 * the container object for each service definition
 */
typedef struct
{
    char     *serviceName;              // unique name of the service
    char     *execPath;                 // path to the service executable binary
    char     **execArgs;                // CLI args to pass to the server during startup (optional)
    uint8_t  execArgCount;
    char     *logicalGroup;             // logical group this service is associated with (optional)
    bool     restartOnFail;             // if true, this service will be restarted when death is detected
    bool     expectStartupAck;          // if true, this service should notify watchdog when it is online
    uint32_t secondsBetweenRestarts;
    uint16_t maxRestartsPerMinute;
    uint16_t restartsWithinPastMinute;
    uint16_t actionOnMaxRestarts;
    bool     autoStart;                 // if true, start this service during watchdog initialization
    time_t   lastRestartTime;           // time the service was started last using realtime clock for user display
    time_t   lastRestartTimeMono;       // time the service was started last using monotonic clock for calculations
    time_t   lastActReceivedTime;       // time the service sent the ACK.  will be 0 when launched
    char     *shutdownToken;            // provided by service ACK.  used during shutdown of the process.
    uint32_t waitSecsOnShutdown;        // number of seconds to wait during "shutdown" of this service
    pid_t    currentPid;                // PID of the service.  only valid after launched
    bool     tempIgoreDeath;            // temporarily ignore death do to forced stop/restart
    uint64_t deathCount;                // number of times this died unexpectedly
    uint32_t serviceIpcPort;            // set as part of the "ack" the service sends
    bool     isJavaService;             // not really managed, but can be reported on (stats, status, RMA)
    bool     singlePhaseStartup;
} serviceDefinition;

/**
 * parses the watchdog configuration file and returns a
 * linked list of serviceDefinition objects
 */
icLinkedList *loadServiceConfig(const char *configDir, const char *homeDir);

/*
 * potentially called before reboot to save off a problematic serviceName.
 * this will be loaded during our next loadServiceConfig so that we can tag
 * that process as a misbehaving and prevent it from causing an endless reboot cycle
 */
void saveMisbehavingService(const char *serviceName);

/*
 * queried during startup to see if we saved a problematic serviceName prior to reboot.
 * after reading the file, it will be deleted.  caller MUST free the returned string.
 */
char *readMisbehavingService();

/*
 * frees the memory associated with the supplied definition.
 */
void destroyServiceDefinition(serviceDefinition *def);

#endif /* CONFIGPARSER_H_ */
