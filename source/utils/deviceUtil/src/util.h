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

#ifndef ZILKER_UTIL_H
#define ZILKER_UTIL_H

/**
 * Dump all of the details about a resource into a newly allocated string.  Caller frees.
 * @param resource
 * @return
 */
char *getResourceDump(const DSResource *resource);

/**
 * Get the value of a resource, respecting resources marked sensitive.  Caller frees.
 *
 * @param resource
 * @return
 */
char *getResourceValue(const DSResource *resource);

/**
 * Get a line of input from the interactive user. Caller frees
 *
 * @return a line of input
 */
char *getInputLine(void);

/**
 * Retrieve an array of tokens read from an interactive user.
 *
 * @param line a line of input provided by the interactive user.
 * @param numArgs the number of tokens returned
 * @return an array of tokens.  Caller frees.
 */
char **getTokenizedInput(const char *line, int *numTokens);

/**
 * Translate resource mode flags into a human-readable string.  The values are positional and take a similar
 * form to the permissions displayed by "ls -l".
 *
 *  Where:
 *      r  Readable
 *      w  Writable
 *      x  Executable
 *      d  Dynamic or dynamic capable
 *      e  Emit events
 *      l  Lazy save next
 *      s  Sensitive
 *
 *  Example:
 *      rwx----  Resource is readable, writable and executable.
 *
 *
 * @param mode the mode associated with the resource.
 * @return a string that must be freed by the caller.
 */
const char *stringifyMode(uint8_t mode);

#endif //ZILKER_UTIL_H
