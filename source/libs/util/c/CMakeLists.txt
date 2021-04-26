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
# CMake definition for the 'util library'
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
file(GLOB SOURCES "src/*.[ch]")

# determine if BEECRYPT or NETTLE for the md5 and base64 support
if (CONFIG_LIB_BEECRYPT)
    set(mylib "beecrypt")
else()
    set(mylib "mbedcrypto")
endif()

# build as shared library
add_library(xhUtil SHARED ${SOURCES})
target_link_libraries(xhUtil xhLog xhTypes ${resolverlib} ${mylib} ${CONFIG_LDFLAG})

# export our 'public' folder so others can include our headers
target_include_directories(xhUtil PUBLIC public)

# install in $PREFIX/lib
install(TARGETS xhUtil DESTINATION lib)
install(DIRECTORY public/icUtil DESTINATION include)

add_zilker_test(
        NAME testStringUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/stringTest.c
        LINK_LIBRARIES xhUtil xhTypes
)

add_zilker_test(
        NAME testRegexUtils
        TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test/src/regexTest.c
        LINK_LIBRARIES xhUtil xhTypes
)

# add our 'unit test' to be part of the "make unitTest"
add_subdirectory(test)
