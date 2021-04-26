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
# CMake definition for the 'device service'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# define source files
# first the main source files (regardless of build options)
file(GLOB mainSrc "src/*.c")
file(GLOB ipcSrc "src/ipc/*.c")
file(GLOB dbaseSrc "src/database/*.c")
file(GLOB deviceSrc "src/device/*.c")
file(GLOB serialSrc "src/serial/*.c")
file(GLOB cameraSrc "deviceDrivers/openHomeCamera/*.c")
file(GLOB testDeviceSrc "deviceDrivers/test*/*.c")
set(SOURCES ${mainSrc} ${ipcSrc} ${dbaseSrc} ${deviceSrc} ${serialSrc}
            ${cameraSrc} ${testDeviceSrc})

# philips lights (if option set)
if (CONFIG_SERVICE_DEVICE_PHILIPS_HUE)
    # add source files
    file(GLOB philipSrc "deviceDrivers/philipsHue/*.c")
    set(SOURCES ${SOURCES} ${philipSrc})

    # include philips device library
    set(XTRA_LIBS ${XTRA_LIBS} xhPhilipsHue)
endif()

# rtcoa thermostats (if option set)
if (CONFIG_SERVICE_DEVICE_RTCOA_TSTAT)
    # add source files
    file(GLOB rtcoaSrc "deviceDrivers/rtcoaWifi/*.c")
    set(SOURCES ${SOURCES} ${rtcoaSrc})

    # include rtcoa device library
    set(XTRA_LIBS ${XTRA_LIBS} xhRtcoaWifi)
endif()

# zigbee (if option set)
if (CONFIG_SERVICE_DEVICE_ZIGBEE)

    # add source files
    file(GLOB subSrc "src/subsystems/zigbee/*.c")
    file(GLOB zigbeeCommonSrc "deviceDrivers/zigbeeDriverCommon.c")
    file(GLOB_RECURSE zigbeeClustersSrc "deviceDrivers/zigbeeClusters/*.c")
    file(GLOB zbDoorSrc "deviceDrivers/zigbeeDoorLock/*.c")
    file(GLOB zbLightSrc "deviceDrivers/zigbeeLight/*.c")
    file(GLOB zbLightContSrc "deviceDrivers/zigbeeLightController/*.c")
    file(GLOB zbLegacyLightSrc "deviceDrivers/zigbeeLegacyLight/*.c")
    file(GLOB zbSensorSrc "deviceDrivers/zigbeeSensor/*.c")
    file(GLOB zbLegacyCommonSrc "deviceDrivers/zigbeeLegacySecurityCommon/*.c")
    file(GLOB zbLegacySensorSrc "deviceDrivers/zigbeeLegacySensor/*.c")
    file(GLOB zbThermoSrc "deviceDrivers/zigbeeThermostat/*.c")
    file(GLOB zbSCSrc "deviceDrivers/zigbeeSecurityController/*.c")
    file(GLOB zbLegacySCSrc "deviceDrivers/zigbeeLegacySecurityController/*.c")
    file(GLOB zbLegacySirenSrc "deviceDrivers/zigbeeLegacySirenRepeater/*.c")
    file(GLOB zbPresenceSrc "deviceDrivers/zigbeePresence/*.c")

    if (CONFIG_SERVICE_DEVICE_ZIGBEE_XBB)
        file(GLOB xbbSrc "deviceDrivers/xbb/*.c")
    endif()

    set(SOURCES
            ${SOURCES}
            ${subSrc}
            ${zigbeeCommonSrc}
            ${zigbeeClustersSrc}
            ${zbLegacyCommonSrc}
            ${zbLegacySensorSrc}
            ${zbDoorSrc}
            ${zbLightSrc}
            ${zbLightContSrc}
            ${zbLegacyLightSrc}
            ${zbSensorSrc}
            ${zbThermoSrc}
            ${xbbSrc}
            ${zbSCSrc}
            ${zbLegacySCSrc}
            ${zbLegacySirenSrc}
            ${zbPresenceSrc})

    # include zhal library
    set(XTRA_LIBS ${XTRA_LIBS} zhal)

endif()

if (CONFIG_SERVICE_NETWORK)
    set(XTRA_LIBS ${XTRA_LIBS} xhNetworkServiceAPI)
endif()

# add library dependencies for this binary
set(xhDeviceServiceLinkLibraries xhLog xhTypes xhTime xhUtil xhConcurrent xhIpc
        xhConfig xhSystem xhUrlHelper xhXmlHelper xhJsonHelper
        xhDeviceHelper xhDeviceDescriptors xhOpenHomeCamera xhCommServiceAPI
        xhPropsServiceAPI xhDeviceServiceAPI xhBackupRestoreServiceAPI
        xhWatchdogServiceAPI xhXmlHelper
        ${XTRA_LIBS} uuid curl xml2 z m)

# Create our executable target
add_zilker_executable(NAME xhDeviceService
        LIBS ${xhDeviceServiceLinkLibraries}
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/public
                 ${CMAKE_CURRENT_SOURCE_DIR}/deviceDrivers
                 ${PROJECT_SOURCE_DIR}/source/libs/zhal/c/public     # zhal may not be built, but need structs from headers
        SOURCES ${SOURCES}
        SERVICE CREATE_STATIC)

# add our 'unit test' to be part of the "make unitTest"
if (CONFIG_SERVICE_DEVICE_ZIGBEE)
    add_zilker_test(
            NAME testZigbeeSubsystem
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/subsystems/zigbee/zigbeeSubsystemTest.c
            WRAPPED_FUNCTIONS deviceServiceGetDevicesBySubsystem deviceServiceGetDeviceDescriptorForDevice getDynamicPath
            LINK_LIBRARIES xhDeviceServiceStatic
            INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/public
    )

    add_zilker_test(
            NAME testJsonDatabase
            TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/database/jsonDatabaseTest.c
            WRAPPED_FUNCTIONS storageLoad storageParse storageSave storageGetKeys storageDelete
            LINK_LIBRARIES xhDeviceServiceStatic xhDeviceHelper
    )
endif()


