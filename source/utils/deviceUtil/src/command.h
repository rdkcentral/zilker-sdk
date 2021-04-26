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

#ifndef ZILKER_COMMAND_H
#define ZILKER_COMMAND_H

#include <icTypes/icLinkedList.h>

typedef bool (*commandExecFunc)(int argc,
                                char *argv[]);

typedef struct _Command Command;

/**
 * Create a command instance.
 *
 * @param name the long name of the command
 * @param shortInteractiveName the optional short name / alias for the command
 * @param argUsage the usage description for any args or NULL if none
 * @param help the usage description for the command
 * @param minArgs the minimum number of arguments for this command
 * @param maxArgs the maximum number of arguments for this command
 * @param func the function to execute when the command is invoked
 *
 * @return the created Command instance
 */
Command *commandCreate(const char *name,
                       const char *shortInteractiveName,
                       const char *argUsage,
                       const char *help,
                       int minArgs,
                       int maxArgs,
                       commandExecFunc func);

/**
 * Destroy a Command instance releasing its resources
 *
 * @param command
 */
void commandDestroy(Command *command);

/**
 * Get the long name of the Command.  Caller frees.
 *
 * @param command
 * @return
 */
char *commandGetName(const Command *command);

/**
 * Get the optional short name of the Command.  Caller frees.
 *
 * @param command
 * @return
 */
char *commandGetShortName(const Command *command);

/**
 * Mark this Command as advanced (it will only appear to the user if advanced mode is enabled)
 *
 * @param command
 */
void commandSetAdvanced(Command *command);

/**
 * Execute a Command.
 *
 * @param command
 * @param argc
 * @param argv
 * @return true if the command succeeded
 */
bool commandExecute(const Command *command, int argc, char **argv);

/**
 * Add an example usage for this Command.  These help the user understand how to use it.
 * @param command
 * @param example
 */
void commandAddExample(Command *command,
                       const char *example);

/**
 * Print the usage of the Command for the user.
 *
 * @param command
 * @param isInteractive true if we are in interactive mode
 * @param showAdvanced true if we are in advanced mode
 */
void commandPrintUsage(const Command *command,
                       bool isInteractive,
                       bool showAdvanced);

#endif //ZILKER_COMMAND_H
