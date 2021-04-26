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

file(GLOB SOURCES "src/*.c" "../src/*.c")

add_zilker_test(
        NAME testCryptoLib
        TYPE unit
        TEST_SOURCES ${SOURCES}
        INCLUDES $<TARGET_PROPERTY:xhCrypto,INCLUDE_DIRECTORIES>
        LINK_LIBRARIES $<TARGET_PROPERTY:xhCrypto,LINK_LIBRARIES>
)

set_property(
        TARGET testCryptoLib
        PROPERTY COMPILE_DEFINITIONS "RES_DIR=\"${CMAKE_CURRENT_LIST_DIR}/resources\""
)

