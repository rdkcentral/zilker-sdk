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
# code as a library - 'linenoise'.
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
set(linenoise_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_linenoise)
set(linenoise_loc ${CROSS_OUTPUT}/lib/liblinenoise${CMAKE_SHARED_LIBRARY_SUFFIX})

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# copy the 'linenoise.cmake' into the extract dir as CMakeLists.txt.
# this is necessary since this opensource does not contain a makefile,
# configure, or cmake way to compile the code.  during the "PATCH_COMMAND",
# this file will get copied into the source directory for use by cmake
configure_file("linenoise.cmake" "CMakeLists.txt" @ONLY)

# define the ext_linenoise via ExternalProject so that it will
#  1. download the code from 'git'
#  2. compile
#  3. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_linenoise' so that the normal 'linenoise'
#       can depend on the external project.
#       we totally skip this step if using Yocto (for XB3/XB6)
ExternalProject_Add(ext_linenoise
        GIT_REPOSITORY https://github.com/antirez/linenoise
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        PATCH_COMMAND /bin/cp ${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt ${linenoise_dir} &&
                      patch -p1 < ${CMAKE_CURRENT_LIST_DIR}/001-linenoise-history.patch
        )

ExternalProject_Get_Property(ext_linenoise install_dir)
message(STATUS "target linenoise loc ${linenoise_loc}")

