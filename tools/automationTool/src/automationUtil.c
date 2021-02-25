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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include <cjson/cJSON.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/parser.h>
#include <errno.h>
#include <icUtil/fileUtils.h>
#include <jsonHelper/jsonHelper.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <dirent.h>
#include "automationUtil.h"
#include "automationConstants.h"

// The data format of automation files
static dataFormat automationDataFormat = DATA_FORMAT_JSON;

// Struct creation/destruction functions
static automationMetadata *createAutomationMetadata(void);
static void destroyAutomationMetadata(automationMetadata *metadata);
static specification *createSpecification(void);
static void destroySpecification(specification *spec);

// Automation conversion functions
static bool parseJsonToAutomation(const cJSON *jsonData, automation *automationContents);
static char *parseAutomationToString(const automation *automationContents);
static cJSON *parseAutomationToJson(const automation *automationContents);
static cJSON *parseAutomationMetadataToJson(const automationMetadata *metadata);
static automationMetadata *parseJsonToAutomationMetadata(const cJSON *metadataJson);

// Helper functions for getting spec info from string representation
static specificationType getSpecType(const char *specification);
static dataFormat getSpecDataFormat(const char *specification);

// Disassembly helper functions
static bool disassembleAutomationMetadata(const automationMetadata *metadata, const char *path);
static bool disassembleAutomationMetadataJSON(const cJSON *metadata, const char *path);

static bool disassembleAutomationSpecifications(const automation *automationContents, const char *path);
static bool disassembleSpecification(const specification *spec, const char *path);
static bool disassembleSpecificationSheens(const cJSON *specJson, const char *path);
static bool disassembleSpecificationLegacy(xmlDocPtr specXml, const char *path);

// Assembly helper functions
static automationMetadata *assembleAutomationMetadata(const char *disassemblyPath);

static specification *assembleSpecification(const char *disassemblyPath);
static specification *assembleSpecificationSheens(const char *disassemblyPath);
static specification *assembleSpecificationLegacy(const char *disassemblyPath);

static int handleSheensSpecificationDirectory(const char* pathname, const char* dname, unsigned char dtype, void* private);

// Formatting/Unformatting javascript functions
static char *prettyPrintJavascript(const char *javascriptSource);
static char *formatPrettyJavascript(const char *prettyJavascript);

/**
 * Gets the absolute path to the base zilker repository. Caller should free memory.
 *
 * @return
 */
char *getZilkerBasePath(void)
{
    char *retVal = NULL;
    const char *zilkerTopDir = getenv(ENV_KEY_ZILKER_SDK_TOP);
    if (zilkerTopDir != NULL)
    {
        retVal = strdup(zilkerTopDir);
    }
    else
    {
        fprintf(stderr, "Could not get zilker directory. Make sure you have your " \
                        "%s environment variable set!\n", ENV_KEY_ZILKER_SDK_TOP);
    }

    return retVal;
}

/**
 * Gets the absolute path to the base automation utility tool. Caller should free memory.
 *
 * @return
 */
char *getBaseUtilityPath(void)
{
    char *retVal = NULL;
    char *zilkerTopDir = getZilkerBasePath();

    if (zilkerTopDir != NULL)
    {
        retVal = stringBuilder("%s%s", zilkerTopDir, URI_AUTOMATION_UTIL_DIR);

        free(zilkerTopDir);
    }

    return retVal;
}

/**
 * Gets the absolute path to the automation utility out directory (where imported automation are stored). Caller
 * should free memory.
 *
 * @return
 */
char *getOutPath(void)
{
    char *retVal = NULL;
    char *baseUtilityPath = getBaseUtilityPath();

    if (baseUtilityPath != NULL)
    {
        retVal = stringBuilder("%s%s", baseUtilityPath, URI_OUT_DIR);

        free(baseUtilityPath);
    }

    return retVal;
}

/**
 * Creates an automation type.
 *
 * @return
 */
automation *createAutomation(void)
{
    automation *retVal = calloc(1, sizeof(automation));

    return retVal;
}

/**
 * Destroy a given automation type.
 *
 * @param automationContents
 */
void destroyAutomation(automation *automationContents)
{
    if (automationContents != NULL)
    {
        destroyAutomationMetadata(automationContents->metadata);
        destroySpecification(automationContents->spec);
        destroySpecification(automationContents->origSpec);
    }
    free(automationContents);
}

/**
 * Parse a full automation file's contents into an in-memory automation representation.
 *
 * @param fileContents - A string representation of an automation file's contents
 * @param automationContents - An automation struct to populate. Must already be allocated!
 * @return - True on successful automation structure population, false otherwise.
 */
bool parseFileToAutomation(const char *fileContents, automation *automationContents)
{
    bool retVal = false;

    if (fileContents != NULL && automationContents != NULL)
    {
        cJSON *fileContentsJson = NULL;
        // First determine the data format of the automation
        switch (automationDataFormat)
        {
            // We only use json for automations for now.
            case DATA_FORMAT_JSON:
                // Verify the file contents are json
                fileContentsJson = cJSON_Parse(fileContents);
                if (cJSON_IsNull(fileContentsJson) == false)
                {
                    // Try to populate our automation
                    retVal = parseJsonToAutomation(fileContentsJson, automationContents);
                }
                else
                {
                    fprintf(stderr, "Automation data format and file data format do not match!\n");
                }
                cJSON_Delete(fileContentsJson);
                break;
            case DATA_FORMAT_XML:
            case DATA_FORMAT_UNKNOWN:
            default:
                fprintf(stderr, "Unsupported automation data formation %d\n", automationDataFormat);
                break;
        }
    }

    return retVal;
}

