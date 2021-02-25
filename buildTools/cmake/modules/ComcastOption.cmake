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
# Comcast OPTION tag
#
# This function will replace the normal
# cmake "option" function in order to support
# dependency and auto-enable/disable of
# options.
#
# name: The name of the option to set
# desc: The description of the option
# default: Default ON/OFF value
#
# optional:
# TYPE The type of variable. Not that cmake treats all values as strings,
#      and thus this setting is for the `cmakegui`.
#
#      BOOL Boolean ON/OFF value
#      STRING A string value
#      INTERNAL A cmake Internal cache value
# FORCE Force variable to be applied regardless of previous state.
#       Normally a variable is updated if, and only if, it has not
#       been previous applied.
# WARNING Do not send FATAL_ERROR on failure to meet dependecies.
# DEPENDS "expression"
#
#         Where "expression" is a "<CONFIG> [ [AND|OR] [NOT] <CONFIG>]"
# ENABLE  <config> <config> <config>
# DISABLE <config> <config> <config>
#
function(CC_OPTION name desc default)
    set(optionarg FORCE WARNING)
    set(singlearg TYPE)
    set(multiarg ENABLE DISABLE DEPENDS)

    cmake_parse_arguments(options "${optionarg}" "${singlearg}" "${multiarg}" ${ARGN})

    # We currently apply FORCE to all `set` commands.
    # This should be revisited sometime.
    if (options_FORCE)
        set(_force FORCE)
    else()
        set(_force "")
    endif()

    if (options_TYPE)
        set(_type ${options_TYPE})
    else()
        set(_type BOOL)
    endif()

    set(${name} ${default} CACHE ${_type} "${desc}" ${_force})

    if (${name})
        if (options_ENABLE)
            foreach(arg ${options_ENABLE})
                set(${arg} ON CACHE BOOL "" ${_force})
            endforeach()
        endif ()

        if (options_DISABLE)
            foreach(arg ${options_DISABLE})
                set(${arg} OFF CACHE BOOL ""  ${_force})
            endforeach()
        endif ()

        if (options_DEPENDS)
            if (options_WARNING)
                set(_fatal WARNING)
            else()
                set(_fatal FATAL_ERROR)
            endif()

            if (NOT (${options_DEPENDS}))
                # If the dependency checks failed then we want to
                # print out a human readable list of failures
                # So replace all `;` (list separator) with spaces.

                string(REGEX REPLACE ";+" " " _expr "${options_DEPENDS}")
                string(REGEX MATCHALL "(CONFIG_[^;]*)" _match "${options_DEPENDS}")

                foreach(arg ${_match})
                    set(_dependencies "${_dependencies}\n${arg}: ${${arg}}")
                endforeach()

                string(STRIP "${_dependencies}" _dependencies)
                message(${_fatal} "${name} dependencies not met! [${_expr}]\n${_dependencies}")
            endif ()
        endif ()
    endif()
endfunction()
