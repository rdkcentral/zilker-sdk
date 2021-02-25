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

//
// Created by tlea on 5/7/18.
//
// Original code from Clay Dearman's work in zigbee_firmware repo.
//

#ifndef ZILKER_UC_COMMON_H
#define ZILKER_UC_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include "deviceDriver.h"

////////////////////////////////////////////////////////////////
// common definitions of device information, messages, etc.
// that is shared between all device types
////////////////////////////////////////////////////////////////
#define NUM_UC_SENSORS  (48)
#define UC_MFG_ID_WRONG (0x4256)
#define UC_MFG_ID       (0x10A0)
#define RTCOA_MFG_ID    (0xABCD)
#define LEGACY_FW_VER_MAX 0xFFFFFE

#define EDEVNOTDISC (-10)

// Ember does not define this cluster by default.
#define ZCL_ALARMS_CLUSTER_ID 0x09

#define TAKEOVER_ZONE_MSG_MAX_LEN 16

// system device types - kept for legacy needs.
// DO NOT ADD NEW SENSOR TYPES HERE, ONLY ON TS!!!
typedef enum
{
    LEGACY_DEVICE_TYPE_INVALID = 0x00,
    DOOR_WINDOW_1 = 0x01,
    SMOKE_1 = 0x02,
    MOTION_1 = 0x03, // Innovonics, small GE
    GLASS_BREAK_1 = 0x04,
    WATER_1 = 0x05,
    INERTIA_1 = 0x06,
    KEYFOB_1 = 0x07,
    REPEATER_SIREN_1 = 0x08,
    MOTION_BIG_GE = 0x09,
    MOTION_SUREN_W_RF = 0x0A, // Suren w/daughter card
    KEYPAD_1 = 0x0B,
    TAKEOVER_1 = 0x0C,
    MICRO_DOOR_WINDOW = 0x0D,
    MTL_REPEATER_SIREN = 0x0E,
    GE_CO_SENSOR_1 = 0x0F,
    // 0x10 is reserved...
            MTL_DOOR_WINDOW = 0x11,
    MTL_GLASS_BREAK = 0x12,
    MTL_GE_SMOKE = 0x13,
    MTL_GE_MOTION = 0x14,
    MTL_SUREN_MOTION = 0x15,
    MTL_GE_CO_SENSOR = 0x16,
    MTL_ED_CO_SENSOR = 0x17, // Everyday CO Sensor
//    MTL_QG_CO_SENSOR    = 0x17, // Quantum Group CO Sensor - not used
//    QG_CO_SENSOR_1      = 0x18, // Quantum Group CO Sensor - not used

    SMC_MOTION = 0x19,
    SMC_SMOKE = 0x1A,
    SMC_SMOKE_NO_SIREN = 0x1B,
    SMC_GLASS_BREAK = 0x1C,
    SMC_CO_SENSOR = 0x1D,

    // Visonic devices...
            MCT_320_DW = 0x20,
    CLIP_CURTAIN = 0x21,
    NEXT_K9_85_MOTION = 0x22,
    MCT_550_FLOOD = 0x23,
    MCT_302_DW_1_WIRED = 0x24,
    MCT_427_SMOKE = 0x25,
    MCT_442_CO = 0x26,

    MCT_100_UNIVERSAL = 0x27,
    MCT_101_1_BTN = 0x28,
    MCT_102_2_BTN = 0x29,
    MCT_103_3_BTN = 0x2A,
    MCT_104_4_BTN = 0x2B,
    MCT_124_4_BTN = 0x2C,
    MCT_220_EMER_BTN = 0x2D,
    MCT_241_PENDANT = 0x2E,
    DISCOVERY_PIR_MOT = 0x2F,
    TOWER_40_MCW_MOTION = 0x30,
    K_940_MCW_MOTION = 0x31,
    DISCOVERY_K9_80_MOT = 0x32,
    DISCOVERY_QUAD_80_MOT = 0x33,
    MCT_441_GAS = 0x34,
    MCT_501_GLASS_BREAK = 0x35,
    MCT_560_TEMP = 0x36,
    NEXTP_K9_85_MOTION = 0x37,

    // HA device types
            RTCOA_THERMOSTAT = 0x80,
    BD_DOOR_LOCK = 0x81,
    ON_OFF_SWITCH = 0x82,
    ON_OFF_LIGHT = 0x83,
    DIMMER_SWITCH = 0x84,
    DIMMABLE_LIGHT = 0x85,
    ASSA_DOOR_LOCK = 0x86,
} uCDeviceType;

