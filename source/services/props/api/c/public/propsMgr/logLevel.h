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
 * logLevel.h
 *
 * Helper to simplify the tracking and adjustment of granual
 * log levels within the various components of the system.
 *
 * Author: jelderton - 8/31/15
 *-----------------------------------------------*/

#ifndef IC_LOGLEVEL_H
#define IC_LOGLEVEL_H

#include <icLog/logging.h>

/*
 * Uses 'name' to construct a property pattern and
 * ask the propsService for the value, which can be used
 * for setting the log level.  For example:
 *   getCustomLogLevel("commService") would look for the
 *   property "logging.commService"
 *
 * returns 'defVal' if the property is not set
 */
logPriority getCustomLogLevel(const char *name, logPriority defVal);

/*
 * Uses 'name' to construct a property pattern and
 * tell the propsService to set that key to the
 * supplied log level value
 */
void setCustomLogLevel(const char *name, logPriority newLevel);

/*
 * Setup routine to get and set the log level for the given 'name'.
 * This also sets up an eventListener to perform a "setLogPriority"
 * when a "property changed event" occurs with the corresponding key.
 */
void autoAdjustCustomLogLevel(const char *name);

#endif // IC_LOGLEVEL_H