/**
 * Writes an in-memory representation of an automation to a new file whose absolute path is <path/filename>.
 *
 * @param automationContents - The automation desired to write to file
 * @param path - The path to the directory containing the automation text file
 * @param filename - The name of the automation text file
 * @return - True if the automation is successfully written to the specified file, false otherwise.
 */
bool writeAutomationToFile(const automation *automationContents, const char *path, const char *filename)
{
    bool retVal = false;

    if (automationContents == NULL)
    {
        fprintf(stderr, "Automation is null!\n");
        return retVal;
    }

    if (path == NULL)
    {
        fprintf(stderr, "Path is null!\n");
        return retVal;
    }

    if (filename == NULL)
    {
        fprintf(stderr, "Filename is null!\n");
        return retVal;
    }

    // Parse out the automation to a string
    char *fileContents = parseAutomationToString(automationContents);
    if (fileContents != NULL)
    {
        char *fullFilename = stringBuilder("%s/%s", path, filename);

        if (fullFilename != NULL)
        {
            // Try to create the file and write it the automation to it
            FILE *file = fopen(fullFilename, "w");
            if (file != NULL)
            {
                size_t len = strlen(fileContents);

                // See if we successfully wrote the whole automation
                retVal = (fwrite(fileContents, 1, len, file) == len);
                if (retVal == false)
                {
                    fprintf(stderr, "Couldn't write automation contents to file %s\n", fullFilename);
                }
                fclose(file);
            }
            else
            {
                char *errStr = strerrorSafe(errno);
                fprintf(stderr, "Couldn't open file %s - error: %s\n", fullFilename, errStr);
                free(errStr);
            }
            free(fullFilename);
        }

        free(fileContents);
    }

    return retVal;
}

/**
 * Disassembles an in-memory automation and writes its parts into a various files. The directory tree of the
 * disassembled automation will be similar to the tree structure of the automation itself (although metadata is grouped
 * into its own directory/file).
 *
 * @param automationContents - The automation to be disassembled
 * @param path - The uri path of the disassembly directory
 * @return
 */
bool disassembleAndWriteAutomation(const automation *automationContents, const char *path)
{
    bool retVal = false;

    if (automationContents == NULL)
    {
        fprintf(stderr, "Automation provided is null\n");
        return retVal;
    }

    if (path == NULL)
    {
        fprintf(stderr, "Disassembly path provided is null\n");
        return retVal;
    }

    // First try to disassmble the metadata
    if (disassembleAutomationMetadata(automationContents->metadata, path) == true)
    {
        // Then the specifications
        if (disassembleAutomationSpecifications(automationContents, path) == true)
        {
            retVal = true;
        }
        else
        {
            fprintf(stderr, "Couldn't disassemble automation specifications\n");
        }
    }
    else
    {
        fprintf(stderr, "Couldn't disassemble automation metadata\n");
    }

    return retVal;
}

/**
 * Attempts to assemble a disassembled automation into an outputted automation file. Will incorporate any changes made
 * to the dissassembled automation. The output automation file will be placed in the assemblyPath provided.
 *
 * @param assemblyPath - The location of the directory to place the assembled automation file
 * @param disassemblyPath - The directory of the disassembled automation to rebuild
 * @return True upon successfully assembling the automation and writing it to file, false otherwise
 */
bool assembleAndWriteAutomation(const char *assemblyPath, const char *disassemblyPath)
{
    bool retVal = false;

    if (assemblyPath == NULL)
    {
        fprintf(stderr, "Assembly path provided is null\n");
        return retVal;
    }

    if (disassemblyPath == NULL)
    {
        fprintf(stderr, "Disassembly path provided is null\n");
        return retVal;
    }

    // First form the automation in memory
    automation *automationContents = createAutomation();
    automationContents->metadata = assembleAutomationMetadata(disassemblyPath);

    char *specPath = stringBuilder("%s%s", disassemblyPath, URI_ORIG_SPECIFICATION_DIR);
    if (specPath != NULL)
    {
        automationContents->origSpec = assembleSpecification(specPath);
        free(specPath);
    }

    specPath = stringBuilder("%s%s", disassemblyPath, URI_SPECIFICATION_DIR);
    if (specPath != NULL)
    {
        automationContents->spec = assembleSpecification(specPath);
        free(specPath);
    }

    automationContents->automationDataFormat = automationDataFormat;

    retVal = writeAutomationToFile(automationContents, assemblyPath, AUTOMATION_UTIL_ASSEMBLED_FILENAME);

    destroyAutomation(automationContents);

    return retVal;
}

/*
 *
 *
 * Internal helper functions below
 *
 *
 */

/**
 * Creates an automationMetadata type.
 *
 * @return
 */
