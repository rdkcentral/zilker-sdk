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
# CMake definition for the zilker 'automation service'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# add library dependencies for this binary
set(SERVICE_LIBS
        xhLog
        xhTypes
        xhTime
        xhUtil
        xhConfig
        xhConcurrent
        icrule
        xhJsonHelper
        xhJsonrpc
        xhCslt
        xhXmlHelper
        xhPropsServiceAPI
        xhAutomationServiceAPI
        xhCommServiceAPI
        xhWatchdogServiceAPI
        xhDeviceServiceAPI
        xhDeviceHelper
        littlesheens
        ccronexpr
        duktape
        z
        ${CONFIG_LDFLAG}
        m
        ${ZILKER_EXE_LIBS})

# use local time instead of GMT for the cron expressions.  Might want to revisit this if we end up using GMT in specs
add_compile_options(-DCRON_USE_LOCAL_TIME)

# Build our source list
file(GLOB SOURCES "src/*.[hc]" "src/engines/*.[hc]" "src/actions/*.[hc]" "src/ccronexpr/ccronexpr.[ch]")

# Create our executable target
add_zilker_executable(NAME xhAutomationService
        LIBS ${SERVICE_LIBS}
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/public
                 ${PROJECT_SOURCE_DIR}/source/services/device/core/public
        SOURCES ${SOURCES}
        SERVICE)

# install actions into 'stock/actions'
# Right now this is an either/or sort of thing
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../resources/internalActionList.xml DESTINATION stock/actions)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../resources/masterActionList.xml
            DESTINATION stock/actions
            RENAME masterActionList.xml)

# add our test to be part of the "make manualTest"
add_zilker_test(
        NAME testAutomationService
        TYPE manual
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/automationServiceTest.c
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test/resources
        WRAPPED_FUNCTIONS storageSave storageLoad storageGetKeys storageDelete
        LINK_LIBRARIES xhAutomationServiceStatic
        )

