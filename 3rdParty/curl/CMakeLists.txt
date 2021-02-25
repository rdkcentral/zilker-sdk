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
# library - 'curl'.
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
set(curl_dir ${CMAKE_CURRENT_BINARY_DIR}/git/src/ext_curl)
set(curl_lib_loc ${CROSS_OUTPUT}/lib/libcurl${CMAKE_SHARED_LIBRARY_SUFFIX})

# some environments need to define the 'CROSS_HOST' for cross-compiling
# as well as LDFLAGS
if ($ENV{CROSS_HOST})
    set(CURL_HOST $ENV{CROSS_HOST})
endif()

#Default location of our CA certificate trust store.
set(CA_CERTS_LOCATION "/vendor/etc/defaults/security/certificates")
#Default name of our master CA certificate file.
set(CA_BUNDLE_NAME "ca-certs.pem")

# use the 'ExternalProject' module of CMake
include(ExternalProject)

# define the ext_curl via ExternalProject so that it will
#  1. download code from git
#  2. configure
#  3. compile
#  4. install to $CROSS_OUTPUT
#
# NOTE: use the name 'ext_curl' so that the normal 'curl'
#       can depend on the external project.
#       again, we totally skip this step if using Yocto (for XB3/XB6)
#
# NOTE: because 'config' for curl has to set some environment variables,
#       we'll use the 'configure_curl.sh.in' as a template to produce
#       a script that will perform the configure (using variables from CMake)
#

# TODO: cURL config is picky. This is a bit flimsy but using the build dir that contains
# the openSSL outputs is not accepted by cURL. It also doesn't accept a bare ${CROSS_OUTPUT} path
# as of 7.69.x.
set(OPENSSL_INSTALL_DIR ${CROSS_OUTPUT})
if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(OPENSSL_INSTALL_DIR ${OPENSSL_INSTALL_DIR}/local)
endif()

configure_file("configure_curl.sh.in" "configure_curl.sh" @ONLY)

ExternalProject_Add(ext_curl
        #GIT_REPOSITORY https://github.com/curl/curl.git
        #GIT_TAG curl-7_60_0
        DEPENDS ext_openssl ext_zlib
        URL https://github.com/curl/curl/releases/download/curl-7_69_1/curl-7.69.1.tar.xz
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        PATCH_COMMAND ${curl_dir}/buildconf
        CONFIGURE_COMMAND /bin/sh ${CMAKE_CURRENT_BINARY_DIR}/configure_curl.sh ${curl_dir})

#export(TARGETS ext_curl FILE /tmp/crud.cmake)
