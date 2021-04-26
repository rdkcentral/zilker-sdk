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
# library - 'xml'.
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
set(xml_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_xml)
set(xml_lib_loc ${CROSS_OUTPUT}/lib/libxml2${CMAKE_SHARED_LIBRARY_SUFFIX})

# some environments need to define the 'CROSS_HOST' for cross-compiling
if ($ENV{CROSS_HOST})
    set(XML_HOST $ENV{CROSS_HOST})
endif()

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# define the ext_xml via ExternalProject so that it will
#  1. download code from git
#  2. configure
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_xml' so that the normal 'xml'
#       can depend on the external project.
#       again, we totally skip this step if using Yocto (for XB3/XB6)
#
# NOTE: because 'config' for xml has to set some environment variables,
#       we'll use the 'configure_xml.sh.in' as a template to produce
#       a script that will perform the configure (using variables from CMake)
#
configure_file("configure_xml.sh.in" "configure_xml.sh" @ONLY)

ExternalProject_Add(ext_xml
#        GIT_REPOSITORY https://github.com/GNOME/libxml2.git
#        GIT_TAG v2.9.8
        URL https://github.com/GNOME/libxml2/archive/v2.9.8.tar.gz
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        CONFIGURE_COMMAND /bin/bash ${CMAKE_CURRENT_BINARY_DIR}/configure_xml.sh ${xml_dir})

ExternalProject_Get_Property(ext_xml install_dir)
message(STATUS "target xml2 loc ${xml_lib_loc}")

