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

# We need the IPC generator to run during the configure step so all the files can be found
get_ipc_generator_command(IPC_GEN_CMD)
execute_process(COMMAND ${IPC_GEN_CMD})

# We still include a custom target for this, to enable auto re-run of the generator when editing xml.  Note that if
# this causes any new C files to be generated this won't work without reloading the CMake project.
add_subdirectory(definitions)
