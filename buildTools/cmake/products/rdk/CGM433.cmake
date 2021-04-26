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

#---------------------------------------------------------
# Technicolor XB7
include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# TODO: Update to xb7, or a more generic RDK denominator.
# We have to leave this as xb6 for now as there are
# platform adapter implications.
#
set(CONFIG_PRODUCT "xb7" CACHE INTERNAL "")
set(CONFIG_RDK_TARGET "tchxb7" CACHE INTERNAL "")

