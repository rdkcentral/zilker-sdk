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

// Ported by dcalde202 on 11/27/18.
// Created by gfaulk200 on 8/10/18.
//

#ifndef CPE_CORE_C_CRONTAB_H
#define CPE_CORE_C_CRONTAB_H

/**
 * add or update a pre-formatted crontab entry in the root crontab file
 * @param entryLine the entire crontab line to add or update
 * @param entryName the name of this entry, for future reference
 * @return 0 on success, non-zero otherwise
 */
int addOrUpdatePreformattedCrontabEntry(const char *entryLine, const char *entryName);

/**
 * remove a crontab entry
 * @param entryName the entry name of the entry to remove
 * @return 0 on success, non-zero otherwise
 */
int removeCrontabEntry(const char *entryName);

/**
 * checks if a crontab entry exists
 * @param entryLine the entire contab line to check if it exists
 * @param entryName the name of this entry
 * @return -1 if the entryName does not exist,
 * 0 if the entryName exists and entryLine is the same,
 * 1 if the entryName exists but entryLine is different
 * -2 if an error occurs
 */
int hasCrontabEntry(const char *entryLine, const char *entryName);


#endif //CPE_CORE_C_CRONTAB_H
