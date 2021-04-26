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
# Device capabilities available to a product.
#

include (ComcastOption)

#
# Device sub-system capabilities
#
cc_option(CONFIG_CAP_ZIGBEE "Zigbee devices supported" OFF)
cc_option(CONFIG_CAP_ZIGBEE_TELEMETRY "Zigbee telemetry gathering" ON DEPENDS CONFIG_CAP_ZIGBEE)

cc_option(CONFIG_CAP_EXT_STORAGE "Support for external storage provided" OFF)
cc_option(CONFIG_CAP_EXT_STORAGE_USB "External storage through USB capable" OFF DEPENDS CONFIG_CAP_EXT_STORAGE)
cc_option(CONFIG_CAP_EXT_STORAGE_MMC "External storage through MMC/SD capable" OFF DEPENDS CONFIG_CAP_EXT_STORAGE)

