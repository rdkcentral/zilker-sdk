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
# library - 'zlib'.
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

# define the ext_zlib via ExternalProject so that it will
#  1. download the code from 'git'
#  2. configure
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_zlib' so that the normal 'zlib'
#       can depend on the external project.
#       we totally skip this step if using Yocto (for XB3/XB6)
ExternalProject_Add(ext_zlib
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG v1.2.11
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        CONFIGURE_COMMAND LDFLAGS=${LIBZ_LDFLAGS} ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_zlib/configure --shared --prefix=${CROSS_OUTPUT} --eprefix=${CROSS_OUTPUT} --uname=linux
        BUILD_COMMAND make
        INSTALL_COMMAND make install
)
ExternalProject_Get_Property(ext_zlib install_dir)
message(STATUS "target zlib loc ${CROSS_OUTPUT}/lib/libz${CMAKE_SHARED_LIBRARY_SUFFIX}")

