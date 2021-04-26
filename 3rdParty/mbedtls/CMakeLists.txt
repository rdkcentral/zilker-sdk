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
# CMake file to download, configure, and build the 3rdParty
# library - 'mbedtls'.
#
# Should NOT be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

if (NOT CROSS_OUTPUT)
    message(FATAL_ERROR "missing definition of CROSS_OUTPUT")
endif()

include(ExternalProject)

#mbedTLS tests are disabled due to a preprocessor munge error on some targets
ExternalProject_Add(
        ext_mbedtls
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/git
        PATCH_COMMAND patch -p1 < ${CMAKE_CURRENT_LIST_DIR}/0001-Ciphersuites-Disable-64-bit-block-and-nonstandard-ci.patch
        COMMAND scripts/config.pl set MBEDTLS_THREADING_PTHREAD
        COMMAND scripts/config.pl set MBEDTLS_THREADING_C
        GIT_REPOSITORY https://github.com/ARMmbed/mbedtls.git
        GIT_TAG mbedtls-2.16.0
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CROSS_OUTPUT} -DUSE_SHARED_MBEDTLS_LIBRARY=On -DENABLE_TESTING=Off
        INSTALL_COMMAND make install
)

ExternalProject_Get_Property(ext_mbedtls install_dir)