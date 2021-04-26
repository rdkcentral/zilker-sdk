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
# CMake definition for the 'config library'
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

# build as shared library
add_library(xhConfig SHARED ${SOURCES})
if (CONFIG_LIB_LEGACY_ENCRYPTION)
    set(extraCfgLibs ssl crypto xhCrypto)
endif()
target_link_libraries(xhConfig xhLog xhUtil xhPropsServiceAPI mbedcrypto xml2 ${extraCfgLibs})

# export our 'public' folder so others can include our headers
# also include libxml2 special path to transitively include when linking against this
target_include_directories(xhConfig PUBLIC public ${CROSS_OUTPUT}/include/libxml2)

# install in $PREFIX/lib
install(TARGETS xhConfig DESTINATION lib)
install(DIRECTORY public/icConfig DESTINATION include)

# add our 'unit test' to be part of the "make unitTest"
if (NOT CONFIG_OS_DARWIN)
    # only add if Linux.  Currently this uses the '--wrap' linker
    # option, which isn't supported on OS X
    add_subdirectory(test)
endif()