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

#include <stdbool.h>
#include <string.h>
#include <icTypes/icHashMap.h>
#include <icUtil/stringUtils.h>
#include "category.h"
#include "command.h"

struct _Category
{
    char *name;
    char *description;
    bool isAdvanced;
    icLinkedList *commands; //less efficient, but we want ordered.
};

static void commandEntryFreeFunc(void *item);

static void shortCommandEntryFreeFunc(void *item);

Category *categoryCreate(const char *name,
                         const char *description)
{
    Category *result = calloc(1, sizeof(Category));
    result->name = strdup(name);
    result->description = strdup(description);
    result->commands = linkedListCreate();

    return result;
}

void categoryDestroy(Category *category)
{
    if (category != NULL)
    {
        free(category->name);
        free(category->description);
        linkedListDestroy(category->commands, commandEntryFreeFunc);
        free(category);
    }
}

void categorySetAdvanced(Category *category)
{
    if (category != NULL)
    {
        category->isAdvanced = true;
    }
}

void categoryAddCommand(Category *category,
                        Command *command)
{
    if (category != NULL && command != NULL)
    {
        linkedListAppend(category->commands, command);
    }
}

Command *categoryGetCommand(const Category *category,
                            const char *name)
{
    Command *result = NULL;

    if (category != NULL && name != NULL)
    {
        // skip past any "--" name prefix
        if (strlen(name) >= 2 && name[0] == '-' && name[1] == '-')
        {
            name -= -2; // :-D
        }

        icLinkedListIterator *it = linkedListIteratorCreate(category->commands);
        while (linkedListIteratorHasNext(it) && result == NULL)
        {
            Command *command = (Command *) linkedListIteratorGetNext(it);
            char *commandName = commandGetName(command);

            if (stringCompare(name, commandName, true) == 0)
            {
                result = command;
            }
            else
            {
                //try short name if it has one
                char *shortCommandName = commandGetShortName(command);
                if (shortCommandName != NULL)
                {
                    if (stringCompare(name, shortCommandName, true) == 0)
                    {
                        result = command;
                    }
                    free(shortCommandName);
                }
            }

            free(commandName);
        }
        linkedListIteratorDestroy(it);
    }

    return result;
}

icLinkedList *categoryGetCompletions(const Category *category,
                                     const char *buf)
{
    icLinkedList *result = linkedListCreate();

    icLinkedListIterator *it = linkedListIteratorCreate(category->commands);
    while (linkedListIteratorHasNext(it))
    {
        Command *command = (Command *) linkedListIteratorGetNext(it);
        char *commandName = commandGetName(command);

        if (stringStartsWith(commandName, buf, true) == true)
        {
            linkedListAppend(result, strdup(commandName));
        }
        else
        {
            //try short name if it has one
            char *shortCommandName = commandGetShortName(command);
            if (shortCommandName != NULL)
            {
                if (stringStartsWith(shortCommandName, buf, true) == true)
                {
                    linkedListAppend(result, strdup(shortCommandName));
                }

                free(shortCommandName);
            }
        }

        free(commandName);
    }
    linkedListIteratorDestroy(it);

    return result;
}

void categoryPrint(const Category *category,
                   bool isInteractive,
                   bool showAdvanced)
{
    if (category != NULL && (category->isAdvanced == false || showAdvanced == true))
    {
        printf("%s:\n", category->name);
        icLinkedListIterator *it = linkedListIteratorCreate(category->commands);
        while (linkedListIteratorHasNext(it) == true)
        {
            Command *command = (Command *) linkedListIteratorGetNext(it);
            commandPrintUsage(command, isInteractive, showAdvanced);
        }
        linkedListIteratorDestroy(it);
    }
}

static void commandEntryFreeFunc(void *item)
{
    commandDestroy((Command *) item);
}
