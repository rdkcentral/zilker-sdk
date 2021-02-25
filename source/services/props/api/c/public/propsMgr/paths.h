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
 * paths.h
 *
 * Similar to the PathsUtil Java object, this returns
 * the set of path prefixes to use for the various storage
 * locations on an iControl CPE.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IC_PATHS_H
#define IC_PATHS_H

/*
 * default path values
 */
#define DEFAULT_DYNAMIC_PATH    "/opt"
#define DEFAULT_STATIC_PATH     "/vendor"
#define CONFIG_SUBDIR           "/etc"
#define DEFAULTS_SUBDIR         "/defaults"

/*
 * Return the path to where the 'dynamic files' are stored.
 * This is a convenience function to obtain the IC_DYNAMIC_DIR_PROP
 * property (which defaults to /opt)
 *
 * NOTE: the caller must free this memory
 */
extern char *getDynamicPath();

/*
 * Return the path to where the 'dynamic config files' are stored.
 * This is a convenience function to obtain the IC_DYNAMIC_DIR_PROP
 * property + /etc (which defaults to /opt/etc)
 *
 * NOTE: the caller must free this memory
 */
extern char *getDynamicConfigPath();

/*
 * Return the path to where the 'static files' are stored.
 * This is a convenience function to obtain the IC_STATIC_DIR_PROP
 * property (which defaults to /vendor)
 *
 * NOTE: the caller must free this memory
 */
extern char *getStaticPath();

/*
 * Return the path to where the static config files are stored.
 * This is a convenience function to obtain the IC_STATIC_DIR_PROP
 * property + /etc (which defaults to /vendor/etc)
 *
 * NOTE: the caller must free this memory
 */
extern char *getStaticConfigPath();

/**
 * Get the path to the default configurations for the current firmware brand
 *
 * @return an allocated string containing the absolute path. You must free this when done.
 */
char *getBrandDefaultsPath();

/*
 * Return the path to where the statistics files are stored.
 *
 * NOTE: the caller must free this memory
 */
extern char *getStatisticPath();

/*
 * Return the path to where the Telemetry files are stored.
 *
 * NOTE: the caller must free this memory
 */
extern char *getTelemetryPath();

/*
 * Retrieve a property as a "path" and return the value.
 * If the property is not defined, the "default" will be returned.
 *
 * NOTE: the caller must free this memory if not NULL
 *
 * @param propName - the property name to get from propsService
 * @param defValue - if not NULL, the default value to return if getting 'propName' fails
 */
extern char *getPropertyAsPath(const char *propName, const char *defValue);

/**
 * Get the path to the trusted CA bundle path (pem format)
 */
char *getCABundlePath(void);

#endif // IC_PATHS_H