static automationMetadata *createAutomationMetadata(void)
{
    automationMetadata *retVal = calloc(1, sizeof(automationMetadata));

    return retVal;
}

/**
 * Destroy a given automationMetadata type.
 *
 * @param automationContents
 */
static void destroyAutomationMetadata(automationMetadata *metadata)
{
    free(metadata);
}

/**
 * Creates a specification type.
 *
 * @return
 */
static specification *createSpecification(void)
{
    specification *retVal = calloc(1, sizeof(specification));

    return retVal;
}

/**
 * Destroy a given specification type.
 *
 * @param automationContents
 */
static void destroySpecification(specification *spec)
{
    if (spec != NULL)
    {
        free(spec->specificationContents);
    }
    free(spec);
}

/**
 * Parses the json data given in order to populate the members of automationContents.
 *
 * @param jsonData
 * @param automationContents
 *
 * @return true if the automation was successfully parsed, false otherwise.
 */
static bool parseJsonToAutomation(const cJSON *jsonData, automation *automationContents)
{
    if (cJSON_IsNull(jsonData) == true)
    {
        fprintf(stderr, "json data is null!\n");
        return false;
    }

    if (automationContents == NULL)
    {
        fprintf(stderr, "automation is NULL\n");
        return false;
    }

    automationMetadata *metadata = automationContents->metadata;
    specification *origSpec = automationContents->origSpec;
    specification *spec = automationContents->spec;

    cJSON *element = NULL;
    const char *specStr = NULL;

    // First populate metadata
    if (metadata == NULL)
    {
        metadata = createAutomationMetadata();
        automationContents->metadata = metadata;
    }

    element = cJSON_GetObjectItem(jsonData, JSON_KEY_ENABLED);
    if (cJSON_IsNull(element) == false && cJSON_IsBool(element) == true)
    {
        metadata->enabled = cJSON_IsTrue(element);
    }
    element = cJSON_GetObjectItem(jsonData, JSON_KEY_DATE_CREATED);
    if (cJSON_IsNull(element) == false && cJSON_IsNumber(element) == true)
    {
        metadata->dataCreated = element->valueint;
    }
    element = cJSON_GetObjectItem(jsonData, JSON_KEY_CONSUMED_COUNT);
    if (cJSON_IsNull(element) == false && cJSON_IsNumber(element) == true)
    {
        metadata->consumedCount = element->valueint;
    }
    element = cJSON_GetObjectItem(jsonData, JSON_KEY_EMITTED_COUNT);
    if (cJSON_IsNull(element) == false && cJSON_IsNumber(element) == true)
    {
        metadata->emittedCount = element->valueint;
    }
    element = cJSON_GetObjectItem(jsonData, JSON_KEY_TRANSCODER_VERSION);
    if (cJSON_IsNull(element) == false && cJSON_IsNumber(element) == true)
    {
        metadata->transcoderVersion = element->valueint;
    }

    // Next the original spec
    element = cJSON_GetObjectItem(jsonData, JSON_KEY_ORIG_SPEC);
    if (cJSON_IsNull(element) == false && cJSON_IsString(element) == true)
    {
        specStr = cJSON_GetStringValue(element);
        specificationType specType = getSpecType(specStr);
        dataFormat specDataFormat = getSpecDataFormat(specStr);

        if (origSpec == NULL)
        {
            origSpec = createSpecification();
            automationContents->origSpec = origSpec;
        }

        origSpec->specType = specType;
        origSpec->specDataFormat = specDataFormat;
        origSpec->specificationContents = strdup(specStr);
    }

    // Finally the specification itself
    element = cJSON_GetObjectItem(jsonData, JSON_KEY_SPEC);
    if (cJSON_IsNull(element) == false && cJSON_IsString(element) == true)
    {
        specStr = cJSON_GetStringValue(element);
        specificationType specType = getSpecType(specStr);
        dataFormat specDataFormat = getSpecDataFormat(specStr);

        if (spec == NULL)
        {
            spec = createSpecification();
            automationContents->spec = spec;
        }

        spec->specType = specType;
        spec->specDataFormat = specDataFormat;
        spec->specificationContents = strdup(specStr);
    }

    return true;
}

/**
 * Get the specification type of the provided string representation of a specification.
 *
 * @param specification - The string contents of the specification
 * @return - The specificationType of the specification provided
 */
static specificationType getSpecType(const char *specification)
{
    specificationType retVal = SPECIFICATION_UNKNOWN;

    if (specification != NULL)
    {
        dataFormat specDataFormat = getSpecDataFormat(specification);

        switch (specDataFormat)
        {
            case DATA_FORMAT_XML:
                // Legacy is the only spec type that currently uses XML
                retVal = SPECIFICATION_LEGACY;
                break;
            case DATA_FORMAT_JSON:
                // Sheens is the only spec type that currently uses JSON
                retVal = SPECIFICATION_SHEENS;
                break;
            case DATA_FORMAT_UNKNOWN:
            default:
                break;
        }
    }

    return retVal;
}

/**
 * Get the specification data format of the provided string representation of a specification.
 *
 * @param specification - The string contents of the specification
 * @return - The data format of the specification provided
 */