#define IS_CO_SENSOR(x) (x == GE_CO_SENSOR_1 || x == MTL_GE_CO_SENSOR || x == MTL_ED_CO_SENSOR || x == SMC_CO_SENSOR || x == MCT_442_CO || x == MCT_441_GAS)
#define IS_SMOKE_SENSOR(x) (x == SMOKE_1 || x == MTL_GE_SMOKE || x == SMC_SMOKE || x == SMC_SMOKE_NO_SIREN || x == MCT_427_SMOKE)

// this is ONLY for the uControl sensors, not HA devices!!!
#define NUM_UC_DEV_TYPES    (TAKEOVER_1+1) // I intentionally did NOT adjust this
// for the micro dw sensor. It uses the
// normal dw sensor entry.

// device manufacturers
typedef enum
{
    ICONTROL = 1,
    MTL,
    NYCE,
    VISONIC,
    SMC,
} uCMfgId;

// device model by designer
// uControl devices
typedef enum
{
    IC_DOOR_WINDOW = 1,
    IC_GE_SMOKE = 2,
    IC_INNOV_MOTION = 3,
    IC_GLASS_BREAK = 4,
    IC_REPEATER_SIREN = 8,
    IC_GE_MOTION = 9,
    IC_SUREN_MOTION = 10,
    IC_TAKEOVER = 12,
    IC_MICRO_DOOR_WINDOW = 13,
    IC_GE_CO_SENSOR = 15,
    IC_ED_CO_SENSOR = 16,
//    IC_QG_CO_SENSOR         = 16,
} uControlDeviceModels;

// MTL devices
typedef enum
{
    MTL_DOOR_WINDOW_1 = 1,
    MTL_GE_SMOKE_1,
    MTL_GLASS_BREAK_1,
    MTL_REPEATER_SIREN_1,
    MTL_GE_MOTION_1,
    MTL_SUREN_MOTION_1,
    MTL_GE_CO_SENSOR_1,
    MTL_ED_CO_SENSOR_1,
//    MTL_QG_CO_SENSOR_1,
            MTL_WATER_1,
} MTLDeviceModels;

// NYCE
typedef enum
{
    NYCE_KEYFOB = 1,
    NYCE_KEYPAD,
} NYCEDeviceModels;

// Visonic devices
typedef enum
{
    VS_DOOR_WINDOW = 1,
} visonicDeviceModels;

// SMC devices
typedef enum
{
    SMC_MOTION_1 = 1,
    SMC_SMOKE_1,
    SMC_SMOKE_NO_SIREN_1,
    SMC_GLASS_BREAK_1,
    SMC_CO_SENSOR_1,
} SMCDeviceModels;


