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
# library - 'uuid'.
#
# Should NOT be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# requires CROSS_OUTPUT to be set
if (NOT CROSS_OUTPUT)
    message(FATAL_ERROR "missing definition of CROSS_OUTPUT")
endif()

# set some local vars
set(uuid_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_uuid)
set(uuid_lib_loc ${CROSS_OUTPUT}/lib/libuuid${CMAKE_SHARED_LIBRARY_SUFFIX})

# some environments need to define the 'CROSS_HOST' for cross-compiling
if ($ENV{CROSS_HOST})
    set(UUID_HOST $ENV{CROSS_HOST})
endif()

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# define the ext_uuid via ExternalProject so that it will
#  1. download code from git
#  2. configure
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_uuid' so that the normal 'uuid'
#       can depend on the external project.
#       again, we totally skip this step if using Yocto (for XB3/XB6)
#
# NOTE: because 'config' for uuid has to set some environment variables,
#       we'll use the 'configure_uuid.sh.in' as a template to produce
#       a script that will perform the configure (using variables from CMake)
#
configure_file("configure_uuid.sh.in" "configure_uuid.sh" @ONLY)

#GIT_REPOSITORY https://git.code.sf.net/p/libuuid/code
ExternalProject_Add(ext_uuid
        URL https://sourceforge.net/projects/libuuid/files/libuuid-1.0.3.tar.gz/download
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        CONFIGURE_COMMAND /bin/bash ${CMAKE_CURRENT_BINARY_DIR}/configure_uuid.sh ${uuid_dir})

ExternalProject_Get_Property(ext_uuid install_dir)
message(STATUS "target uuid loc ${uuid_lib_loc}")


