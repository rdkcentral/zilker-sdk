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
 * propsHelper.h
 *
 * Helper functions to make getting properties (and
 * typecasting them) less painful.
 *
 * Author: jelderton - 6/17/16
 *-----------------------------------------------*/

#ifndef ZILKER_PROPSHELPER_H
#define ZILKER_PROPSHELPER_H

#include <stdint.h>
#include <stdbool.h>

#include <propsMgr/propsService_event.h>
#include <propsMgr/propsService_pojo.h>


/*
 * queries propsService to see if a property is set.
 *
 * @param propName - the property name to query from propsService
 */
bool hasProperty(const char *propName);

/*
 * Checks to see if this particular property key and/or value is
 * changable by any remote request (Server or XConf).  Will return
 * false if changing this property is not allowed.
 */
bool canServerSetProperty(const char *propName, const char *propsValue);

/*
 * queries propsService for a property that has a string value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * NOTE: the caller must free the return string via free() - if not NULL
 *
 * @param propName - the property name to get from propsService
 * @param defValue - if not NULL, the default value to return if getting 'propName' fails
 */
char *getPropertyAsString(const char *propName, const char *defValue);

/**
 * Retrieve the value of a Property changed event as a string.
 *
 * @param event The event caused by a property change.
 * @param defValue The default value if event does not contain one.
 * @return A unmodified read-only pointer to the original event string.
 */
const char *getPropertyEventAsString(const cpePropertyEvent *event, const char *defValue);

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
int32_t getPropertyAsInt32(const char *propName, int32_t defValue);

/**
 * Retrieve the value of a Property changed event as a 32-bit signed Integer.
 *
 * @param event The event caused by a property change.
 * @param defValue The default value if event does not contain one.
 * @return A 32-bit signed Integer
 */
int32_t getPropertyEventAsInt32(const cpePropertyEvent *event, int32_t defValue);

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
uint32_t getPropertyAsUInt32(const char *propName, uint32_t defValue);

/**
 * Retrieve the value of a Property changed event as a 32-bit
 * unsigned Integer.
 *
 * @param event The event caused by a property change.
 * @param defValue The default value if event does not contain one.
 * @return A 32-bit unsigned signed Integer
 */
uint32_t getPropertyEventAsUInt32(const cpePropertyEvent *event, uint32_t defValue);

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
int64_t getPropertyAsInt64(const char *propName, int64_t defValue);

/**
 * Retrieve the value of a Property changed event as a 64-bit signed Integer.
 *
 * @param event The event caused by a property change.
 * @param defValue The default value if event does not contain one.
 * @return A 64-bit signed Integer
 */
int64_t getPropertyEventAsInt64(const cpePropertyEvent *event, int64_t defValue);

/*
 * queries propsService for a property that has an integer value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
uint64_t getPropertyAsUInt64(const char *propName, uint64_t defValue);

/**
 * Retrieve the value of a Property changed event as a 64-bit
 * unsigned Integer.
 *
 * @param event The event caused by a property change.
 * @param defValue The default value if event does not contain one.
 * @return A 64-bit unsigned Integer
 */
uint64_t getPropertyEventAsUInt64(const cpePropertyEvent *event, uint64_t defValue);

/*
 * queries propsService for a property that has a boolean value
 * if not defined or the service is not responding, the default
 * value will be returned.
 *
 * @param propName - the property name to get from propsService
 * @param defValue - the default value to return if getting 'propName' fails
 */
bool getPropertyAsBool(const char *propName, bool defValue);

/**
 * Retrieve the value of a Property changed event as a Boolean.
 *
 * Values of '[Tt]rue', '[Yy]es', and '1' will return a
 * Boolean value of "true"; otherwise "false".
 *
 * @param event The event caused by a property change.
 * @param defValue The default value if event does not contain one.
 * @return A Boolean
 */
bool getPropertyEventAsBool(const cpePropertyEvent *event, bool defValue);

/**
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param propValue - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 * @param source - sets the source of the property to determine priority level
 */
propSetResult setPropertyValue(const char *propName, const char *propValue, bool overwrite, propSource source);

/**
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param value - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 * @param source - sets the source of the property to determine priority level
 */
propSetResult setPropertyUInt32(const char *propName, uint32_t value, bool overwrite, propSource source);

/**
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param value - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 * @param source - sets the source of the property to determine priority level
 */
propSetResult setPropertyInt32(const char *propName, int32_t value, bool overwrite, propSource source);

/**
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param value - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 * @param source - sets the source of the property to determine priority level
 */
propSetResult setPropertyUInt64(const char *propName, uint64_t value, bool overwrite, propSource source);

/**
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param value - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 * @param source - sets the source of the property to determine priority level
 */
propSetResult setPropertyInt64(const char *propName, int64_t value, bool overwrite, propSource source);

/**
 * creates a request to propsService to set a property.  if 'overwrite'
 * is 'false', will not perform the set unless this property is missing.
 *
 * NOTE: sets the property source as PROPERTY_SRC_DEVICE
 *
 * @param propName - the property name to set into propsService
 * @param value - thr property value to set
 * @param overwrite - if false, query propsService first to prevent overwriting exiting property
 * @param source - sets the source of the property to determine priority level
 */
propSetResult setPropertyBool(const char *propName, bool value, bool overwrite, propSource source);

#endif // ZILKER_PROPSHELPER_H
