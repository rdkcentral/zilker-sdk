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
# CMake definition for the 'url library'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------


# setup include paths
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/public
        ${ZILKER_INCLUDE_DIR}
        ${CROSS_OUTPUT}/include     # 3rd Party
        ${PROJECT_SOURCE_DIR}/source/services/props/api/c/public
)

# define source files (using wildcard)
file(GLOB SOURCES "src/*.c")

# build as shared library
add_library(xhUrlHelper SHARED ${SOURCES})
target_link_libraries(xhUrlHelper xhLog xhSystem xhTypes xhUtil curl xhPropsServiceAPI xhCrypto)

if (CONFIG_SERVICE_PKI)
    target_link_libraries(xhUrlHelper xhPkiServiceAPI)
endif()

# export our 'public' folder so others can include our headers
target_include_directories(xhUrlHelper PUBLIC public)

# install in $PREFIX/lib
install(TARGETS xhUrlHelper DESTINATION lib)
install(DIRECTORY public/urlHelper DESTINATION include)

# add our 'unit test' to be part of the "make unitTest"
add_subdirectory(test)