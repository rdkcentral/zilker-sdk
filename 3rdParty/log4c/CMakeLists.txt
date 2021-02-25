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
# library - 'liblog4c'.
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
set(log4c_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_liblog4c)
set(log4c_lib_loc ${CROSS_OUTPUT}/lib/liblog4c${CMAKE_SHARED_LIBRARY_SUFFIX})

# some environments need to define the 'CROSS_HOST' for cross-compiling
# as well as LDFLAGS
if ($ENV{CROSS_HOST})
    set(LOG4C_HOST $ENV{CROSS_HOST})
endif()
if ($ENV{CFLAGS})
    set(LOG4C_CFLAGS $ENV{CFLAGS})
endif()
if ($ENV{LDFLAGS})
    set(LOG4C_LDFLAGS $ENV{LDFLAGS})
endif()

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# define the ext_liblog4c via ExternalProject so that it will
#  1. download code from git
#  2. configure
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_liblog4c' so that the normal 'log4c'
#       can depend on the external project.
#       again, we totally skip this step if using Yocto (for XB3/XB6)
#
# NOTE: because 'config' for log4c has to set some environment variables,
#       we'll use the 'configure_log4c.sh.in' as a template to produce
#       a script that will perform the configure (using variables from CMake)
#
configure_file("configure_log4c.sh.in" "configure_log4c.sh" @ONLY)

# old URL that quit working for some reason
#        URL https://cfhcable.dl.sourceforge.net/project/log4c/log4c/1.2.4/log4c-1.2.4.tar.gz
ExternalProject_Add(ext_liblog4c
        URL https://astuteinternet.dl.sourceforge.net/project/log4c/log4c/1.2.4/log4c-1.2.4.tar.gz
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        CONFIGURE_COMMAND /bin/bash ${CMAKE_CURRENT_BINARY_DIR}/configure_log4c.sh ${log4c_dir})

ExternalProject_Get_Property(ext_liblog4c install_dir)

message(STATUS "target liblog4c loc ${log4c_lib_loc}")

