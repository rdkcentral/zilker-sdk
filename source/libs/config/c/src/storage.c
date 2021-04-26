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
// Created by tlea on 4/4/18.
//

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <pthread.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icConfig/storage.h>
#include <icConfig/backupUtils.h>
#include <icConfig/simpleProtectConfig.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <icTypes/icStringHashMap.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/threadUtils.h>
#include <propsMgr/paths.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define LOG_TAG "storage"

#define STORAGE_DIR "storage"

static pthread_mutex_t mtx;
static bool loadInternalLocked(const char *namespace, const char *key, char **contentOut, const StorageCallbacks *cb);

static pthread_once_t initOnce = PTHREAD_ONCE_INIT;
static void oneTimeInit(void)
{
    /* Use a reentrant lock just in case a parser wants to load something else (e.g., keys) */
    mutexInitWithType(&mtx, PTHREAD_MUTEX_RECURSIVE);
}

typedef struct
{
    char* main; // the primary filename
    char* backup; // the backup
    char* temp; //the temporary working file
    char* bad; //path to move any invalid files to
} StorageFilePaths;

static char* getNamespacePath(const char* namespace)
{
    char* configDir = getDynamicConfigPath();
    char* path = stringBuilder("%s/%s/%s", configDir, STORAGE_DIR, namespace);

    free(configDir);

    return path;
}

static void getFilepaths(const char* namespace, const char* key, StorageFilePaths* paths)
{
    char* path = getNamespacePath(namespace);

    paths->main = stringBuilder("%s/%s", path, key);

    paths->backup = stringBuilder("%s/%s.bak", path, key);

    paths->temp = stringBuilder("%s/%s.tmp", path, key);

    paths->bad = stringBuilder("%s/%s.bad", path, key);

    free(path);
}

static void freeFilePaths(StorageFilePaths* paths)
{
    free(paths->main);
    free(paths->backup);
    free(paths->temp);
    free(paths->bad);
}

bool storageSave(const char* namespace, const char* key, const char* value)
{
    bool result = true;

    if (namespace == NULL || key == NULL || value == NULL)
    {
        icLogError(LOG_TAG, "storageSave: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    uint64_t startMillis = getMonotonicMillis();

    mutexLock(&mtx);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    //ensure the namespace directory exists
    char* path = getNamespacePath(namespace);

    struct stat buf;
    if (stat(path, &buf) && errno == ENOENT)
    {
        if (mkdir_p(path, 0777) == -1)
        {
            icLogError(LOG_TAG, "storageSave: failed to create directory %s: %s", path, strerror(errno));
            result = false;
        }
    }

    if (result == true)
    {
        //write to the temp file first, then safely move with our backupUtils
        FILE* fp = fopen(paths.temp, "w");
        if (fp != NULL)
        {
            if (fputs(value, fp) < 0)
            {
                icLogError(LOG_TAG, "storageSave: failed to store value at %s", paths.temp);
                result = false;
            }

            if (fflush(fp) != 0)
            {
                char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "%s: fflush failed when trying to save %s with error: %s", __func__, paths.temp, errStr);
                free(errStr);
                result = false;
            }
            int fd = fileno(fp);
            int rc = fsync(fd);
            if(rc != 0)
            {
                char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "storageSave:  fsync on %s failed when trying to save %s with error: %s",paths.temp,path,errStr);
                free(errStr);
                result = false;
            }
            fclose(fp);

            if (result == true)
            {
                safeFileSave(paths.temp, paths.main, paths.backup); //make the swap safely and with backup

                uint64_t endMillis = getMonotonicMillis();
                icLogDebug(LOG_TAG, "%s: saved file %s in %" PRId64 "ms", __FUNCTION__, paths.main, endMillis - startMillis);
            }
        }
        else
        {
            char *errStr = strerrorSafe(errno);
            icLogError(LOG_TAG, "storageSave: fopen failed for %s: %s", paths.temp, errStr);
            free(errStr);
            result = false;
        }
    }

    mutexUnlock(&mtx);

    free(path);
    freeFilePaths(&paths);

    return result;
}

static inline bool parseAny(const char *fileData, void *data)
{
    return fileData != NULL;
}

static const StorageCallbacks parseAnyCallbacks = {
        .parse = parseAny
};

