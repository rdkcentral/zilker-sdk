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
 * touchstone.h
 *
 * integration library to allow basic adjustments to
 * the Touchstone system prior to execution and/or activation.
 * intended to be utilized externally to integrate with control
 * systems (without requiring manual intervention via console).
 * utilizes many of the Zilker/Touchstone libraries, so ensure
 * the "rpath" is setup properly when linking against this library.
 *
 * Author: jelderton - 7/1/16
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#ifndef PUBLIC_TOUCHSTONE_H
#define PUBLIC_TOUCHSTONE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * returns if Touchstone is currently activated.
 */
bool touchstoneIsActivated();

/*
 * returns if Touchstone is currently running
 */
bool touchstoneIsRunning();

/*
 * return the hostname of the server Touchstone would activate
 * and communicate with.
 *
 * @return - hostname; caller must release via free() if not NULL
 */
char *touchstoneGetServerHostname();

/*
 * attempts to adjust the hostname of the server Touchstone
 * will activate against.  will be ignored if the system is
 * already activated.
 *
 * @return - true if successful
 */
bool touchstoneSetServerHostname(const char *hostname);

/*
 * attempts to reset Touchstone to factory defaults.
 */
bool touchstoneResetToFactory();

/*
 * restarts all of the Touchstone processes.  note that
 * this will fail if touchstone is not enabled.
 */
bool touchstoneRestart();

#ifdef __cplusplus
}
#endif

#endif // PUBLIC_TOUCHSTONE_H
