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

/*-----------------------------------------------
 * fileUtils.h
 *
 * Helper utility to perform for filesystem or file io
 *
 * Author: mkoch - 4/26/18
 *-----------------------------------------------*/

#ifndef IC_FILEUTILS_H
#define IC_FILEUTILS_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

typedef int (*directoryHandler)(const char* pathname,
                                const char* dname,
                                unsigned char dtype,
                                void* private);

/**
 * Create all directories in the path
 * @param path the directory path to create
 * @param mode the permissions on any directories that are created
 * @return 0 if success, < 0 if failed with errno set
 */
int mkdir_p(const char *path, mode_t mode);

/**
 * Read the full contents of the supplied ASCII file.
 *
 * NOTE: caller is responsible for freeing the returned memory.
 *
 * @param filename - the file name to read from
 * @return - returns the contents of file name or NULL if error occurred
 */
char *readFileContents(const char *filename);

/**
 * Read full contents from filename.
 * Also trims off the extra '\n' char if it exists from contents
 *
 * NOTE: caller is responsible for freeing the returned memory.
 *
 * @param filename - the file name to read from
 * @return - returns the contents but trimmed down or NULL if error occurred
 */
char *readFileContentsWithTrim(const char *filename);

/**
 * Writes contents to filename.
 * If file does not exist; it will be created.
 *
 * NOTE: clearing out the contents of the file if it exists
 *
 * @param filename - the file to write to
 * @param contents - the contents to add
 * @return true if successful, false if otherwise
 */
bool writeContentsToFileName(const char *filename, const char *contents);

/**
 * Copy between file streams (if the streams are not NULL).
 * this will cleanup the streams at the end (regardless of success)
 *
 * @param source the source file stream
 * @param dest the destination file stream
 */
bool copyFile(FILE *source, FILE *dest);

/**
 * Copy a file contents by path
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 */
bool copyFileByPath(const char *sourcePath, const char *destPath);

/**
 * Move a file from one path to another.  Works across filesystems.
 *
 * @param sourcePath the source path
 * @param destPath the destination path
 */
bool moveFile(const char *sourcePath, const char *destPath);

/*
 * returns if the file exists and has size to it
 */
bool doesNonEmptyFileExist(const char *filename);

/*
 * returns if the file exists
 */
bool doesFileExist(const char *filename);

/**
 * returns if the directory exists
 */
bool doesDirExist(const char *dirPath);

/**
 * list a directory reading out all files and directories.
 *
 * Each file (regular file, directory, symlink, etc) will be handed off
 * to a provided routine to handle the individual directory or file. This
 * allows callers to provide their own functionality with each file.
 *
 * It is safe to call this routine recursively through the directory
 * routine handler. This will create the ability to recurse through
 * an entire directory tree.
 *
 * @param dir The directory to parse files in.
 * @param handler The routine that will handle individual files.
 * @param private Private data that will be handed to the callback routine for each file.
 * @return Zero on success. Otherwise errno will be returned as the value.
 */
int listDirectory(const char* dir,
                  directoryHandler handler,
                  void* private);

/*
 * helper to recursively delete a directory and the files within it
 */
bool deleteDirectory(const char *path);

/**
 * Delete individual directories/files/symlinks from a directory.
 *
 * @param pathname The directory to delete the incoming file from.
 * @param dname The file/directory/symlink to delete.
 * @param dtype The file type (directory/symlink/file)
 * @param private Unused for deletion.
 * @return Zero on success, otherwise the errno will be returned as the value.
 */
int deleteDirHandler(const char* pathname, const char* dname, unsigned char dtype, void* private);

/**
 * Delete a file
 * @param filename full path to the file to be deleted
 */
bool deleteFile(const char *filename);


/**
 * Copy one directory, and all sub-directories/files,
 * to another location.
 *
 * Behaviour is the same as the bash command `cp` with
 * the option `-a`. Thus a directory `A` will be copied
 * to another directory `B`. If `B` already exists then
 * `B` will placed _within_ `B`. If the sub-`B` already
 * exists then any matching files will be overwritten.
 *
 * Example:
 * cp -a dir/mydir some/where
 *
 * If `where` does _not_ exist then all contents of`mydir`
 * will be copied to `where` after it is created.
 *
 * If `where` _does_ exist then the directory `mydir` will
 * be created _within_ `where`, and all contents copied.
 *
 * @param src The source directory to copy.
 * @param dst The destination to copy contents of source into
 * @return True on successful copy of all content.
 */
bool copyDirectory(const char* src, const char* dst);

/*
 * create a marker file of zero length
 */
bool createMarkerFile(const char *path);

#endif // IC_FILEUTILS_H
