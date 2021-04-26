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

# Get ipc generator root output directory
# Argument: the name of the variable to set
macro(get_ipc_generator_output_dir arg)
    if (CONFIG_PLATFORM_RDK)
        set(${arg} ${CMAKE_BINARY_DIR}/generated)
    else()
        set(${arg} ${CMAKE_SOURCE_DIR}/build/generated)
    endif()
endmacro()

# Get the command to run the ipc generator
# Argument: the name of the variable to set
function(get_ipc_generator_command arg)
    if (CONFIG_PLATFORM_RDK)
        get_ipc_generator_output_dir(IPC_GEN_OUT_DIR)
        set(IPC_GEN_OPTS "-s" "${CMAKE_SOURCE_DIR}" -o "${IPC_GEN_OUT_DIR}" "-d" "${CONFIG_RDK_REPO_REL_URL}")
    else()
        set(IPC_GEN_OPTS "-s" "${CMAKE_SOURCE_DIR}" "-jl")
    endif()
    set(${arg} ${CMAKE_SOURCE_DIR}/tools/ipcGenerator/ipcGenerator.sh ${IPC_GEN_OPTS} PARENT_SCOPE)
endfunction()

# Get ipc generator output directory for the current source directory
# Argument: the name of the variable to set
function(get_ipc_generator_current_dir arg)
    # Compute where are generated files live at
    file(RELATIVE_PATH CURRENT_RELATIVE_PATH ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    get_ipc_generator_output_dir(IPC_GEN_OUTPUT)
    set(${arg} "${IPC_GEN_OUTPUT}/${CURRENT_RELATIVE_PATH}" PARENT_SCOPE)
endfunction()


