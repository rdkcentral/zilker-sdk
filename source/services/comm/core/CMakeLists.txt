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
# CMake definition for the 'comm service'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# add library dependencies for this binary
set(SERVICE_LIBS xhLog xhTypes xhTime xhUtil xhConcurrent xhConfig xhCron xhSystem xhReset xhXmlHelper xhUrlHelper
        xhDeviceHelper xhDeviceSimplified xhCommServiceAPI xhPropsServiceAPI xhBackupRestoreServiceAPI
        xhAutomationServiceAPI xhDeviceServiceAPI xhWatchdogServiceAPI xhSolar )

if (CONFIG_SERVICE_PKI)
    set(SERVICE_LIBS ${SERVICE_LIBS} xhPkiServiceAPI)
endif()

# define source files
file(GLOB SOURCES "src/*.[hc]" "src/sample/*.[hc]" "src/sample/event/*.[hc]")

# Create our executable target
add_zilker_executable(NAME xhCommService
        LIBS ${SERVICE_LIBS}
        INCLUDES ${PROJECT_SOURCE_DIR}/source/services/device/core/public
        SOURCES ${SOURCES}
        SERVICE)

if (CONFIG_SERVICE_COMM)
    # unit tests
    add_zilker_test(
            NAME testMessageQueue
            TYPE unit
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/messageQueueTest.c
            LINK_LIBRARIES xhCommServiceStatic
    )
endif()

