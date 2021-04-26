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
# CMake definition for the 'sysinfo HAL library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# to ensure we always have a HAL implementation, only build
# for the platform that is supported.  each implementation
# will have the same target name (including the stub version),
# ensuring we build only one of these.

# determine which 'platform' implementation to build
if (${CONFIG_PRODUCT} STREQUAL "xb3")
    add_subdirectory(platforms/xb3)

elseif (${CONFIG_PRODUCT} STREQUAL "xb6" OR ${CONFIG_PRODUCT} STREQUAL "xb7")
    add_subdirectory(platforms/xb6)

elseif (${CONFIG_PRODUCT} STREQUAL "linux")
    add_subdirectory(platforms/development)

else()
    # platform not defined, use the 'stub' implementation
    add_subdirectory(stub)

endif ()

# add our test to be part of the "make manualTest"
add_subdirectory(test)

