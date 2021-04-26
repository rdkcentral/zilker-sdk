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
 * commonProperties.h
 *
 * List of well-known properties that are used by
 * many different clients/services
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IC_COMMONPROPERTIES_H
#define IC_COMMONPROPERTIES_H

#include <stddef.h>
#include <icBuildtime.h>

// properties dealing with path locations.  most are model specific
//
#define IC_DYNAMIC_DIR_PROP     "IC_DYNAMIC_DIR"        // ex: /opt
#define IC_STATIC_DIR_PROP      "IC_STATIC_DIR"         // ex: /vendor
#define IC_SOS_DIR_PROP         "IC_SOS_CACHE"          // ex: /data/sos

// properties that are ensured to be available (also shared with server)
//
#define TELEMETRY_CAPABILITIES                    "telemetry.capabilities"
#define TELEMETRY_STATE                           "telemetry.state"
#define TELEMETRY_DAYS_REMAINING                  "telemetry.daysRemaining"
#define TELEMETRY_FAST_UPLOAD_TIMER_BOOL_PROPERTY "telemetry.fast.upload.timer.flag"
#define TELEMETRY_ALLOW_UPLOAD                    "telemetry.upload"
#define TELEMETRY_MAX_ALLOWED_FILE_STORAGE        "telemetry.maxAllowedFileStorage"
#define TELEMETRY_HOURS_REMAINING                 "telemetry.hoursRemaining"
#define TELEMETRY_HOURS_REMAINING_CURRENT_VAL     "telemetry.hoursRemaining.currentValue"
#define CONFIG_FASTBACKUP_BOOL_PROPERTY           "config.fastbackuptimer.flag"
#define DISCOVER_DISABLED_DEVICES_BOOL_PROPERTY   "discover.disabled.devices.flag"
#define NO_CAMERA_UPGRADE_BOOL_PROPERTY           "camera.noupgrade.flag"
#define CAMERA_FW_UPGRADE_DELAY_SECONDS_PROPERTY  "camera.fw.update.delay.seconds"
#define CPE_BLACKLISTED_DEVICES_PROPERTY_NAME     "cpe.blacklistedDevices"

// timezone property.  if set, should match a key in the etc/timezone.properties file
//
#define POSIX_TIME_ZONE_PROP    "POSIX_TZ"      /* POSIX 1003.1 mapping of what the server provided (ex: CST6CDT,M3.2.0,M11.1.0) */

// properties we receive "from" the server (via server properties)
//
#define SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER                 "cpe.sslCert.validateForHttpsToServer"
#define SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE                 "cpe.sslCert.validateForHttpsToDevice"
#define DEVICE_DESC_BLACKLIST                                 "deviceDescriptor.blacklist"
#define CPE_CAMERA_PING_INTERVAL_SEC                          "cpe.camera.pingIntervalSeconds"
#define CPE_CAMERA_ONLINE_DETECTION_THRESHOLD_CNT             "cpe.camera.onlineDetectionThreshold"
#define CPE_CAMERA_OFFLINE_DETECTION_THRESHOLD_CNT            "cpe.camera.offlineDetectionThreshold"