////////////////////////////////////////////////////////////////
// message command byte definitions
////////////////////////////////////////////////////////////////
typedef enum
{
    // the do nothing message
            NULL_MESSAGE = 0x00,

    // messages from anyone
            DEVICE_ANNOUNCE = 0x01,
    ANNOUNCE_REPLY = 0x02,
    DEVICE_INFO = 0x03,
    DEVICE_STATUS = 0x04,
    DEVICE_CHECKIN = 0x05,
    DEVICE_SIG_STR = 0x06,
    DEVICE_STATE_CHANGE = 0x07,
    DEVICE_SERIAL_NUM = 0x08,
    DEVICE_KEYFOB_EVENT = 0x09,
    DEVICE_KEYPAD_EVENT = 0x0A,
    TAKEOVER_ZONE_ADDED = 0x0B,
    TAKEOVER_ZONE_EVENT = 0x0C,
    TAKEOVER_KEYPAD_EVENT = 0x0D,
    TAKEOVER_INFO = 0x0E,
    PING_MSG = 0x0F,
    TAKEOVER_SET_ZN_TP_CMP = 0x10,
    TAKEOVER_RES_PH_NUM_CMP = 0x11,
    TAKEOVER_SETUP_SIREN_CMP = 0x12,
    TAKEOVER_FROM_PIM = 0x13,

    // messages from the module
            CHECK_SIG_STR = 0x20,
    ENABLE_SIREN = 0x21,
    OK_TO_SLEEP = 0x22,
    DEVICE_CONFIG = 0x23,
    DEVICE_PAIRED = 0x24,
    DEVICE_NUMBER = 0x25,
    DEVICE_REMOVE = 0x26,
    ENTER_BOOTLOADER = 0x27,
    RESEND_MESSAGE = 0x28,
    SHORT_SLEEP = 0x29,
    SET_LED = 0x2A,
    SIREN_MODE = 0x2B,
    SIREN_STATE = 0x2C,
    SET_WHITE_LED = 0x2D,
    TAKEOVER_BRAIN_SUCK = 0x2E,
    TAKEOVER_ZONE_MSG = 0x2F,
    TAKEOVER_DISP_MSG = 0x30,
    FORCED_JOIN = 0x31,
    BOOTLOAD_MSG = 0x32,
    SEND_PING = 0x33,
    TS_REBOOTED_MSG = 0x34,
    TAKEOVER_SET_ZONE_TYPE = 0x35,
    TAKEOVER_RES_PH_NUM = 0x36,
    TAKEOVER_SETUP_SIREN = 0x37,
    TAKEOVER_ZONE_BYPASS = 0x38,
    TAKEOVER_TO_PIM = 0x39,
    FORCE_REJOIN = 0x3A,

    // messages from sensors

    // messages from routers
            GODPARENT_INFO = 0x40,
} uCMessages;

// device status definition
typedef struct
{
    union
    {
        struct
        {
            uint8_t primaryAlarm:1;
            uint8_t secondaryAlarm:1;
            uint8_t tempFaultLow:1;
            uint8_t tempFaultHigh:1;
            uint8_t tamper:1;
            uint8_t lowBattery:1;
            uint8_t trouble:1;
            uint8_t externalPowerFail:1;
        } fields1;
        uint8_t byte1;
    };

    union
    {
        struct
        {
            uint8_t commFail:1;
            uint8_t test:1;
            uint8_t batteryBad:1;
            uint8_t bootloadFail:1;
            uint8_t unused:4;
        } fields2;
        uint8_t byte2;
    };
} uCDeviceStatus;

/**
 * Temperature measurement in degrees C.
 * Data frames represent this as a two byte string,
 * i.e., [tempInt].[tempFrac].
 * E.g., 23.3°C looks like 0x17 0x03
 */
typedef struct
{
    /**
     * Tenths of a degree
     */
    uint8_t tempFrac;
    /**
     * The whole degree
     */
    int8_t tempInt;
} uCTemp;

// sensor configuration - this is the only part
// the sensor needs to keep up with
typedef struct
{
    // temperature limits
    uCTemp lowTempLimit;
    uCTemp highTempLimit;

    // low battery threshold
    uint16_t lowBattThreshold;

    // device number
    uint8_t devNum;

    // enables
    union
    {
        struct
        {
            uint8_t unused:1;
            uint8_t magSwitchEnable:1;
            uint8_t extContactEnable:1;
            uint8_t tamperIsMagnetic:1;
            uint8_t armed:1;
            uint8_t tempLowFaultEnable:1;
            uint8_t tempHighFaultEnable:1;
            uint8_t sensorPaired:1;
        } fields;
        uint8_t byte;
    } enables;

    // hibernate time in seconds
    uint16_t hibernateDuration;

    // nap time in seconds
    uint16_t napDuration;

    /* This and the mfg/model ids (in ucDeviceInfoMessage) may or may not be present depending on device */
    // TODO is this really there?
    uint8_t region;
} uCSensorConfig;

