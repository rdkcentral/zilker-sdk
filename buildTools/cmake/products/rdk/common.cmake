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

#---------------------------------------------------------
# Common RDKB product stuff

set(CONFIG_BRAND "comcast" CACHE INTERNAL "")

set(CONFIG_CFLAGS "-std=gnu99" CACHE INTERNAL "")

set(CONFIG_ENABLE_BREAKPAD ON CACHE BOOL "")
set(CONFIG_DEBUG_BREAKPAD ON CACHE BOOL "")
set(CONFIG_DEBUG_BREAKPAD_WRAPPER ON CACHE BOOL "")

set(CONFIG_DYNAMIC_PATH "/nvram/icontrol" CACHE INTERNAL "")
set(CONFIG_STATIC_PATH "/opt/icontrol" CACHE INTERNAL "")

set(CONFIG_OS Linux CACHE INTERNAL "")
set(CONFIG_CPU atom CACHE INTERNAL "")

set(CONFIG_PLATFORM_RDK ON CACHE BOOL "")

set(CONFIG_NET_IFACE_ETHERNET "erouter0" CACHE INTERNAL "")

set(CONFIG_LIB_LOG ON CACHE BOOL "")

# This device is an Scene system (i.e. not a security system)
set(CONFIG_SERVICE_DEVICE_ZIGBEE_STARTUP_TIMEOUT_SECONDS 120 CACHE STRING "")
set(CONFIG_SERVICE_DEVICE_GENERATE_DEFAULT_LABELS ON CACHE BOOL "")

set(ZILKER_EXE_LIBS rt pthread CACHE INTERNAL "")

# RDK build environment setup
set(OE_INSTALL_PREFIX "/opt/icontrol" CACHE INTERNAL "")
set(USE_RELEASE_VERSION ON CACHE BOOL "")

# TODO: Needs correct location to a Nexus instance that holds the ipcGenerator binary
set(CONFIG_RDK_REPO_REL_URL "https://example.com/path-to-ipcGenerator" CACHE INTERNAL "")
