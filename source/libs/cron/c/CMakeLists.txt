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
# CMake definition for the 'cron library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------


# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

# Compute where our generated files live
get_ipc_generator_current_dir(CURRENT_SOURCE_GENERATED_DIR)

# Add in our generated source files
file(GLOB_RECURSE GENERATED_SOURCES ${CURRENT_SOURCE_GENERATED_DIR}/src/*.c)

# build as shared library
add_library(xhCron SHARED ${SOURCES} ${GENERATED_SOURCES})
target_link_libraries(xhCron xhLog xhPropsServiceAPI xhIpc xhConfig)

# export our 'public' folder so others can include our headers
target_include_directories(xhCron PUBLIC public ${CURRENT_SOURCE_GENERATED_DIR}/public)

# install in $PREFIX/lib
install(TARGETS xhCron DESTINATION lib)
install(DIRECTORY public/xhCron DESTINATION include)

if (NOT CONFIG_OS_DARWIN)
    # Unit Tests
    add_zilker_test(
        NAME testCronEventRegistrar
        TYPE unit
        # We can't mock out things in the same translation unit, so instead of depending on the library we depend on sources
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/cronEventRegistrarTest.c ${SOURCES} ${GENERATED_SOURCES}
        WRAPPED_FUNCTIONS addOrUpdatePreformattedCrontabEntry removeCrontabEntry register_cronEvent_eventListener unregister_cronEvent_eventListener getStaticPath
        LINK_LIBRARIES xhLog xhPropsServiceAPI xhIpc xhConfig
    )

    # xhCron isn't linked to the test executable, so add its generated headers to the test target
    target_include_directories(testCronEventRegistrar PRIVATE $<TARGET_PROPERTY:xhCron,INTERFACE_INCLUDE_DIRECTORIES>)
endif()