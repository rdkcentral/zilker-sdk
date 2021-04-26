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

#include "command.h"
#include <string.h>
#include <icTypes/icLinkedList.h>

struct _Command
{
    char *name;
    char *shortInteractiveName;
    char *argUsage;
    char *help;
    bool isAdvanced;
    int minArgs;
    int maxArgs; // -1 for unlimited
    commandExecFunc func;
    icLinkedList *examples;
};


Command *commandCreate(const char *name,
                       const char *shortInteractiveName,
                       const char *argUsage,
                       const char *help,
                       int minArgs,
                       int maxArgs,
                       commandExecFunc func)
{
    Command *result = calloc(1, sizeof(Command));
    result->name = strdup(name);
    if (shortInteractiveName != NULL)
    {
        result->shortInteractiveName = strdup(shortInteractiveName);
    }
    if (argUsage != NULL)
    {
        result->argUsage = strdup(argUsage);
    }
    result->help = strdup(help);
    result->minArgs = minArgs;
    result->maxArgs = maxArgs;
    result->func = func;

    return result;
}

void commandDestroy(Command *command)
{
    if (command != NULL)
    {
        free(command->name);
        free(command->shortInteractiveName);
        free(command->argUsage);
        free(command->help);
        linkedListDestroy(command->examples, NULL);
        free(command);
    }
}

char *commandGetName(const Command *command)
{
    if (command != NULL)
    {
        return strdup(command->name);
    }
    return NULL;
}

char *commandGetShortName(const Command *command)
{
    if (command != NULL && command->shortInteractiveName != NULL)
    {
        return strdup(command->shortInteractiveName);
    }
    return NULL;
}

void commandSetAdvanced(Command *command)
{
    if (command != NULL)
    {
        command->isAdvanced = true;
    }
}

bool commandExecute(const Command *command,
                    int argc,
                    char **argv)
{
    bool result = false;

    if (command != NULL)
    {
        if (argc >= command->minArgs && (command->maxArgs == -1 || argc <= command->maxArgs))
        {
            result = command->func(argc, argv);
        }
        else
        {
            printf("Invalid args\n");
        }
    }

    return result;
}

void commandAddExample(Command *command,
                       const char *example)
{
    if (command != NULL && example != NULL)
    {
        if (command->examples == NULL)
        {
            command->examples = linkedListCreate();
        }
        linkedListAppend(command->examples, (void *) strdup(example));
    }
}

void commandPrintUsage(const Command *command,
                       bool isInteractive,
                       bool showAdvanced)
{
    if (command != NULL && (command->isAdvanced == false || showAdvanced == true))
    {
        if (isInteractive)
        {
            if (command->shortInteractiveName != NULL)
            {
                printf("\t%s|%s %s : %s\n", command->name,
                       command->shortInteractiveName,
                       command->argUsage == NULL ? "" : command->argUsage,
                       command->help);
            }
            else
            {
                printf("\t%s %s : %s\n",
                       command->name,
                       command->argUsage == NULL ? "" : command->argUsage,
                       command->help);
            }
        }
        else
        {
            printf("\t--%s %s : %s\n",
                   command->name,
                   command->argUsage == NULL ? "" : command->argUsage,
                   command->help);
        }

        if (command->examples != NULL)
        {
            printf("\tExamples:\n");
            icLinkedListIterator *it = linkedListIteratorCreate(command->examples);
            while (linkedListIteratorHasNext(it) == true)
            {
                if (isInteractive == true)
                {
                    printf("\t\t%s\n", (char *) linkedListIteratorGetNext(it));
                }
                else
                {
                    printf("\t\t--%s\n", (char *) linkedListIteratorGetNext(it));
                }
            }
            linkedListIteratorDestroy(it);
            printf("\n");
        }
    }
}