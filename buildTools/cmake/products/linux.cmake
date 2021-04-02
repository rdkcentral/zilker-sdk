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
# Product file for development environments.
#
# This product will be treated a little differently than others.
# Instead of using one of the "platform" configurations we
# will instead list out each and every CONFIG desired.
# This allows a developer to see exactly which items are
# included in their development build.
#
# Remember, you can override, or add new, any CONFIG
# by supplying cmake with the -D<key>=<value> parameter.
#

# Set up base product type
set(CONFIG_PRODUCT "linux" CACHE INTERNAL "")
set(CONFIG_BRAND "devpartner" CACHE INTERNAL "")

set(CONFIG_DYNAMIC_PATH "/tmp" CACHE INTERNAL "")
set(CONFIG_STATIC_PATH "/opt/zilker" CACHE INTERNAL "")

# Set up cross-compile and host environment
if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "")
    set(CONFIG_OS "Linux" CACHE INTERNAL "")
else()
    # Linux & Darwin
    set(CONFIG_OS "${CMAKE_HOST_SYSTEM_NAME}" CACHE INTERNAL "")
endif()

set(CONFIG_CPU "x86_64" CACHE INTERNAL "")

# Set up product specific C FLAGS
if ("${CONFIG_OS}" STREQUAL "Linux")
    set(CONFIG_CFLAGS "-D__linux__ -D_DEFAULT_SOURCE -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE -O0 -fsanitize=undefined,address -fno-omit-frame-pointer" CACHE INTERNAL "")
    set(CONFIG_CXXFLAGS "-D__linux__ -D_DEFAULT_SOURCE -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L -D_GNU_SOURCE -O0 -fsanitize=undefined,address -fno-omit-frame-pointer" CACHE INTERNAL "")
    set(CONFIG_SERVICE_AUTOMATIONS ON CACHE BOOL "")
elseif("${CONFIG_OS}" STREQUAL "Darwin")
    set(CONFIG_CFLAGS "-DSQLITE_DISABLE_LFS -DBUILDENV_mac -DUSE_CLOCK_FOR_EVENT_ID -DNO_MONOTONIC -O0 -fsanitize=undefined,address -fno-omit-frame-pointer" CACHE INTERNAL "")
    set(CONFIG_CXXFLAGS "-DSQLITE_DISABLE_LFS -DBUILDENV_mac -DUSE_CLOCK_FOR_EVENT_ID -DNO_MONOTONIC -O0 -fsanitize=undefined,address -fno-omit-frame-pointer" CACHE INTERNAL "")
else()
    message(FATAL_ERROR "Unsupported Host OS specified")
endif()

if ("$ENV{CLION_IDE}" STREQUAL "TRUE")
    # IDE will not have the cross output set which we need until we update
    # third party.
    string(TOLOWER ${CONFIG_OS} _early_lower)

    # Needed by legacy builds
    # CMake 3.16 introduced a change for what CMAKE_SOURCE_DIR points to in files included with -C option.
    if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
        set(CROSS_OUTPUT ${CMAKE_SOURCE_DIR}/../build/3rdParty/${_early_lower} CACHE INTERNAL "")
    else()
        set(CROSS_OUTPUT ${CMAKE_SOURCE_DIR}/build/3rdParty/${_early_lower} CACHE INTERNAL "")
    endif()

    # IDE requires a single process.
    set(CONFIG_DEBUG_SINGLE_PROCESS ON CACHE BOOL "")
endif()

set(CONFIG_CAP_ZIGBEE ON CACHE BOOL "")
set(CONFIG_CAP_ZIGBEE_TELEMETRY OFF CACHE STRING "")

set(CONFIG_CAP_EXT_STORAGE ON CACHE BOOL "")
set(CONFIG_CAP_EXT_STORAGE_USB ON CACHE BOOL "")
set(CONFIG_CAP_EXT_STORAGE_MMC ON CACHE BOOL "")

set(CONFIG_LIB_IPC_SOCKET ON CACHE BOOL "")
set(CONFIG_LIB_XML2 ON CACHE BOOL "")
set(CONFIG_LIB_ICRULE ON CACHE BOOL "")
set(CONFIG_LIB_CSLT ON CACHE BOOL "")
set(CONFIG_LIB_CMOCKA ON CACHE BOOL "")
set(CONFIG_LIB_LOG ON CACHE BOOL "")
set(CONFIG_LIB_LOG_STDOUT ON CACHE BOOL "")
set(CONFIG_LIB_INTEGRATIONS ON CACHE BOOL "")

set(CONFIG_SERVICE_DEVICE ON CACHE BOOL "")
set(CONFIG_SERVICE_DEVICE_ZIGBEE ON CACHE BOOL "")
set(CONFIG_SERVICE_DEVICE_ZIGBEE_SERIAL "/dev/usbserial" CACHE STRING "")
#set(CONFIG_SERVICE_DEVICE_ZIGBEE_PIEZO ON CACHE BOOL "")
#set(CONFIG_SERVICE_DEVICE_ZIGBEE_XBB ON CACHE BOOL "")
#set(CONFIG_SERVICE_DEVICE_ZIGBEE_XBB_AUTO_DISCOVERY ON CACHE BOOL "")
set(CONFIG_SERVICE_DEVICE_ZIGBEE_STARTUP_TIMEOUT_SECONDS 120 CACHE STRING "")

set(CONFIG_SERVICE_DEVICE_CAMERAS ON CACHE BOOL "")
set(CONFIG_SERVICE_BACKUP_RESTORE ON CACHE BOOL "")

set(CONFIG_DEBUG_SUPPORTED ON CACHE BOOL "")
#set(CONFIG_DEBUG_ZITH ON CACHE BOOL "")
#set(CONFIG_DEBUG_ZITH_CI_TESTS ON CACHE BOOL "")
set(CONFIG_DEBUG_COMM ON CACHE BOOL "")
set(CONFIG_DEBUG_AUTOMATIONS ON CACHE BOOL "")
#set(CONFIG_DEBUG_IPC ON CACHE BOOL "")

set(CONFIG_TOOL_DEVTOOLS ON CACHE BOOL "")

# Internal build system only!
set(ZILKER_BUILD_3RDPARTY ON CACHE INTERNAL "")
set(ZILKER_VERIFY_3RDPARTY ON CACHE INTERNAL "")
set(ZILKER_INSTALL_LIBS ON CACHE INTERNAL "")

if ("${CONFIG_OS}" STREQUAL "Darwin")
    # Darwin
    set(ZILKER_EXE_LIBS pthread CACHE INTERNAL "")
else()
    # Linux
    set(ZILKER_EXE_LIBS rt pthread CACHE INTERNAL "")
endif()

#set( CACHE BOOL "")

