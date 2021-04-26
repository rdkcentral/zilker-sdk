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

include(CMakeParseArguments)
include(IPCGenerator)

# Add a Zilker service or generic executable target.
# Arguments:
# - NAME the executable name (e.g., xhMyService). Targets with this name and <NAME>Static will be generated.
# - SOURCES (optional) If specified, the target will be generated with these sources. Defaults to src/*.c (recursive).
# - LIBS (optional) Additional libraries to link the service executable against
# Options
# - SERVICE (optional) Build as a service. This will install to sbin or integrate into 'zilker' in single process mode.
# - DEVTOOL (optional) Do not install for non-development products
# - CREATE_STATIC (optional) Force the creation of a static library for non-SERVICE executables
function(add_zilker_executable)
    set(options SERVICE DEVTOOL CREATE_STATIC)
    set(singleValueArgs NAME)
    set(multiValueArgs SOURCES LIBS INCLUDES)
    cmake_parse_arguments(ZILKER_PROG "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if (ZILKER_PROG_SERVICE AND ZILKER_PROG_DEVTOOL)
        message(FATAL_ERROR "DEVTOOL and SERVICE cannot be used for the same target")
    endif()

    if (NOT ZILKER_PROG_NAME)
        message(FATAL_ERROR "No NAME given to add_zilker_executable")
    endif()

    if (ZILKER_PROG_DEVTOOL)
        if (CONFIG_TOOL_DEVTOOLS)
            message(STATUS "Adding developer tool ${ZILKER_PROG_NAME}")
        else()
            return()
        endif()
    else()
        message(STATUS "Adding ${ZILKER_PROG_NAME}")
    endif()

    if (NOT ZILKER_PROG_SOURCES)
        file(GLOB_RECURSE ZILKER_PROG_SOURCES "src/*.c")
    endif()

    if (ZILKER_PROG_SERVICE)
        # Compute where are generated files live at
        get_ipc_generator_current_dir(CURRENT_SOURCE_GENERATED_DIR)
        file(GLOB_RECURSE ZILKER_PROG_GENERATED_SOURCES "${CURRENT_SOURCE_GENERATED_DIR}/src/*.c")
        set(ZILKER_PROG_GENERATED_INCLUDES_DIR "${CURRENT_SOURCE_GENERATED_DIR}/src")
    else()
        set(ZILKER_PROG_GENERATED_SOURCES "")
    endif()

    # ZILKER_EXE_LIBS is a global that may list "basic" libs such as pthread, depending on target C library
    # E.g., glibc needs pthread but bionic (android) targets cannot link to pthread.
    list(APPEND ZILKER_PROG_LIBS ${ZILKER_EXE_LIBS})

    if (CONFIG_ENABLE_BREAKPAD)
        list(APPEND ZILKER_PROG_LIBS xhBreakpadHelper)
    endif()

    set(MY_TARGETS "")

    # Create static archives on 'linux' for unit test support
    if  ((CONFIG_PRODUCT_LINUX AND ZILKER_PROG_SERVICE) OR ZILKER_PROG_CREATE_STATIC)
        set(STATIC_NAME "${ZILKER_PROG_NAME}Static")
        add_library(${STATIC_NAME} STATIC ${ZILKER_PROG_SOURCES} ${ZILKER_PROG_GENERATED_SOURCES})
        add_dependencies(${STATIC_NAME} ipcgen-xml)
        target_link_libraries(${STATIC_NAME} ${ZILKER_PROG_LIBS})

        list(APPEND MY_TARGETS ${STATIC_NAME})
    endif()

    # Services in single process mode are special: All the main()s are defined as unique entry
    # points and linked into a big 'zilker' executable.
    if (CONFIG_DEBUG_SINGLE_PROCESS AND ZILKER_PROG_SERVICE)
        # When building in single process mode, provide static archives for each service
        # to assemble into the 'zilker' test executable
        # This is an linux build and will always have the 'static' target set up.

        # Add this executable to the 'ZILKER_SINGLE_PROC_LIBS' environment.
        # The environment var will be added 'zilker' target's library list (see project CMakeLists.txt)
        if (DEFINED ENV{ZILKER_SINGLE_PROC_LIBS})
            set(ENV{ZILKER_SINGLE_PROC_LIBS} "$ENV{ZILKER_SINGLE_PROC_LIBS};${STATIC_NAME}")
        else()
            set(ENV{ZILKER_SINGLE_PROC_LIBS} "${STATIC_NAME}")
        endif()
    else()
        add_executable(${ZILKER_PROG_NAME} ${ZILKER_PROG_SOURCES} ${ZILKER_PROG_GENERATED_SOURCES})
        list(APPEND MY_TARGETS ${ZILKER_PROG_NAME})
        zilker_add_main_common(${ZILKER_PROG_NAME})

        set(binDest bin)
        if (CONFIG_TOOL_DEVTOOLS AND ZILKER_PROG_DEVTOOL)
            set(binDest bin/devTools)
        endif()
        if (ZILKER_PROG_SERVICE)
            set(binDest sbin)
        endif()

        # install in $PREFIX/sbin (services) $PREFIX/bin (utilities, etc.)
        install(TARGETS ${ZILKER_PROG_NAME} DESTINATION ${binDest})

        target_link_libraries(${ZILKER_PROG_NAME} ${ZILKER_PROG_LIBS})
    endif()

    foreach(MY_TARGET ${MY_TARGETS})
        if (ZILKER_PROG_INCLUDES)
            target_include_directories(${MY_TARGET} PRIVATE ${ZILKER_PROG_INCLUDES})
        else()
            target_include_directories(${MY_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
        endif()

        # 3rdParty includes
        target_include_directories(${MY_TARGET} PRIVATE "${CROSS_OUTPUT}/include")
        #xml2 is special, it is installed in a subdirectory instead of the normal public include path.
        target_include_directories(${MY_TARGET} PRIVATE "${CROSS_OUTPUT}/include/libxml2")

        # Generated includes
        target_include_directories(${MY_TARGET} PRIVATE ${ZILKER_PROG_GENERATED_INCLUDES_DIR})
    endforeach()

endfunction()
