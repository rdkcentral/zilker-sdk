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

#
# Generate a C "config" file with all
# options that are defined.
#
# fout: The file name, with path, to write out to.
#       The previous file will be overwritten.
#
function(CC_GENERATE_CDEFINES fout)
    set(_cdefined "/**Auto-generated configuration file. DO NOT EDIT! */\n")

    get_cmake_property(_variableNames VARIABLES)
    list (SORT _variableNames)
    foreach (_variableName ${_variableNames})
        if (_variableName MATCHES "^CONFIG_.*")
            if (_variableName MATCHES ".*_NUMBER" OR _variableName MATCHES ".*_SECONDS")
                set(_cdefined "${_cdefined}\n#define ${_variableName} ${${_variableName}}")
            elseif ("${${_variableName}}" STREQUAL "ON")
                set(_cdefined "${_cdefined}\n#define ${_variableName} 1")
            elseif("${${_variableName}}" STREQUAL "OFF" OR "${${_variableName}}" STREQUAL "")
                set(_cdefined "${_cdefined}\n//${_variableName} is not defined")
            else()
                set(_cdefined "${_cdefined}\n#define ${_variableName} \"${${_variableName}}\"")
            endif()
        endif()
    endforeach()

    # Make sure we have a new-line on the last line
    set(_cdefined "${_cdefined}\n")

    message(STATUS "Writing C config header to: ${fout}")

    file(WRITE ${fout} ${_cdefined})
endfunction()