static dataFormat getSpecDataFormat(const char *specification)
{
    dataFormat retVal = DATA_FORMAT_UNKNOWN;

    if (specification != NULL)
    {
        cJSON *jsonData = NULL;
        xmlDocPtr xmlData = NULL;

        // See if we can successfully parse the spec into JSON.
        if ((jsonData = cJSON_Parse(specification)) != NULL)
        {
            retVal = DATA_FORMAT_JSON;
            // Free our temp memory
            cJSON_Delete(jsonData);
        }
        else if ((xmlData = xmlParseMemory(specification, (int) strlen(specification))) != NULL)
        {
            retVal = DATA_FORMAT_XML;
            // Free our temp memory
            xmlFreeDoc(xmlData);
        }
    }

    return retVal;
}

/**
 * Parses an in-memory representation of an automation into a string.
 *
 * @param automationContents - The in-memory struct representation of an automation
 * @return - A string representation of the automation, or NULL on failure.
 */
static char *parseAutomationToString(const automation *automationContents)
{
    char *retVal = NULL;

    if (automationContents == NULL)
    {
        return retVal;
    }

    cJSON *automationJsonData;

    switch (automationDataFormat)
    {
        case DATA_FORMAT_JSON:
            automationJsonData = parseAutomationToJson(automationContents);
            if (cJSON_IsNull(automationJsonData) == false)
            {
                retVal = cJSON_Print(automationJsonData);
            }
            cJSON_Delete(automationJsonData);
            break;
        case DATA_FORMAT_XML:
        case DATA_FORMAT_UNKNOWN:
        default:
            break;
    }

    return retVal;
}

/**
 * Parse an automation into a JSON representation
 *
 * @param automationContents - The automation to parse
 * @return a cJSON pointer to a JSON representation of an automation, or NULL on error.
 */
static cJSON *parseAutomationToJson(const automation *automationContents)
{
    cJSON *retVal = NULL;

    if (automationContents == NULL)
    {
        return retVal;
    }

    retVal = parseAutomationMetadataToJson(automationContents->metadata);
    if (cJSON_IsNull(retVal) == false)
    {
        // Append the specification
        if (automationContents->spec != NULL && automationContents->spec->specificationContents != NULL)
        {
            cJSON_AddStringToObject(retVal, JSON_KEY_SPEC, automationContents->spec->specificationContents);
        }

        // And the original specification
        if (automationContents->origSpec != NULL && automationContents->origSpec->specificationContents != NULL)
        {
            cJSON_AddStringToObject(retVal, JSON_KEY_ORIG_SPEC, automationContents->origSpec->specificationContents);
        }
    }

    return retVal;
}

/**
 * Parse an automation metadata type to a JSON representation
 *
 * @param metadata - The automation metadata to parse
 * @return - A cJSON representation of the automation metadata, or NULL on failure
 */
static cJSON *parseAutomationMetadataToJson(const automationMetadata *metadata)
{
    cJSON *retVal = NULL;

    if (metadata == NULL)
    {
        return retVal;
    }

    retVal = cJSON_CreateObject();
    cJSON_AddBoolToObject(retVal, JSON_KEY_ENABLED, metadata->enabled);
    cJSON_AddNumberToObject(retVal, JSON_KEY_DATE_CREATED, metadata->dataCreated);
    cJSON_AddNumberToObject(retVal, JSON_KEY_CONSUMED_COUNT, metadata->consumedCount);
    cJSON_AddNumberToObject(retVal, JSON_KEY_EMITTED_COUNT, metadata->emittedCount);
    cJSON_AddNumberToObject(retVal, JSON_KEY_TRANSCODER_VERSION, metadata->transcoderVersion);

    return retVal;
}

/**
 * Parses a cJSON representation of metadata into an automation metadata type
 *
 * @param metadataJson - The cJSON representation of metadata to parse
 * @return - An automationMetadata representation of the metadata, or NULL on failure
 */
static automationMetadata *parseJsonToAutomationMetadata(const cJSON *metadataJson)
{
    automationMetadata *retVal = NULL;

    if (cJSON_IsNull(metadataJson) == true)
    {
        return retVal;
    }

    retVal = createAutomationMetadata();
    getCJSONBool(metadataJson, JSON_KEY_ENABLED, &(retVal->enabled));
    getCJSONInt(metadataJson, JSON_KEY_DATE_CREATED, (int *) &(retVal->dataCreated));
    getCJSONInt(metadataJson, JSON_KEY_CONSUMED_COUNT, (int *) &(retVal->consumedCount));
    getCJSONInt(metadataJson, JSON_KEY_EMITTED_COUNT, (int *) &(retVal->emittedCount));
    getCJSONInt(metadataJson, JSON_KEY_TRANSCODER_VERSION, (int *) &(retVal->transcoderVersion));

    return retVal;
}

/**
 * Disassembles an automation metadata and outputs it into a file contained in the provided directory path
 *
 * @param metadata - The automation metadata to disassemble
 * @param path - The path of the directory to write the disassembled metadata to
 * @return - True upon successfully writing the automation metadata to file, false otherwise.
 */
