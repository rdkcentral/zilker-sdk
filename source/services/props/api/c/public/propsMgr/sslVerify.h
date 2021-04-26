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
 * sslVerify.h
 *
 * Define enumerations for and helper functions for getting the
 * 'ssl verify' setting (SSL_CERT_VALIDATE) as defined within properties.
 *
 * Author: jelderton - 6/16/18
 *-----------------------------------------------*/

#ifndef IC_SSL_VERIFY_H
#define IC_SSL_VERIFY_H

#include <stdbool.h>
#include "commonProperties.h"
#include <propsMgr/propsService_event.h>

/*
 * list of verify categories
 */
typedef enum {
    SSL_VERIFY_CATEGORY_FIRST = 0,
    SSL_VERIFY_HTTP_FOR_SERVER = SSL_VERIFY_CATEGORY_FIRST,
    SSL_VERIFY_HTTP_FOR_DEVICE,
    SSL_VERIFY_CATEGORY_LAST = SSL_VERIFY_HTTP_FOR_DEVICE
} sslVerifyCategory;

/*
 * enumeration of possible verify values
 */
typedef enum {
    SSL_VERIFY_INVALID = -1,
    SSL_VERIFY_NONE,
    SSL_VERIFY_HOST,
    SSL_VERIFY_PEER,
    SSL_VERIFY_BOTH,
} sslVerify;

/*
 * Return the sslVerify value of the SSL_CERT_VALIDATE property
 * for the given category (as each can be different)
 */
sslVerify getSslVerifyProperty(sslVerifyCategory category);

/*
 * Returns the property key for the requested category, e.g., for listening to changes
 * @returns the property key name for this category.
 * @note this will only return null if a nonexistent enum value is passed, otherwise the result is guaranteed to exist.
 */
const char *sslVerifyPropKeyForCategory(sslVerifyCategory cat);

/**
 * Fetch the verify level setting for a property event
 * @param event Any property event. Deletions will fall back on the SSL_VERIFY_BOTH.
 * @param cat The category the event must represent.
 * @return SSL_VERIFY_INVALID if the property does not match the category, otherwise a valid SSL_VERIFY_ level.
 */
sslVerify sslVerifyConvertCPEPropEvent(cpePropertyEvent *event, sslVerifyCategory cat);

#endif // IC_SSL_VERIFY_H