bool storageLoad(const char* namespace, const char* key, char** value)
{
    bool result = false;

    if (value == NULL)
    {
        icLogError(LOG_TAG, "storageLoad: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    *value = NULL;

    mutexLock(&mtx);

    char *data = NULL;
    if ((result = loadInternalLocked(namespace, key, &data, &parseAnyCallbacks)) == true)
    {
        *value = data;
        data = NULL;
    }
    free(data);

    mutexUnlock(&mtx);

    return result;
}

/**
 * Load a storage item, possibly falling back on backup.
 * If the backup is used, any invalid input is moved to "key.bad," and replaced with the backup.
 * @param namespace
 * @param key
 * @param contentOut If non-null, this will point to the raw file data.
 * @param cb Non-null pointer to a StorageCallbacks that implements parse.
 * @return true when data was loaded
 */
static bool loadInternalLocked(const char *namespace, const char *key, char **contentOut, const StorageCallbacks *cb)
{
    if (cb == NULL || cb->parse == NULL)
    {
        icLogError(LOG_TAG, "%s: No parse callback supplied", __func__);
        return false;
    }

    if (namespace == NULL || key == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __func__);
        return false;
    }

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    fileToRead whichFile = chooseFileToRead(paths.main, paths.backup, NULL);
    const char *filepath = NULL;
    char *data = NULL;
    bool ok = false;

    switch (whichFile)
    {
        case ORIGINAL_FILE:
            filepath = paths.main;
            data = readFileContents(filepath);
            if ((ok = cb->parse(data, cb->parserCtx)) == false)
            {
                icLogWarn(LOG_TAG,
                          "Unable to parse file at %s, attempting to use backup. "
                              "The bad file, if it exists, will be moved to %s",
                          filepath,
                          paths.bad);

                if (rename(paths.main, paths.bad) != 0)
                {
                    if (isIcLogPriorityTrace() == true)
                    {
                        char *errStr = strerrorSafe(errno);
                        icLogTrace(LOG_TAG, "%s: unable to rename %s to %s: %s", __func__, paths.main, paths.bad, errStr);
                        free(errStr);
                    }
                }

                whichFile = BACKUP_FILE;
            }
            break;

        case BACKUP_FILE:
            /* Handled later: ORIGINAL_FILE may have switched to BACKUP_FILE above */
            break;

        case FILE_NOT_PRESENT:
            icLogWarn(LOG_TAG, "No file found for %s/%s", namespace, key);
            break;

        default:
            icLogError(LOG_TAG, "%s: Unsupported file path type [%d]!", __func__, whichFile);
            break;
    }

    if (whichFile == BACKUP_FILE)
    {
        filepath = paths.backup;
        free(data);
        data = readFileContents(filepath);
        ok = cb->parse(data, cb->parserCtx);
        if (ok == true)
        {
            if (copyFileByPath(filepath, paths.main) == false)
            {
                icLogWarn(LOG_TAG, "Failed to copy restored backup at %s to %s!", filepath, paths.main);
            }

            /* Even if (unlikely) the copy failed, the data was still loaded and is usable */
            icLogInfo(LOG_TAG, "%s/%s restored from backup", namespace, key);
        }
    }

    if (ok == false && whichFile != FILE_NOT_PRESENT)
    {
        icLogError(LOG_TAG,
                   "Unable to parse file for %s/%s (filename %s)!",
                   namespace,
                   key,
                   stringCoalesceAlt(filepath, "(none)"));
    }

    if (contentOut != NULL)
    {
        *contentOut = data;
        data = NULL;
    }

    free(data);
    freeFilePaths(&paths);

    return ok;
}

bool storageParse(const char *namespace, const char *key, const StorageCallbacks *cb)
{
    pthread_once(&initOnce, oneTimeInit);

    mutexLock(&mtx);

    bool ok = loadInternalLocked(namespace, key, NULL, cb);

    mutexUnlock(&mtx);

    return ok;
}

typedef struct XMLContext
{
    xmlDoc *doc;
    const char *encoding;
    const char *docName;
    int parseOptions;
} XMLContext;

typedef struct JSONContext
{
    cJSON *json;
} JSONContext;

static bool parseXML(const char *fileData, void *xmlCtx)
{
    bool xmlIsValid = false;
    XMLContext *xmlContext = xmlCtx;

    if (stringIsEmpty(fileData) == false)
    {
        xmlContext->doc = xmlReadMemory(fileData,
                                        (int) strlen(fileData),
                                        xmlContext->docName,
                                        xmlContext->encoding,
                                        xmlContext->parseOptions);

        xmlIsValid = xmlContext->doc != NULL;
    }

    return xmlIsValid;
}

xmlDoc *storageLoadXML(const char *namespace, const char *key, const char *encoding, int xmlParserOptions)
{
    XMLContext ctx = {
            .doc = NULL,
            .encoding = encoding,
            .docName = key,
            .parseOptions = xmlParserOptions
    };

    const StorageCallbacks xmlParser = {
            .parse = parseXML,
            .parserCtx = &ctx,
    };

    if (storageParse(namespace, key, &xmlParser) == false)
    {
        icLogWarn(LOG_TAG, "%s: %s/%s is not valid XML!", __func__, namespace, key);
    }

    return ctx.doc;
}

static bool parseJSON(const char *fileData, void *jsonCtx)
{
    JSONContext *jsonContext = jsonCtx;

    jsonContext->json = cJSON_Parse(fileData);

    return jsonContext->json != NULL;
}

cJSON *storageLoadJSON(const char *namespace, const char *key)
{
    JSONContext ctx = {
            .json = NULL
    };

    const StorageCallbacks jsonParser = {
            .parse = parseJSON,
            .parserCtx = &ctx
    };

    if (storageParse(namespace, key, &jsonParser) == false)
    {
        icLogWarn(LOG_TAG, "%s: %s/%s is not valid JSON!", __func__, namespace, key);
    }

    return ctx.json;
}

/*
 * delete the main file, its backup, and any temp file
 */
bool storageDelete(const char* namespace, const char* key)
{
    bool result = true;

    if (namespace == NULL || key == NULL)
    {
        icLogError(LOG_TAG, "storageDelete: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    mutexLock(&mtx);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    if (unlink(paths.main) == -1)
    {
        icLogError(LOG_TAG, "storageDelete: failed to unlink %s: %s", paths.main, strerror(errno));
        result = false;
    }

    //silently ignore any errors deleting temp or backup
    unlink(paths.backup);
    unlink(paths.temp);
    unlink(paths.bad);

    mutexUnlock(&mtx);

    freeFilePaths(&paths);
    return result;
}

bool storageDeleteNamespace(const char* namespace)
{
    bool result = true;

    if (namespace == NULL)
    {
        icLogError(LOG_TAG, "storageDeleteNamespace: invalid args");
        return false;
    }

    pthread_once(&initOnce, oneTimeInit);

    //its nuts how hard it is to remove a directory and its contents from C.  How bad is this solution?
    char* path = getNamespacePath(namespace);
    char* cmd = (char*) malloc(strlen(path) + 8); //"rm -rf " + \0
    sprintf(cmd, "rm -rf %s", path);
    mutexLock(&mtx);
    if (system(cmd) != 0)
    {
        icLogError(LOG_TAG, "storageDeleteNamespace: failed to delete directory %s", path);
        result = false;
    }
    mutexUnlock(&mtx);

    free(path);
    free(cmd);
    return result;
}

icLinkedList* storageGetKeys(const char* namespace)
{
    if (namespace == NULL)
    {
        icLogError(LOG_TAG, "storageGetKeys: invalid args");
        return NULL;
    }

    pthread_once(&initOnce, oneTimeInit);

    icLinkedList* result = NULL;
    char* path = getNamespacePath(namespace);

    DIR* dir;
    struct dirent *entry;

    mutexLock(&mtx);
    if ((dir = opendir(path)) == NULL)
    {
        icLogError(LOG_TAG, "storageGetKeys: failed to open namespace directory %s: %s", path, strerror(errno));
    }
    else
    {
        result = linkedListCreate();

        icStringHashMap* regularFiles = stringHashMapCreate();
        icStringHashMap* bakFiles = stringHashMapCreate();

        while((entry = readdir(dir)) != NULL)
        {
            if (entry->d_type != DT_DIR)
            {
                //our list of keys will be all files that are not .bak, .bad or .tmp PLUS any .bak files that are
                // missing their regular entry
                //dont add our backup or temp files
                if (strcmp(entry->d_name, STORAGE_KEY_FILE_NAME) == 0)
                {
                    icLogDebug(LOG_TAG, "Skipping storage key file");
                    continue;
                }
                size_t nameLen = strlen(entry->d_name);
                if(nameLen > 4) //the only ones that could have a .bak, .bad, or .tmp extension
                {
                    char* extension = entry->d_name + (nameLen - 4);
                    if(strcmp(extension, ".bak") == 0)
                    {
                        stringHashMapPut(bakFiles, strdup(entry->d_name), NULL);
                    }
                    else if(strcmp(extension, ".tmp") != 0 && strcmp(extension, ".bad") != 0)
                    {
                        stringHashMapPut(regularFiles, strdup(entry->d_name), NULL);
                    }
                }
                else //it was too short to be one of the files we are trying to filter... just add it
                {
                    stringHashMapPut(regularFiles, strdup(entry->d_name), NULL);
                }
            }
        }

        //now we have a list of regular files (keys) and .bak files.  If we have a .bak file for which there is no
        // regular file, add it to our results
        icStringHashMapIterator* it = stringHashMapIteratorCreate(bakFiles);
        while(stringHashMapIteratorHasNext(it))
        {
            char* key;
            char* value;
            if(stringHashMapIteratorGetNext(it, &key, &value))
            {
                //truncate the .bak file at the dot
                key[strlen(key) - 4] = '\0';
                if(stringHashMapContains(regularFiles, key) == false)
                {
                    stringHashMapPut(regularFiles, strdup(key), NULL);
                }
            }
        }
        stringHashMapIteratorDestroy(it);

        //now collect our final results from regularFiles
        it = stringHashMapIteratorCreate(regularFiles);
        while(stringHashMapIteratorHasNext(it))
        {
            char* key;
            char* value;
            if(stringHashMapIteratorGetNext(it, &key, &value))
            {
                linkedListAppend(result, strdup(key));
            }
        }
        stringHashMapIteratorDestroy(it);

        stringHashMapDestroy(regularFiles, NULL);
        stringHashMapDestroy(bakFiles, NULL);
        closedir(dir);
    }
    mutexUnlock(&mtx);

    free(path);
    return result;
}

bool storageRestoreNamespace(const char* namespace, const char* basePath)
{
    bool ret = false;

    struct stat sinfo;

    char* restorePath = stringBuilder("%s/%s/%s", basePath, STORAGE_DIR, namespace);

    pthread_once(&initOnce, oneTimeInit);

    if (doesDirExist(restorePath) == true)
    {
        bool pathExists = false;
        char* configPath = getNamespacePath(namespace);

        mutexLock(&mtx);
        if (stat(configPath, &sinfo) == 0)
        {
            if (!deleteDirectory(configPath))
            {
                /*
                 * Avoiding multiple return points. Just set a flag
                 * that we were not able to remove the existing namespace.
                 */
                pathExists = true;
                icLogError(LOG_TAG, "storageRestoreNamespace: failed to delete namespace directory %s: %s",
                           configPath, strerror(errno));
            }
        }

        /*
         * Only copy over the namespace if the new location
         * does not exist. If we allowed the namespace to be
         * copied while already existing we could pollute
         * the namespace.
         */
        if (!pathExists)
        {
            ret = copyDirectory(restorePath, configPath);
            if (!ret)
            {
                icLogError(LOG_TAG, "storageRestoreNamespace: failed to copy namespace directory %s -> %s: %s",
                           restorePath, configPath, strerror(errno));
            }
        }
        else
        {
            icLogError(LOG_TAG, "storageRestoreNamespace: failed to create namespace directory %s: %s",
                       configPath, strerror(errno));
        }
        mutexUnlock(&mtx);

        free(configPath);
    }
    else
    {
        // Not specifically an error, just nothing to do.  Just log and still return success
        ret = true;
        icLogDebug(LOG_TAG, "storageRestoreNamespace: failed to find namespace directory %s to restore",
                   restorePath);
    }

    free(restorePath);

    return ret;
}

const char *getStorageDir()
{
    return STORAGE_DIR;
}

bool storageGetMtime(const char *namespace, const char *key, struct timespec *mtime)
{
    if (mtime == NULL || namespace == NULL || key == NULL)
    {
        return false;
    }

    bool ok = false;

    pthread_once(&initOnce, oneTimeInit);

    mutexLock(&mtx);

    StorageFilePaths paths;
    getFilepaths(namespace, key, &paths);

    struct stat fileInfo;
    errno = 0;
    if (stat(paths.main, &fileInfo) == 0)
    {
        ok = true;
#ifdef CONFIG_OS_DARWIN
        *mtime = fileInfo.st_mtimespec;
#else
        *mtime = fileInfo.st_mtim;
#endif
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *error = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "Cannot stat %s/%s: %s", namespace, key, error);
    }

    mutexUnlock(&mtx);

    return ok;
}