static bool disassembleAutomationMetadata(const automationMetadata *metadata, const char *path)
{
    bool retVal = false;
    if (path == NULL)
    {
        fprintf(stderr, "Path provided is null\n");
        return retVal;
    }

    if (metadata == NULL)
    {
        fprintf(stderr, "Automation metadata provided is null\n");
        return retVal;
    }
    cJSON *metadataJson = NULL;

    switch (automationDataFormat)
    {
        case DATA_FORMAT_JSON:
            metadataJson = parseAutomationMetadataToJson(metadata);
            retVal = disassembleAutomationMetadataJSON(metadataJson, path);
            if (retVal == false)
            {
                fprintf(stderr, "Couldn't disassemble automation metadata\n");
            }

            cJSON_Delete(metadataJson);
            break;
        case DATA_FORMAT_XML:
        case DATA_FORMAT_UNKNOWN:
        default:
            break;
    }

    return retVal;
}

/**
 * Disassembles a cJSON representation of automation metadata and outputs it into a file contained in the provided
 * directory path
 *
 * @param metadata - The automation metadata to disassemble
 * @param path - The path of the directory to write the disassembled metadata to
 * @return - True upon successfully writing the automation metadata to file, false otherwise.
 */
static bool disassembleAutomationMetadataJSON(const cJSON *metadata, const char *path)
{
    bool retVal = false;
    if (path == NULL)
    {
        fprintf(stderr, "Path provided is null \n");
        return retVal;
    }

    if (metadata == NULL)
    {
        fprintf(stderr, "Automation metadata provided is null\n");
        return retVal;
    }

    char *metadataDir = stringBuilder("%s%s", path, URI_METADATA_DIR);
    if (metadataDir != NULL)
    {
        if (mkdir_p(metadataDir, 0775) == 0)
        {
            char *metadataFilename = stringBuilder("%s/%s", metadataDir, AUTOMATION_UTIL_METADATA_FILENAME);
            if (metadataFilename != NULL)
            {
                FILE *filePtr = fopen(metadataFilename, "w");
                if (filePtr != NULL)
                {
                    char *metadataStr = cJSON_Print(metadata);
                    if (metadataStr != NULL)
                    {
                        size_t len = strlen(metadataStr);
                        retVal = (fwrite(metadataStr, 1, len, filePtr) == len);
                        if (retVal == false)
                        {
                            fprintf(stderr, "Failed to write metadata to file");
                        }

                        free(metadataStr);
                    }

                    fclose(filePtr);
                }
                else
                {
                    fprintf(stderr, "Could not create metadata file %s\n", metadataFilename);
                }

                free(metadataFilename);
            }
        }
        else
        {
            fprintf(stderr, "Could not create metadata directory");
        }

        free(metadataDir);
    }

    return retVal;
}

/**
 * Disassembles the original specification and the translated specification contained in the provided automation. Then,
 * writes those disassembled specifications into separate directories contains in the provided directory path.
 *
 * @param automationContents - The automation to pull the specifications from
 * @param path - The path of the directory to write the disassembled metadata to
 * @return - True upon successfully writing the disassembled automation specifications to files, false otherwise.
 */
static bool disassembleAutomationSpecifications(const automation *automationContents, const char *path)
{
    bool retVal = false;
    if (path == NULL)
    {
        fprintf(stderr, "Path provided is null \n");
        return retVal;
    }

    if (automationContents == NULL)
    {
        fprintf(stderr, "Automation provided is null\n");
        return retVal;
    }

    if (automationContents->origSpec != NULL)
    {
        char *origSpecDir = stringBuilder("%s%s", path, URI_ORIG_SPECIFICATION_DIR);
        if (origSpecDir != NULL)
        {
            if (mkdir_p(origSpecDir, 0775) == 0)
            {
                retVal = disassembleSpecification(automationContents->origSpec, origSpecDir);
                if (retVal == false)
                {
                    fprintf(stderr, "Couldn't disassemble original specification\n");
                }
            }
            else
            {
                fprintf(stderr, "Couldn't create original specification directory\n");
            }

            free(origSpecDir);
        }
    }

    if (automationContents->spec != NULL)
    {
        char *specDir = stringBuilder("%s%s", path, URI_SPECIFICATION_DIR);
        if (specDir != NULL)
        {
            if (mkdir_p(specDir, 0775) == 0)
            {
                retVal &= disassembleSpecification(automationContents->spec, specDir);
                if (retVal == false)
                {
                    fprintf(stderr, "Couldn't disassemble specification\n");
                }
            }
            else
            {
                fprintf(stderr, "Couldn't create specification directory\n");
            }

            free(specDir);
        }
    }

    return retVal;
}

/**
 * Disassembles a given specification and then outputs the disassembly into some structure of directory/files contained
 * in the provided path. The structure of the disassembly is dependent on the specification type.
 *
 * @param spec - The specification to disassemble
 * @param path - The path to contain the disassembled specification
 * @return - True upon successfully writing the disassembled automation specification to files, false otherwise.
 */
