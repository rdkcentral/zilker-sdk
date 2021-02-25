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
# Define the Zilker 'dependencies' within the CMake system.
# Should be included by other CMake files (not a standalone file).
#
# Assumes the CROSS_OUTPUT and CMAKE_PREFIX_PATH have been established
#
#--------------------------------------------------------------------------------------

#
# look for libbreakpad_client.a
# if success, saves the location in ${BREAKPAD}
#
macro(find_breakpad)
    find_library(BREAKPAD
            NAMES breakpad_client
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${BREAKPAD} STREQUAL "BREAKPAD-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'breakpad'")
    else()
        message(STATUS "  using 'breakpad' at ${BREAKPAD}")
    endif()
endmacro()

#
# look for libbreakpad-wrapper.so
# if success, saves the location in ${BREAKPAD_WRAPPER}
#
macro(find_breakpad_wrapper)
    find_library(BREAKPAD_WRAPPER
            NAMES breakpadwrapper
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${BREAKPAD_WRAPPER} STREQUAL "BREAKPAD_WRAPPER-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'breakpadwrapper'")
    else()
        message(STATUS "  using 'breakpadwrapper' at ${BREAKPAD_WRAPPER}")
    endif()
endmacro()


#
# look for libcjson.a
# if success, saves the location in ${CJSON}
#
macro(find_cjson)
    find_library(CJSON
            NAMES cjson
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${CJSON} STREQUAL "CJSON-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'cjson'")
    else()
        message(STATUS "  using 'cjson' at ${CJSON}")
    endif()
endmacro()

#
# look for libcurl.so
# if success, saves the location in ${CURL}
#
macro(find_curl)
    find_library(CURL
            NAMES curl
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${CURL} STREQUAL "CURL-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'curl'")
    else()
        message(STATUS "  using 'curl' at ${CURL}")
    endif()
endmacro()

#
# look for libopenssl.so
# if success, saves the location in ${OPENSSL}
#
macro(find_openssl)
    find_library(OPENSSL
            NAMES crypto ssl
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${OPENSSL} STREQUAL "OPENSSL-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'openssl'")
    else()
        message(STATUS "  using 'openssl' at ${OPENSSL}")
    endif()
endmacro()

#
# TODO: libparodus
#
macro(find_libparodus)
endmacro()

# Find a single library with the given name
# If found, it will be stored in ${NAME}. Otherwise, build will halt.
macro(find_lib name)
    string(TOUPPER ${name} libname)
    find_library(
            ${libname}
            NAMES ${name}
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib" "${CROSS_OUTPUT}/local/lib"
            NO_DEFAULT_PATH
    )
    if(${${libname}} STREQUAL "${libname}-NOTFOUND")
        message(FATAL_ERROR " missing dependency '${name}'")
    else()
        message(STATUS "  using '${name}' at ${${libname}}")
    endif()
endmacro()

#
# look for libuuid.so
# if success, saves the location in ${LIBUUID}
#
macro(find_libuuid)
    find_library(LIBUUID
            NAMES uuid
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${LIBUUID} STREQUAL "LIBUUID-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'libuuid'")
    else()
        message(STATUS "  using 'libuuid' at ${LIBUUID}")
    endif()
endmacro()

#
# look for libxml2.so
# if success, saves the location in ${LIBXML}
#
macro(find_libxml)
    find_library(LIBXML
            NAMES xml2 xml
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${LIBXML} STREQUAL "LIBXML-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'libxml'")
    else()
        message(STATUS "  using 'libxml' at ${LIBXML}")
    endif()
endmacro()

#
# look for zlib (libz.so)
# if success, saves the location in ${ZLIB}
#
macro(find_zlib)
    find_library(ZLIB
            NAMES z libz
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${ZLIB} STREQUAL "ZLIB-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'zlib'")
    else()
        message(STATUS "  using 'zlib' at ${ZLIB}")
    endif()
endmacro()

#
# look for log4c (liblog4cso)
# if success, saves the location in ${LOG4C}
#
macro(find_log4c)
    # TODO: find_log4c
    find_library(LOG4C
            NAMES log4c
            PATHS ${CROSS_OUTPUT}
            HINTS "${CROSS_OUTPUT}/lib"
            NO_DEFAULT_PATH)
    if (${LOG4C} STREQUAL "LOG4C-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'log4c'")
    else()
        message(STATUS "  using 'log4c' at ${LOG4C}")
    endif()
endmacro()

