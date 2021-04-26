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
// Created by Christian Leithner on 1/3/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <errno.h>
#include "automationUtil.h"
#include "automationConstants.h"

static void printUsage(void);
static void importAutomation(const char *automationPath, const char *name);
static void assembleAutomation(const char *name);
static void disassembleAutomation(const char *name);
static void disassembleImportedAutomation(const automation *automationContents, const char *automationDir);
static void removeAutomation(const char *name);
static void clearAllAutomations(void);

typedef enum
{
    ACTION_NONE,
    ACTION_IMPORT,
    ACTION_ASSEMBLE,
    ACTION_DISASSEMBLE,
    ACTION_REMOVE,
    ACTION_CLEAR
} actionMode;

/**
 * Main entrypoint for the program
 *
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[])
{
    int c, returnVal = EXIT_SUCCESS;
    actionMode action = ACTION_NONE;
    char *name = NULL;
    char *importPath = NULL;

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"i:n:a:d:r:ch")) != -1)
    {
        switch (c)
        {
            case 'i':       // import
                action = ACTION_IMPORT;
                importPath = strdup(optarg);
                break;
            case 'n':
                name = strdup(optarg);
                break;
            case 'a':       // assemble
                action = ACTION_ASSEMBLE;
                name = strdup(optarg);
                break;
            case 'd':       // disassemble
                action = ACTION_DISASSEMBLE;
                name = strdup(optarg);
                break;
            case 'r':       // remove
                action = ACTION_REMOVE;
                name = strdup(optarg);
                break;
            case 'c':       // clear
                action = ACTION_CLEAR;
                break;
            case 'h':       // help option
                printUsage();
                break;
            default:
                fprintf(stderr,"Unknown option '%c'\n",c);
                returnVal = EXIT_FAILURE;
        }
    }

    switch (action)
    {
        case ACTION_IMPORT:
            if (importPath != NULL)
            {
                importAutomation(importPath, name);
            }
            else
            {
                returnVal = EXIT_FAILURE;
            }
            break;
        case ACTION_ASSEMBLE:
            if (name != NULL)
            {
                assembleAutomation(name);
            }
            else
            {
                returnVal = EXIT_FAILURE;
            }
            break;
        case ACTION_DISASSEMBLE:
            if (name != NULL)
            {
                disassembleAutomation(name);
            }
            else
            {
                returnVal = EXIT_FAILURE;
            }
            break;
        case ACTION_REMOVE:
            if (name != NULL)
            {
                removeAutomation(name);
            }
            else
            {
                returnVal = EXIT_FAILURE;
            }
            break;
        case ACTION_CLEAR:
            clearAllAutomations();
            break;
        case ACTION_NONE:
        default:
            break;
    }

    free(name);
    free(importPath);

    return returnVal;
}

/**
 * Prints the usage of this binary.
 */
static void printUsage(void)
{
    printf("Usage:\n");
    printf("\tautomationUtil [-h] [-i path] [-n name] [-a name] [-d name] [-r name] [-c]\n");
    printf("\t\t-h : prints this usage message.\n");
    printf("\t\t-i : imports a valid automation file at the specified \"<path>\" into " \
           "the working output directory \"out\". This utility will use the name specified by \"name\", " \
           "otherwise will us the filename of the automation provided.\n");
    printf("\t\t-n : Used with -i option. Specifies an optional name to use for the directory of " \
           "an imported automation.\n");
    printf("\t\t-a : assembles an imported automation with the given name. The assembled " \
           "automation will be available in \"<name>/assembled\".\n");
    printf("\t\t-d : disassembles an imported automation with the given name. The disassembled " \
           "automation can be found in \"<name>/disassembled\". Edits made to the disassembled files will "
           "be reflected in the automation upon assembly (see: -a).\n");
    printf("\t\t-r : removes an imported automation with the same name as \"<name>\".\n");
    printf("\t\t-c : clears all imported automations.\n");
}

