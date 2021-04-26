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
# CMake definition for the 'reset library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
        ${CROSS_OUTPUT}/include/libxml2     # 3rd Party libxml2
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.[ch]" "src/icrule/*.[ch]" "src/sheens/*.[ch]" "src/sheens/js/*.h" "public/xhCslt/*.h")

file(GLOB SHEENS_JS "src/sheens/js/*.js")

execute_process(COMMAND ${CMAKE_CURRENT_LIST_DIR}/stringifyFiles.sh ${SHEENS_JS})

# build as static library
add_library(xhCslt SHARED ${SOURCES} src/sheens/sheens_transcoder_negative.c)
target_link_libraries(xhCslt xhLog xhTypes icrule xhJsonrpc xhDeviceServiceAPI uuid cjson xml2 ${CONFIG_LDFLAG})

# export our 'public' folder so others can include our headers
target_include_directories(xhCslt PUBLIC public)

#TODO: Decouple this library from these (concrete transcoders are coupled to the below listed services).
#These includes are only imported for constants, the respective library executables are intentionally not linked
#to avoid unnecessarily strong coupling
get_ipc_generator_output_dir(IPC_GEN_OUT)
target_include_directories(xhCslt PRIVATE ${PROJECT_SOURCE_DIR}/source/services/automation/core/public  #automation service constants
                                          ${IPC_GEN_OUT}/source/services/automation/api/c/public) #automation API constants

# install in $PREFIX/lib
install(TARGETS xhCslt DESTINATION lib)
install(DIRECTORY public/cslt DESTINATION include)

add_subdirectory(test)
