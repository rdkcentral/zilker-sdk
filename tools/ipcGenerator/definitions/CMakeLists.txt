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

set(IPCDEF_XML
        automationService.xml
        backupRestoreService.xml
        commService.xml
        deviceService.xml
        propsService.xml
        watchdogService.xml
        cronEvent.xml
        pkiService.xml)

get_ipc_generator_output_dir(GEN_OUTPUT_DIR)
get_ipc_generator_command(IPC_GEN_CMD)

add_custom_command(
        OUTPUT ${GEN_OUTPUT_DIR}/gen.hash
        COMMAND ${IPC_GEN_CMD}
        DEPENDS ${IPCDEF_XML}
)

add_custom_target(ipcgen-xml DEPENDS ${GEN_OUTPUT_DIR}/gen.hash)