/**
 * Imports the provided automation found at the path 'automationPath'.
 *
 * @param automationPath - The path to import the automation to
 * @param name - An optional name for the imported automation. If NULL, uses the basename of automationPath
 */
static void importAutomation(const char *automationPath, const char *name)
{
    if (automationPath == NULL)
    {
        fprintf(stderr, "Automation path not supplied! Can't import\n");
        return;
    }

    automation *automationContents = createAutomation();
    char *automationDirName = (char *) name;

    // Make sure the file exists and isn't a directory
    if (doesNonEmptyFileExist(automationPath) == true && doesDirExist(automationPath) == false)
    {
        char *fileContents = readFileContents(automationPath);
        if (fileContents != NULL)
        {
            // Try to parse the file given by the path "automationPath" into an automation struct. This will validate
            // the contents of the file as a valid automation. We want to do this rather than lazily copy the file contents
            // to a local copy.
            if (parseFileToAutomation(fileContents, automationContents) == true)
            {
                // Figure out the name to use for the imported automation. If one is given, use that. Else use the file name
                if (name == NULL)
                {
                    automationDirName = basename(automationPath);
                }

                // Create the directory for the new automation (name it the same as the automation file name
                char *outDirPath = getOutPath();
                if (outDirPath != NULL)
                {
                    char *pathToImportedCopy = stringBuilder("%s/%s", outDirPath, automationDirName);
                    if (pathToImportedCopy != NULL)
                    {
                        if (doesDirExist(pathToImportedCopy) == true)
                        {
                            fprintf(stderr, "Automation %s already exists! Not importing.", automationDirName);
                        }
                        else
                        {
                            if (mkdir_p(pathToImportedCopy, 0775) == 0)
                            {
                                // Write the automation to a local copy in the "out" dir.
                                if (writeAutomationToFile(automationContents, pathToImportedCopy, AUTOMATION_UTIL_ORIG_AUTOMATION_FILENAME) == false)
                                {
                                    fprintf(stderr, "Couldn't import automation: %s\n", automationDirName);
                                    rmdir(pathToImportedCopy);
                                }

                                free(pathToImportedCopy);
                            }
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Couldn't create directory for imported automation: %s\n",
                                stringCoalesce(pathToImportedCopy));
                    }

                    free(outDirPath);
                }
                else
                {
                    fprintf(stderr, "Wasn't able to get the automation utility output directory.");
                }
            }
            else
            {
                fprintf(stderr, "Unable to import automation at %s.", automationPath);
            }

            free(fileContents);
        }
    }
    else
    {
        fprintf(stderr, "File %s doesn't exist or is a directory", automationPath);
    }

    // mem cleanup
    destroyAutomation(automationContents);
}

/**
 * Assembles an imported automation by the given name
 *
 * @param name - The name of the imported automation to assemble
 */
static void assembleAutomation(const char *name)
{
    if (name == NULL)
    {
        fprintf(stderr, "Automation name not supplied! Can't assemble\n");
        return;
    }

    char *outDir = getOutPath();
    if (outDir != NULL)
    {
        char *automationPath = stringBuilder("%s/%s", outDir, name);
        if (automationPath != NULL)
        {
            if (doesDirExist(automationPath) == true)
            {
                char *disassembledDir = stringBuilder("%s%s", automationPath, URI_DISASSEMBLED_DIR);

                if (disassembledDir != NULL)
                {
                    char *assemblyDir = stringBuilder("%s%s", automationPath, URI_ASSEMBLED_DIR);

                    if (assemblyDir != NULL)
                    {
                        if (mkdir_p(assemblyDir, 0775) == 0)
                        {
                            assembleAndWriteAutomation(assemblyDir, disassembledDir);
                        }

                        free(assemblyDir);
                    }

                    free(disassembledDir);
                }
            }
            else
            {
                fprintf(stderr,"Automation %s not found. Have you imported it?", name);
            }

            free(automationPath);
        }

        free(outDir);
    }
}

