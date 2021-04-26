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
# Top-level CMake definition for the ZILKER-SDK project
# Leverages an environment variable to determine which platform
# we will be compiling for (linux, darwin, pi)
# and contains a special-case for running with the CLion IDE (assumes linux platform)
#
# Required environment variables include:
#   ZILKER_SDK_TOP       - top of our tree
#   CROSS_OUTPUT     - output of 3rdParty compile (subdirs of sbin, bin, lib, include)
#   CONFIG_PRODUCT   - model to build for
#
# NOTE: Depends on the '3rdParty' project to be built
#       so the external dependencies are in place before
#       compiling this project (prevent compile/link problems)
#       See 3rdParty/CMakeLists.txt for details
#
#--------------------------------------------------------------------------------------

# define the CMake we require
cmake_minimum_required(VERSION 3.2)

# Enable verbose building
#set(CMAKE_VERBOSE_MAKEFILE ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/buildTools/cmake/modules")

# include the cmake modules
include(AddZilkerTest)
include(CheckCCompilerFlag)
include(GenerateCdefines)
include(IPCGenerator)
include(AddZilkerExecutable)
include(AddZilkerAPI)
include(AddZilkerMainCommon)

include(buildTools/cmake/configure.cmake)

if (NOT CONFIG_OS_DARWIN)
    # Don't set ld --as-needed; output NEEDED entries for all items in target_link_libraries.
    set(CMAKE_LINK_WHAT_YOU_USE TRUE)
endif()

# search headers/libraries in the toolchain dir
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

#
# The 'project' command does more than the documentation claims.
# It will actively go find your compilers and determine ABI information
# Due to the fact that we are cross-compiling we must have a number of
# CMAKE_ variables configured before we call project. This is very
# unfortunate IMO as this should be the first thing done in our set up.
#
project(zilker-sdk)

# This is called after the project is setup, so now we can configure CMAKE with any
# additional toolchain values we have

if (ZILKER_BUILD_SSP)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 4.9 OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.9)
            set(STACK_PROT "-fstack-protector-strong --param=ssp-buffer-size=4")
        else()
            set(STACK_PROT "-fstack-protector-all --param=ssp-buffer-size=4")
        endif()

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${STACK_PROT}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${STACK_PROT}")
        message(STATUS "Enabling stack protector: ${STACK_PROT}")
        unset(STACK_PROT)
    endif()
else()
    message(WARNING "Stack protector disabled by ZILKER_BUILD_SSP")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CONFIG_CFLAGS} -Wno-pointer-sign")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CONFIG_CXXFLAGS}  -Wno-pointer-sign")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CONFIG_LDFLAGS}")

# This is all the global stuff across platforms.  Anything that relies on checking
# compiler flags has to be put in here as well, since that can be done after the project
# is setup


# Ignore these warnings for now
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")

if (CONFIG_PRODUCT STREQUAL "linux" AND NOT CONFIG_OS_DARWIN)
    # OS X doesn't know these flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-truncation")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-truncation")

    # Turn on all warnings as errors! (but not on Mac for now)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-enum-compare")

# This one seems weird. We get a "size" passed in as (n) Bytes.
# Then do a `memset` with that and we get a warning saying
# "hey you haven't multiplied by the size of the data!". Clearly
# we did at some point, or knew we were "talking" in Bytes.
# This option is only there in newer GCC versions
check_c_compiler_flag("-Wno-memset-elt-size" HAS_NO_MEMSET_ELT_SIZE)

