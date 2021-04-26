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

#--------------------------------------------------------------------------------------
#
# CMake definition for the device libraries
# Should not be utilized directly, but imported from our top-level
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# the device libraries to compile
#
add_subdirectory(camera/c)
add_subdirectory(descriptors/c)
add_subdirectory(helper/c)
add_subdirectory(simplified/c)

# add 'philips hue lights' if the option is set
if (CONFIG_SERVICE_DEVICE_PHILIPS_HUE)
    add_subdirectory(xhPhilipsHue/c)
endif (CONFIG_SERVICE_DEVICE_PHILIPS_HUE)

# add 'rtcoa thermostats' if the option is set
if (CONFIG_SERVICE_DEVICE_RTCOA_TSTAT)
    add_subdirectory(xhRtcoaWifi/c)
endif (CONFIG_SERVICE_DEVICE_RTCOA_TSTAT)

# add 'xhSonos audio' if the option is set
if (CONFIG_SERVICE_DEVICE_SONOS)
    add_subdirectory(xhSonos/c)
endif (CONFIG_SERVICE_DEVICE_SONOS)


