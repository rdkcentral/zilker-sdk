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

//
// Created by mkoch201 on 12/5/18.
//

#ifndef ZILKER_SUBSYSTEMMANAGERCALLBACKS_H
#define ZILKER_SUBSYSTEMMANAGERCALLBACKS_H

/*
 * Callback for when a subsystem has been initialized.
 */
typedef void (*subsystemInitializedFunc)(const char *subsystemName);

/*
 * Callback for when a subsystem is ready to work with devices(pair, etc).
 */
typedef void (*subsystemReadyForDevicesFunc)(const char *subsystemName);

#endif //ZILKER_SUBSYSTEMMANAGERCALLBACKS_H
