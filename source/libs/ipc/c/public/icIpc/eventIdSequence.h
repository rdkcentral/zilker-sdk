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
 * eventIdSequence.h
 *
 * Support for a 'shared memory' segment that is used
 * to serve as a 'unique eventId sequencer'.  Each process
 * that needs an eventId would use this code to:
 *  - read the current eventId value
 *  - increment by 1
 *  - write the new value
 *  - use the new value
 *
 * Created because we need a way to create a unique
 * identifier to prevent duplicate events from the same CPE.
 * Due to the fact our server expects these identifiers
 * to be sequential, we cannot simply grab a random number.
 *
 * The more complicated part is that we have several services
 * running as independent processes, and need to ensure they
 * don't have to synchronize with one-another OR create
 * duplicate identifiers.
 *
 * In the past, we resolved this by adding a kernel-mod,
 * however, that approach is not portable and cannot
 * be used anymore.
 *
 * Because this is uses a 'shared memory segment', it also
 * creates and uses a System V semaphore to deal with the
 * concurrency of multiple processes trying to obtain and
 * update the global eventId
 *
 * In following with System V conventions on semaphors and
 * shared-mem, something needs to create the constructs and
 * eventually delete them.
 *
 * Author: jelderton - 7/10/15
 *-----------------------------------------------*/

#ifndef IC_EVENT_ID_SEQUENCE_H
#define IC_EVENT_ID_SEQUENCE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * creates the underlying shared memory and
 * semaphore constructs.  should only be called
 * once by a 'master' (such as watchdog)
 */
//extern bool createSharedSegment();

/*
 * destroys the underlying shared memory and
 * semaphore constructs.  should only be called
 * once by a 'master' (such as watchdog)
 */
//extern bool destroySharedSegment();

/*
 * Return the next eventId available for use.
 * this value is global to the entire CPE and
 * is guaranteed to be unique.  Because this
 * has to work across multiple processes, it
 * can take some time to retrieve
 *
 * Will return 0 if unable to obtain an eventId
 */
extern uint64_t getNextEventId();


#endif // IC_EVENT_ID_SEQUENCE_H