static bool disassembleSpecification(const specification *spec, const char *path)
{
    bool retVal = false;
    if (path == NULL)
    {
        fprintf(stderr, "Path provided is null \n");
        return retVal;
    }

    if (spec == NULL || spec->specificationContents == NULL)
    {
        fprintf(stderr, "Automation specification provided is null\n");
        return retVal;
    }

    cJSON *specJson = NULL;
    xmlDocPtr specXml = NULL;

    switch (spec->specType)
    {
        case SPECIFICATION_SHEENS:
            specJson = cJSON_Parse(spec->specificationContents);
            if (cJSON_IsNull(specJson) == false)
            {
                retVal = disassembleSpecificationSheens(specJson, path);
                if (retVal == false)
                {
                    fprintf(stderr, "Couldn't disassemble sheens specification");
                }
            }
            cJSON_Delete(specJson);
            break;
        case SPECIFICATION_LEGACY:
            specXml = xmlParseMemory(spec->specificationContents, (int) strlen(spec->specificationContents));
            if (specXml != NULL)
            {
                retVal = disassembleSpecificationLegacy(specXml, path);
                if (retVal == false)
                {
                    fprintf(stderr, "Couldn't disassemble legacy specification");
                }

                xmlFreeDoc(specXml);
            }
            break;
        case SPECIFICATION_UNKNOWN:
        default:
            break;
    }

    return retVal;
}

/**
 * Disassembles a cJSON representation of a sheens specification. The disassembly will then be written into a series of
 * directories/files created/contained in the provided path. Each level in the directory tree will be a JSON object,
 * with the name of the directory being the key for that JSON entry. The files in these nested directories contain a
 * JSON entry to attach to its parent directory JSON element.
 *
 * As an example
 *
 * {
 *      a: "{ \"b\": \"{ \"c\": \"{\"fruit\": "\apple\" , \"vegetable\": \"cucumber\"}\"}\"}"
 * }
 *
 * might yield the directory structure:
 *  a
 *  |
 *  --> b
 *      |
 *      --> c
 *
 * where a and b are directories, but c is a file containing JSON data. The directory drill down may not go all the way
 * down to the deepest json elements.
 *
 * Currently, the disassembly only goes as deep as partitioning out sheens nodes into their own files.
 *
 * @param specJson - The cJSON representation of a sheens specification
 * @param path - The path to write the disassembly to
 * @return - True upon successfully writing the disassembled automation specification to files, false otherwise.
 */
static bool disassembleSpecificationSheens(const cJSON *specJson, const char *path)
{
    bool retVal = false;
    if (path == NULL)
    {
        fprintf(stderr, "Path provided is null \n");
        return retVal;
    }

    if (specJson == NULL)
    {
        fprintf(stderr, "Sheens Json specification provided is null\n");
        return retVal;
    }

    // Create a copy that we can safely mess with and cleanup later
    cJSON *specCopy = cJSON_Duplicate(specJson, true);

    // We're going to disassemble sheens into metadata and nodes.
    // Detach the nodes item from the metadata
    cJSON *nodes = cJSON_DetachItemFromObject(specCopy, SHEENS_KEY_NODES);

    // Start with the metadata
    char *metadataPath = stringBuilder("%s%s", path, URI_METADATA_DIR);
    if (metadataPath != NULL)
    {
        if (mkdir_p(metadataPath, 0775) == 0)
        {
            char *sheensMetadataFile = stringBuilder("%s/%s", metadataPath, AUTOMATION_UTIL_METADATA_FILENAME);
            if (sheensMetadataFile != NULL)
            {
                char *sheensMetadata = cJSON_Print(specCopy);
                if (sheensMetadata != NULL)
                {
                    FILE *filePtr = fopen(sheensMetadataFile, "w");
                    if (filePtr != NULL)
                    {
                        size_t len = strlen(sheensMetadata);
                        retVal = (fwrite(sheensMetadata, 1, len, filePtr) == len);

                        fclose(filePtr);
                    }
                    else
                    {
                        fprintf(stderr, "Couldn't open sheens metadata file\n");
                    }

                    free(sheensMetadata);
                }

                free(sheensMetadataFile);
            }
        }
        else
        {
            fprintf(stderr, "Couldn't create sheens metadata directory\n");
        }
    }

    // next do the nodes
    char *nodesPath = stringBuilder("%s%s", path, URI_NODES_DIR);
    if (nodesPath != NULL)
    {
        if (mkdir_p(nodesPath, 0775) == 0)
        {
            cJSON *element = NULL;
            char *elementKey = NULL;

            cJSON_ArrayForEach(element, nodes)
            {
                elementKey = element->string;
                if (elementKey != NULL)
                {
                    char *elementNodePath = stringBuilder("%s/%s", nodesPath, elementKey);

                    if (elementNodePath != NULL)
                    {
                        if (mkdir_p(elementNodePath, 0775) == 0)
                        {
                            char *sheensNodeFile = stringBuilder("%s/%s_node", elementNodePath, elementKey);
                            if (sheensNodeFile != NULL)
                            {
                                char *nodeContents = cJSON_Print(element);
                                if (nodeContents != NULL)
                                {
                                    char *nodeOutputPretty = prettyPrintJavascript(nodeContents);
                                    if (nodeOutputPretty != NULL)
                                    {
                                        FILE *filePtr = fopen(sheensNodeFile, "w");
                                        if (filePtr != NULL)
                                        {
                                            size_t len = strlen(nodeOutputPretty);
                                            retVal = (fwrite(nodeOutputPretty, 1, len, filePtr) == len);

                                            fclose(filePtr);
                                        }
                                        else
                                        {
                                            fprintf(stderr, "Couldn't open sheens metadata file\n");
                                        }

                                        free(nodeOutputPretty);
                                    }

                                    free(nodeContents);
                                }

                                free(sheensNodeFile);
                            }
                        }
                        else
                        {
                            fprintf(stderr, "Couldn't create node directory %s\n", elementKey);
                        }

                        free(elementNodePath);
                    }
                }
            }
        }
        else
        {
            fprintf(stderr, "Couldn't create sheens nodes directory\n");
        }
    }

    cJSON_Delete(specCopy);
    cJSON_Delete(nodes);

    return retVal;
}

