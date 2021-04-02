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

include (ComcastOption)

# TODO: The platform adapter service _may_ need to be removed as it is most likely unncessary
#       for the RDKB products. However, nothing will build at the moment without it.
cc_option(CONFIG_PLATFORM_RDK "Comcast RDK platform template" OFF
          ENABLE
          CC_INTERNAL_PLATFORM_COMMON_L1
          CONFIG_LIB_BEECRYPT
          CONFIG_LIB_INTEGRATIONS
          CONFIG_SERVICE_RDK_INTEGRATION
          DISABLE
          CONFIG_CAP_ZIGBEE_TELEMETRY)

cc_option(CONFIG_PLATFORM_HEADLESS "Headless device platform template" OFF
          ENABLE
          CC_INTERNAL_PLATFORM_COMMON_L2)

#
# Internal to platforms
#
cc_option(CC_INTERNAL_PLATFORM_COMMON_L2 "Common items, more restrictive. Includes all of L1" OFF
          ENABLE
          CC_INTERNAL_PLATFORM_COMMON_L1
          CONFIG_LIB_SHUTDOWN
          CONFIG_LIB_XML2
          CONFIG_SERVICE_BACKUP_RESTORE)

cc_option(CC_INTERNAL_PLATFORM_COMMON_L1 "Common items to everyone" OFF
          ENABLE
          CONFIG_CAP_ZIGBEE
          CONFIG_LIB_IPC_SOCKET
          CONFIG_SERVICE_DEVICE
          CONFIG_SERVICE_DEVICE_ZIGBEE
          CONFIG_SERVICE_DEVICE_CAMERA
          CONFIG_SERVICE_AUTOMATIONS)
