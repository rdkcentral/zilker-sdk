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

/*
 * Created by Thomas Lea on 9/27/19.
 */

#include <deviceService/deviceService_ipc.h>
#include "category.h"
#include "command.h"
#include "eventHandler.h"
#include "util.h"
#include "coreCategory.h"
#include "zigbeeCategory.h"
#include <string.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <propsMgr/paths.h>
#include "linenoise.h"

#define HISTORY_FILE ".xhDeviceUtilHistory"
#define HISTORY_MAX 100
static bool VT100_MODE = true;

static bool showAdvanced = false;

static bool running = true; //Global for now, a command could set to false to terminate interactive session
static icLinkedList *categories = NULL;

static void buildCategories()
{
    categories = linkedListCreate();

    linkedListAppend(categories, buildCoreCategory());
    linkedListAppend(categories, buildZigbeeCategory());
}

static void destroyCategories()
{
    linkedListDestroy(categories, (linkedListItemFreeFunc) categoryDestroy);
}

static void showInteractiveHelp(bool isInteractive)
{
    icLinkedListIterator *it = linkedListIteratorCreate(categories);
    while (linkedListIteratorHasNext(it) == true)
    {
        Category *category = (Category *) linkedListIteratorGetNext(it);
        categoryPrint(category, isInteractive, showAdvanced);
    }
    linkedListIteratorDestroy(it);
}

/*
 * Here we are guaranteed to have at least 1 argument
 */
static bool handleCommand(int argc,
                          char **args)
{
    bool result = false;

    icLinkedListIterator *it = linkedListIteratorCreate(categories);
    while (linkedListIteratorHasNext(it) == true)
    {
        Category *category = (Category *) linkedListIteratorGetNext(it);
        stringToLowerCase(args[0]);
        Command *command = categoryGetCommand(category, args[0]);
        if (command != NULL) //found it
        {
            char **argv = (argc == 1) ? NULL : &(args[1]);
            result = commandExecute(command, argc - 1, argv);
            break;
        }
    }
    linkedListIteratorDestroy(it);

    return result;
}

static Command *findCommand(char *name)
{
    Command *result = NULL;

    icLinkedListIterator *it = linkedListIteratorCreate(categories);
    while (result == NULL && linkedListIteratorHasNext(it) == true)
    {
        Category *category = (Category *) linkedListIteratorGetNext(it);
        stringToLowerCase(name);
        result = categoryGetCommand(category, name);
    }
    linkedListIteratorDestroy(it);

    return result;
}

static bool handleInteractiveCommand(char **args,
                                     int numArgs)
{
    bool result = true;

    if (args != NULL && numArgs > 0)
    {
        // check for a couple of special commands first
        if (stringCompare(args[0], "quit", true) == 0 || stringCompare(args[0], "exit", true) == 0)
        {
            running = false;
        }
        else if (stringCompare(args[0], "help", true) == 0)
        {
            if (numArgs > 1)
            {
                // show help for a command
                Command *command = findCommand(args[1]);
                if (command != NULL)
                {
                    commandPrintUsage(command, true, showAdvanced);
                }
                else
                {
                    printf("Invalid command\n");
                }
            }
            else
            {
                // show full help
                showInteractiveHelp(true);
            }
        }
        else
        {
            Command *command = findCommand(args[0]);
            if (command != NULL)
            {
                char **argv = (numArgs == 1) ? NULL : &(args[1]);
                result = commandExecute(command, numArgs - 1, argv);
            }
            else
            {
                printf("Invalid command\n");
                result = false;
            }
        }
    }

    return result;
}

static void completionCallback(const char *buf, linenoiseCompletions *lc)
{
    icLinkedListIterator *it = linkedListIteratorCreate(categories);
    char *myBuf = strdup(buf);
    while (linkedListIteratorHasNext(it) == true)
    {
        Category *category = (Category *) linkedListIteratorGetNext(it);
        stringToLowerCase(myBuf);

        icLinkedList *completions = categoryGetCompletions(category, myBuf);
        icLinkedListIterator *completionsIt = linkedListIteratorCreate(completions);
        while(linkedListIteratorHasNext(completionsIt))
        {
            char *aCompletion = (char*)linkedListIteratorGetNext(completionsIt);
            linenoiseAddCompletion(lc, aCompletion);
        }
        linkedListIteratorDestroy(completionsIt);
        linkedListDestroy(completions, NULL);
    }
    linkedListIteratorDestroy(it);

    free(myBuf);
}

int main(int argc,
         char **argv)
{
    bool rc = false;
    char *confDir = getDynamicConfigPath();
    char *histFile = stringBuilder("%s/%s", confDir, HISTORY_FILE);

    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_ERROR);

    buildCategories();

    // handle the special option "--waitForService" if present (it must be first)
    if (stringCompare("--waitForService", argv[1], true) == 0)
    {
        waitForServiceAvailable(DEVICESERVICE_IPC_PORT_NUM, 30);

        //remove this from our cmd line
        argc--;
        argv++;
    }

    // handle the special option "--showAdvanced" if present (it must be first after the above)
    if (stringCompare("--showAdvanced", argv[1], true) == 0)
    {
        showAdvanced = true;

        //remove this from our cmd line
        argc--;
        argv++;
    }

    // handle the --help command, then exit
    if (stringCompare("--help", argv[1], true) == 0)
    {
        showInteractiveHelp(false);
        destroyCategories();
        closeIcLogger();
        free(confDir);
        free(histFile);
        return EXIT_SUCCESS;
    }

    if (stringCompare("--novt100", argv[1], true) == 0)
    {
        VT100_MODE = false;

        //remove this from our cmd line
        argc--;
        argv++;
    }

    if (VT100_MODE == true)
    {
        linenoiseSetCompletionCallback(completionCallback);
        linenoiseHistorySetMaxLen(HISTORY_MAX);

        if(confDir != NULL)
        {
            linenoiseHistoryLoad(histFile);
        }
    }

    if (argc == 1) // nothing on the command line so go interactive
    {
        registerEventHandlers();

        while (running)
        {
            int numArgs = 0;
            char *line = NULL;

            if (VT100_MODE == true)
            {
                line = linenoise("deviceUtil> ");

                if (line == NULL)
                {
                    break;
                }

                linenoiseHistoryAdd(line);
            }
            else
            {
                printf("\ndeviceUtil> ");
                fflush(stdout);
                line = getInputLine();

                if (line == NULL)
                {
                    break;
                }
            }

            char **args = getTokenizedInput(line, &numArgs);
            handleInteractiveCommand(args, numArgs);

            for (int i = 0; i < numArgs; i++)
            {
                free(args[i]);
            }
            free(args);
            free(line);
        }

        unregisterEventHandlers();
        rc = true;

        if(confDir != NULL)
        {
            linenoiseHistorySave(histFile);
        }
    }
    else
    {
        // locate and execute the command specified on the command line, skipping argv[0] (this program)
        char **args = &(argv[1]);
        rc = handleCommand(argc - 1, args);
    }

    destroyCategories();

    closeIcLogger();

    free(confDir);
    free(histFile);

    if (rc != true)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}