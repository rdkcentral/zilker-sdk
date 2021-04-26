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
// Created by John Elderton on 6/18/15.
//


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

//#include <zlog.h>
#include <icLog/logging.h>
#include "linkedTest.h"
#include "sortedTest.h"
#include "hashTest.h"
#include "queueTest.h"
#include "bufferTest.h"
#include "stringBufferTest.h"

/*
 *
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  typesTest <-a|-l|-s|-h| -q|-b|-x>\n");
    fprintf(stderr, "    -a : run all tests\n");
    fprintf(stderr, "    -l : run linkedList test\n");
    fprintf(stderr, "    -s : run sortedList test\n");
    fprintf(stderr, "    -h : run hashMap test\n");
    fprintf(stderr, "    -q : run queue test\n");
    fprintf(stderr, "    -b : run buffer test\n");
    fprintf(stderr, "    -x : run string buffer test\n");
    fprintf(stderr, "\n");
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    int c;
    bool doLinked = false;
    bool doSorted = false;
    bool doHash = false;
    bool doQueue = false;
    bool doBuffer = false;
    bool doStringBuffer = false;

    initIcLogger();

    while ((c = getopt(argc, argv, "alshqbxH")) != -1)
    {
        switch (c)
        {
            case 'a':
                doLinked = true;
                doSorted = true;
                doHash = true;
                doQueue = true;
                doBuffer = true;
                doStringBuffer = true;
                break;

            case 'l':
                doLinked = true;
                break;

            case 's':
                doSorted = true;
                break;

            case 'h':
                doHash = true;
                break;

            case 'q':
                doQueue = true;
                break;

            case 'b':
                doBuffer = true;
                break;
                
            case 'x':
                doStringBuffer = true;
                break;

            case 'H':
            default:
            {
                printUsage();
                closeIcLogger();
                return 1;
            }
        }
    }

    if (doLinked == false && doSorted == false && doHash == false && doQueue == false && doBuffer == false
            && doStringBuffer == false)
    {
        fprintf(stderr, "no options provided, use -H option for help\n");
        closeIcLogger();
        return 1;
    }

    if (doLinked == true)
    {
        printf("\n\nRunning LinkedList Test:\n");
        if (runLinkedTests() == false)
        {
            closeIcLogger();
            return 1;
        }
        printf("LinkedList test SUCCESS!\n");
    }
    if (doSorted == true)
    {
        printf("\n\nRunning SortedList Test:\n");
        if (runSortedTests() == false)
        {
            closeIcLogger();
            return 1;
        }
        printf("SortedList test SUCCESS!\n");
    }
    if (doHash == true)
    {
        printf("\n\nRunning HashMap Test:\n");
        if (runHashTests() == false)
        {
            closeIcLogger();
            return 1;
        }
        printf("HashMap test SUCCESS!\n");
    }
    if (doQueue == true)
    {
        printf("\n\nRunning Queue Test:\n");
        if (runQueueTests() == false)
        {
            closeIcLogger();
            return 1;
        }
        printf("Queue test SUCCESS!\n");
    }
    if (doBuffer == true)
    {
        printf("\n\nRunning Buffer Test:\n");
        if (runBufferTests() == false)
        {
            closeIcLogger();
            return 1;
        }
        printf("Buffer test SUCCESS!\n");
    }
    if (doStringBuffer == true)
    {
        printf("\n\nRunning String Buffer Test:\n");
        if (runStringBufferTests() == false)
        {
            closeIcLogger();
            return 1;
        }
        printf("StringBuffer test SUCCESS!\n");
    }

    closeIcLogger();

    // no errors?
    //
    closeIcLogger();
    return 0;
}


