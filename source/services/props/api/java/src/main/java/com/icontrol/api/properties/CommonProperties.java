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

package com.icontrol.api.properties;

public interface CommonProperties
{
    // well known property keys, many are also defined and used by the native layer
    //
    public static final String TAMPER_ENABLED_BOOL_PROP                     = "device.tamper.enabled"; // cannot change, one form comes from server
    public static final String LOGGING_DEBUG_BOOL_PROP                      = "logging.debug";
    public static final String SSL_CERT_VALIDATE_FOR_HTTPS_TO_SERVER        = "cpe.sslCert.validateForHttpsToServer";
    public static final String SSL_CERT_VALIDATE_FOR_HTTPS_TO_DEVICE        = "cpe.sslCert.validateForHttpsToDevice";
    public static final String COREDUMPS_SAVE                               = "coredumps.save";
//  public static final String TELEMETRY_CAPABILITIES                       = "telemetry.capabilities";
//  public static final String TELEMETRY_STATE                              = "telemetry.state";
//  public static final String TELEMETRY_ALLOW_UPLOAD                       = "telemetry.upload";
    public static final String TELEMETRY_MAX_ALLOWED_FILE_STORAGE           = "telemetry.maxAllowedFileStorage";
    public static final String TELEMETRY_HOURS_REMAINING                    = "telemetry.hoursRemaining";
    public static final String IGNORE_BATTERY_BOOL_PROPERTY                 = "ignore.removed.battery.flag";
    public static final String IGNORE_SCREENSAVER_BOOL_PROPERTY             = "ignore.screensaver.flag";
    public static final String CONFIG_FASTBACKUP_BOOL_PROPERTY              = "config.fastbackuptimer.flag";
    public static final String IGNORE_CORES_BOOL_PROPERTY                   = "ignore.cores.flag";
    public static final String NO_CAMERA_UPGRADE_BOOL_PROPERTY              = "camera.noupgrade.flag";
    public static final String GUARDIAN_BYPASS_BOOL_PROPERTY                = "guardian.bypass.flag";
    public static final String REMOTE_SIREN_SILENT_BOOL_PROPERTY            = "siren.remote.silent.flag";
    public static final String LOCAL_PIEZO_SILENT_BOOL_PROPERTY             = "siren.onboardpiezo.silent.flag";
    public static final String TOUCHSCREEN_DURESSCODE_DISABLED              = "touchscreen.duressCode.disabled";
    public static final String TOUCHSCREEN_SPEAKER_SILENT_BOOL_PROPERTY     = "touchscreen.speaker.silent.flag";
    public static final String ZIGBEE_FW_UPGRADE_NO_DELAY_BOOL_PROPERTY     = "zigbee.fw.upgrade.nodelay.flag";
    public static final String CPE_GEOCODING_ENABLED_BOOL                   = "cpe.geocoding.enabled.flag";
    public static final String CAM_MOTION_BLACKOUT_OVERRIDE_SECONDS_PROPERTY = "cam.motion.blackout.override.seconds";
    public static final String DISCOVER_DISABLED_DEVICES_BOOL_PROPERTY      = "discover.disabled.devices.flag";
//  public static final String TELEMETRY_FAST_UPLOAD_TIMER_BOOL_PROPERTY    = "telemetry.fast.upload.timer.flag";
    public static final String CAMERA_FW_UPGRADE_DELAY_SECONDS_PROPERTY     = "camera.fw.update.delay.seconds";
//  public static final String ENABLE_CORES_DURING_UPGRADE                  = "corefiles.enabled.during.upgrade.flag";
    public static final String EVENTQUEUE_DUMP_ENABLED                      = "eventqueue.dump.enabled";
    public static final String HOME_BUTTON_ENABLED_PROPERTY                 = "homebutton.enabled.flag";
    public static final String PERSIST_CPE_SETUPWIZARD_STATE                = "persist.cpe.setupwizard.state";
    public static final String PNP_UN_MANAGED_NETWORK_FORCE                 = "pnp.unmanaged.network.force";

