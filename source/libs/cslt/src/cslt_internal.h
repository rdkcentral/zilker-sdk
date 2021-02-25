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
// Created by Boyd, Weston on 4/17/18.
//

#ifndef ZILKER_CSLT_INTERNAL_H
#define ZILKER_CSLT_INTERNAL_H

#include <stdint.h>
#include <cslt/cslt.h>

#ifndef unused
#define unused(v) ((void) (v))
#endif

#ifndef unlikely
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) (x)
#endif
#endif

#define FACTORY_HASHMAP_KEYLEN(key) ((uint16_t) strlen(key))
#define SHEEN_MSGSIZE SIZE_MAX

void cslt_register_factory(cslt_factory_t* factory);
void cslt_register_transcoder(const cslt_transcoder_t* transcoder);

#endif //ZILKER_CSLT_INTERNAL_H
