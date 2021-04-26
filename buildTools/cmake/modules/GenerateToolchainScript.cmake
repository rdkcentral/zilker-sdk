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

include(CMakeParseArguments)

#
# Generate a shell script that contains environment variables
# of the compiler, path, options based on our selected toolchain
#
# fout: The file name, with path, to write out to.
#       The previous file will be overwritten.
#
function(CC_GENERATE_TOOLCHAIN_SCRIPT fout)
    # header
    set(_buffer "#!/bin/env bash\n#\n# Auto-generated file. DO NOT EDIT!\n#\n\n")

    # compiler as CC and CFLAGS
    set(_buffer "${_buffer}\nexport CC=${CMAKE_C_COMPILER}\n")
    set(_buffer "${_buffer}\nexport CFLAGS=\"${CMAKE_C_FLAGS}\"\n")

    # linker
    set(_buffer "${_buffer}\nexport LDFLAGS=\"${CMAKE_SHARED_LINKER_FLAGS}\"\n")

    # path - assume parent dir of CC
    set(_buffer "${_buffer}\nexport PATH=\"\$PATH:`dirname \$CC`\"\n")

    # additional optional variables
    if(TC_CROSS_HOST)
        set(_buffer "${_buffer}\nexport CROSS_HOST=\"${TC_CROSS_HOST}\"\n")
    endif()
    if(TC_CROSS_HOST2)
        set(_buffer "${_buffer}\nexport CROSS_HOST2=\"${TC_CROSS_HOST2}\"\n")
    endif()
    if(TC_SKIP_CMOCKA)
        set(_buffer "${_buffer}\nexport SKIP_CMOCKA=\"${TC_SKIP_CMOCKA}\"\n")
    endif()

    # Make sure we have a new-line on the last line
    set(_buffer "${_buffer}\n")

    message(STATUS "Writing Toolchain script: ${fout}")
    file(WRITE ${fout} ${_buffer})
    unset(_buffer)
endfunction()
