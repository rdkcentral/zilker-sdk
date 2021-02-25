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
// Created by tlea on 7/9/19.
//

#ifndef ZILKER_ZIGBEEHEALTHCHECK_H
#define ZILKER_ZIGBEEHEALTHCHECK_H

#include <icBuildtime.h>

/**
 * Start monitoring the zigbee network for health.  It is safe to call this multiple times, such as when a
 * related property changes.
 */
void zigbeeHealthCheckStart();

/**
 * Stop monitoring the zigbee network for health
 */
void zigbeeHealthCheckStop();

/**
 * Set the state of the network health (as reported by zhal)
 *
 * @param problemExists
 */
void zigbeeHealthCheckSetProblem(bool problemExists);

#endif //ZILKER_ZIGBEEHEALTHCHECK_H
