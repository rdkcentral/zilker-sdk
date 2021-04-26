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
# CMake definition for the 'breakpad helper library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------


# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include/breakpad     # 3rd Party
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "public/*.h")

# build as static library
add_library(xhBreakpadHelper SHARED ${SOURCES})

# export our 'public' folder so others can include our headers
target_include_directories(xhBreakpadHelper PUBLIC public)

set_target_properties(xhBreakpadHelper PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        )

# install in $PREFIX/lib
install(TARGETS xhBreakpadHelper DESTINATION lib)
install(FILES ${HEADERS} DESTINATION include)


if (CONFIG_DEBUG_BREAKPAD)
    if (CONFIG_DEBUG_BREAKPAD_WRAPPER)
        target_link_libraries(xhBreakpadHelper breakpadwrapper)
    else()
        # add library for Breakpad
        target_link_libraries(xhBreakpadHelper ${BREAKPAD})
    endif()
endif()