    public static final String ALARM_SKIP_POLICE_PANIC_IMMEDIATE_AUDIBLE = "alarm.skipPolicePanicImmediateAudibleAlarm";
    public static final String DEVICE_DESC_BLACKLIST                     = "deviceDescriptor.blacklist";
    public static final String QUICKARM_COUNTDOWN                        = "touchscreen.quickarmcountdown.length";
    public static final String TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY = "touchscreen.sensor.commFail.troubleDelay";
    public static final String TOUCHSCREEN_SENSOR_COMMFAIL_ALARM_DELAY   = "touchscreen.sensor.commFail.alarmDelay";
    public static final String MAX_CAMERAS_ALLOWED                       = "touchscreen.camera.maxAllowed";
    public static final String CAMERA_MOTION_BLACKOUT_SECONDS            = "camera.motion.blackoutSeconds";
    public static final String MAX_DOOR_LOCKS_ALLOWED                    = "touchscreen.doorLock.maxAllowed";
    public static final String RESET_TO_FACTORY_ENABLED                  = "cpe.allowResetToFactory";
    public static final String TROUBLE_ACK_EXPIRATION_TIME_MINUTES       = "cpe.troubles.ackExpireMinutes";
    public static final String TROUBLE_DEFER_SLEEP_HOURS                 = "cpe.troubles.deferSleepHours";
    public static final String REDIRECTOR_ENABLED                        = "redirector.enabled";
    public static final String REDIRECTOR_SERVER_CATEGORY                = "redirector.serverCategory";
    public static final String REDIRECTOR_POD_NAME                       = "redirector.podName";
    public static final String REDIRECTOR_CLIENT_URL                     = "redirector.clientUrl";
    public static final String XCONF_URL                                 = "cpe.xconf.url";
    public static final String XCONF_FIRMWARE_CONFIGURATION_QUERY_RANGE  = "cpe.xconf.firmwareConfigurationQueryRange";
    public static final String XCONF_FEAUTURE_CONTROL_QUERY_INTERVAL     = "cpe.xconf.featureControlQueryInterval";
    public static final String CURRENT_XHUI_VERSION                      = "cpe.xhui.version";
    //properties starting with this prefix are just forwarded along to ZigbeeService as global properties
    public static final String ZIGBEE_SUBSYSTEM_PREFIX                   = "cpe.zigbee.";
    public static final String TWV_MAX_CALL_LEN_KEY                      = "cellular.twoWayVoice.maxCallLength";
    public static final String TWV_ENABLED_KEY                           = "cellular.twoWayVoice.enabled";
    public static final String DISPLAY_TECH_APP_PROP                     = "activation.techapp.enabled";
    public static final String MAX_PRMS_ALLOWED                          = "cpe.prm.maxAllowed";
    public static final String ASK_INSTALLER_SETTING_ENTRY_SCREEN        = "cpe.settings.installerEntryScreen";
    public static final String MIN_TROUBLE_VOLUME_TIER_PROPERTY          = "cpe.troubles.minTroubleVolume";
    public static final String MIN_HOMETONE_VOLUME_TIER_PROPERTY         = "cpe.hometones.minHometoneVolume";
    public static final String TIER_PROP_CPE_CAMERA_PING_INTERVAL_SEC    = "cpe.camera.pingIntervalSeconds";
    public static final String TIER_PROP_CPE_CAMERA_ONLINE_DETECTION_THRESHOLD_CNT = "cpe.camera.onlineDetectionThreshold";
    public static final String TIER_PROP_CPE_CAMERA_OFFLINE_DETECTION_THRESHOLD_CNT = "cpe.camera.offlineDetectionThreshold";
    public static final String TIER_PROP_TS_CAMERA_MAX_ALLOW             = "touchscreen.camera.maxAllowed";
    public static final String GATEWAY_MIGRATION_ENABLED                 = "cpe.gateway.migration.enabled";
    public static final String NO_ALARM_ON_COMM_FAILURE                  = "cpe.troubles.noAlarmOnCommFailure";
    public static final String TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY = "cpe.troubles.annunciationIntervalMinutes";
    public static final String MAX_PRE_LOW_BATTERY_DAYS_PROP             = "cpe.trouble.preLowBatteryDays";
    public static       String SERVER_PROP_KEY                           = "cpe.troubles.ackExpireMinutes";
    public static final String TVR_ENABLED                               = "tvr.enabled";
    public static final String TVR_BANNER_ENABLED                        = "tvr.bannerEnabled";
    public static final String PROPERTY_PREFIX                           = "cpe.diagnostics.";
    public static final String PANIC_SCREEN_DISABLED                     = "panicScreenDisabled";
    public static final String CPE_WIFI_FREQUENCY_BAND                   = "cpe.wifi.frequency.band";

    // Changes to support the latest UL 985 standard
    public static final String SAFETY_TROUBLES_ACK_REQUIRES_PIN_TIER_PROPERTY = "cpe.troubles.safety.acknowledgementRequiresPIN";

    // for LnF recovery, to allow the server as well as command line (xconf is already supported) setting of the property
    public static final String LNF_RECOVERY_ENABLED_PROP_KEY           = "cpe.gatewaySync.lnfRecoveryEnabled";
    public static final String LNF_RECOVERY_ATTEMPTS_KEY               = "cpe.gatewaySync.lnfRecoveryAttempts";
    public static final String LNF_RECOVERY_RETRY_INTERVAL_KEY         = "cpe.gatewaySync.lnfRecoveryRetryIntervalSecs";
    public static final String LNF_RECOVERY_EAP_JOIN_TIMEOUT_KEY       = "cpe.gatewaySync.lnfEapJoinTimeoutSecs";
    public static final String LNF_RECOVERY_CREDS_AND_JOIN_TIMEOUT_KEY = "cpe.gatewaySync.lnfGetCredsAndJoinTimeoutSecs";

    // for cell adapter campaign
    public static final String CELL_ADAPTER_INSTALLATION_STATUS        = "cpe.cellAdapter.installationStatus";

    // for audio
    public static final String TTS_ENABLED                              = "cpe.tts.enabled";
    // a float value where 1.0 is "normal speech rate", .5 is half the normal speech rate, etc.
    public static final String TTS_RATE                                 = "cpe.tts.rate";

    public static final int DEFAULT_MAX_CAMERAS_ALLOWED = 6;
    public static final int DEFAULT_MAX_DOOR_LOCKS_ALLOWED = 4;
}
