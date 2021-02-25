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
# library - 'breakpad'.
#
# Should NOT be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# requires CROSS_OUTPUT to be set
if (NOT CROSS_OUTPUT)
    message(FATAL_ERROR "missing definition of CROSS_OUTPUT")
endif()

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# set some local vars
set(depot_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_breakpad)
set(bpad_dir ${CMAKE_CURRENT_BINARY_DIR}/git/breakpad)
set(bpad_lib_loc ${CROSS_OUTPUT}/lib/libbreakpad_client${CMAKE_STATIC_LIBRARY_SUFFIX})

# define the ext_breakpad via ExternalProject so that it will
#  1. download depot_tools from google
#  2. execute 'fetch breakpad' (via patch_command)
#  3. configure breakpad
#  4. compile breakpad
#  5. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_breakpad' so that the normal 'breakpad'
#       can depend on the external project.
#       again, we totally skip this step if using Yocto (for XB3/XB6)
#
# NOTE: because 'config' for breakpad has to set some environment variables,
#       we'll use the 'configure_breakpad.sh.in' as a template to produce
#       a script that will perform the configure (using variables from CMake)
#
configure_file("configure_breakpad.sh.in" "configure_breakpad.sh" @ONLY)

ExternalProject_Add(ext_breakpad
        GIT_REPOSITORY https://chromium.googlesource.com/breakpad/breakpad
        # Latest master commit as of 4/3/2019
        GIT_TAG e2b3b80e439905ba7be9416a61b55df8da0c698f
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        CONFIGURE_COMMAND /bin/bash ${CMAKE_CURRENT_BINARY_DIR}/configure_breakpad.sh ${depot_dir}
        BUILD_COMMAND make
        INSTALL_COMMAND make install
)
ExternalProject_Get_Property(ext_breakpad install_dir)
message(STATUS "target breakpad loc ${bpad_lib_loc}")


