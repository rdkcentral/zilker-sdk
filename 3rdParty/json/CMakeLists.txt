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
# library - 'cjson'.
#
# Should NOT be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
# NOTE: this is generally not used anymore since IoTivity contains
#       and publishes a compatible cjson library
#
#--------------------------------------------------------------------------------------

# requires CROSS_OUTPUT to be set
if (NOT CROSS_OUTPUT)
    message(FATAL_ERROR "missing definition of CROSS_OUTPUT")
endif()

# set some local vars
set(json_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_cjson)
set(json_lib_loc ${CROSS_OUTPUT}/lib/libcjson${CMAKE_SHARED_LIBRARY_SUFFIX})

# use the 'ExternalProject' module of CMake
include(ExternalProject)

if ($ENV{BUILD_TCHAIN} MATCHES "android.*")
    # Android doesn't really support 'Locale' in the C/C++ layer
    set(otherJsonArgs "-DENABLE_LOCALES=Off")
endif()


# define the ext_cjson via ExternalProject so that it will
#  1. download the code from 'git'
#  2. patch
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_cjson' so that the normal 'cjson'
#       can depend on the external project.
#       we totally skip this step if using Yocto (for XB3/XB6)

configure_file("patch_json.sh.in" "patch_json.sh" @ONLY)

ExternalProject_Add(ext_cjson
        GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
        GIT_TAG v1.7.10
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        PATCH_COMMAND /bin/sh ${CMAKE_CURRENT_BINARY_DIR}/patch_json.sh ${json_dir}
        CMAKE_ARGS -DENABLE_CJSON_TEST=Off ${otherJsonArgs} -DCMAKE_INSTALL_PREFIX=${CROSS_OUTPUT}
        INSTALL_COMMAND make install
)
#GIT_TAG v1.4.5
ExternalProject_Get_Property(ext_cjson install_dir)
message(STATUS "target cjson loc ${json_lib_loc}")

#show_cmake_vars()