/**
 * Disassembles an imported automation with the given name
 *
 * @param name - The name of the imported automation to disassemble
 */
static void disassembleAutomation(const char *name)
{
    if (name == NULL)
    {
        fprintf(stderr, "Automation name not supplied! Can't disassemble\n");
        return;
    }

    char *outDir = getOutPath();
    if (outDir != NULL)
    {
        char *automationPath = stringBuilder("%s/%s", outDir, name);
        if (automationPath != NULL)
        {
            if (doesDirExist(automationPath) == true)
            {
                char *pathToOrigAutomation = stringBuilder("%s/%s", automationPath, AUTOMATION_UTIL_ORIG_AUTOMATION_FILENAME);
                if (pathToOrigAutomation != NULL)
                {
                    if (doesNonEmptyFileExist(pathToOrigAutomation) == true)
                    {
                        char *fileContents = readFileContents(pathToOrigAutomation);
                        if (fileContents != NULL)
                        {
                            automation *automationContents = createAutomation();
                            if (parseFileToAutomation(fileContents, automationContents) == true)
                            {
                                disassembleImportedAutomation(automationContents, automationPath);
                            }

                            destroyAutomation(automationContents);
                            free(fileContents);
                        }
                    }
                    else
                    {
                        fprintf(stderr, "Original automation for %s not found. You may need to delete and " \
                                        "reimport it.", name);
                    }

                    free(pathToOrigAutomation);
                }
            }
            else
            {
                fprintf(stderr,"Automation %s not found. Have you imported it?", name);
            }

            free(automationPath);
        }

        free(outDir);
    }
}

/**
 * Disassembles the given automation into a created disassmbled directory inside the provided automationDir path.
 *
 * @param automationContents
 * @param automationDir
 */
static void disassembleImportedAutomation(const automation *automationContents, const char *automationDir)
{
    if (automationContents == NULL)
    {
        fprintf(stderr, "Can't disassemble automation; automation is NULL");
        return;
    }

    if (automationDir == NULL)
    {
        fprintf(stderr, "Can't disassemble automation; automationDir not provided");
        return;
    }

    char *disassembledDirectory = stringBuilder("%s%s", automationDir, URI_DISASSEMBLED_DIR);
    if (disassembledDirectory != NULL)
    {
        if (mkdir_p(disassembledDirectory, 0775) == 0)
        {
            if (disassembleAndWriteAutomation(automationContents, disassembledDirectory) == false)
            {
                fprintf(stderr, "Couldn't disassemble automation");
            }
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            fprintf(stderr, "Couldn't create directory %s : %s", disassembledDirectory, errStr);
            free(errStr);
        }

        free(disassembledDirectory);
    }
}

/**
* Removes an imported automation with the given name
*
* @param name - The name of the imported automation to remove
*/
static void removeAutomation(const char *name)
{
    if (name == NULL)
    {
        fprintf(stderr, "Automation name not supplied! Can't remove\n");
        return;
    }

    char *outDir = getOutPath();
    if (outDir != NULL)
    {
        char *automationPath = stringBuilder("%s/%s", outDir, name);
        if (automationPath != NULL)
        {
            if (doesDirExist(automationPath) == true)
            {
                deleteDirectory(automationPath);
            }
            else
            {
                fprintf(stderr,"Automation %s not found. Have you imported it?", name);
            }

            free(automationPath);
        }

        free(outDir);
    }
}

/**
 * Clears all imported automations
 */
static void clearAllAutomations(void)
{
    char *outDir = getOutPath();
    if (outDir != NULL)
    {
        if (listDirectory(outDir, deleteDirHandler, NULL) != 0)
        {
            char *strError = strerrorSafe(errno);
            fprintf(stderr, "Failed to clear out directory: %s", strError);
            free(strError);
        }

        free(outDir);
    }
}