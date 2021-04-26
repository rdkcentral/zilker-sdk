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
# Define all "services" that Zilker provides.
#
# All services should be "disabled" by default. Thus
# a user must knowingly "enable" a particular service.
#

include (ComcastOption)

# The property service is a special case in that it will _always_
# be enabled. Thus we forceably set it to ON
cc_option(CONFIG_SERVICE_PROPERTY ON CACHE INTERNAL "Property service" FORCE
          ENABLE CONFIG_LIB_XML2 CONFIG_LIB_JSON)

# The watchdog service is a special case in that it will _always_
# be enabled. Thus we forceably set it to ON
cc_option(CONFIG_SERVICE_WATCHDOG ON CACHE INTERNAL "Watchdog service" FORCE
          ENABLE CONFIG_LIB_XML2)

# The communication service is a special case in that it will _always_
# be enabled. Thus we forceably set it to ON
cc_option(CONFIG_SERVICE_COMM ON CACHE INTERNAL "Communication service" FORCE
          ENABLE
          CONFIG_LIB_XML2
          CONFIG_LIB_JSON
          CONFIG_LIB_UUID
          CONFIG_LIB_MBEDTLS
          CONFIG_LIB_ZLIB
          CONFIG_LIB_CURL
          CONFIG_LIB_OPENSSL)

cc_option(CONFIG_SERVICE_DEVICE "Support various devices" OFF
          ENABLE CONFIG_LIB_JSON CONFIG_LIB_UUID CONFIG_LIB_CURL CONFIG_LIB_LINENOISE)
cc_option(CONFIG_SERVICE_DEVICE_ZIGBEE "Zigbee devices supported" OFF
          ENABLE CONFIG_LIB_LOG4C
          DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_CAP_ZIGBEE AND CONFIG_LIB_LOG4C)
cc_option(CONFIG_SERVICE_DEVICE_ZIGBEE_SERIAL "Zigbee serial device" "" TYPE STRING
          DEPENDS CONFIG_SERVICE_DEVICE_ZIGBEE)
cc_option(CONFIG_SERVICE_DEVICE_ZIGBEE_PIEZO "Zigbee piezo supported" OFF
          DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_CAP_ZIGBEE)
cc_option(CONFIG_SERVICE_DEVICE_ZIGBEE_XBB "XBB Zigbee battery supported" OFF
          DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_SERVICE_DEVICE_ZIGBEE)
cc_option(CONFIG_SERVICE_DEVICE_ZIGBEE_XBB_AUTO_DISCOVERY "XBB Zigbee battery auto discovery supported" OFF
          DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_SERVICE_DEVICE_ZIGBEE AND CONFIG_SERVICE_DEVICE_ZIGBEE_XBB)
cc_option(CONFIG_SERVICE_DEVICE_PHILIPS_HUE "" OFF
          DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_SERVICE_DEVICE_ZIGBEE)
cc_option(CONFIG_SERVICE_DEVICE_RTCOA_TSTAT "" OFF
          DEPENDS CONFIG_SERVICE_DEVICE AND CONFIG_SERVICE_DEVICE_ZIGBEE)
cc_option(CONFIG_SERVICE_DEVICE_SONOS "" OFF
          DEPENDS CONFIG_SERVICE_DEVICE)
cc_option(CONFIG_SERVICE_DEVICE_CAMERA "" OFF
          DEPENDS CONFIG_SERVICE_DEVICE)
cc_option(CONFIG_SERVICE_DEVICE_GENERATE_DEFAULT_LABELS "" OFF
          DEPENDS CONFIG_SERVICE_DEVICE)

cc_option(CONFIG_SERVICE_BACKUP_RESTORE "" OFF)

cc_option(CONFIG_SERVICE_AUTOMATIONS "" OFF
          ENABLE CONFIG_LIB_ICRULE CONFIG_LIB_CSLT CONFIG_LIB_LITTLESHEENS CONFIG_LIB_CCRONEXPR
          DEPENDS CONFIG_LIB_ICRULE AND CONFIG_LIB_CSLT)

cc_option(CONFIG_SERVICE_RDK_INTEGRATION "Support RDK Integration Service" OFF)

#cc_option(CONFIG_SERVICE_ "" OFF)