// device state information - used by devices to keep track
// of what they should be doing and used by the touchscreen
// module to keep track of sensors and what messages to
// send and expect.
typedef enum
{
    UC_BOOT_NOT_DEFAULTED,  // 0
    UC_INITIALIZING,        // 1
    UC_DEFAULTING,          // 2
    UC_DEFAULTED,           // 3
    UC_REJOINING,           // 4
    UC_JOINING,             // 5
    UC_JOINED,              // 6
    UC_WAITING_ANN_REPLY,   // 7
    UC_WAITING_DEVICE_NUM,  // 8
    UC_PAIRING,             // 9
    UC_PAIRED_AWAKE,        // 10
    UC_NORMAL_OPERATION,    // 11
    UC_JOIN_FAILED,         // 12
    UC_REJOIN_FAILED,       // 13
    UC_NO_NETWORK,          // 14
    UC_COMM_FAIL,           // 15
    UC_DELETED,             // 16
    UC_BOOTLOAD_ENTRY,      // 17
    UC_BOOTLOADING,         // 18
    UC_KEYFOB_BOOTING,      // 19
    UC_NOT_MONITORED,       // 20
    UC_HA_BOOTLOAD_REQ,     // 21
    UC_HA_BOOTLOAD_WAIT,    // 22
    UC_HA_BOOTLOAD_BUSY,    // 23
    UC_BOOTLOAD_FAILED,     // 24
    UC_BOOTLOAD_RECOVERY_PENDING,     // 25
    UC_WAITING_TS_ACCEPTANCE,// 26
    UC_REJOIN_PENDING,      // 27
    UC_UNKNOWN
} uCDeviceState;

typedef enum
{
    UC_OFF,
    UC_ON,
    UC_BLINK,
    UC_FLASH,
} uCLedState;

typedef enum
{
    UC_RED,
    UC_GREEN,
    UC_AMBER,
} uCLedColor;

// device information structure that the module has to keep track of
typedef struct
{
    // device state information
    uint8_t endpoint;
    uCDeviceState state;
//    int16s          stateTimer;  // 1/5 second ticks to remain in this state
    int32_t stateTimer;  // 1/5 second ticks to remain in this state
    uCMessages pendingMsgToSensor;
    // device type
    uCDeviceType devType;
    // EUI64
//    uint64_t      eui;
    // last message sequence number
    uint8_t lastSeqNum;
    // status from last message
    uCDeviceStatus status;
//    // firmware version
    uint8_t godParent;
    int8_t godParentRSSI;
    uint8_t godParentLQI;
//    uint16_t          nodeId;
//    uint16_t          mfgId;
//    uint16_t          deviceModel;
} uCDeviceInfo;

////////////////////////////////////////////////////////////////
// message structures - this is the payload of a zcl message frame
////////////////////////////////////////////////////////////////

// zcl manufacturer specific message header
typedef struct
{
    uint8_t zclFrameControl;
    uint8_t mfgId[2];
    uint8_t seqNum;
    uint8_t cmd;
} zclMsgHeader;

// zcl manufacturer specific message definition
typedef struct
{
    zclMsgHeader header;
//    uint8_t           payload[26];
//    uint8_t           payload[42];
    uint8_t payload[47];
} zclMsgFrame;

// device serial number message
typedef struct
{
    uint8_t devNum;
    char serNum[17];
} uCDeviceSerNumMsg;

// device info message
typedef struct
{
    // device firmware version
    uint8_t firmwareVer[3];

    // device type
    uCDeviceType devType;

    // current device status
    uCDeviceStatus devStatus;

    // configuration
    uCSensorConfig config;

    // new device identifier stuff
    uint16_t mfgId;
    uint16_t deviceModel;
} uCDeviceInfoMessage;

