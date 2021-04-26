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
// Created by jelder380 on 1/23/19.
//

#include <icBuildtime.h>

#ifndef ZILKER_DEVICESERVICEGATHERER_H
#define ZILKER_DEVICESERVICEGATHERER_H

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include <icIpc/ipcStockMessagesPojo.h>

/**
 * Collects all information about each device
 * on the system, which is added to the
 * runtimeStats output per device.
 *
 * Each list added contains the following:
 *
 * The 5 most recent rejoin info,
 * The 5 most recent check-in info,
 * The 8 most recent attribute reports,
 * All of the devices endpoints and resources,
 * The device counters:
 *      Total rejoins for device,
 *      Total Secure rejoins for device,
 *      Total Un-secure rejoins for device,
 *      Total Duplicate Sequence Numbers for device,
 *      Total Aps Ack Failures for device
 *
 * NOTE: each device list has to be in the format:
 *
 * "uuid,manufacturer,model,firmwareVersion,
 * AttributeReportTime1,ClusterId1,AttributeId1,Data1,
 * AttributeReportTime2,ClusterId2,AttributeId2,Data2,
 * AttributeReportTime3,ClusterId3,AttributeId3,Data3,
 * AttributeReportTime4,ClusterId4,AttributeId4,Data4,
 * AttributeReportTime5,ClusterId5,AttributeId5,Data5,
 * AttributeReportTime6,ClusterId6,AttributeId6,Data6,
 * AttributeReportTime7,ClusterId7,AttributeId7,Data7,
 * AttributeReportTime8,ClusterId8,AttributeId8,Data8,
 * rejoinTime1,isSecure1,
 * rejoinTime2,isSecure2,
 * rejoinTime3,isSecure3,
 * rejoinTime4,isSecure4,
 * rejoinTime5,isSecure5,
 * checkInTime1,
 * checkInTime2,
 * checkInTime3,
 * checkInTime4,
 * checkInTime5,
 * type,hardwareVersion,lqi(ne/fe),
 * rssi(ne/fe),temperature,batteryVoltage,
 * lowBattery,commFailure,troubled,
 * bypassed,tampered,faulted,
 * totalRejoinCounter,
 * totalSecureRejoinCounter,
 * totalUnSecureRejoinCounter,
 * totalDuplicateSequenceNumberCounter,
 * totalApsAckFailureCounter"
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectAllDeviceStatistics(runtimeStatsPojo *output);

/**
 * Collect all of the Zigbee counters from Zigbee core
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectZigbeeNetworkCounters(runtimeStatsPojo *output);

/**
 * Collect all of the Zigbee Core Network status
 * includes panID, channel, openForJoin, network up,
 * and eui64
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectZigbeeCoreNetworkStatistics(runtimeStatsPojo *output);

/**
 * Collect all of the Zigbee device firmware failures
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectAllDeviceFirmwareEvents(runtimeStatsPojo *output);

/**
 * Collect zigbee channel status and add them into
 * the runtime stats hash map
 *
 * @param output
 */
void collectChannelScanStats(runtimeStatsPojo *output);

/**
 * Collects stats about Cameras and add them into
 * the runtime stats hash map
 *
 * @param output - the hash map container
 */
void collectCameraDeviceStats(runtimeStatsPojo *output);

/**
 * Collect the device service status
 * TODO: determine what this could be
 *
 * @param output - the serviceStatus HashMap
 */
void collectAllDeviceStatus(serviceStatusPojo *output);

#endif // CONFIG_SERVICE_DEVICE_ZIGBEE

#endif //ZILKER_DEVICESERVICEGATHERER_H
