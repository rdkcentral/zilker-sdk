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
 * fileHelper.h
 *
 * file I/O functions to keep touchstone.c clean
 *
 * Author: jelderton - 7/1/16
 *-----------------------------------------------*/

#ifndef TSTONE_FILEHELPER_H
#define TSTONE_FILEHELPER_H

#include <stdlib.h>
#include <stdbool.h>

/*
 * parse a communication.conf file.  yes, crappy to duplicate
 * what the service does, but need a way to extract data when
 * the service is down.
 *
 * caller must free the returned string
 */
char *extractHostnameFromCommConf(const char *xmlFile);

/*
 * extract the contents of /tmp/server.txt
 */
char *extractHostnameFromMarker(const char *txtFile);

/*
 * create a /tmp/server.txt file, using 'hostname' as the contents
 */
bool saveHostnameToMarker(const char *txtFile, const char *hostname);


#endif // TSTONE_FILEHELPER_H
