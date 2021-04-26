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
# Product file for Raspberry Pi
#

# Set up base product type
set(CONFIG_PRODUCT "pi" CACHE INTERNAL "")
set(CONFIG_BRAND "devpartner" CACHE INTERNAL "")
set(CONFIG_OS "Linux" CACHE INTERNAL "")
set(CONFIG_CPU arm CACHE INTERNAL "")

# runtime paths
set(CONFIG_DYNAMIC_PATH "/opt" CACHE INTERNAL "")
set(CONFIG_STATIC_PATH "/opt/zilker" CACHE INTERNAL "")

# main platform scope definition
set(CONFIG_PLATFORM_HEADLESS ON CACHE BOOL "")

# additional options
set(CONFIG_LIB_LOG ON CACHE BOOL "")
set(CONFIG_LIB_LOG_SYSLOG ON CACHE BOOL "")

set(CONFIG_SERVICE_DEVICE_CAMERAS ON CACHE BOOL "")
set(CONFIG_SERVICE_DEVICE_ZIGBEE_STARTUP_TIMEOUT_SECONDS 120 CACHE STRING "")
set(CONFIG_CAP_ZIGBEE_TELEMETRY OFF CACHE STRING "")
set(CONFIG_SERVICE_RDK_INTEGRATION OFF CACHE STRING "")
set(CONFIG_DEBUG_BREAKPAD_SYMBOLS OFF CACHE STRING "")
set(ZILKER_EXE_LIBS rt pthread CACHE INTERNAL "")

# Internal build system only!
set(ZILKER_BUILD_3RDPARTY ON CACHE INTERNAL "")
set(ZILKER_VERIFY_3RDPARTY ON CACHE INTERNAL "")
set(ZILKER_INSTALL_LIBS ON CACHE INTERNAL "")