/**
 * Disassembles a legacy XML specification into its parts and then outputs the disassembly to a file contained in the
 * provided directory path.
 *
 * Since legacy XML specs are fairly easy to read as is, this just outputs the entire specification into a single
 * file.
 *
 * @param specXml - An XML representation of a legacy specification
 * @param path - The path to write the disassembly to
 * @return - True upon successfully writing the disassembled automation specification to a file, false otherwise.
 */
static bool disassembleSpecificationLegacy(xmlDocPtr specXml, const char *path)
{
    bool retVal = false;
    if (path == NULL)
    {
        fprintf(stderr, "Path provided is null \n");
        return retVal;
    }

    if (specXml == NULL)
    {
        fprintf(stderr, "Legacy xml specification provided is null\n");
        return retVal;
    }

    xmlChar *buffer;
    int bufferLen = 0;

    // Just dump to file. Unlike sheens, the xml for legacy rules isn't that hard to digest.
    xmlDocDumpFormatMemory(specXml, &buffer, &bufferLen, 1);

    if (bufferLen > 0)
    {
        char *filePath = stringBuilder("%s/%s", path, AUTOMATION_UTIL_LEGACY_FILENAME);

        if (filePath != NULL)
        {
            FILE *filePtr = fopen(filePath, "w");

            retVal = (fwrite(buffer, 1, bufferLen, filePtr) == bufferLen);
            if (retVal == false)
            {
                fprintf(stderr, "Couldn't output legacy specification");
            }

            fclose(filePtr);
            free(filePath);
        }

        xmlFree(buffer);
    }
    else
    {
        fprintf(stderr, "Couldn't dump legacy specification memory");
    }

    return retVal;
}

/**
 * Assembles the disassembled automation metadata contained in the provided disassembly directory path.
 *
 * @param disassemblyPath - The path to the directory containing the disassembled metadata
 * @return - The automation metadata type, or NULL on error
 */
static automationMetadata *assembleAutomationMetadata(const char *disassemblyPath)
{
    automationMetadata *retVal = NULL;

    if (disassemblyPath != NULL)
    {
        char *disassemblyMetadataPath = stringBuilder("%s%s/%s", disassemblyPath, URI_METADATA_DIR, AUTOMATION_UTIL_METADATA_FILENAME);

        if (disassemblyMetadataPath != NULL && doesNonEmptyFileExist(disassemblyMetadataPath))
        {
            char *metadataContents = readFileContents(disassemblyMetadataPath);

            if (metadataContents != NULL)
            {
                cJSON *metadataJson;
                switch (automationDataFormat)
                {
                    case DATA_FORMAT_JSON:
                        metadataJson = cJSON_Parse(metadataContents);
                        retVal = parseJsonToAutomationMetadata(metadataJson);
                        break;
                    case DATA_FORMAT_XML:
                    case DATA_FORMAT_UNKNOWN:
                    default:
                        break;
                }

                free(metadataContents);
            }
        }

        free(disassemblyMetadataPath);
    }

    return retVal;
}

/**
 * Assembles the disassembled automation specification contained in the provided disassembly directory path.
 *
 * @param disassemblyPath - The path to the directory containing the disassembled specification
 * @return - The automation specification type, or NULL on error
 */
static specification *assembleSpecification(const char *disassemblyPath)
{
    specification *retVal = NULL;

    if (disassemblyPath != NULL)
    {
        if ((retVal = assembleSpecificationSheens(disassemblyPath)) != NULL)
        {
            // Drop down to return
        }
        else if ((retVal = assembleSpecificationLegacy(disassemblyPath)) != NULL)
        {
            // Drop down to return
        }
    }

    return retVal;
}

/**
 * Assembles the disassembled sheens specification contained in the provided disassembly directory path.
 *
 * This function first builds a JSON representation of the disassembled sheens spec. Then parses that JSON representation
 * into a specification type.
 *
 * @param disassemblyPath - The path to the directory containing the disassembled sheens specification
 * @return - The automation specification type, or NULL on error
 */