if (HAS_NO_MEMSET_ELT_SIZE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-memset-elt-size")
endif()

#-----------------------------------------------
# set global variables
#-----------------------------------------------

# define the output directories for 'make install'.
# (this was previously referred to as our 'mirror')
#   mirror   = TOP/build/$platform/mirror    -- distribution output folder
#   include  = TOP/build/$platform/include   -- generated/internal headers (icBuildtime.h, valid_brands.h)
#   staging  = TOP/build/$platform/staging   -- temp area (secupg, packaging, branding)
#   3rdParty = TOP/build/$platform/3rdParty  -- default $CROSS_OUTPUT for built dependencies
#
set(ZILKER_BUILD_DIR ${PROJECT_SOURCE_DIR}/build/${CONFIG_PRODUCT})
set(ZILKER_INCLUDE_DIR ${ZILKER_BUILD_DIR}/include)
set(ZILKER_STAGING_DIR ${ZILKER_BUILD_DIR}/staging)

set(ENV{ZILKER_STAGING_DIR} ${ZILKER_STAGING_DIR})
set(ENV{CMAKE_PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR})

# bitbake wants mirror to go to a diffent location
if (OE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX ${OE_INSTALL_PREFIX})
else()
    set(CMAKE_INSTALL_PREFIX ${ZILKER_BUILD_DIR}/mirror)
endif()

message(STATUS "using INSTALL_PREFIX (mirror output) of ${CMAKE_INSTALL_PREFIX}")

# if inside a 'bitbake' environment, then CROSS_OUTPUT will not
# be set or useable since we don't build the third-party dependencies.
# therefore, look for the supplied 'OE_SYSROOT' and use that if available
if (OE_SYSROOT)
    # enables .../usr/include .../usr/lib
    set(CROSS_OUTPUT ${OE_SYSROOT}/usr)
elseif(NOT CROSS_OUTPUT)
    # create CROSS_OUTPUT variable to match the environment variable
    set(CROSS_OUTPUT $ENV{CROSS_OUTPUT})
    if (NOT CROSS_OUTPUT)
        # no environment var, so hard-code the default
        set(CROSS_OUTPUT /opt/icontrol)
    endif()
    # Make sure our install paths exist to prevent a compiler panic when nothing is actually installed
    file(MAKE_DIRECTORY ${CROSS_OUTPUT}/lib ${CROSS_OUTPUT}/local/lib)
endif()
set(CROSS_OUTPUT_LOCAL "${CROSS_OUTPUT}/local")
message(STATUS "using CROSS_OUTPUT   (3rdParty deps) of ${CROSS_OUTPUT}")

# when NOT on a Mac, setup RPATH
if (NOT CONFIG_OS_DARWIN)
    # set default flags for linking libraries/executables
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}\
     -L${CROSS_OUTPUT}/lib -Wl,-rpath-link=${CROSS_OUTPUT}/lib\
     -L${CROSS_OUTPUT}/local/lib -Wl,-rpath-link=${CROSS_OUTPUT}/local/lib")

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}\
     -L${CROSS_OUTPUT}/lib -Wl,-rpath-link=${CROSS_OUTPUT}/lib\
     -L${CROSS_OUTPUT}/local/lib -Wl,-rpath-link=${CROSS_OUTPUT}/local/lib")

    # normally we would want to set the RPATH to be defined as part of
    # the 'make install' step.  however, we're not installing directly
    # to the actual runtime location (install to build/linux/mirror vs /opt/icontrol).
    # therefore, disable RPATH totally from our build as to not cause confusion.
    if (NOT CONFIG_DEBUG_SINGLE_PROCESS)
        SET(CMAKE_SKIP_BUILD_RPATH TRUE)
    else()
        # add the automatically determined parts of the RPATH
        # which point to directories outside the build tree to the install RPATH
        SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
        # Set install rpath
        SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
        # Make sure 3rd party stuff is in the RPATH/RUNPATH
        SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/local/lib")
        SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-rpath,${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/local/lib")
    endif()
else()
    # disable RPATH on a Mac
    SET(CMAKE_SKIP_BUILD_RPATH TRUE)

    # set default flags for linking libraries/executables
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}\
     -L${CROSS_OUTPUT}/lib -L${CROSS_OUTPUT}/local/lib")

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}\
     -L${CROSS_OUTPUT}/lib -L${CROSS_OUTPUT}/local/lib")
endif()

#-----------------------------------------------
# generate ipc sources/headers
#-----------------------------------------------
add_subdirectory(tools/ipcGenerator)

#-----------------------------------------------
# generate headers used during buildtime
#-----------------------------------------------

cc_generate_cdefines(${ZILKER_INCLUDE_DIR}/icBuildtime.h)

# Set up some internal build environment defines.
string(TOLOWER ${CONFIG_OS} _lower)
string(TOUPPER ${CONFIG_CPU} _upper)
add_definitions(-DBUILDENV_${_lower}
                -DBUILDENV_${CONFIG_PRODUCT}
                -DBUILDENV_${_upper}
                -DBUILDENV_ZILKER
                -D_GNU_SOURCE)

# create 'valid_brands.h' header file and setup which branding
# tarball we'll unpack during the 'install' phase
add_subdirectory(branding)

#-----------------------------------------------
# global compile options
#-----------------------------------------------

include_directories(${ZILKER_INCLUDE_DIR}
                    ${CROSS_OUTPUT}/include
                    ${CROSS_OUTPUT}/local/include)

#-----------------------------------------------
# additional build args depending on integration options
#-----------------------------------------------

#-----------------------------------------------
# unit tests
#-----------------------------------------------

