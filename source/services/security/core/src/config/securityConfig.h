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
 * securityConfig.h
 *
 * Main configuration for the security (alarm panel) layer.
 * Persists the user codes and alarm panel options.
 *
 * NOTE: this does NOT contain current alarm panel status information.
 *
 * Author: jelderton -  10/30/18.
 *-----------------------------------------------*/

#ifndef ZILKER_SECURITYCONFIG_H
#define ZILKER_SECURITYCONFIG_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <icTypes/icLinkedList.h>
#include <securityService/securityService_pojo.h>

/*
 * defaults and ranges
 */
#define ENTRY_DELAY_SEC_MIN     30     // range: 30 sec - 4 min
#define ENTRY_DELAY_SEC_MAX     (4 * 60)
#define EXIT_DELAY_SEC_MIN      45     // range: 45 sec - 4 min
#define EXIT_DELAY_SEC_MAX      (4 * 60)
#define DIALER_DELAY_SEC_MIN    15     // range: 15 - 45 sec
#define DIALER_DELAY_SEC_MAX    45
#define SWINGER_TRIPS_MIN       1
#define SWINGER_TRIPS_MAX       6


/*
 * one time initialization of the security configuration.
 * will attempt to read the $IC_CONF/etc/securityConf file.
 */
void initSecurityConfig();

/*
 * cleanup called during process shutdown
 */
void destroySecurityConfig();

/*
 * called during RMA/Restore
 */
bool restoreSecurityConfig(char *tempDir, char *destDir);

/*
 * return the internal version of the config file.
 * here to support legacy SMAP communication to the server.
 */
uint64_t getSecurityConfigVersion();

/*
 * return the 'installer' code string.
 * caller is responsible for releasing the returned string.
 */
char *getInstallerUserCode();

/*
 * change the 'installer' code.  returns true
 * if the change was applied (because it's different)
 */
bool setInstallerUserCode(const char *code);

/*
 * return the 'master' code string.
 * caller is responsible for releasing the returned string.
 */
char *getMasterUserCode();

/*
 * change the 'master' code.  returns true
 * if the change was applied (because it's different)
 */
bool setMasterUserCode(const char *code);

/*
 * returns if use of a duress user is allowed; which is
 * dictated by a property value
 */
bool isDuressUserAllowed();

/*
 * return the 'duress' code string.
 * caller is responsible for releasing the returned string.
 * note that the notion of a duress user may be disabled.
 * see DURESSCODE_DISABLED property.
 */
char *getDuressUserCode();

/*
 * change the 'duress' code.  returns true
 * if the change was applied (because it's different).
 * note that the notion of a duress user may be disabled.
 * see DURESSCODE_DISABLED property.
 */
bool setDuressUserCode(const char *code);

/*
 * returns a linked list of all known keypadUserCode objects.
 * caller is responsible for releasing the returned list.
 */
icLinkedList *getAllUserCodes(bool includeInternal);

/*
 * adds a new user code (does not send the "user added" event)
 */
bool addUserCode(keypadUserCode *user);

/*
 * updates an existing user code (does not send the "user modified" event)
 */
bool updateUserCode(keypadUserCode *user);

/*
 * delete an existing user code (does not send the "user deleted" event)
 */
bool deleteUserCode(keypadUserCode *user);

/*
 * return the current "entry delay" setting (in seconds)
 */
uint16_t getEntryDelaySecsSetting();

/*
 * change the current "entry delay" setting.  returns
 * true if this was applied.  note that the valid range
 * is between ENTRY_DELAY_SEC_MIN and ENTRY_DELAY_SEC_MAX
 */
bool setEntryDelaySecsSetting(uint16_t value);

/*
 * return the current "exit delay" setting (in seconds)
 */
uint16_t getExitDelaySecsSetting();

/*
 * change the current "exit delay" setting.  returns
 * true if this was applied.  note that the valid range
 * is between EXIT_DELAY_SEC_MIN and EXIT_DELAY_SEC_MAX
 */
bool setExitDelaySecsSetting(uint16_t value);

/*
 * return the current "dialer delay" setting (in seconds)
 */
uint16_t getDialerDelaySecsSetting();

/*
 * change the current "dialer delay" setting.  returns
 * true if this was applied.  note that the valid range
 * is between DIALER_DELAY_SEC_MIN and DIALER_DELAY_SEC_MAX
 */
bool setDialerDelaySecsSetting(uint16_t value);

/*
 * return the number of minutes that the alarm siren should alarm
 */
uint16_t getAlarmSirenDurationMinutes();

/*
 * returns if "swinger shutdown" is enabled or not
 */
bool isSwingerShutdownSettingEnabled();

/*
 * enable/disable "swinger shutdown"
 * returns true if this was applied
 */
bool setSwingerShutdownSettingEnabled(bool flag);

/*
 * return the current "swinger shutdown" max trips setting.
 * note: should only be utilized if swinger shutdown is enabled.
 */
uint8_t getSwingerShutdownMaxTripsSetting();

/*
 * change the current "swinger shutdown" max trips setting.
 * returns true if this was applied.  note that the valid range
 * is between SWINGER_TRIPS_MIN and SWINGER_TRIPS_MAX
 */
bool setSwingerShutdownMaxTripsSetting(uint8_t value);

/*
 * returns if "fire alarm verification" is enabled
 */
bool isFireAlarmVerificationSettingEnabled();

/*
 * enable/disable "fire alarm verification" setting
 * returns true if this was applied
 */
bool setFireAlarmVerificationSettingEnabled(bool flag);

/*
 * returns if sending "test alarm" codes is enabled
 */
bool isTestAlarmSendCodesSettingEnabled();

/*
 * enable/disable sending "test alarm" codes setting
 * returns true if this was applied
 */
bool setTestAlarmSendCodesSettingEnabled(bool flag);

/*
 * return if the 'defer troubles during sleep hours' option is enabled
 */
bool isDeferTroublesEnabled();

/*
 * enable/disable the 'defer troubles during sleep hours' option
 */
bool setDeferTroublesEnabled(bool flag);

/*
 * populate the 'defer troubles during sleep hours' configuration.
 */
void getDeferTroublesConfiguration(deferTroublesConfig *container);

/*
 * update the 'defer troubles during sleep hours' configuration
 */
bool setDeferTroublesConfiguration(deferTroublesConfig *container);

/*
 * linkedListItemFreeFunc to delete a keypadUserCode object
 */
void freeKeypadUserCodeFromList(void *item);

#endif // ZILKER_SECURITYCONFIG_H
