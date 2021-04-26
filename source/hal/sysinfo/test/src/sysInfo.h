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

#ifndef _SYSINFOTC200_H_
#define _SYSINFOTC200_H_

#define SERNUM_LENGTH 32

int sysinfo_mac_address(void);
int sysinfo_hw_version(void);
int sysinfo_serial_number(void);
int sysinfo_get_dev_mem_lock(void);
int sysinfo_set_dev_mem_lock(void);

#endif  //_SYSINFOTC200_H_