// status message, also used for checkin and getting signal strength
typedef struct
{
    uint8_t devNum;
    uCDeviceStatus status;
    /* Frame data is big endian! Use be16toh to read it. */
    uint16_t batteryVoltage;
    int8_t rssi;
    uint8_t lqi;
    int temperature;

    uint8_t hasExtra; // if non-zero, the fields below are populated
    uint16_t QSDelay;
    uint8_t retryCount;
    uint8_t rejoinCount;
} uCStatusMessage;

// status message, also used for checkin and getting signal strength
typedef struct
{
    uint8_t devNum;
    uCDeviceStatus status;
    uint8_t batteryVoltage[2];
    int8_t rssi;
    uint8_t lqi;
    int8_t tempInt;
    uint8_t tempFrac;
    uint16_t QSDelay;
    uint8_t retryCount;
    uint8_t rejoinCount;
} uCNewStatusMessage;

// keyfob message
typedef struct
{
    uint8_t devNum;
    uint8_t buttons;
    uint8_t batteryVoltage[2];
    int8_t rssi;
    uint8_t lqi;
    uint16_t presses;
    uint16_t successes;
} uCKeyfobMessage;

typedef enum
{
    LEGACY_ACTION_NONE = 0,
    LEGACY_ACTION_ARM_STAY,
    LEGACY_ACTION_PANIC,
    LEGACY_ACTION_ARM_AWAY,
    LEGACY_ACTION_DISARM,
    LEGACY_ACTION_PANEL_STATUS,
} LegacyActionButton;

// keypad message
typedef struct
{
    uint8_t devNum;
    uint8_t actionButton;
    char code[4];
    /** Battery voltage in big-endian millivolts */
    uint8_t batteryVoltage[2];
    int8_t rssi;
    uint8_t lqi;
    /** Whole degrees C */
    int8_t tempInt;
    /** Tenths of a degree C */
    uint8_t tempFrac;
} uCKeypadMessage;

// configure message
typedef struct
{
    uCSensorConfig config;
} uCConfigMessage;

// ok to sleep message
typedef struct
{
    uint8_t sleep;  // 0=not ok to sleep, else ok to sleep
} uCSleeMessage;

// device state change message
typedef struct
{
    uCDeviceState state;
} uCDeviceStateChangeMessage;

// set led message
typedef struct
{
    uint8_t state;
    uint8_t duration;
    uint8_t color;
} uCSetLedMessage;

typedef enum
{
    PANEL_STATE_ARMED = 1,                 // start this with a 1!!!
    PANEL_STATE_ARMING,                  // 2
    PANEL_STATE_DISARMED,                // 3
    PANEL_STATE_ENTRY_DELAY,             // 4
    PANEL_STATE_ENTRY_DELAY_REMAINING,   // 5
    PANEL_STATE_EXIT_DELAY_REMAINING,    // 6
    PANEL_STATE_ALARM,                   // 7
    PANEL_STATE_ALARM_CLEAR,             // 8
    PANEL_STATE_ALARM_CANCELLED,         // 9
    PANEL_STATE_ALARM_RESET,             // 10
    PANEL_STATE_READY,                   // 11
    PANEL_STATE_NOT_READY,               // 12
} uCPanelState;

typedef enum
{
    ALARM_TYPE_NONE,
    ALARM_TYPE_BURG,
    ALARM_TYPE_FIRE,
    ALARM_TYPE_MEDICAL,
    ALARM_TYPE_POLICE_PANIC,
    ALARM_TYPE_FIRE_PANIC,
    ALARM_TYPE_MEDICAL_PANIC,
} uCAlarmType;

typedef enum
{
    ARM_TYPE_NONE,
    ARM_TYPE_AWAY,
    ARM_TYPE_STAY,
    ARM_TYPE_NIGHT
} uCArmType;

