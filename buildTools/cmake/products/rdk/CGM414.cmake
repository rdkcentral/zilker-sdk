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
# Technicolor XB6
include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# Set up base product type
set(CONFIG_PRODUCT "xb6" CACHE INTERNAL "")
set(CONFIG_RDK_TARGET "tchxb6" CACHE INTERNAL "")
set(CONFIG_SERVICE_DEVICE_SQLITE_MIGRATION ON CACHE BOOL "")
set(CONFIG_SERVICE_DEVICE_ZIGBEE_XBB ON CACHE BOOL "")
set(CONFIG_SERVICE_DEVICE_ZIGBEE_XBB_AUTO_DISCOVERY ON CACHE BOOL "")
