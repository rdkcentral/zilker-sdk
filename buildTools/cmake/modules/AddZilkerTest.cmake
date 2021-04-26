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
#-

include(CMakeParseArguments)


# Defines a target for unit tests in the standard way
#
# add_zilker_test(
#     NAME myTest                                              # New target name, also executable name
#     TYPE [ unit | manual ]                                   # Defaults to 'unit'
#     TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/test.c # The list of test source files
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test       # Working directory when test is run
#     INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src/subdir          # Any non-public includes in addition to the defaults
#                                                              # ${CMAKE_CURRENT_SOURCE_DIR}/src
#                                                              # ${ZILKER_INCLUDE_DIR}
#                                                              # ${CROSS_OUTPUT}/include
#     WRAPPED_FUNCTIONS printf                                 # list of functions that you want to mock out
#     LINK_LIBRARIES myLibrary                                 # list of libraries that you need to link against
# )
function(add_zilker_test)

    set(options NONE)
    set(oneValueArgs NAME TYPE WORKING_DIRECTORY)
    set(multiValueArgs TEST_SOURCES TEST_ARGS INCLUDES WRAPPED_FUNCTIONS LINK_LIBRARIES)
    cmake_parse_arguments(ZILKER_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Check required args
    if (NOT DEFINED ZILKER_TEST_NAME)
        message(FATAL_ERROR "Missing required argument NAME")
    elseif (NOT DEFINED ZILKER_TEST_TEST_SOURCES)
        message(FATAL_ERROR "Missing required argument TEST_SOURCES")
    endif()

    # Darwin can't handle the '--wrap' option.  bail if that's the case here
    if (DEFINED ZILKER_TEST_WRAPPED_FUNCTIONS AND CONFIG_OS_DARWIN)
        message(WARNING "Unable to build unit test ${ZILKER_TEST_NAME}; OSX does not support the 'wrap' option")
        return()
    endif()
    message(STATUS "Adding unit test ${ZILKER_TEST_NAME}")

    # default optional args
    if (NOT DEFINED ZILKER_TEST_WORKING_DIRECTORY)
        set(ZILKER_TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    # undefined ones should be okay
    if (NOT ZILKER_TEST_TYPE)
        set(ZILKER_TEST_TYPE unit)
    endif()

    # setup a unit test, but such that it is NOT part of the 'make all' sequence.
    # allows us to only build/run unit tests on the platforms we can (meaning
    # not during a cross-compile session).  should be build/ran as part of
    # the 'make unitTest' command

    # setup include paths
    include_directories(
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${ZILKER_INCLUDE_DIR}
            ${CROSS_OUTPUT}/include         # 3rd Party
            #xml2 is special, it is installed in a subdirectory instead of the normal public include path.
            ${CROSS_OUTPUT}/include/libxml2
            ${ZILKER_TEST_INCLUDES}         # Custom includes
    )

    # build as an executable, but NOT part of the "make all" sequence
    add_executable(${ZILKER_TEST_NAME}
                   EXCLUDE_FROM_ALL
                   ${ZILKER_TEST_TEST_SOURCES})

    # Let the tests use any generated private headers (e.g., *_ipc_handler.h)
    get_ipc_generator_current_dir(CURRENT_SOURCE_GENERATED_DIR)
    target_include_directories(${ZILKER_TEST_NAME} PRIVATE "${CURRENT_SOURCE_GENERATED_DIR}/src")

    #loop through and add wrap args
    if (DEFINED ZILKER_TEST_WRAPPED_FUNCTIONS)
        set(ZILKER_TEST_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl")

        foreach(WRAPPED_FUNCTION ${ZILKER_TEST_WRAPPED_FUNCTIONS})
            set(ZILKER_TEST_LINK_FLAGS "${ZILKER_TEST_LINK_FLAGS},--wrap=${WRAPPED_FUNCTION}")
        endforeach()
    else()
        set(ZILKER_TEST_LINK_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    endif()

    if ("${CONFIG_OS}" STREQUAL "Linux")
        # Enable RPATH for test binaries no matter what, just makes them easier to run externally
        set(ZILKER_TEST_LINK_FLAGS "${ZILKER_TEST_LINK_FLAGS} -Wl,-rpath,${CROSS_OUTPUT}/lib")
        set_target_properties(${ZILKER_TEST_NAME} PROPERTIES SKIP_BUILD_RPATH false)
        #message(STATUS "using CMAKE_EXE_LINKER_FLAGS of ${ZILKER_TEST_LINK_FLAGS}")
    endif()

    set_target_properties(${ZILKER_TEST_NAME} PROPERTIES LINK_FLAGS "${ZILKER_TEST_LINK_FLAGS}")


    # add library dependencies for this test-binary
    target_link_libraries(${ZILKER_TEST_NAME} ${ZILKER_TEST_LINK_LIBRARIES} cmocka ${ZILKER_EXE_LIBS})

    zilker_add_main_common(${ZILKER_TEST_NAME})

    # run as a 'test', however need to force the LD_LIBRARY_PATH since
    # the RPATH will not be set (as this was not 'installed' via make)
    if ("${ZILKER_TEST_TYPE}" STREQUAL "unit")
        add_test(NAME ${ZILKER_TEST_NAME}
                 COMMAND ${ZILKER_TEST_NAME} ${ZILKER_TEST_TEST_ARGS}
                 WORKING_DIRECTORY ${ZILKER_TEST_WORKING_DIRECTORY})

        if (CONFIG_OS_DARWIN)
            set_property(TEST ${ZILKER_TEST_NAME} PROPERTY ENVIRONMENT "DYLD_LIBRARY_PATH=${CROSS_OUTPUT}/lib:${CMAKE_INSTALL_PREFIX}/lib")
        else()
            set_property(TEST ${ZILKER_TEST_NAME} PROPERTY ENVIRONMENT "LD_LIBRARY_PATH=${CROSS_OUTPUT}/lib:${CMAKE_INSTALL_PREFIX}/lib")
        endif()
    endif()

    # add dependency on the custom target 'unitTest' so it will be
    # built/ran when specifically asked to
    #message(STATUS "Adding test ${ZILKER_TEST_NAME} of type ${ZILKER_TEST_TYPE}")
    add_dependencies(${ZILKER_TEST_TYPE}Test ${ZILKER_TEST_NAME})

endfunction() # SETUP_TARGET_FOR_COVERAGE
