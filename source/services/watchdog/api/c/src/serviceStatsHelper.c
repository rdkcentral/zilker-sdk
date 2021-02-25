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
// Created by jelder380 on 1/7/19.
//

#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
#include <stdio.h>

#include "watchdog/serviceStatsHelper.h"
#include <icLog/logging.h>

#define LOG_TAG "serviceStatsHelper"

// cpu usage keys
#define USER_CPU_USAGE_SECS_KEY "userCpuSec"
#define SYSTEM_CPU_USAGE_SECS_KEY "sysCpuSec"

// memory keys
#define MEM_TOTAL_SIZE_BYTES_KEY "memTotal"
#define MEM_RES_SET_SIZE_BYTES_KEY "memResSet"
#define MEM_SHARE_SIZE_BYTES_KEY "memShare"
#define MEM_TEXT_SIZE_BYTES_KEY "memTxt"
#define MEM_DATA_STACK_SIZE_BYTES_KEY "memData"

// sizes of various strings
#define SIZE_OF_USER_CPU 21
#define SIZE_OF_SYS_CPU 21

// files to look through
#define STATM_FILE_NAME "/proc/self/statm"

// struct for some basic process stats
typedef struct _procMemStats
{
    unsigned long stat_size;
    unsigned long stat_resident;
    unsigned long stat_share;
    unsigned long stat_text;
    unsigned long stat_lib;
    unsigned long stat_data;
    unsigned long stat_dt;
} procMemStats;

// private functions
static bool getMemoryProcessUsage(procMemStats *memStats);

/*
 * Gathers the process stats and adds them to the runtime stats
 */
void collectServiceStats(runtimeStatsPojo *output)
{
    procMemStats memStats;
    int pageSize;
    struct rusage usage;
    char userCPU[SIZE_OF_USER_CPU];
    char systemCPU[SIZE_OF_SYS_CPU];

    icLogInfo(LOG_TAG, "%s: handling collect service process stats", __FUNCTION__);

    // gather the stats
    bool rc = getMemoryProcessUsage(&memStats);

    if (!rc)
    {
        icLogError(LOG_TAG, "%s: unable to get process memory information... Bailing", __FUNCTION__);
        return;
    }

    getrusage(RUSAGE_SELF, &usage);
    pageSize = getpagesize();

    snprintf(userCPU, SIZE_OF_USER_CPU,"%ld.%ld", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec/10000);
    snprintf(systemCPU, SIZE_OF_SYS_CPU,"%ld.%ld", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec/10000);

    // now add them to output
    put_string_in_runtimeStatsPojo(output,USER_CPU_USAGE_SECS_KEY, userCPU);
    put_string_in_runtimeStatsPojo(output,SYSTEM_CPU_USAGE_SECS_KEY,systemCPU);
    put_long_in_runtimeStatsPojo(output,MEM_TOTAL_SIZE_BYTES_KEY,memStats.stat_size*pageSize/1024);
    put_long_in_runtimeStatsPojo(output,MEM_RES_SET_SIZE_BYTES_KEY,memStats.stat_resident*pageSize/1024);
    put_long_in_runtimeStatsPojo(output,MEM_SHARE_SIZE_BYTES_KEY,memStats.stat_share*pageSize/1024);
    put_long_in_runtimeStatsPojo(output,MEM_TEXT_SIZE_BYTES_KEY,memStats.stat_text*pageSize/1024);
    put_long_in_runtimeStatsPojo(output,MEM_DATA_STACK_SIZE_BYTES_KEY,memStats.stat_data*pageSize/1024);

    icLogInfo(LOG_TAG, "%s: done handling collect service process stats",__FUNCTION__);
}

/**
 * Helper function for getting the basic process stats from /proc/self
 *
 * @param memStats - the stats object
 */
static bool getMemoryProcessUsage(procMemStats *memStats)
{
    memset(memStats,0,sizeof(procMemStats));
    FILE *statmFILE = fopen(STATM_FILE_NAME,"r");
    if (statmFILE == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to get process memory stats; unable to open %s",__FUNCTION__,STATM_FILE_NAME);
        return false;
    }

    int rc = fscanf(statmFILE,"%ld %ld %ld %ld %ld %ld %ld",
                    &(memStats->stat_size),&(memStats->stat_resident),&(memStats->stat_share),
                    &(memStats->stat_text),&(memStats->stat_lib),&(memStats->stat_data),(&memStats->stat_dt));
    fclose(statmFILE);

    if (rc != 7)
    {
        icLogError(LOG_TAG, "%s: unable to read file %s", __FUNCTION__,STATM_FILE_NAME);
        return false;
    }

    return true;
}