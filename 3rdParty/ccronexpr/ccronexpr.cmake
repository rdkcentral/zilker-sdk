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
# CMake definition to compile ccronexpr as a library.
# The opensource code does not provide a makefile, configure, or cmake
#
#--------------------------------------------------------------------------------------

# setup include paths
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

# define source files (using wildcard)
file(GLOB SOURCES "ccronexpr.c")

# use local time instead of GMT for the cron expressions.  Might want to revisit this if we end up using GMT in specs
add_compile_options(-DCRON_USE_LOCAL_TIME)

# build as shared library
add_library(ccronexpr SHARED ${SOURCES})

# install .so in $CROSS_OUTPUT/lib
set(CMAKE_INSTALL_PREFIX @CROSS_OUTPUT@)
install(TARGETS ccronexpr DESTINATION lib)

# install header in $CROSS_OUTPUT/include
install(FILES ccronexpr.h DESTINATION include)
