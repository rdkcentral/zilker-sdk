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

# Create a service API DSO target
# Arguments:
# - NAME the service API library name (e.g., myServiceAPI)
# - SOURCES (optional) if set, use sources to build the target (defaults to src/*.c, recursive)
# - LIBS (optional) Additional libraries to link against.
function(add_zilker_api)
    set(singleValueArgs NAME)
    set(multiValueArgs SOURCES LIBS)
    cmake_parse_arguments(ZILKER_API "" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ZILKER_API_NAME)
        message(FATAL_ERROR "No NAME given to add_zilker_api")
    endif()

    # Compute where are generated files live at
    get_ipc_generator_current_dir(CURRENT_SOURCE_GENERATED_DIR)

    set(API_INCLUDES
            "${ZILKER_INCLUDE_DIR}"
            "${CROSS_OUTPUT}/include"     # 3rd Party
            "${CROSS_OUTPUT}/include/libxml2" #xml2 is special, it is installed into include/libxml2 instead of include
    )

    if(NOT ZILKER_API_SOURCES)
        file(GLOB_RECURSE ZILKER_API_SOURCES "src/*.c")
    endif()

    # Add in our generated source files
    file(GLOB_RECURSE ZILKER_API_GENERATED_SOURCES ${CURRENT_SOURCE_GENERATED_DIR}/src/*.c)

    add_library(${ZILKER_API_NAME} SHARED ${ZILKER_API_SOURCES} ${ZILKER_API_GENERATED_SOURCES})
    add_dependencies(${ZILKER_API_NAME} ipcgen-xml)

    target_link_libraries(${ZILKER_API_NAME} xhIpc ${ZILKER_API_LIBS})

    # export our 'public' folder so others can include our headers
    target_include_directories(${ZILKER_API_NAME}
            PUBLIC public ${CURRENT_SOURCE_GENERATED_DIR}/public
            PRIVATE ${API_INCLUDES}
    )

    # install in $PREFIX/lib
    install(TARGETS ${ZILKER_API_NAME} DESTINATION lib)
endfunction()