// alarm state message
typedef struct
{
    uCPanelState alarmEvent;
    uint8_t armType;
    uint8_t delayRemaining;
    uCAlarmType alarmType;
    uint8_t silent;
} uCPanelStateMessage;

// set white led message
typedef struct
{
    uint8_t brightness; // duty cycle for PWM (0-100)
    uint8_t onTime;     // on time, per second, in 100mS increments (0-10)
} uCSetWhiteLedMessage;

//TODO: rename
typedef enum
{
    sirenSoundOff = 0,
    sirenSoundAlarm = 3,
    sirenSoundFire = 4
} ucTakeoverSirenSound;

typedef struct
{
    ucTakeoverSirenSound sound;
    uint8_t volume;
    uCSetWhiteLedMessage strobeMode;
} uCWarningMessage;

/******************************************************/
// uControl bootloader mfg specific message definitions
// These are used for getting OTA info to and from a UC
// router so that it can do the OTA upgrade of a device
// that is in his child table.
/******************************************************/
// bootloader message
typedef struct
{
    uint8_t length;
    uint64_t src_destEUI;
    uint8_t payload[38];
} uCBootloadMessage;

// godparent info - this is sent from a u control router when
// it receives the special "B" bootloader message from a sensor
typedef struct
{
    uint8_t routerDevNum;
    uint8_t sensorDevNum;
    uint64_t sensorEui;
    int8_t rssi;
    uint8_t lqi;
} uCGodparentMessage;

// uControl error codes
typedef enum
{
    UC_SUCCESS = 0,
    UC_SENSOR_NOT_FOUND_IN_TABLE,
    UC_NO_EMPTY_SLOT_IN_IN_TABLE,
} uCStatus;

bool parseDeviceInfoMessage(const uint8_t *message, uint16_t messageLen, uCDeviceInfoMessage *deviceInfoMessage);

/**
 * Get a string representing th ucDeviceInfoMessage.
 *
 * @param message the message to convert to a string
 * @return a string representation of the message.  Caller frees.
 */
char *deviceInfoMessageToString(uCDeviceInfoMessage *message);

typedef enum
{
    UC_DEVICE_CLASS_UNKNOWN,
    UC_DEVICE_CLASS_CONTACT_SWITCH,
    UC_DEVICE_CLASS_SMOKE,
    UC_DEVICE_CLASS_CO,
    UC_DEVICE_CLASS_MOTION,
    UC_DEVICE_CLASS_GLASS_BREAK,
    UC_DEVICE_CLASS_WATER,
    UC_DEVICE_CLASS_VIBRATION,
    UC_DEVICE_CLASS_SIREN,
    UC_DEVICE_CLASS_KEYFOB,
    UC_DEVICE_CLASS_KEYPAD,
    UC_DEVICE_CLASS_PERSONAL_EMERGENCY,
    UC_DEVICE_CLASS_REMOTE_CONTROL,
} uCLegacyDeviceClassification;

typedef enum
{
    panelTypeUnknown,
    panelTypeVista,
    panelTypeDsc
} uCPanelType;

typedef struct
{
    char manufacturer[33]; //per zigbee spec, max of 32 + \0
    char model[33]; //per zigbee spec, max of 32 + \0
    uint8_t hardwareVersion;
    bool isMainsPowered;
    bool isBatteryBackedUp;
    bool isPairing;
    bool isFaulted; //used during pairing only
    bool isTampered; //used during pairing only
    bool isTroubled; //used during pairing only
    uint8_t pendingApsAckSeqNum; //used during pairing only
    uint8_t devNum;
    uCDeviceType devType;
    uCLegacyDeviceClassification classification;
    char *upgradeAppFilename; //non-null if a firmware upgrade is pending
    char *upgradeBootloaderFilename; //non-null if a firmware upgrade is pending
    uint8_t endpointId;
    uint8_t firmwareVer[3]; //last known/reported firmware version
    bool firmwareUpgradePending;
    uint8_t legacyDeviceSeqNum;
    uCPanelType mismatchedPanelType; //only valid if this is a PIM.  Indicates the incorrect panel requiring correction
    uint16_t lowBatteryVoltage;
    uint16_t preventLatchCount;
    bool preventLatchWasReset; //part of latch logic
    uint16_t lowBatteryCount; //part of latch logic
} LegacyDeviceDetails;

