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
 * eventListener.h
 *
 * Register for a variety of events to feed into
 * Trouble, Zone, and Alarm sub-services.
 *
 * Author: jelderton - 8/26/15
 *-----------------------------------------------*/

#ifndef IC_TROUBLE_EVENTLISTENER_H
#define IC_TROUBLE_EVENTLISTENER_H

/*
 * register with various services for any event
 * that can be consumed by the zone, trouble,
 * or alarm sub-services.
 *
 * should be called once all of the services
 * are online and ready for processing
 */
void setupSecurityServiceEventListeners();

/*
 * called during shutdown to cleanup event listeners
 */
void removeSecurityServiceEventListeners();


#endif // IC_TROUBLE_EVENTLISTENER_H
