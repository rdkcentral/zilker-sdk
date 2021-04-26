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
# CMake definition for the 'touchstone integration test utility'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include         # 3rd Party
)

# NOTE: special handling if the CONFIG_SERVICE_COMM_AUTO_ASSOCIATE option is set.
# NOTE: when on, Touchstone will not begin activating automatically,
# NOTE: and therefore we should build/install this utility so QA/Dev
# NOTE: can start activation.

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

if (CONFIG_SERVICE_COMM_AUTO_ASSOCIATE)

    # automatic start of activation.  build this as part of the 'manualTest' target

    # build as an executable, but NOT part of the "make all" sequence
    add_executable(touchstoneTest EXCLUDE_FROM_ALL ${SOURCES})

    # add library dependencies for this binary
    target_link_libraries(touchstoneTest xhLog touchstone pthread ${ZILKER_EXE_LIBS})

    # build when 'manualTest' target is used
    add_dependencies(manualTest touchstoneTest)

else()

    # build as an executable; part of the "make all" sequence
    add_executable(touchstoneTest ${SOURCES})

    # install in $PREFIX/bin
    install(TARGETS touchstoneTest DESTINATION bin)

    # add library dependencies for this binary
    target_link_libraries(touchstoneTest xhLog touchstone pthread ${ZILKER_EXE_LIBS})

endif()
