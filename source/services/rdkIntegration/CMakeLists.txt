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

# setup include paths (we want to borrow some stuff from 3rd party such as json)
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include             # 3rd Party
        ${CROSS_OUTPUT}/include/ccsp
        ${CROSS_OUTPUT}/include/dbus-1.0
        ${CROSS_OUTPUT}/lib/dbus-1.0/include
        ${CROSS_OUTPUT}/include/libparodus
)

add_definitions(-D_ANSC_LINUX
                -D_ANSC_USER
                -D_ANSC_LITTLE_ENDIAN_
)

# Remove -Werror for CCSP headers.
string(REPLACE " -Werror" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REPLACE " -Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# define source files (using wildcard)
set(mainSrc src/rdkIntegrationService.c)

if(NOT CONFIG_PRODUCT_ANGELSENVY)
    set(mainSrc ${mainSrc} src/parodus_interface.c)
    file(GLOB ccspSrc "src/XHFW_Ssp/*.c")
endif()

set(SOURCES ${mainSrc} ${ccspSrc})

# add library dependencies for this binary
set(SERVICE_LIBS xhLog xhTypes xhUtil xhConcurrent xhJsonHelper xhPropsServiceAPI xhPkiServiceAPI xhCommServiceAPI xhWatchdogServiceAPI ${ZILKER_EXE_LIBS})

if (CONFIG_DEBUG_BREAKPAD)
    set(SERVICE_LIBS ${SERVICE_LIBS} xhBreakpadHelper)
endif()

# add RDK library dependencies for this binary
if(NOT CONFIG_PRODUCT_ANGELSENVY)
set(RDK_LIBS ccsp_common dbus-1 cjson libparodus nanomsg wrp-c syscfg sysevent utapi utctx rdkloggers)
endif()

# build as an executable
add_executable(xhRdkIntegrationService ${SOURCES} config/XHFW.xml)
zilker_add_main_common(xhRdkIntegrationService)

target_link_libraries(xhRdkIntegrationService ${RDK_LIBS} ${SERVICE_LIBS})

# Surpress inline function declared, but never defined warnings from CCSP headers
target_compile_options(xhRdkIntegrationService PRIVATE -fgnu89-inline)

# install in $PREFIX/sbin
install(TARGETS xhRdkIntegrationService DESTINATION sbin)

# install in $PREFIX/ccsp/xhfw/
install(FILES config/XHFW.xml DESTINATION ccsp/xhfw COMPONENT config)