typedef struct
{
    uint16_t lowBattThreshold;
    bool magSwitchEnabled;
    bool extContactEnabled;
    bool tamperIsMagnetic;
} LegacyDeviceConfig;

/**
 * Get a clone of a device details struct
 */
LegacyDeviceDetails *cloneLegacyDeviceDetails(const LegacyDeviceDetails *src);

/**
 * Destroy legacy device details.
 * @param details
 */
void legacyDeviceDetailsDestroy(LegacyDeviceDetails *details);

/**
 * Helper to auto-free details
 */
void legacyDeviceDetailsDestroy__auto(LegacyDeviceDetails **details);

/**
 * Get the details about a legacy device based on its device type and firmware version.
 *
 * @param devType  the legacy device type
 * @param firmwareVersion  the device's reported firmware version. The most significant byte is meaningless.
 * @param details when successful, will be populated with the details.  Caller frees with freeLegacyDeviceDetails()
 * @return true on success
 */
bool getLegacyDeviceDetails(uCDeviceType devType, uint32_t firmwareVersion, LegacyDeviceDetails **details);

/**
 * Get legacy device configuration info
 * @param devType
 * @return a structure describing the device's properties
 */
LegacyDeviceConfig getLegacyDeviceConfig(uCDeviceType devType);

/**
 * Get a message payload for the legacy device config message appropriate to send to a legacy device.
 *
 * @param devType
 * @param devNum
 * @param region
 * @param payload will be populated with the payload upon success.  Caller frees.
 * @param payloadLen will be populated with the payload length upon success.
 *
 * @return true on success
 */
bool getLegacyDeviceConfigMessage(uCDeviceType devType, uint8_t devNum, uint8_t region, uint8_t **payload,
                                  uint8_t *payloadLen);

/**
 * Parse the message into a device status structure
 *
 * @param endpointId
 * @param message
 * @param messageLen
 * @param status
 * @return true on success
 */
bool parseDeviceStatus(uint8_t endpointId, const uint8_t *message, uint16_t messageLen, uCStatusMessage *status);

/**
 * Parse the message into a keypad message structure
 * @param message the raw zhal mesasge
 * @param messageLen
 * @param keypadMessage Message struct to fill in
 * @return
 */
bool parseKeypadMessage(uint8_t *message, uint16_t messageLen, uCKeypadMessage *keypadMessage);

/**
 * Parse the message into a keyfob message structure
 * @param message
 * @param messageLen
 * @param keyfobMessage
 * @return
 */
bool parseKeyfobMessage(uint8_t *message, uint16_t messageLen, uCKeyfobMessage *keyfobMessage);

/**
 * Get a Zilker compatible firmware version from the 3 byte legacy version in the device info message.
 *
 * @param deviceInfoMessage
 * @return the firmware version
 */
uint32_t getFirmwareVersionFromDeviceInfoMessage(uCDeviceInfoMessage deviceInfoMessage);

/**
 * Convert a legacy firmware version byte array to a native uint32.
 * @param firmwareVer The legacy firmware version from a ucDeviceInfoMessage or LegacyDeviceDetails
 * @return
 */
uint32_t convertLegacyFirmwareVersionToUint32(const uint8_t firmwareVer[3]);

/**
 * Update common device resources with a status message
 * @param deviceService
 * @param eui64
 * @param status
 * @param isBatteryLow true if the battery is in a low condition
 */
void legacyDeviceUpdateCommonResources(const DeviceServiceCallbacks *deviceService,
                                       uint64_t eui64,
                                       const uCStatusMessage *status,
                                       bool isBatteryLow);


#endif //ZILKER_UC_COMMON_H
