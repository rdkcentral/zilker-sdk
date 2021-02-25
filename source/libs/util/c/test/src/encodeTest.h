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
 * encodeTest.h
 *
 * Compare old encode/decode functions to 3rdParty
 * counterparts.  Eventually this can be removed
 * as it is a litmus test so we can find suitable
 * replacements.
 *
 * Author: jelderton - 4/18/16
 *-----------------------------------------------*/

#ifndef ZILKER_ENCODETEST_H
#define ZILKER_ENCODETEST_H

#include <stdbool.h>

bool runEncodeTests();


#endif //ZILKER_ENCODETEST_H
