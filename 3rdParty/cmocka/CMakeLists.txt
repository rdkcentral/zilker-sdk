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
# CMake file to download, configure, and build the 3rdParty
# library - 'cmocka'.
#
# Should NOT be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
# NOTE: this is only used for unit/manual tests and should ONLY
#       be built for dev environments (darwin, linux)
#
#--------------------------------------------------------------------------------------

# requires CROSS_OUTPUT to be set
if (NOT CROSS_OUTPUT)
    message(FATAL_ERROR "missing definition of CROSS_OUTPUT")
endif()

# set some local vars
set(cmocka_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_cmocka)

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# define the ext_cmocka via ExternalProject so that it will
#  1. download the code from 'git'
#  2. patch
#  3. compile
#  4. install to $CROSS_OUTPUT
#
ExternalProject_Add(ext_cmocka
        URL https://cmocka.org/files/1.1/cmocka-1.1.1.tar.xz
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CROSS_OUTPUT} -DCMAKE_BUILD_TYPE=Debug
        INSTALL_COMMAND make install
)
ExternalProject_Get_Property(ext_cmocka install_dir)