#define MAX_CAMERAS_ALLOWED                                   "touchscreen.camera.maxAllowed"
#define CAMERA_MOTION_BLACKOUT_SECONDS                        "camera.motion.blackoutSeconds"
#define MAX_DOOR_LOCKS_ALLOWED                                "touchscreen.doorLock.maxAllowed"
#define DEVICE_DESC_BLACKLIST_URL_OVERRIDE                    "deviceDescriptor.blacklist.url.override"
#define DEVICE_DESC_WHITELIST_URL_OVERRIDE                    "deviceDescriptor.whitelist.url.override"
#define NO_ALARM_ON_COMM_FAILURE                              "cpe.troubles.noAlarmOnCommFailure"
#define DURESSCODE_DISABLED                                   "touchscreen.duressCode.disabled"
#define PRELOW_BATTERY_DAYS_PROPERTY                          "cpe.trouble.preLowBatteryDays"
#define PRELOW_BATTERY_DAYS_DEV_PROPERTY                      "cpe.trouble.preLowBatteryDays.dev.flag"
#define DEFAULT_PRE_LOW_BATTERY_DAYS                          0
#define CPE_ZIGBEE_REPORT_DEVICE_INFO_ENABLED                 "cpe.zigbee.reportDeviceInfo.enabled"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_ENABLED                     "cpe.diagnostics.zigBeeData.enabled"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_PER_CHANNEL_NUMBER_OF_SCANS "cpe.diagnostics.zigBeeData.numberOfScansPerChannel"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DURATION_MS    "cpe.diagnostics.zigBeeData.channelScanDurationMs"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_CHANNEL_SCAN_DELAY_MS       "cpe.diagnostics.zigBeeData.perChannelScanDelayMs"
#define CPE_DIAGNOSTIC_ZIGBEEDATA_COLLECTION_DELAY_MIN        "cpe.diagnostics.zigBeeData.preCollectionDelayMinutes"
#define CPE_ZIGBEE_CHANNEL_CHANGE_ENABLED_KEY                 "cpe.zigbee.channelChange.enabled"
#define CPE_ZIGBEE_CHANNEL_CHANGE_MAX_REJOIN_WAITTIME_MINUTES "cpe.zigbee.channelChange.maxWaitTimeMin"
#define ZIGBEE_FW_UPGRADE_NO_DELAY_BOOL_PROPERTY              "zigbee.fw.upgrade.nodelay.flag"
#define CPE_GEOCODING_ENABLED_BOOL                            "cpe.geocoding.enabled.flag"
#define CPE_DEBUG_GEOCODING_BOOL                              "cpe.geocoding.debugging.flag"
#define ZIGBEE_HEALTH_CHECK_INTERVAL_MILLIS                   "cpe.zigbee.healthCheck.intervalMillis"
#define ZIGBEE_HEALTH_CHECK_CCA_THRESHOLD                     "cpe.zigbee.healthCheck.ccaThreshold"
#define ZIGBEE_HEALTH_CHECK_CCA_FAILURE_THRESHOLD             "cpe.zigbee.healthCheck.ccaFailureThreshold"
#define ZIGBEE_HEALTH_CHECK_RESTORE_THRESHOLD                 "cpe.zigbee.healthCheck.restoreThreshold"
#define ZIGBEE_HEALTH_CHECK_DELAY_BETWEEN_THRESHOLD_RETRIES_MILLIS "cpe.zigbee.healthCheck.delayBetweenThresholdRetriesMillis"
#define PKI_CERT_CA_NAME                                      "pki.cert.ca.name"
#define PKI_CERT_VALIDITY_DAYS                                "pki.cert.validityDays"
#define PKI_CERT_MINIMUM_REMAINING_VALIDITY_DAYS              "pki.cert.minRemainingValidityDays"

// configuration values we receive "from" the server (via server config)
//
#define CONVERGE_CAMERA_TROUBLE_ENABLED                     "convergeCameraTroubleEnabled"
#define DEVICE_DESCRIPTOR_LIST                              "deviceDescriptorList"
#define DEVICE_FIRMWARE_URL_NODE                            "deviceFirmwareBaseUrl"
#define CAMERA_FIRMWARE_URL_NODE                            "cameraFirmwareBaseUrl"
#define MOTION_EVENT_BLACKOUT_NODE                          "motionEventsBlackoutPeriod"
#define TELEMETRY_EXPIRATION_DAYS_NODE                      "maxTelemetryCollectionDays"
#define PAN_ID_CONFLICT_ENABLED_PROPERTY_NAME               "cpe.zigbee.panIdConflict.enabled"
#define MAX_CAMERAS_TO_DISCOVER_PROP_NAME                   "camera.maxToDiscover"
#define ZIGBEE_DEFENDER_PAN_ID_CHANGE_THRESHOLD_OPTION      "cpe.zigbee.defender.panIdChangeThreshold"
#define ZIGBEE_DEFENDER_PAN_ID_CHANGE_WINDOW_MILLIS_OPTION  "cpe.zigbee.defender.panIdChangeWindowMillis"
#define ZIGBEE_DEFENDER_PAN_ID_CHANGE_RESTORE_MILLIS_OPTION "cpe.zigbee.defender.panIdChangeRestoreMillis"

// possible string values for "cpe.sslCert.validate*" properties
//
#define SSL_VERIFICATION_TYPE_NONE "none"
#define SSL_VERIFICATION_TYPE_HOST "host"
#define SSL_VERIFICATION_TYPE_PEER "peer"
#define SSL_VERIFICATION_TYPE_BOTH "both"

// Internal CPE properties
//
#define XCONF_TELEMETRY_MAX_FILE_ROLL_SIZE  "xconf.telemetry.max.roll.size"
#define PKI_CERT_CA_NAME                    "pki.cert.ca.name"
#define TEST_FASTTIMERS_PROP                "cpe.tests.fastTimers"

// Enum for the Property "persist.cpe.setupwizard.state"
//
#define PERSIST_CPE_SETUPWIZARD_STATE       "persist.cpe.setupwizard.state"
typedef enum
{
    ACTIVATION_NOT_STARTED      = -1,   // activation has not started
    ACTIVATION_STARTED          = 0,    // activation has started
    CLOUD_ASSOCIATION_COMPLETE  = 1,    // done with cloud association (meaning activated with the server)
    ACTIVATION_COMPLETE         = 2,    // done with activation (THIS IS THE TRUE "activation complete")
    ACTIVATION_FLOW_FINISHED    = 3     // the finish button has been pushed (this allows LPM, screen, and Invoker to work properly)
} setupWizardState;

#endif // IC_COMMONPROPERTIES_H

