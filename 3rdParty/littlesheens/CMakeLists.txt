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

cmake_minimum_required(VERSION 2.8)

# requires CROSS_OUTPUT to be set
if (NOT CROSS_OUTPUT)
    message(FATAL_ERROR "missing definition of CROSS_OUTPUT")
endif()

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# define the ext_littlesheens via ExternalProject so that it will
#  1. download the code from 'git'
#  2. configure
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_littlesheens' so that the normal 'littlesheens'
#       can depend on the external project.
#       we totally skip this step if using Yocto (for XB3/XB6)
ExternalProject_Add(ext_littlesheens
        GIT_REPOSITORY https://github.com/Comcast/littlesheens.git
        GIT_TAG 0.1
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        PATCH_COMMAND ${CMAKE_CURRENT_LIST_DIR}/patch.sh ${CMAKE_CURRENT_LIST_DIR}
        CMAKE_ARGS "${CMAKE_ARGS}" "-DBUILD_MODEL=${CONFIG_PRODUCT}" "-DCMAKE_INSTALL_PREFIX=${CROSS_OUTPUT}"
        INSTALL_DIR ${CROSS_OUTPUT}
)
