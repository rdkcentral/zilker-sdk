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
// Created by Christian Leithner on 1/5/20.
//

#ifndef ZILKER_AUTOMATIONUTIL_H
#define ZILKER_AUTOMATIONUTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    DATA_FORMAT_UNKNOWN,
    DATA_FORMAT_JSON,
    DATA_FORMAT_XML
} dataFormat;

typedef enum {
    SPECIFICATION_UNKNOWN,
    SPECIFICATION_SHEENS,
    SPECIFICATION_LEGACY
} specificationType;

typedef struct {
    dataFormat specDataFormat;
    specificationType specType;
    char *specificationContents;
} specification;

typedef struct {
    bool enabled;
    time_t dataCreated;
    uint64_t consumedCount;
    uint64_t emittedCount;
    uint32_t transcoderVersion;
} automationMetadata;

typedef struct {
    specification *spec;
    specification *origSpec;
    automationMetadata *metadata;
    dataFormat automationDataFormat;
} automation;

/**
 * Gets the absolute path to the base zilker repository. Caller should free memory.
 *
 * @return
 */
char *getZilkerBasePath(void);

/**
 * Gets the absolute path to the base automation utility tool. Caller should free memory.
 *
 * @return
 */
char *getBaseUtilityPath(void);

/**
 * Gets the absolute path to the automation utility out directory (where imported automation are stored). Caller
 * should free memory.
 *
 * @return
 */
char *getOutPath(void);

/**
 * Creates an automation type.
 *
 * @return
 */
automation *createAutomation(void);

/**
 * Destroy a given automation type.
 *
 * @param automationContents
 */
void destroyAutomation(automation *automationContents);

/**
 * Parse a full automation file's contents into an in-memory automation representation.
 *
 * @param fileContents - A string representation of an automation file's contents
 * @param automationContents - An automation struct to populate. Must already be allocated!
 * @return - True on successful automation structure population, false otherwise.
 */
bool parseFileToAutomation(const char *fileContents, automation *automationContents);

/**
 * Writes an in-memory representation of an automation to a new file whose absolute path is <path/filename>.
 *
 * @param automationContents - The automation desired to write to file
 * @param path - The path to the directory containing the automation text file
 * @param filename - The name of the automation text file
 * @return - True if the automation is successfully written to the specified file, false otherwise.
 */
bool writeAutomationToFile(const automation *automationContents, const char *path, const char *filename);

/**
 * Disassembles an in-memory automation and writes its parts into a various files. The directory tree of the
 * disassembled automation will be similar to the tree structure of the automation itself (although metadata is grouped
 * into its own directory/file).
 *
 * @param automationContents - The automation to be disassembled
 * @param path - The uri path of the disassembly directory
 * @return
 */
bool disassembleAndWriteAutomation(const automation *automationContents, const char *path);

/**
 * Attempts to assemble a disassembled automation into an outputted automation file. Will incorporate any changes made
 * to the dissassembled automation. The output automation file will be placed in the assemblyPath provided.
 *
 * @param assemblyPath - The location of the directory to place the assembled automation file
 * @param disassemblyPath - The directory of the disassembled automation to rebuild
 * @return True upon successfully assembling the automation and writing it to file, false otherwise
 */
bool assembleAndWriteAutomation(const char *assemblyPath, const char *disassemblyPath);

#endif //ZILKER_AUTOMATIONUTIL_H
