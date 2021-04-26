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
# CMake definition for the Zilker 'branding'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# nothing to compile, simply run scripts to:
#  1. place the appropriate config files into 'staging' (producing tarballs)
#  2. create the valid_brands.h header and place in the global include area (next to icBuildtime.h)
#
set(ENV{BUILD_MODEL} ${CONFIG_PRODUCT})

# rdk based products have minor tweaks to their brands.
#
if(CONFIG_PLATFORM_RDK)
    set(ENV{BUILD_PRODUCT} rdk)
endif()

# List any branding out-of-band trees here. They will be staged in ZILKER_STAGING_DIR/<name>, excluding _.*_ entries
set(ENV{BUILD_BRAND_OOB} "fsroot mirror")
execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createBranding.sh ${PROJECT_SOURCE_DIR} ${ZILKER_STAGING_DIR})
execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createHeader.sh ${ZILKER_STAGING_DIR}/branding/valid_brands.txt ${ZILKER_INCLUDE_DIR}/valid_brands.h)

# setup installation command to unpack the desired branding tarball
# which should be defined by the TARGET_BRAND variable (as setup by parent makefile)
#install(CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/etc/defaults)")
#install(CODE "execute_process(COMMAND tar -C ${CMAKE_INSTALL_PREFIX}/etc/defaults -xf ${ZILKER_STAGING_DIR}/branding/${TARGET_BRAND}.tar)")
