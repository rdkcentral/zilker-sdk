#
# Copyright 2021 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#

#
# Debug configuration options for Zilker
#

include(ComcastOption)

cc_option(CONFIG_DEBUG_SUPPORTED "Support debugging" OFF)

cc_option(CONFIG_DEBUG_BREAKPAD "Support Breakpad debugging" OFF)
cc_option(CONFIG_DEBUG_BREAKPAD_WRAPPER "Support RDK Breakpad wrapper" OFF DEPENDS CONFIG_DEBUG_BREAKPAD)
cc_option(CONFIG_DEBUG_BREAKPAD_SYMBOLS "Support Breakpad symbols" OFF DEPENDS CONFIG_DEBUG_BREAKPAD)
cc_option(CONFIG_DEBUG_BREAKPAD_DUMP_PATH "Breakpad dump file path" "/tmp/minidump" TYPE STRING)
cc_option(CONFIG_DEBUG_TELEMETRY_UPLOAD_DIRECTORY "Telemetry upload directory path" "/tmp/zigbeeTelemetry/upload" TYPE STRING)

cc_option(CONFIG_DEBUG_ZITH_CI_TESTS "Support functional testing harness" OFF
          ENABLE CONFIG_DEBUG_ZITH CONFIG_DEBUG_SINGLE_PROCESS
          DEPENDS CONFIG_DEBUG_ZITH)
cc_option(CONFIG_DEBUG_ZITH "Support functional testing harness" OFF)

cc_option(CONFIG_DEBUG_SINGLE_PROCESS "Force all services to run as a single process" OFF)

cc_option(CONFIG_DEBUG_ZIGBEE_REMOTE_IP "Zigbee remote IP supported" "" TYPE STRING DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_SERVICE_DEVICE_ZIGBEE)

cc_option(CONFIG_DEBUG_RESTOREUTIL "Build the xhRestoreUtil for testing RMA restore." OFF)

# Debug logging
cc_option(CONFIG_DEBUG_LOG_LEVEL "Logging level for icDebugLog message Lowest [1 -> *] Highest" 1 TYPE STRING)

cc_option(CONFIG_DEBUG_IPC "Turn debugging for IPC on" OFF)
cc_option(CONFIG_DEBUG_RMA "Turn debugging for RMA on" OFF)
cc_option(CONFIG_DEBUG_EXPR "Turn debugging for Expression Engine on" OFF)
cc_option(CONFIG_DEBUG_TPOOL "Turn debugging for Thread Pools on" OFF)
cc_option(CONFIG_DEBUG_COMM "Turn debugging for Communication Service on" OFF)
cc_option(CONFIG_DEBUG_AUTOMATIONS "Turn debugging for Automation Service on" OFF)
