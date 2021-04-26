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

/****************************************************************************
*
* @file     sysinfoHW.c
* @brief    Default implementation of the sysInfo HAL.  Used to link against during build
*           so that the correct platform implementation can be installed and used
*           at runtime.
* @date     June 28, 2012
* @Author   jgleason
*
****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysinfo/sysinfoHAL.h>


int hal_sysinfo_get_hwver(char *hwVersion, int hwVersionLen)
{
    return -1;
}

int hal_sysinfo_get_serialnum(char *serialNumber, uint8_t length)
{
    return -1;
}

int hal_sysinfo_get_macaddr(uint8_t macAddress[6])
{
    return -1;
}

bool hal_sysinfo_get_capability(hal_sysinfo_capability_t capability)
{
    return false;
}

int hal_sysinfo_set_capability(hal_sysinfo_capability_t capability)
{
    return -1;
}

int hal_sysinfo_get_manufacturer(char *manufacturer, uint8_t length)
{
    return -1;
}

int hal_sysinfo_get_model(char *model, uint8_t length)
{
    return -1;
}

int hal_sysinfo_get_version()
{
    return 3;
}

int hal_sysinfo_get_rem_dev_info(hal_sysinfo_rem_dev_info** remDevInfo)
{
    return -1;
}