static specification *assembleSpecificationSheens(const char *disassemblyPath)
{
    specification *retVal = NULL;

    if (disassemblyPath != NULL)
    {
        char *metadataPath = stringBuilder("%s%s/%s", disassemblyPath, URI_METADATA_DIR, AUTOMATION_UTIL_METADATA_FILENAME);
        if (metadataPath != NULL)
        {
            char *metadataFileContents = readFileContents(metadataPath);
            if (metadataFileContents != NULL)
            {
                cJSON *baseSpecJson = cJSON_Parse(metadataFileContents);
                if (baseSpecJson != NULL)
                {
                    char *nodesDir = stringBuilder("%s%s", disassemblyPath, URI_NODES_DIR);
                    if (nodesDir != NULL)
                    {
                        cJSON *nodesElement = cJSON_AddObjectToObject(baseSpecJson, SHEENS_KEY_NODES);
                        listDirectory(nodesDir, handleSheensSpecificationDirectory, &nodesElement);

                        free(nodesDir);
                    }

                    char *specificationContents = cJSON_PrintUnformatted(baseSpecJson);

                    char *formattedSpecContents = formatPrettyJavascript(specificationContents);
                    free(specificationContents);

                    retVal = createSpecification();
                    retVal->specificationContents = formattedSpecContents;
                    retVal->specType = SPECIFICATION_SHEENS;
                    retVal->specDataFormat = DATA_FORMAT_JSON;
                }

                cJSON_Delete(baseSpecJson);
                free(metadataFileContents);
            }

            free(metadataPath);
        }
    }

    return retVal;
}

/**
 * Assembles the disassembled legacy specification contained in the provided disassembly directory path.
 *
 * @param disassemblyPath - The path to the directory containing the disassembled legacy specification
 * @return - The automation specification type, or NULL on error
 */
static specification *assembleSpecificationLegacy(const char *disassemblyPath)
{
    specification *retVal = NULL;

    if (disassemblyPath != NULL)
    {
        char *legacySpecPath = stringBuilder("%s/%s", disassemblyPath, AUTOMATION_UTIL_LEGACY_FILENAME);
        if (legacySpecPath != NULL)
        {
            retVal = createSpecification();
            char *legacySpecContents = readFileContents(legacySpecPath);
            retVal->specificationContents = legacySpecContents;
            retVal->specType = SPECIFICATION_LEGACY;
            retVal->specDataFormat = DATA_FORMAT_XML;

            free(legacySpecPath);
        }
    }

    return retVal;
}

/**
 * directoryHandler implementation for listDirectory. This function builds a cJSON object by recursively parsing a
 * directory structure depth-first. Each directory is considered a key where its contents are a new json object. Each
 * regular file is a plain old JSON object.
 *
 * @param pathname - The path to the directory containing the current file
 * @param dname - The name of the file or directory we are currently handling
 * @param dtype - The type of the file/directory we are currently handling
 * @param private - Private argument containing JSON data used to build up the final cJSON object
 * @return - 0 if successful, else an errno upon failure
 * @note Implementation relies on no more than 1 json file for any directory
 */
static int handleSheensSpecificationDirectory(const char* pathname, const char* dname, unsigned char dtype, void* private)
{
    int retVal = 0;
    // This will either be a pointer to the parent cJSON element pointer, or a pointer to something that we plan to
    // replace with the a new cJSON pointer populated by a files contents.
    cJSON **parentElement = (cJSON **) private;

    if (pathname != NULL && dname != NULL)
    {
        char *path = stringBuilder("%s/%s", pathname, dname);

        if (path != NULL)
        {
            char *fileContents;
            cJSON *element = NULL;

            switch (dtype)
            {
                case DT_DIR:
                    retVal = listDirectory(path, handleSheensSpecificationDirectory, &element);
                    if (element != NULL)
                    {
                        cJSON_AddItemReferenceToObject(*parentElement, dname, element);
                    }

                    free(path);
                    break;
                case DT_REG:
                    fileContents = readFileContents(path);
                    if (fileContents != NULL)
                    {
                        //TODO: Address Sanitizer reports this is leaky, not sure why. Need to investigate further
                        *parentElement = cJSON_Parse(fileContents);

                        free(fileContents);
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return retVal;
}

/**
 * Prettify a string containing some javascript source in order to make it more human readable.
 *
 * TODO: Improve
 *
 * @param javascriptSource - A string containing javascript source
 * @return - A new string with human readability improvements. Caller should free this memory.
 */
static char *prettyPrintJavascript(const char *javascriptSource)
{
    char *retVal = NULL;

    if (javascriptSource != NULL)
    {
        // Replace all "\n" with the literal newline characters and tab indent. The tab indention is pretty rigid, but
        // provides a decent readability improvement.
        retVal = str_replace((char *)javascriptSource, "\\n", "\n\t\t\t\t\t");
    }

    return retVal;
}

/**
 * Unprettify a string containing some javascript source in order to make it similar to a typically formatted
 * JSON string dump.
 *
 * TODO: Improve
 *
 * @param javascriptSource - A string containing prettified javascript source
 * @return - A new, unprettified version of the prettified javadscript. Caller should free this memory
 */
static char *formatPrettyJavascript(const char *prettyJavascript)
{
    char *retVal = NULL;

    if (prettyJavascript != NULL)
    {
        // Inverse of prettyPrintJavascript
        retVal = str_replace((char *)prettyJavascript, "\\n\\t\\t\\t\\t\\t", "\n");
    }

    return retVal;
}