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

#
# Libraries to build.
#
# Some of these libraries may be an external dependency, however,
# the base product requires it to function.
#

include(ComcastOption)

cc_option(CONFIG_LIB_IPC_SOCKET "Support Socket IPC" OFF)
cc_option(CONFIG_LIB_IPC_NANOMSG "Support Nanomsg IPC" OFF)

cc_option(CONFIG_LIB_PARODUS "Support Parodus" OFF)

cc_option(CONFIG_LIB_BEECRYPT "Support Bee crypt" OFF)
cc_option(CONFIG_LIB_XML2 "Support XML2" OFF)
cc_option(CONFIG_LIB_ICRULE "Support Legacy iControl Rule parser" OFF)
cc_option(CONFIG_LIB_CSLT "Support C Schema Language Transcoder" OFF)
cc_option(CONFIG_LIB_SQLITE "Support SQLite" OFF)
cc_option(CONFIG_LIB_INTEGRATIONS "Support RDK integrations" OFF)
cc_option(CONFIG_LIB_LITTLESHEENS "" OFF)
cc_option(CONFIG_LIB_OPENSSL "" OFF)
cc_option(CONFIG_LIB_JSON "" OFF)
cc_option(CONFIG_LIB_ZLIB "" OFF)
cc_option(CONFIG_LIB_UUID "" OFF)
cc_option(CONFIG_LIB_TAR "" OFF)
cc_option(CONFIG_LIB_MBEDTLS "" OFF)
cc_option(CONFIG_LIB_CURL "" OFF)
cc_option(CONFIG_LIB_CMOCKA "" OFF)
cc_option(CONFIG_LIB_CCRONEXPR "" OFF)
cc_option(CONFIG_LIB_LINENOISE "" OFF)

# Utility libraries/executables
cc_option(CONFIG_LIB_SHUTDOWN "Support shutdown" OFF)

cc_option(CONFIG_LIB_LOG "Enable library for log wrapping" OFF)
cc_option(CONFIG_LIB_LOG_SYSLOG "syslog logging" OFF DEPENDS CONFIG_LIB_LOG)
cc_option(CONFIG_LIB_LOG_STDOUT "Regular old stdout" OFF DEPENDS CONFIG_LIB_LOG)
cc_option(CONFIG_LIB_LOG_RDKLOG "RDK logging" OFF DEPENDS CONFIG_LIB_LOG)
cc_option(CONFIG_LIB_LOG_LOG4C "LOG4C logging" OFF
          ENABLE CONFIG_LIB_LOG4C
          DEPENDS CONFIG_LIB_LOG)

cc_option(CONFIG_LIB_LOG4C "LOG4C library" OFF)

