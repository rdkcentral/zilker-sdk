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
# CMake definition for the 'config library unit tests'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# setup a unit test, but such that it is NOT part of the 'make all' sequence.
# allows us to only build/run unit tests on the platforms we can (meaning
# not during a cross-compile session).  should be build/ran as part of
# the 'make unitTest' command

# define source files (using wildcard)
# Don't dynamically link to xhConfig - we need to wrap its undefined getDynamicConfiPath symbol
file(GLOB SOURCES "src/*.c" "../src/*.c")

add_zilker_test(
        NAME testConfigLib
        TEST_SOURCES ${SOURCES}
        LINK_LIBRARIES xhJsonHelper $<TARGET_PROPERTY:xhConfig,LINK_LIBRARIES>
        INCLUDES $<TARGET_PROPERTY:xhConfig,INCLUDE_DIRECTORIES>
        WRAPPED_FUNCTIONS getDynamicConfigPath
        TEST_ARGS "-a"
)