# create a custom target named 'unitTest' to build/run unit tests.
# allows us to keep the tests optional and not part of 'make all',
# which is helpful by only build/run tests on platforms we can
# (meaning not during a cross-compile session).
#
# for example:
#   make all      (does normal build, but not unit tests)
#   make unitTest (builds and runs unit tests)
#
add_custom_target(unitTest COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)
#add_custom_target(unitTest COMMAND ${CMAKE_CTEST_COMMAND} -V)     # verbose mode
enable_testing()

# make another custom target named 'manualTest', to build development
# test binaries that are more interactive in nature (ex: talk to real devices)
# any "tests" that depend on this are simply wanting a way to be compiled
# in mass.
add_custom_target(manualTest)

#-----------------------------------------------
# locate 3rdParty dependencies
#-----------------------------------------------

# locate all of the 3rdParty libraries that we require.
# each should be located in our $CROSS_OUTPUT (even Yocto builds).
# since these are manditory, fail if any are missing
#
set(CMAKE_PREFIX_PATH ${CROSS_OUTPUT})
message(STATUS "locating 3rdParty dependencies in ${CROSS_OUTPUT}")

if (ZILKER_VERIFY_3RDPARTY)
    include(FindThirdParty)

    find_cjson()
    find_curl()
    find_lib(mbedtls)
    find_lib(mbedcrypto)
    find_lib(mbedx509)
    find_libuuid()
    find_libxml()
    find_zlib()

    if (CONFIG_DEBUG_BREAKPAD)
        # Should use look for breakpad wrapper or breakpad
        if (CONFIG_DEBUG_BREAKPAD_WRAPPER)
            find_breakpad_wrapper()
        else()
            find_breakpad()
        endif()
        # Check whether this build is dumping the symbols or its done external
        if (CONFIG_DEBUG_BREAKPAD_SYMBOLS)
            find_breakpad_dump_syms()
        endif()
    endif()

    if (CONFIG_LIB_LOG_LOG4C)
        find_log4c()
    endif()

    if (CONFIG_LIB_OPENSSL)
        find_lib(ssl)
    endif()
endif()


#-----------------------------------------------
# create dependency graph
#-----------------------------------------------
if (CONFIG_PRODUCT_ANGELSENVY)
    add_subdirectory(tools/automationTool)
endif()

# setup up a dependency graph by adding the various
# components within our tree.  ideally this could be
# handled within the modules (lib, hal, services), however
# there are quite a few dependencies that span outside
# of that organized structure (ex: libxhSystem depends on xhPropsServiceAPI)

#--------------
# libraries
#--------------

# first, the foundation libraries
#
add_subdirectory(source/libs/log/c)
add_subdirectory(source/libs/time/c)
add_subdirectory(source/libs/types/c)
add_subdirectory(source/libs/util/c)

# now HAL libraries (some depend on log and util libs)
#
add_subdirectory(source/hal)

# the remaining libraries that do not depend
# on any of our service APIs
#
add_subdirectory(source/libs/concurrent/c)
add_subdirectory(source/libs/ipc/c)
add_subdirectory(source/libs/config/c)
add_subdirectory(source/libs/xml/c)
add_subdirectory(source/libs/ssdp/c)
add_subdirectory(source/libs/json/c)
add_subdirectory(source/libs/jsonrpc/c)
add_subdirectory(source/libs/cron/c)
add_subdirectory(source/libs/solar/c)
add_subdirectory(source/libs/crypto/c)

if (CONFIG_LIB_ICRULE)
    add_subdirectory(source/libs/libicrule)
endif()

if (CONFIG_LIB_CSLT)
    add_subdirectory(source/libs/cslt)
endif()

# service APIs (regardless of disabled services)
#
add_subdirectory(source/services/backupRestore/api/c)
add_subdirectory(source/services/comm/api/c)
add_subdirectory(source/services/device/api/c)
add_subdirectory(source/services/props/api/c)
add_subdirectory(source/services/watchdog/api/c)
add_subdirectory(source/services/automation/api/c)

# the rest of our libraries (now that core service APIs are in place)
#
add_subdirectory(source/libs/url/c)
add_subdirectory(source/libs/reset/c)
add_subdirectory(source/libs/system/c)
add_subdirectory(source/libs/device)

if (CONFIG_DEBUG_BREAKPAD)
    add_subdirectory(source/libs/breakpad/c)
endif()

# integrations
if (CONFIG_LIB_INTEGRATIONS)
    add_subdirectory(source/integrations)
endif()

# zigbee supporting libraries
#
if (CONFIG_SERVICE_DEVICE_ZIGBEE)
    # build 'zhal' if supporting zigbee
    add_subdirectory(source/libs/zhal/c)
endif ()

#-----------------------------------------------
# services and corresponding utilities
#-----------------------------------------------

