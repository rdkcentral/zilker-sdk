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
 * commServiceCommon.h
 *
 * Set of common values used throughout the
 * CommService source files.
 *
 * Author: jelderton - 7/6/15
 *-----------------------------------------------*/

#ifndef COMM_COMMON_H
#define COMM_COMMON_H

#include <icBuildtime.h>        // possible 'verbose' flags set
#include <icLog/logging.h>

/*
 * logging "category name" used within the service
 * happens to match the one used in the "code generation"
 * @see commService_ipc_handler.c
 */
#define COMM_LOG     "commService"

#endif // COMM_COMMON_H
