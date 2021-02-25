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
// Created by jelder380 on 2/15/19.
//

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>

#include <icTime/timeUtils.h>
#include <icLog/logging.h>
#include <icReset/shutdown.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>
#include <propsMgr/paths.h>
#include <icTypes/icLinkedList.h>

#include "common.h"
#include "systemStatsCollector.h"
#include "procMgr.h"
#include "ipcHandler.h"

// whole system keys
#define SYS_MEM_USAGE_KEY "sysMemUse"
#define SYS_LOAD_AVERAGE_KEY "sysLoadAvg"

// system up time key
#define TOTAL_UPTIME_KEY "sysUpTime"

// reboot reason keys
#define REBOOT_SUB_REASON_KEY "wdRebootStatus"
#define REBOOT_REASON_KEY "rebootReason"

// service stat key
#define SERVICE_PROCESS_START_KEY "processStarts"

// files/directories to look through
#define MEM_INFO_FILE_NAME "/proc/meminfo"
#define LOAD_AVG_FILE_NAME "/proc/loadavg"

// struct for some basic system stats
typedef struct
{
    uint32_t mem_total;
    uint32_t mem_free;
    float load_avg_one_min;
    float load_avg_five_min;
    float load_avg_fifteen_min;
    uint32_t load_avg_numerator;
    uint32_t load_avg_denominator;
    uint32_t load_avg_last_proc_id;
} sysMemStats;

// private functions
static bool getSystemMemUsage(sysMemStats *memStats);

/**
 * Function for getting all of the system data
 * and adding it into our runtime stats
 *
 * NOTE: For IPC calls
 *
 * @param output - runtime stats hash map
 */
void collectSystemStats(runtimeStatsPojo *output)
{
    // gather the load average and total system memory usage as percentage
    sysMemStats memStats;
    bool rc = getSystemMemUsage(&memStats);

    // only add load average and memory if it was gathered
    if (rc)
    {
        // convert memory usage into readable values
        float usedMemory = (float) (memStats.mem_total - memStats.mem_free)/(memStats.mem_total) * 100;
        char *usedPercent = stringBuilder("%.2f", usedMemory);

        // convert sys load average into readable values
        char *loadAvg = stringBuilder(
                "%.2f %.2f %.2f "
                "%"PRIu32"/%"PRIu32" %"PRIu32,
                memStats.load_avg_one_min, memStats.load_avg_five_min, memStats.load_avg_fifteen_min,
                memStats.load_avg_numerator, memStats.load_avg_denominator, memStats.load_avg_last_proc_id);

        // add to payload
        put_string_in_runtimeStatsPojo(output,SYS_MEM_USAGE_KEY, usedPercent);
        put_string_in_runtimeStatsPojo(output,SYS_LOAD_AVERAGE_KEY, loadAvg);

        // cleanup
        free(loadAvg);
        free(usedPercent);
    }
    else
    {
        icLogError(WDOG_LOG, "%s: unable to get system memory and load avg information, not adding to stats", __FUNCTION__);
    }

    // gather the total system up time in seconds and add to payload
    time_t sysUptime = getCurrentTime_t(true);
    put_long_in_runtimeStatsPojo(output,TOTAL_UPTIME_KEY, (unsigned long) sysUptime);

    icLogInfo(WDOG_LOG, "%s: done gathering system stats", __FUNCTION__);
}

/**
 * Function for getting reboot stats
 *
 * NOTE: For IPC calls
 *
 * @param output - runtime stats hash map
 */
