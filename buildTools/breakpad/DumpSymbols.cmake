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

# Make sure this was requested.
if (CONFIG_DEBUG_BREAKPAD_SYMBOLS)
    find_program(BREAKPAD_DUMP_SYMS
            NAMES dump_syms)
    if (${BREAKPAD_DUMP_SYMS} STREQUAL "BREAKPAD_DUMP_SYMS-NOTFOUND")
        message(FATAL_ERROR "  missing dependency 'dump_syms'")
    else()
        message(STATUS "  using 'dump_syms' at ${BREAKPAD_DUMP_SYMS}")
    endif()
    message(STATUS "  Dumping symbols to ${ZILKER_BUILD_DIR}/symbols")
    # Setup our output directory
    execute_process(COMMAND mkdir -p ${ZILKER_BUILD_DIR}/symbols)
    # Dump symbols in the right directory structure
    execute_process(COMMAND ${PROJECT_SOURCE_DIR}/buildTools/breakpad/dump_symbols.sh ${BREAKPAD_DUMP_SYMS} ${ZILKER_BUILD_DIR}/symbols
            ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/sbin ${CMAKE_INSTALL_PREFIX}/bin
            RESULT_VARIABLE ret_var)
    if (NOT "${ret_var}" STREQUAL 0)
        message(FATAL_ERROR "  Failed dumping symbols")
    endif ()

    message(STATUS "  Finished dumping symbols")
endif()
