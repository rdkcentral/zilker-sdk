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

# Simple function to set up a program consturctor and destructor, which are each run
# before main() and just before the program ends (either at exit() or after main())

function(zilker_add_main_common TARGET)
    get_property(TARGET_TYPE TARGET ${TARGET} PROPERTY TYPE)
    if(NOT "${TARGET_TYPE}" STREQUAL "EXECUTABLE")
        message(FATAL_ERROR "Target ${TARGET} is not executable: zilker_add_main_common should not be used for non-exe targets.")
    endif()

    set_property(TARGET ${TARGET} APPEND PROPERTY SOURCES "${PROJECT_SOURCE_DIR}/source/main/common.c")
    target_link_libraries(${TARGET} xhLog)
endfunction()
