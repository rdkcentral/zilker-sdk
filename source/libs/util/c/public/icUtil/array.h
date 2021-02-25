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

#ifndef ZILKER_ARRAY_H
#define ZILKER_ARRAY_H

/**
 * Get the length of an array, in elements
 * @param a an array pointer
 * @example
 * @code
 * my_t arr[] = { memb, memb };
 * array_length(arr) // 2
 * @endcode
 */
#define ARRAY_LENGTH(a) sizeof((a)) / sizeof(*(a))

#endif //ZILKER_ARRAY_H
