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
# CMake definition for the Zilker '3rd Party' depenencies.
# Should NOT be imported via 'add_subdirectory()'.
# This is setup to be utilized directly to prevent re-building
# and allow for all of these depenencies to be built prior
# to the main codebase (see description below).
#
# should be build via:
#   mkdir -p $ZILKER_SDK_TOP/build/3rdParty/cmake
#   cd $ZILKER_SDK_TOP/build/3rdParty/cmake
#   cmake $ZILKER_SDK_TOP/3rdParty
#   make all install -j 4
#
#--------------------------------------------------------------------------------------

# define the CMake we requre
if (${CMAKE_MAJOR_VERSION} MATCHES "2")
    # done here to support cross-compiling via Docker, which
    # usually has 'cmake 2.8.12' installed
    cmake_minimum_required(VERSION 2.8)
else()
    # most of our targets (darwin, linux, yocto)
    cmake_minimum_required(VERSION 3.2)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../buildTools/cmake/modules")

include(../buildTools/cmake/configure.cmake)
include(../buildTools/cmake/modules/GenerateToolchainScript.cmake)

# make the thirdParty project to build these open-source
# dependencies "when manually requested".  because most of these dependencies
# are imported via ExternalProject_add(), they have the side-effect of
# being built "every time" we execute 'make' when they are directly
# added as a dependency.  therefore, all of these will be lumped under
# this new custom project so we can build when we want vs all of the time.
#

# create our custom target
#
project(3rdParty)

#-----------------------------------------------
# set global variables (copied from ../CMakeLists.txt)
#-----------------------------------------------
set(BUILD_TCHAIN $ENV{BUILD_TCHAIN})
set(BUILD_BINTYPE $ENV{BUILD_BINTYPE})
set(BUILD_MODEL "${CONFIG_PRODUCT}")
set(ENV{BUILD_MODEL} "${CONFIG_PRODUCT}")

# define the output directories for 'make install'.
#
set(ZILKER_BUILD_DIR "${PROJECT_SOURCE_DIR}/../build/${CMAKE_PROJECT_NAME}/${BUILD_TCHAIN}${BUILD_BINTYPE}")

if (NOT EXISTS "${ZILKER_BUILD_DIR}")
    file(MAKE_DIRECTORY "${ZILKER_BUILD_DIR}")
endif()

# bitbake wants mirror to go to a diffent location
#
if (OE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX ${OE_INSTALL_PREFIX})
else()
    set(CMAKE_INSTALL_PREFIX ${ZILKER_BUILD_DIR})
endif()
  
message(STATUS "using INSTALL_PREFIX of ${CMAKE_INSTALL_PREFIX}")

# if inside a 'bitbake' environment, then CROSS_OUTPUT will not
# be set or useable since we don't build the third-party dependencies.
# therefore, look for the supplied 'OE_SYSROOT' and use that if available
#
if (OE_SYSROOT)
    # enables .../usr/include .../usr/lib
    set(CROSS_OUTPUT ${OE_SYSROOT}/usr)
elseif(NOT CROSS_OUTPUT)
    # create CROSS_OUTPUT variable to match the environment variable
    set(CROSS_OUTPUT $ENV{CROSS_OUTPUT})
    if (NOT CROSS_OUTPUT)
        # no environment var, so hard-code the default
        set(CROSS_OUTPUT "${ZILKER_BUILD_DIR}")
    endif()
    file(MAKE_DIRECTORY ${CROSS_OUTPUT}/lib ${CROSS_OUTPUT}/local/lib)
endif()

set(ENV{CROSS_OUTPUT} "${CROSS_OUTPUT}")
set(CROSS_OUTPUT_LOCAL "${CROSS_OUTPUT}/local")
set(CMAKE_LIBRARY_PATH "${CROSS_OUTPUT}" "${CROSS_OUTPUT_LOCAL}")
set(CMAKE_PREFIX_PATH ${CROSS_OUTPUT} ${CROSS_OUTPUT_LOCAL})

message(STATUS "using CROSS_OUT of ${CROSS_OUTPUT}")

# enable when debugging cmake issues
#
#show_cmake_vars()

# set default flags for linking libraries/executables
#
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CONFIG_LDFLAGS} -L${CROSS_OUTPUT}/lib -L${CROSS_OUTPUT}/local/lib")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${CONFIG_LDFLAGS} -L${CROSS_OUTPUT}/lib -L${CROSS_OUTPUT}/local/lib")

# normally we would want to set the RPATH to be defined as part of
# the 'make install' step.  however, we're not installing directly
# to the actual runtime location (install to build/linux/mirror vs /opt/icontrol).
# therefore, disable RPATH totally from our build as to not cause confusion.
#
SET(CMAKE_SKIP_BUILD_RPATH TRUE)

#-----------------------------------------------
# global compile options
#-----------------------------------------------

add_definitions(-DBUILDENV_ZILKER -D_GNU_SOURCE)

include_directories(${ZILKER_INCLUDE_DIR} ${CROSS_OUTPUT}/include ${CROSS_OUTPUT}/local/include)

# set C++ compiler options
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CONFIG_CFLAGS} -std=c11")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CONFIG_CXXFLAGS} -std=c++11")

message(STATUS "  CMAKE_C_FLAGS:       ${CMAKE_C_FLAGS}")
# only applicable for non-RDK environments that need
# to download & build various libraries
#
if (ZILKER_BUILD_3RDPARTY)
    # pull in each of the libraries
    #
    if (CONFIG_LIB_JSON)
        add_subdirectory(json)
    endif ()

    if (CONFIG_LIB_ZLIB)
        add_subdirectory(zlib)
    endif ()

    # support for "certificate and signatures for Upgrade"
    if (CONFIG_LIB_OPENSSL)
        add_subdirectory(openssl)
    endif()

    if (CONFIG_LIB_UUID)
        add_subdirectory(libuuid)
    endif ()

    if (CONFIG_LIB_XML2)
        add_subdirectory(libxml)
    endif ()

    if (CONFIG_LIB_MBEDTLS)
        add_subdirectory(mbedtls)
    endif ()

    if (CONFIG_LIB_CURL)
        add_subdirectory(curl)
    endif()

    if (CONFIG_LIB_LITTLESHEENS)
        add_subdirectory(littlesheens)
    endif()

    if (CONFIG_DEBUG_BREAKPAD)
        add_subdirectory(breakpad)
    endif()

    if (CONFIG_LIB_LOG4C)
        add_subdirectory(log4c)
    endif()

    if (CONFIG_LIB_CMOCKA)
        add_subdirectory(cmocka)
    endif()

    if (CONFIG_LIB_CCRONEXPR)
        add_subdirectory(ccronexpr)
    endif()

    if (CONFIG_LIB_LINENOISE)
        add_subdirectory(linenoise)
    endif()

endif() # NOT BUILD_YOCTO

# generate script with the toolchain variables.
# used by non-cmake subprojects
#
CC_GENERATE_TOOLCHAIN_SCRIPT(${CROSS_OUTPUT}/toolchain.sh)

# create marker file when "make install"
# (used by setup_build.sh script)
#
install(FILES README.txt DESTINATION . RENAME target_$ENV{BUILD_TCHAIN}$ENV{BUILD_BINTYPE})
