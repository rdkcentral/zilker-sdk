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
 * ipcHandler.c
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <icBuildtime.h>
#include <icLog/logging.h>                      // logging subsystem
#include <icIpc/ipcMessage.h>
#include <icIpc/eventConsumer.h>
#include <icIpc/ipcReceiver.h>
#include <icUtil/stringUtils.h>
#include <icSystem/softwareCapabilities.h>
#include <watchdog/serviceStatsHelper.h>
#include <deviceHelper.h>
#include <icTime/timeUtils.h>
#include <securityService/securityZoneHelper.h>
#include "config/securityConfig.h"
#include "alarm/alarmPanel.h"
#include "alarm/systemMode.h"
#include "trouble/troubleState.h"
#include "zone/securityZone.h"
#include "securitySystemTracker.h"
#include "securityService_ipc_handler.h"        // local IPC handler
#include "common.h"                             // common set of defines for securityService
#include "broadcastEvent.h"
#include "internal.h"
#include "eventListener.h"

/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_securityService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    //
    collectEventStatistics(output, input);
    collectIpcStatistics(get_securityService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

    // security stats
    getAlarmPanelStatsDetailsPublic(output);
    collectTroubleEventStatistics(output);
    collectArmDisarmFailureEvents(output);
    put_int_in_runtimeStatsPojo(output, "unackAlarmEvents", getAlarmMessagesNeedingAcknowledgementCount());

    output->serviceName = strdup(SECURITY_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    // TODO: more stats

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_securityService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
{
    // get trouble, zone, and alarm data
    //
    uint32_t troubleCount = getTroubleCountPublic(true);
    put_int_in_serviceStatusPojo(output, "TROUBLE_COUNT", troubleCount);
    getSecurityZoneStatusDetailsPublic(output);
    getAlarmPanelStatusDetailsPublic(output);

    // add number of un-acknowledged alarm messages we have in memory
    //
    put_int_in_serviceStatusPojo(output, "UNACK_ALARM_MESSAGES", getAlarmMessagesNeedingAcknowledgementCount());

    return IPC_SUCCESS;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   details - both the 'temp dir' the config was extracted to, and the 'target dir' of where to store
 */
IPCCode handle_securityService_CONFIG_RESTORED_request(configRestoredInput *input, configRestoredOutput* output)
{
    bool didSomething = false;

    if (supportSystemMode() == true)
    {
        // load system mode
        //
        if (restoreSystemModeConfig(input->tempRestoreDir, input->dynamicConfigPath) == true)
        {
            didSomething = true;
        }
    }

    if (supportAlarms() == true)
    {
        // restore security settings
        //
        if (restoreSecurityConfig(input->tempRestoreDir, input->dynamicConfigPath) == true)
        {
            didSomething = true;
        }
    }

    // restore trouble information if available
    if (restoreTroubleConfig(input->tempRestoreDir, input->dynamicConfigPath) == true)
    {
        didSomething = true;
    }

    output->action = (didSomething) ? CONFIG_RESTORED_RESTART : CONFIG_RESTORED_FAILED;

    return IPC_SUCCESS;
}

/**
 * Return the current systemMode
 *   output - response from service (implementation must allocate, and caller must free the string)
 **/
IPCCode handle_GET_CURRENT_SYSTEM_MODE_request(char **output)
{
    if (supportSystemMode() == true)
    {
        // get the mode, then convert/return the string label
        //
        systemModeSet mode = getCurrentSystemMode();
        *output = strdup(systemModeNames[mode]);
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "system does not support 'system mode'");
    return IPC_INVALID_ERROR;
}

/**
 * Switch from one systemMode to another
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_SET_CURRENT_SYSTEM_MODE_request(systemModeRequest *input, bool *output)
{
    if (supportSystemMode() == true)
    {
        systemModeSet mode = SYSTEM_MODE_HOME, loop;
        bool ok = false;

        // convert input from string to systemModeSet
        //
        for (loop = SYSTEM_MODE_HOME; loop <= SYSTEM_MODE_VACATION; loop++)
        {
            if (strcmp(input->systemMode, systemModeNames[loop]) == 0)
            {
                // found the match
                //
                ok = true;
                mode = loop;
                break;
            }
        }

        if (ok == true)
        {
            // perform the "set system mode"
            //
            if (setCurrentSystemMode(mode, input->requestId) == true)
            {
                // success
                //
                *output = true;
                return IPC_SUCCESS;
            }
        }

        // something went wrong
        //
        *output = false;
        return IPC_GENERAL_ERROR;
    }

    icLogWarn(SECURITY_LOG, "system does not support 'system mode'");
    return IPC_INVALID_ERROR;
}

/**
 * Return the list of known systemMode names
 *   output - response from service
 **/
IPCCode handle_GET_ALL_SYSTEM_MODES_request(systemModeList *output)
{
    // NOTE: doesn't matter if we support systemMode or not.  this is just informational

    // need a better way to do this, but loop through the known strings
    // and add them to 'output' (while assigning the count)
    //
    systemModeSet loop;
    for (loop = SYSTEM_MODE_HOME ; loop <= SYSTEM_MODE_VACATION ; loop++)
    {
        // dup the label, the shove into output
        //
        put_systemMode_in_systemModeList_list(output, strdup(systemModeNames[loop]));
    }

    return IPC_SUCCESS;
}

/**
 * get the systemMode configuration version
 *   output - response from service
 **/
IPCCode handle_SYSTEM_MODE_CONFIG_VERSION_request(uint64_t *output)
{
    // return the config file version
    //
    *output = getSystemModeConfigFileVersion();
    return IPC_SUCCESS;
}

/**
 * get the version of the Alarm configuration file
 *   output - response from service
 **/
IPCCode handle_ALARM_CONFIG_VERSION_request(uint64_t *output)
{
    *output = getSecurityConfigVersion();
    return IPC_SUCCESS;
}

/**
 * get the version of the User configuration file
 *   output - response from service
 **/
IPCCode handle_USER_CONFIG_VERSION_request(uint64_t *output)
{
    *output = getSecurityConfigVersion();
    return IPC_SUCCESS;
}

/**
 * Arm the system (standard mechanism)
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_ARM_SYSTEM_request(armRequest *input, armResult *output)
{
    if (supportAlarms() == true)
    {
        // pass along to alarmPanel
        //
        output->result = performArmRequestPublic(ARM_TYPE_DELAY,
                                                 (const char *) input->armCode,
                                                 input->armSource, input->armMode,
                                                 (int16_t) input->exitDelayOverrideSeconds, 0);
        icLogInfo(SECURITY_LOG, "arm request == %s", armResultTypeLabels[output->result]);

        // before returning see if this is a arm failure reason
        //
        addArmFailureEventToTracker(output->result);

        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "arm not supported");
    return IPC_INVALID_ERROR;
}

/**
 * Arm the system via Rule Execution
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_ARM_SYSTEM_FOR_RULE_request(armForRuleRequest *input, armResult *output)
{
    if (supportAlarms() == true)
    {
        // pass along to alarmPanel
        //
        output->result = performArmRequestPublic(ARM_TYPE_FROM_RULE,
                                                 NULL,  // user code
                                                 ARM_SOURCE_LOCAL_RULE, input->armMode,
                                                 (int16_t) input->exitDelayOverrideSeconds, input->token);
        icLogInfo(SECURITY_LOG, "arm from rule == %s", armResultTypeLabels[output->result]);

        // before returning see if this is a arm failure reason
        //
        addArmFailureEventToTracker(output->result);

        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "arm not supported");
    return IPC_INVALID_ERROR;
}

/**
 * Quick-Arm the system for alarm testing
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_ARM_SYSTEM_QUICK_FOR_ALARM_TEST_request(int32_t input, armResult *output)
{
    if (supportAlarms() == true)
    {
        // pass along to alarmPanel
        //
        output->result = performArmRequestPublic(ARM_TYPE_QUICK_FOR_TEST,
                                                 NULL,  // user code
                                                 ARM_SOURCE_CPE_KEYPAD, ARM_METHOD_AWAY,
                                                 (int16_t) input, 0);
        icLogInfo(SECURITY_LOG, "arm from quick test == %s", armResultTypeLabels[output->result]);

        // before returning see if this is a arm failure reason
        //
        addArmFailureEventToTracker(output->result);

        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "arm not supported");
    return IPC_INVALID_ERROR;
}

/**
 * Disarm the system (standard mechanism)
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_DISARM_SYSTEM_request(disarmRequest *input, disarmResult *output)
{
    if (supportAlarms() == true)
    {
        // pass along to alarmPanel
        //
        output->result = performDisarmRequestPublic(DISARM_TYPE_STANDARD, (const char *) input->disarmCode,
                                                    input->armSource, 0);
        icLogInfo(SECURITY_LOG, "disarm request == %s", disarmResultTypeLabels[output->result]);

        // before returning see if this is disarm failure reason
        //
        addDisarmFailureEventToTracker(output->result);

        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "disarm not supported");
    return IPC_INVALID_ERROR;
}

/**
 * Disarm the system via Rule Execution
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_DISARM_SYSTEM_FOR_RULE_request(char *token, disarmResult *output)
{
    if (supportAlarms() == true)
    {
        // pass along to alarmPanel
        //
        output->result = performDisarmRequestPublic(DISARM_TYPE_FROM_RULE, NULL, ARM_SOURCE_LOCAL_RULE, token);
        icLogInfo(SECURITY_LOG, "disarm for rule == %s", disarmResultTypeLabels[output->result]);

        // before returning see if this is disarm failure reason
        //
        addDisarmFailureEventToTracker(output->result);

        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "disarm not supported");
    return IPC_INVALID_ERROR;
}

/**
 * Disarm the system (standard mechanism)
 *   output - response from service (boolean)
 **/
IPCCode handle_DISARM_SYSTEM_FROM_TEST_request(bool *output)
{
    bool isAlarmSupported = supportAlarms();
    if (isAlarmSupported == true && isAlarmPanelInTestModePublic() == true)
    {
        // pass along to alarmPanel
        //
        disarmResultType result = performDisarmRequestPublic(DISARM_TYPE_FOR_TEST, NULL, ARM_SOURCE_CPE_KEYPAD, 0);
        icLogInfo(SECURITY_LOG, "disarm from test == %s", disarmResultTypeLabels[result]);
        if (result == SYSTEM_DISARM_SUCCESS)
        {
            *output = true;
        }
        else
        {
            *output = false;

            // since it was a failure add the reason to tracker
            addDisarmFailureEventToTracker(result);
        }
        return IPC_SUCCESS;
    }

    if (isAlarmSupported == false)
    {
        icLogWarn(SECURITY_LOG, "disarm not supported");
    }
    else
    {
        icLogWarn(SECURITY_LOG, "alarm panel is not in test mode");
    }
    return IPC_INVALID_ERROR;
}

/**
 * return the number of troubles known to the system
 *   input - if true, include 'acknowledged' troubles (boolean)
 *   output - response from service
 **/
IPCCode handle_GET_TROUBLE_COUNT_request(bool input, uint32_t *output)
{
    // ask troubleState for the number of troubles
    //
    *output = getTroubleCountPublic(input);
    return IPC_SUCCESS;
}

/**
 * Get the total list of known troubles
 *   input - IPC request - get all troubles
 *   output - IPC response - list of troubleObj
 **/
IPCCode handle_GET_TROUBLE_LIST_request(getTroublesInput *input, troubleObjList *output)
{
    // ask troubleState for a copy of the current troubles, placing
    // them into the list within the troubleEventList POJO
    //
    getTroublesPublic(output->troubles, TROUBLE_FORMAT_OBJ, input->includeAck, input->sortAlgo);
    return IPC_SUCCESS;
}

/**
 * Get the list of known troubles for a specific device
 *   input - IPC request - get troubles for a specific device/endpoint
 *   output - IPC response - list of troubleObj
 **/
IPCCode handle_GET_TROUBLES_FOR_DEVICE_request(getTroublesTargetedInput *input, troubleObjList *output)
{
    // ask troubleState for a copy of the current troubles, placing
    // them into the list within the troubleEventList POJO
    //
    if (input != NULL && input->targetId != NULL)
    {
        char *deviceUri = createDeviceUri(input->targetId);
        getTroublesForDeviceUriPublic(output->troubles, deviceUri, TROUBLE_FORMAT_OBJ, input->includeAck,
                                      input->sortAlgo);
        free(deviceUri);
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_INVALID_ERROR;
    }
}

/**
 * Get the list of known troubles for a specific device service uri prefix, could be a device uri, endpoint uri, or resource uri
 *   input - IPC request - get troubles for a specific device/endpoint
 *   output - IPC response - list of troubleObj
 **/
IPCCode handle_GET_TROUBLES_FOR_URI_request(getTroublesTargetedInput *input, troubleObjList *output)
{
    getTroublesForDeviceUriPublic(output->troubles, input->targetId, TROUBLE_FORMAT_OBJ, input->includeAck,
                                  input->sortAlgo);
    return IPC_SUCCESS;
}

/**
 * acknowledge a trouble.
 *   input - value to send to the service
 **/
IPCCode handle_ACK_TROUBLE_request(uint64_t input)
{
    acknowledgeTroublePublic(input);
    return IPC_SUCCESS;
}

/**
 * un-acknowledge a trouble.
 *   input - value to send to the service
 **/
IPCCode handle_UNACK_TROUBLE_request(uint64_t input)
{
    unacknowledgeTroublePublic(input);
    return IPC_SUCCESS;
}

/**
 * If defering troubles during sleep hours
 *   output - response from service (boolean)
 **/
IPCCode handle_IS_DEFER_TROUBLES_DURING_SLEEP_HOURS_ENABLED_request(bool *output)
{
    // get from our config
    *output = isDeferTroublesEnabled();
    return IPC_SUCCESS;
}

/**
 * Get the 'defer troubles during sleep hours' configuration.
 *   output - used for the 'defer troubles during sleep hours' feature
 **/
IPCCode handle_GET_DEFER_TROUBLES_CONFIG_request(deferTroublesConfig *output)
{
    // get from our config
    getDeferTroublesConfiguration(output);
    return IPC_SUCCESS;
}

/**
 * Set the 'defer troubles during sleep hours' configuration.
 *   input - used for the 'defer troubles during sleep hours' feature
 **/
IPCCode handle_SET_DEFER_TROUBLES_CONFIG_request(deferTroublesConfig *input)
{
    // pass along to config
    setDeferTroublesConfiguration(input);
    return IPC_SUCCESS;
}

/**
 * Get the current panel status
 *   output - response from service
 **/
IPCCode handle_GET_SYSTEM_PANEL_STATUS_request(systemPanelStatus *output)
{
    // get alarm panel state
    //
    populateSystemPanelStatusPublic(output);
    return IPC_SUCCESS;
}

/**
 * If the system is currently in alarm, returns the information about the alarm
 *   output - response to GET_CURRENT_ALARM_STATUS
 **/
IPCCode handle_GET_CURRENT_ALARM_STATUS_request(currentAlarmStatus *output)
{
    if (supportAlarms() == true)
    {
        // get status of the current alarm in progress
        //
        populateSystemCurrentAlarmStatusPublic(output);
    }
    return IPC_SUCCESS;
}

/**
 * Put the system into a panic mode
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_START_PANIC_ALARM_request(panicRequest *input, bool *output)
{
    if (supportAlarms() == true)
    {
        // pass along to alarmPanel
        //
        icLogInfo(SECURITY_LOG, "starting PANIC via IPC; type=%s source=%s", alarmPanicTypeLabels[input->panicType], armSourceTypeLabels[input->armSource]);
        uint64_t sesId = startPanicAlarmPublic(input->panicType, input->armSource);
        if (sesId > 0)
        {
            *output = true;
        }
        else
        {
            icLogInfo(SECURITY_LOG, "start of PANIC via IPC failed");
            *output = false;
        }
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "panic/alarms not supported");
    return IPC_INVALID_ERROR;
}

/**
 * Returns true if there is an un-acknowledged alarm session
 *   output - response from service (boolean)
 **/
IPCCode handle_HAS_ALARM_SESSION_TO_ACK_request(bool *output)
{
    if (supportAlarms() == true)
    {
        // ask alarmPanel how many we have to acknowledge
        //
        uint16_t count = getDormantAlarmSessionCountPublic();
        if (count == 0)
        {
            // nothing to ack
            //
            *output = false;
        }
        else
        {
            *output = true;
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "alarms not supported; unable to acknowledge alarm sessions");
        *output = false;
    }
    return IPC_SUCCESS;
}

/**
 * Acknowledges the alarm session.  Called by the UI once it knows the user has witnessed the alarm.
 **/
IPCCode handle_ACK_ALARM_SESSION_request()
{
    if (supportAlarms() == true)
    {
        // ask alarmPanel to do this
        //
        acknowledgeDormantAlarmSessionsPublic();
    }
    else
    {
        icLogWarn(SECURITY_LOG, "alarms not supported; unable to acknowledge alarm sessions");
    }
    return IPC_SUCCESS;
}

/**
 * Return if the system is in "test mode"
 *   output - response from service (boolean)
 **/
IPCCode handle_IN_TEST_MODE_request(bool *output)
{
    // ask alarmPanel
    *output = isAlarmPanelInTestModePublic();
    return IPC_SUCCESS;
}

/**
 * Puts the system is in "test mode".  If autoExitSeconds is greater than zero, test mode
will automatically terminate after the timeout.  this will block until we get the ACK_TEST_ALARM call, indicating
the server processed the change
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_SET_TEST_MODE_request(uint32_t input, alarmTestModeResult *output)
{
    // put the alarm panel in 'test mode' for a duration of time.
    // like the legacy code, this will block until we get the ack
    // saying the server processed the change
    //
    output->testResp = alarmPanelStartTestModePublic(input);
    return IPC_SUCCESS;
}

/**
 * Take the system out of "test mode"
 *   output - response from service (boolean)
 **/
IPCCode handle_UNSET_TEST_MODE_request(bool *output)
{
    // directly take the system out of test mode (vs waiting for the timer to expire)
    //
    alarmPanelEndTestModePublic();
    *output = true;
    return IPC_SUCCESS;
}

/**
 * gets the "test alarm send central station codes" enabled flag
 *   output - response from service (boolean)
 **/
IPCCode handle_IS_TEST_ALARM_SEND_CODE_ENABLED_request(bool *output)
{
    // ask our config
    //
    *output = isTestAlarmSendCodesSettingEnabled();
    return IPC_SUCCESS;
}

/**
 * sets the "test alarm send central station codes" enabled flag
 *   input - value to send to the service (boolean)
 **/
IPCCode handle_SET_TEST_ALARM_SEND_CODE_ENABLED_request(bool input)
{
    // ask our config
    //
    setTestAlarmSendCodesSettingEnabled(input);
    return IPC_SUCCESS;
}

/**
 * return the list of unfaulted zone ids during alarm test
 *   output - response from service
 **/
IPCCode handle_GET_UNFAULTED_ZONES_FOR_ALARM_TEST_request(unfaultedZoneIdsForAlarmTest *output)
{
    // TODO: get unfaulted zones for alarm test
    icLogError(SECURITY_LOG, "GET_UNFAULTED_ZONES_FOR_ALARM_TEST not supported yet");
    return IPC_GENERAL_ERROR;
}

/**
 * gets the "fire alarm verification" enabled flag
 *   output - response from service (boolean)
 **/
IPCCode handle_IS_FIRE_VERIFY_ENABLED_request(bool *output)
{
    // ask our config
    //
    *output = isFireAlarmVerificationSettingEnabled();
    return IPC_SUCCESS;
}

/**
 * sets the "fire alarm verification" enabled flag
 *   input - value to send to the service (boolean)
 **/
IPCCode handle_SET_FIRE_VERIFY_ENABLED_request(bool input)
{
    // pass to our config
    //
    setFireAlarmVerificationSettingEnabled(input);
    return IPC_SUCCESS;
}

/**
 * gets the "swinger shutdown" enabled flag
 *   output - response from service (boolean)
 **/
IPCCode handle_IS_SWINGER_SHUTDOWN_ENABLED_request(bool *output)
{
    // ask config
    //
    *output = isSwingerShutdownSettingEnabled();
    return IPC_SUCCESS;
}

/**
 * sets the "swinger shutdown" enabled flag
 *   input - value to send to the service (boolean)
 **/
IPCCode handle_SET_SWINGER_SHUTDOWN_ENABLED_request(bool input)
{
    // forward to the config
    //
    setSwingerShutdownSettingEnabled(input);
    return IPC_SUCCESS;
}

/**
 * gets the "swinger shutdown" max trip count
 *   output - response from service
 **/
IPCCode handle_GET_SWINGER_SHUTDOWN_MAX_TRIPS_request(uint32_t *output)
{
    // ask config
    //
    *output = getSwingerShutdownMaxTripsSetting();
    return IPC_SUCCESS;
}

/**
 * sets the "swinger shutdown" max trip count
 *   input - value to send to the service
 **/
IPCCode handle_SET_SWINGER_SHUTDOWN_MAX_TRIPS_request(uint32_t input)
{
    // forward to the config
    //
    if(setSwingerShutdownMaxTripsSetting((uint8_t) input))
    {
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_INVALID_ERROR;
    }
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_SWINGER_SHUTDOWN_MAX_TRIPS_RANGE_request(validAlarmRange *output)
{
    // steal from our config header
    //
    output->min = SWINGER_TRIPS_MIN;
    output->max = SWINGER_TRIPS_MAX;
    return IPC_SUCCESS;
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_ENTRY_DELAY_request(uint32_t *output)
{
    // ask config
    //
    *output = getEntryDelaySecsSetting();
    return IPC_SUCCESS;
}

/**
 *   input - value to send to the service
 **/
IPCCode handle_SET_ENTRY_DELAY_request(uint32_t input)
{
    // tell config
    //
    if(setEntryDelaySecsSetting((uint16_t)input))
    {
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_INVALID_ERROR;
    }
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_ENTRY_DELAY_RANGE_request(validAlarmRange *output)
{
    // steal from the config header
    //
    output->min = ENTRY_DELAY_SEC_MIN;
    output->max = ENTRY_DELAY_SEC_MAX;
    return IPC_SUCCESS;
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_EXIT_DELAY_request(uint32_t *output)
{
    // get from config
    //
    *output = getExitDelaySecsSetting();
    return IPC_SUCCESS;
}

/**
 *   input - value to send to the service
 **/
IPCCode handle_SET_EXIT_DELAY_request(uint32_t input)
{
    // pass to config
    //
    if(setExitDelaySecsSetting((uint16_t)input))
    {
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_INVALID_ERROR;
    }
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_EXIT_DELAY_RANGE_request(validAlarmRange *output)
{
    // steal from config header
    //
    output->min = EXIT_DELAY_SEC_MIN;
    output->max = EXIT_DELAY_SEC_MAX;
    return IPC_SUCCESS;
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_DIALER_DELAY_request(uint32_t *output)
{
    // get from config
    //
    *output = getDialerDelaySecsSetting();
    return IPC_SUCCESS;
}

/**
 *   input - value to send to the service
 **/
IPCCode handle_SET_DIALER_DELAY_request(uint32_t input)
{
    // save in config
    //
    if(setDialerDelaySecsSetting((uint16_t)input))
    {
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_INVALID_ERROR;
    }
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_DIALER_DELAY_RANGE_request(validAlarmRange *output)
{
    // grab from the config header
    //
    output->min = DIALER_DELAY_SEC_MIN;
    output->max = DIALER_DELAY_SEC_MAX;
    return IPC_SUCCESS;
}

/**
 * add a cross-zone association
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_ADD_CROSS_ZONE_ASSOC_request(crossZoneAssociation *input, crossZoneResult *output)
{
    // TODO: Needs implementation
    icLogWarn(SECURITY_LOG, "cross-zones not supported");
    return IPC_INVALID_ERROR;
}

/**
 * update a cross-zone association
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_MOD_CROSS_ZONE_ASSOC_request(crossZoneAssociation *input, crossZoneResult *output)
{
    // TODO: Needs implementation
    icLogWarn(SECURITY_LOG, "cross-zones not supported");
    return IPC_INVALID_ERROR;
}

/**
 * delete a cross-zone association
 *   input - value to send to the service
 **/
IPCCode handle_DEL_CROSS_ZONE_ASSOC_request(crossZoneAssociation *input)
{
    // TODO: Needs implementation
    icLogWarn(SECURITY_LOG, "cross-zones not supported");
    return IPC_INVALID_ERROR;
}

/**
 * return the cross-zone associations
 *   output - response from service
 **/
IPCCode handle_GET_CROSS_ZONE_ASSOC_request(crossZoneAssociationList *output)
{
    // TODO: Needs implementation
    icLogWarn(SECURITY_LOG, "cross-zones not supported");
    return IPC_INVALID_ERROR;
}

/**
 * add a user code
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_ADD_USER_CODE_request(keypadUserCodeRequest *input, bool *output)
{
    if (supportAlarms() == true)
    {
        // send to our config, and if successful broadcast the event
        //
        *output = addUserCode(input->userCode);
        if (*output == true)
        {
            // send event from here. no other reason to broadcast the event unless
            // it comes from an IPC request.
            //
            broadcastUserCodeEvent(ALARM_EVENT_USER_CODE_ADDED, getSecurityConfigVersion(), input->userCode, input->armSource);
        }
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "user codes not supported");
    return IPC_INVALID_ERROR;
}

/**
 * update a user code
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_MOD_USER_CODE_request(keypadUserCodeRequest *input, bool *output)
{
    if (supportAlarms() == true)
    {
        // send to our config, and if successful broadcast the event
        //
        *output = updateUserCode(input->userCode);
        if (*output == true)
        {
            // send event from here. no other reason to broadcast the event unless
            // it comes from an IPC request.
            //
            broadcastUserCodeEvent(ALARM_EVENT_USER_CODE_MOD, getSecurityConfigVersion(), input->userCode, input->armSource);
        }
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "user codes not supported");
    return IPC_INVALID_ERROR;
}

/**
 * delete a user code.  only looks at uuid and source
 *   input - value to send to the service
 **/
IPCCode handle_DEL_USER_CODE_request(keypadUserCodeRequest *input)
{
    if (supportAlarms() == true)
    {
        // send to our config, and if successful broadcast the event
        //
        if (deleteUserCode(input->userCode) == true)
        {
            // send event from here. no other reason to broadcast the event unless
            // it comes from an IPC request.
            //
            broadcastUserCodeEvent(ALARM_EVENT_USER_CODE_DEL, getSecurityConfigVersion(), input->userCode, input->armSource);
        }
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "user codes not supported");
    return IPC_INVALID_ERROR;
}

/**
 * return the known user codes
 *   output - response from service
 **/
IPCCode handle_GET_USER_CODES_request(keypadUserCodeList *output)
{
    if (supportAlarms() == true)
    {
        // ask our config to get ALL user codes
        //
        linkedListDestroy(output->userCodes, freeKeypadUserCodeFromList);
        output->userCodes = getAllUserCodes(true);
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "user codes not supported");
    return IPC_INVALID_ERROR;
}

/**
 * validate the user code and return the authorization level
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_VALIDATE_USER_CODE_request(char *input, keypadCodeValidation *output)
{
    if (supportAlarms() == true)
    {
        // TODO: Needs implementation
        // ask our user authenticator
        //
        output->authorityLevel = KEYPAD_USER_LEVEL_INVALID;
        return IPC_SUCCESS;
    }

    icLogWarn(SECURITY_LOG, "user codes not supported");
    return IPC_INVALID_ERROR;
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_ALL_ZONE_NUMBERS_request(securityZoneNumList *output)
{
    // TODO: get all zone numbers
    icLogError(SECURITY_LOG, "GET_ALL_ZONE_NUMBERS not supported yet");
    return IPC_GENERAL_ERROR;
}

/**
 *   output - response from service
 **/
IPCCode handle_GET_ALL_ZONES_request(securityZoneList *output)
{
    // copy each known securityZone into the output->list
    //
    extractAllSecurityZonesPublic(output->zoneArray);
    return IPC_SUCCESS;
}

/**
 *   input - zoneNumber to query for
 *   output - define a 'security' zone (one that can be associated with alarmService)
 **/
IPCCode handle_GET_ZONE_FOR_NUM_request(uint32_t input, securityZone *output)
{
    // get the zone with this 'zoneNumber'
    //
    if (extractSecurityZoneForNumberPublic(input, output) == true)
    {
        return IPC_SUCCESS;
    }

    // unable to locate the zone for this number
    //
    return IPC_INVALID_ERROR;
}

/*
 * checks to see if we're armed, alarming, arming, or upgrading - which
 * would prevent a modification to the zone
 */
static updateZoneResultCode canUpdateZones()
{
    updateZoneResultCode rc = UPDATE_ZONE_SUCCESS;

    // first see if we're armed
    //
    systemPanelStatus *panelStatus = create_systemPanelStatus();
    populateSystemPanelStatusPublic(panelStatus);
    if(panelStatus->alarmStatus == ALARM_STATUS_ARMED ||
       panelStatus->alarmStatus == ALARM_STATUS_ARMING ||
       panelStatus->alarmStatus == ALARM_STATUS_ALARM ||
       panelStatus->alarmStatus == ALARM_STATUS_ENTRY_DELAY)
    {
        rc = UPDATE_ZONE_FAIL_ARMED_ARMING;
    }
    destroy_systemPanelStatus(panelStatus);

    return rc;
}

/**
 *   input - value to send to the service
 *   output - if update was successful (boolean)
 **/
IPCCode handle_UPDATE_ZONE_request(updateSecurityZoneRequest *input, updateSecurityZoneResult *output)
{
    // first check state to see if this is allowed right now
    //
    updateZoneResultCode check = canUpdateZones();
    if (check != UPDATE_ZONE_SUCCESS)
    {
        output->resultCode = check;
        icLogWarn(SECURITY_LOG, "UPDATE_ZONE request denied; %s", updateZoneResultCodeLabels[check]);
        return IPC_SUCCESS;
    }

    // now attempt to update the zone with data in 'input'
    //
    output->resultCode = updateSecurityZonePublic(input->zone, input->requestId);
    icLogDebug(SECURITY_LOG, "UPDATE_ZONE request returning rc = %s", updateZoneResultCodeLabels[output->resultCode]);
    return IPC_SUCCESS;
}

/**
 *   input - IPC request - input to toggle bypass a securityZone
 *   output - response from service (boolean)
 **/
IPCCode handle_BYPASS_ZONE_TOGGLE_request(bypassZoneToggleRequest *input, bool *output)
{
    // not sure why, but we don't ever check to see if this is acceptable when armed
    //
    if(bypassToggleSecurityZonePublic(input->displayIndex, input->userCode, input->bypassSource, input->requestId) == true)
    {
        *output = true;
    }
    else
    {
        *output = false;
    }
    icLogDebug(SECURITY_LOG, "BYPASS_ZONE_TOGGLE request returning %s", (*output == true) ? "true" : "false");
    return IPC_SUCCESS;
}

/**
 *   input - value to send to the service
 *   output - if update was successful (boolean)
 **/
IPCCode handle_REMOVE_ZONE_request(removeSecurityZoneRequest *input, bool *output)
{
    // remove the zone using the provided zoneNumber
    //
    if (removeSecurityZonePublic(input->zoneNum, input->requestId) == true)
    {
        *output = true;
    }
    else
    {
        *output = false;
    }
    icLogDebug(SECURITY_LOG, "REMOVE_ZONE request returning %s", (*output == true) ? "true" : "false");
    return IPC_SUCCESS;
}

/**
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_REORDER_ZONES_request(securityZoneNumList *input, bool *output)
{
    // TODO: reorder zones
    return IPC_GENERAL_ERROR;
}

/**
 * To retrieve list of zones that prevent arming
 *   output - response from service
 **/
IPCCode handle_GET_ZONES_PREVENT_ARMING_request(securityZoneArmStatusDetailsList *output)
{
    // Free up the memory in the output list, should be empty
    linkedListDestroy(output->zoneArmStatusDetails, (linkedListItemFreeFunc)destroy_securityZoneArmStatusDetails);
    // Populate with our details
    output->zoneArmStatusDetails = getAllZoneArmStatusPublic();

    return IPC_SUCCESS;
}

/**
 * slight variation to GET_ZONES_PREVENT_ARMING to perform a quick check to see if something is faulted or troubled
 *   output - response from service (boolean)
 **/
IPCCode handle_ARE_ZONES_PREVENT_ARMING_request(bool *output)
{
    if (supportAlarms() == true)
    {
        // ask alarmPanel
        *output = areAnyZonesFaultedOrTroubledPublic();
    }
    else
    {
        icLogDebug(SECURITY_LOG, "alarms not supported, returning false for ARE_ZONES_PREVENT_ARMING");
        *output = false;
    }
    return IPC_SUCCESS;
}

