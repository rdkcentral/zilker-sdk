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
# CMake definition for the 'log library'
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

# default to no external libraries
set(LOG_LIBS "")

# define source files depending on the logging option chosen
if(CONFIG_LIB_LOG_LOG4C)
    message(STATUS "Logger: log4c")

    set(LOG_SRC src/loggingLog4c.c)
    set(LOG_LIBS "log4c")   # log4c log lib
elseif(CONFIG_LIB_LOG_RDKLOG)
    message(STATUS "Logger: RDK")

    set(LOG_SRC src/loggingRdk.c)
    set(LOG_LIBS "rdk")     # rdk log lib
elseif(CONFIG_LIB_LOG_STDOUT)
    message(STATUS "Logger: stdout")

    set(LOG_SRC src/loggingDebug.c)
elseif(CONFIG_LIB_LOG_SYSLOG)
    message(STATUS "Logger: syslog")

    set(LOG_SRC src/loggingSyslog.c)
else()
    message(FATAL_ERROR "Unable to obtain desired logger; bailing")
endif()

# Add the logging common file
set(LOG_SRC ${LOG_SRC} src/loggingCommon.c)

# build as shared library
add_library(xhLog SHARED ${LOG_SRC})
target_link_libraries(xhLog ${LOG_LIBS})

# export our 'public' folder so others can include our headers
target_include_directories(xhLog PUBLIC public)

# install in $PREFIX/lib
install(TARGETS xhLog DESTINATION lib)
install(DIRECTORY public/icLog DESTINATION include)

#TODO: test target
