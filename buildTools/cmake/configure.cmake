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
# Configuration set up and validation
#

include(ComcastOption)

#
# Set up the main platforms we will use
# This should come first as it will actually
# enable and disable some configuration options.
# These updates need to occur before our "cc_option"
# macro so that the dependency checks will pass.
#
include(${CMAKE_CURRENT_LIST_DIR}/platforms.cmake)

#
# Now set up the capabilities and services. This will
# perform the dependency checks to make sure we have everything.
#
include(${CMAKE_CURRENT_LIST_DIR}/services.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/debug.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/capabilities.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/tools.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/libs.cmake)

# Internal Zilker configuration that should not be exposed to
# the "system"
cc_option(ZILKER_BUILD_3RDPARTY "Build 3rd party libraries." OFF)
cc_option(ZILKER_VERIFY_3RDPARTY "Verify 3rd party libraries are installed." OFF)
cc_option(ZILKER_INSTALL_LIBS "Install some 3rd party libraries" OFF)
cc_option(ZILKER_BUILD_SSP "Build with stack smashing protection" ON)

if (NOT DEFINED CONFIG_PRODUCT OR "${CONFIG_PRODUCT}" STREQUAL "")
    message(FATAL_ERROR "Invalid CONFIG_PRODUCT specified.")
endif()

string(TOUPPER ${CONFIG_PRODUCT} _upper)
set(CONFIG_PRODUCT_${_upper} ON CACHE INTERNAL "")

if (NOT DEFINED CONFIG_OS)
    message(STATUS "No OS defined, assumed development machine")
    set(CONFIG_OS ${CMAKE_HOST_SYSTEM_NAME} ON CACHE INTERNAL "")
endif ()

string(TOUPPER ${CONFIG_OS} _upper)
set(CONFIG_OS_${_upper} ON CACHE INTERNAL "")

if (DEFINED CONFIG_CPU)
    string(TOUPPER ${CONFIG_CPU} _upper)
    set(CONFIG_CPU_${_upper} ON CACHE INTERNAL "")
endif ()

if (NOT DEFINED CONFIG_DYNAMIC_PATH OR "${CONFIG_DYNAMIC_PATH}" STREQUAL "")
    message(FATAL_ERROR "Invalid CONFIG_DYNAMIC_PATH specified.")
endif()

if (NOT DEFINED CONFIG_DYNAMIC_PATH OR "${CONFIG_STATIC_PATH}" STREQUAL "")
    message(FATAL_ERROR "Invalid CONFIG_STATIC_PATH specified.")
endif()

if (NOT DEFINED CONFIG_BRAND OR "${CONFIG_BRAND}" STREQUAL "")
    set(CONFIG_BRAND "icontrol" CACHE INTERNAL "")
endif()

message(STATUS "")
message(STATUS "Configuration Information")
message(STATUS "  Product:      ${CONFIG_PRODUCT}")
message(STATUS "  Brand:        ${CONFIG_BRAND}")
message(STATUS "  OS:           ${CONFIG_OS}")
message(STATUS "  CPU:          ${CONFIG_CPU}")
message(STATUS "  Dynamic Path: ${CONFIG_DYNAMIC_PATH}")
message(STATUS "  Static Path:  ${CONFIG_STATIC_PATH}")
message(STATUS "  CFLAGS:       ${CONFIG_CFLAGS}")
message(STATUS "  CXXFLAGS:     ${CONFIG_CXXFLAGS}")
message(STATUS "  LDFLAGS:      ${CONFIG_LDFLAGS}")

set(CMAKE_SYSTEM_NAME ${CONFIG_OS})

set(CMAKE_SYSTEM_PROCESSOR ${CONFIG_CPU})

#message(STATUS "")
