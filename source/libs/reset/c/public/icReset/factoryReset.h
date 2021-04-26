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
 * factoryReset.h
 *
 * Set of functions available for resetting the device
 * to the factory settings.
 *
 * Author: jelderton, gfaulkner - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_FACTORYRESET_H
#define IC_FACTORYRESET_H

/*
 * Function to reset the CPE to factory defaults, and then reboot
 * Note that some settings are not reset
 */
extern void resetToFactory();

/*
 * Resets the device completely, so that the branded factory defaults will be used
 */
extern void resetForRebranding();

#endif // IC_FACTORYRESET_H