void collectRebootStats(runtimeStatsPojo *output)
{
    // get the reboot reason && the code
    //
    shutdownReason reason = getShutdownReasonCode(true);
    uint32_t rebootStatusCode = getShutdownStatusCode(true);

    // will get SHUTDOWN_MISSING if reason file no longer exists
    // will get SHUTDOWN_IGNORE if reason needs to be ignored
    //
    if (reason != SHUTDOWN_IGNORE && reason != SHUTDOWN_MISSING)
    {
        // add reboot reason to hash map
        put_string_in_runtimeStatsPojo(output, REBOOT_REASON_KEY, (char *) shutdownReasonNames[reason]);

        // add reboot status code to hash map, as a string converted to the hex value
        char *statusCode = stringBuilder("0x%08"PRIx32, rebootStatusCode);
        put_string_in_runtimeStatsPojo(output, REBOOT_SUB_REASON_KEY, statusCode);

        // cleanup
        free(statusCode);
    }
}

/**
 * Function for getting the stats for all services running
 * from Watchdog
 *
 * NOTE: For IPC calls
 *
 * @param output - runtime stats hash map
 */
void collectServiceListStats(runtimeStatsPojo *output)
{
    // gather all of the process watchdog is gathering
    //
    icLinkedList *services = linkedListCreate();
    getAllServiceProcessInfo(services);

    // and loop through them
    //
    icLinkedListIterator *iter = linkedListIteratorCreate(services);
    while(linkedListIteratorHasNext(iter))
    {
        processInfo *currentService = (processInfo *) linkedListIteratorGetNext(iter);
        if (currentService->serviceName == NULL)
        {
            // skip
            continue;
        }

        // defaulted to 1 for java process
        //
        uint32_t processStartCount = 1;

        // if the process is not a java service
        //
        if (currentService->isJava == false)
        {
            // the process start count is really the death count plus 1
            //
            processStartCount = (uint32_t) currentService->deathCount + 1;
        }

        // add to stats
        //
        char *processStartKey = stringBuilder("%s_%s", currentService->serviceName, SERVICE_PROCESS_START_KEY);
        put_int_in_runtimeStatsPojo(output, processStartKey, processStartCount);

        // cleanup
        free(processStartKey);
    }

    // cleanup
    linkedListIteratorDestroy(iter);
    linkedListDestroy(services, (linkedListItemFreeFunc) destroy_processInfo);
}

/**
 * Helper function for getting the system stats
 *
 * @param memStats - the stats object
 */
static bool getSystemMemUsage(sysMemStats *memStats)
{
    memset(memStats, 0, sizeof(sysMemStats));

    // grab system memory info
    //
    FILE *memFile = fopen(MEM_INFO_FILE_NAME,"r");
    if (memFile == NULL)
    {
        icLogError(WDOG_LOG, "%s: unable to get system memory stats; unable to open %s",__FUNCTION__,MEM_INFO_FILE_NAME);
        return false;
    }

    int rc = fscanf(memFile,"MemTotal:         %"PRIu32" kB\nMemFree:          %"PRIu32" kB", &(memStats->mem_total),&(memStats->mem_free));
    fclose(memFile);

    if (rc != 2)
    {
        icLogError(WDOG_LOG, "%s: unable to read file %s", __FUNCTION__,MEM_INFO_FILE_NAME);
        return false;
    }

    // grab system load average
    //
    FILE *loadAvgFile = fopen(LOAD_AVG_FILE_NAME,"r");
    if (loadAvgFile == NULL)
    {
        icLogError(WDOG_LOG, "%s: unable to get load average stats; unable to open %s",__FUNCTION__,LOAD_AVG_FILE_NAME);
        return false;
    }

    rc = fscanf(loadAvgFile,"%g %g %g %"PRIu32"/%"PRIu32" %"PRIu32, &(memStats->load_avg_one_min), &(memStats->load_avg_five_min),
                &(memStats->load_avg_fifteen_min), &(memStats->load_avg_numerator), &(memStats->load_avg_denominator),
                &(memStats->load_avg_last_proc_id));
    fclose(loadAvgFile);

    if (rc != 6)
    {
        icLogError(WDOG_LOG, "%s: unable to read file %s", __FUNCTION__,LOAD_AVG_FILE_NAME);
        return false;
    }

    return true;
}