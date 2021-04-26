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
 * sampleChannel.h
 *
 * sample channel implementation
 *
 * Author: jelderton - 9/10/15
 *-----------------------------------------------*/

#ifndef SAMPLE_CHANNEL_H
#define SAMPLE_CHANNEL_H

#include <stdint.h>
#include <stdbool.h>

#include <icBuildtime.h>
#include <commMgr/commService_pojo.h>
#include "../channel.h"
#include "../message.h"

#define SAMPLE_CHANNEL_NAME        "sample channel"       // for commHostConfig and commChannelStatus

/*
 * create the sample channel object.  will populate the
 * function pointers within the 'channel'
 */
channel *createSampleChannel();

#endif // SAMPLE_CHANNEL_H