# backup/restore service
if (CONFIG_SERVICE_BACKUP_RESTORE)
    add_subdirectory(source/services/backupRestore/core)
    add_subdirectory(source/utils/backupUtil)

    if (CONFIG_DEBUG_RESTOREUTIL)
        add_subdirectory(source/utils/restoreUtil)
    endif()
endif()

# comm service
if (CONFIG_SERVICE_COMM)
    add_subdirectory(source/services/comm/core)
endif()

if (CONFIG_SERVICE_DEVICE)
    # device service
    add_subdirectory(source/services/device/core)
    add_subdirectory(source/utils/deviceUtil)

    if (CONFIG_SERVICE_DEVICE_SQLITE_MIGRATION)
        add_subdirectory(source/services/device/sqliteDbMigration)
    endif ()
endif ()

if (CONFIG_SERVICE_AUTOMATIONS)
    # automation service
    add_subdirectory(source/services/automation/core)
    add_subdirectory(source/utils/automationUtil)
endif()

# props service
if (CONFIG_SERVICE_PROPERTY)
    add_subdirectory(source/services/props/core)
    add_subdirectory(source/utils/properties)
endif()

if (CONFIG_SERVICE_WATCHDOG)
# watchdog service
    add_subdirectory(source/services/watchdog/core)
    add_subdirectory(source/utils/serviceUtil)
endif()

# rdk integration service
if (CONFIG_SERVICE_RDK_INTEGRATION)
    add_subdirectory(source/services/rdkIntegration)
endif()

#-----------------------------------------------
# utilities
#-----------------------------------------------

# mandatory utils
add_subdirectory(source/utils/resetToFactoryDefaults)
add_subdirectory(source/utils/systemInfo)
add_subdirectory(source/utils/cronEventUtil)

# special case for IDE
if (CONFIG_DEBUG_SINGLE_PROCESS)
    # build a single process, using as many libraries as we need.
    # this allows us to debug/execute a single binary within the CLion IDE
    #
    include_directories(
            ${ZILKER_INCLUDE_DIR}
            ${CROSS_OUTPUT}/include
    )

    # make the binary target, using the 1 source file
    add_executable(zilker source/main/main.c)
    zilker_add_main_common(zilker)

    if (ZILKER_EXE_LIBS)
        target_link_libraries(zilker ${ZILKER_EXE_LIBS})
    endif()

    if (DEFINED ENV{ZILKER_SINGLE_PROC_LIBS})
        target_link_libraries(zilker $ENV{ZILKER_SINGLE_PROC_LIBS})
    endif()

    # install in $PREFIX/sbin
    install(TARGETS zilker DESTINATION sbin)

    # custom "debug" target for the IDE so that we can
    # perform a "make all install" and run the executable
    # with all of our libraries in the 'mirror' area
    #
    if (CONFIG_DEBUG_BREAKPAD)
        target_link_libraries(zilker breakpad_client)
        add_custom_target(debug_${PROJECT_NAME}
                $(MAKE) zilker install symbols
                COMMENT "Installing ${PROJECT_NAME}")
    else()
        add_custom_target(debug_${PROJECT_NAME}
                $(MAKE) zilker install
                COMMENT "Installing ${PROJECT_NAME}")
    endif()
endif()

if(CONFIG_DEBUG_ZITH_CI_TESTS)
    add_test(NAME zithCITests COMMAND ${CMAKE_SOURCE_DIR}/tools/zith/zithCITests.sh -b $<TARGET_FILE:zilker> -m ${ZILKER_BUILD_DIR}/mirror WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

#-----------------------------------------------
# install default configuration and scripts
#-----------------------------------------------

# filesystem
add_subdirectory(filesystem)


#-----------------------------------------------
# breakpad dump symbols support
#-----------------------------------------------

# Workaround for the lack of post_install steps.
# add_subdirectory is executed in order, this one must be last.
if (CONFIG_DEBUG_BREAKPAD_SYMBOLS)
    # Make sure this is the LAST directory added.
    add_subdirectory(buildTools/breakpad)
    # Add any variables you need during post install.
    install(CODE "set(CONFIG_DEBUG_BREAKPAD_SYMBOLS \"${CONFIG_DEBUG_BREAKPAD_SYMBOLS}\")")
    install(CODE "set(ZILKER_BUILD_DIR \"${ZILKER_BUILD_DIR}\")")
    install(CODE "set(PROJECT_SOURCE_DIR \"${PROJECT_SOURCE_DIR}\")")
    # Add any xhProperties to your post install.
    #get_property(SOME_PROPERTY GLOBAL PROPERTY SOME_PROPERTY)
    #install(CODE "set(SOME_PROPERTY \"${SOME_PROPERTY}\")")
endif()

