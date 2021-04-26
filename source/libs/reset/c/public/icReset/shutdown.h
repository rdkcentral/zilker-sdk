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
 * shutdown.h
 *
 * Set of functions available for rebooting the device.
 *
 * Author: jelderton, gfaulkner - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_SHUTDOWN_H
#define IC_SHUTDOWN_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    SHUTDOWN_IGNORE = 0,
    SHUTDOWN_BB_DOWN,
    SHUTDOWN_LOW_MEM,
    SHUTDOWN_SERVER,
    SHUTDOWN_UNKNOWN,
    SHUTDOWN_RECOVERY_FAIL,
    SHUTDOWN_PROCESS_MANAGEMENT,
    SHUTDOWN_KERNEL_PANIC,
    SHUTDOWN_MISSING,       // not saved to a file, just used internally
    SHUTDOWN_RESET		    // not saved to a file, just used internally
} shutdownReason;

/**
 * this must match the enum above and unknown must (currently) always be last
 */
static const char *shutdownReasonNames[] = {
    "ignore",
    "bbDown",
    "lowMem",
    "server",
    "unknown",
    "recoveryFail",
    "processManagement",
    "kernelPanic",
    "missing",      // not used, only here to keep symmetrical
    "reset",		// not used, only here to keep symmetrical
    NULL
};

/*
 * if the CONFIG_LIB_SHUTDOWN option is set:
 *    store the 'reason' to our REBOOT_REASON_FILE, then perform
 *    a system reboot.  This is the preferred mechanism since it
 *    attempts to coordinate with running services so that we
 *    flush caches and preserve states prior to rebooting
 * otherwise:
 *    perform a soft restart of all services via watchdog
 */
void icShutdown(shutdownReason reason);

/*
 * same as 'icShutdown', but performed after the 'delaySecs'
 * has elapsed.
 */
void icDelayedShutdown(shutdownReason reason, uint16_t delaySecs);

/*
 * attempts to read the REBOOT_REASON_FILE and return the
 * contents as an enumeration value.
 *
 * NOTE: after reading this will *delete* the reason file.
 *       to simply check the file without deleting, use the
 *       'peekShutdownReasonCode() function
 *
 *       This will most likely return SHUTDOWN_IGNORE
 *       if the CONFIG_LIB_SHUTDOWN option is NOT SET
 *       or if the reboot reason needs to be ignored.
 *
 *       This will return SHUTDOWN_MISSING if the
 *       reboot reason file does not exist.
 */
shutdownReason getShutdownReasonCode(bool forTelemetry);

/*
 * attempts to read the REBOOT_REASON_FILE and return the
 * contents as an enumeration value.  unlike the
 * getShutdownReasonCode() function, this is NOT destructive
 *
 * NOTE: This will most likely return SHUTDOWN_IGNORE
 *       if the CONFIG_LIB_SHUTDOWN option is NOT SET
 *       or if the reboot reason needs to be ignored.
 *
 *       This will return SHUTDOWN_MISSING if the
 *       reboot reason file does not exist.
 */
shutdownReason peekShutdownReasonCode(bool forTelemetry);

/*
 * Attempts to read the shutdown status file, will use
 * the 'forTelemetry' flag to determine which file to look at.
 *
 * NOTE: after reading this will *delete* the status file.
 *       to simply check the file without deleting, use the
 *       'peekShutdownStatusCode() function
 *
 * NOTE: the shutdown status code from the file,
 *       will return 0 if file does not exist
 */
uint32_t getShutdownStatusCode(bool forTelemetry);
/*
 * Attempts to read the shutdown status file, will use
 * the 'forTelemetry' flag to determine which file to look at.
 * unlike getShutdownStatusCode(), this is not destructive.
 *
 * NOTE: the shutdown status code from the file,
 *       will return 0 if file does not exist
 */
uint32_t peekShutdownStatusCode(bool forTelemetry);

/*
 * converts the supplied string to the shutdownReason enum code.
 * primarily used by CLI utilities when performing a shutdown
 */
shutdownReason getShutdownCodeForString(const char *reason);

/*
 * record a shutdown reason code.
 * Called both internally and externally.
 */
void recordShutdownReason(shutdownReason reason);

/*
 * record a shutdown reason code on start up.
 * Will remove reboot reason file(s) if they exist.
 */
void recordShutdownReasonOnStartUp(shutdownReason reason);

/*
 * Record a Shutdown status code, will rewrite both
 * normal status code file and the telemetry (xconf) file.
 */
void recordShutdownStatusCode(uint32_t statusCode);

#endif // IC_SHUTDOWN_H
