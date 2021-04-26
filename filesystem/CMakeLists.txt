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
# CMake definition to install scripts and configuration files into our 'mirror'
# Should not be utilized directly, but imported from our parent
# file via the 'add_subdirectory()' routine.
#
#--------------------------------------------------------------------------------------

# nothing to compile, just define files to install into the mirror.
# note that each line is attempting to 'copy' a file during the 'install'
# phase of the build.

# NOTE:
# NOTE: CMake is not the best when it comes to 'install' of a linked-file.
# NOTE: essentially it will copy the link and there is not an option to follow
# NOTE: the link.  unfurtunately we use a lot of links for scripts and config files....
# NOTE: to make matters worse, when running via 'bitbake' (yocto environment) the
# NOTE: "install(CODE...)" does not do anything.  therefore, we'll simply replicate
# NOTE: the 'etc' and 'scripts' subdirectories into the staging area (ZILKER_STAGING_DIR)
# NOTE: to convert the soft links into real files.
# NOTE:

# done as part of the 'configuration' phase (similar to branding generating tarballs & headers)
# NB: createBrandDefaults is also run by recipes via createMiniMirror provided by buildTools/functions.sh
execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createCopies.sh ${CMAKE_CURRENT_SOURCE_DIR}/etc ${ZILKER_STAGING_DIR})
if (CONFIG_RDK_TARGET)
    execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createCopies.sh ${CMAKE_CURRENT_SOURCE_DIR}/rdk/${CONFIG_RDK_TARGET}/etc ${ZILKER_STAGING_DIR})
    execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createCopies.sh ${CMAKE_CURRENT_SOURCE_DIR}/rdk/${CONFIG_RDK_TARGET}/scripts ${ZILKER_STAGING_DIR})
else()
    execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createCopies.sh ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_PRODUCT}/etc ${ZILKER_STAGING_DIR})
    execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createCopies.sh ${CMAKE_CURRENT_SOURCE_DIR}/${CONFIG_PRODUCT}/scripts ${ZILKER_STAGING_DIR})
endif()
#execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createCopies.sh ${CMAKE_CURRENT_SOURCE_DIR}/scripts ${ZILKER_STAGING_DIR})
execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createBrandDefaults.sh ${CONFIG_BRAND} ${ZILKER_STAGING_DIR}/branding ${ZILKER_STAGING_DIR}/etc RESULT_VARIABLE retVal)
if (${retVal})
    message(FATAL_ERROR "configuration failed, see previous message(s)")
endif()

# create version file.  look for USE_RELEASE_VERSION option to signify using the current day for the version details
if (USE_RELEASE_VERSION)
    execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createBasicVersion.sh ${PROJECT_SOURCE_DIR} ${ZILKER_STAGING_DIR}/etc -d)
else()
    execute_process(COMMAND /bin/bash ${CMAKE_CURRENT_SOURCE_DIR}/createBasicVersion.sh ${PROJECT_SOURCE_DIR} ${ZILKER_STAGING_DIR}/etc)
endif()

# create variable to represent our script to perform the cp/chmod on "libraries"
#set(HELPER ${PROJECT_SOURCE_DIR}/buildTools/make/installFile.sh)

# Install branding for a regular build.
# NB: Recipes must reinstall any fsroot branding. createMiniMirror handles reinstalling branding.tar and branded mirror
install(DIRECTORY ${ZILKER_STAGING_DIR}/etc/defaults  DESTINATION etc)
install(DIRECTORY ${ZILKER_STAGING_DIR}/branding/oob/${CONFIG_BRAND}/mirror/ DESTINATION ${CMAKE_INSTALL_PREFIX} OPTIONAL USE_SOURCE_PERMISSIONS)
install(FILES ${ZILKER_STAGING_DIR}/etc/version       DESTINATION etc)

# install all of the 'etc' files from 'staging/etc' into 'mirror/etc'
install(DIRECTORY ${ZILKER_STAGING_DIR}/etc/  DESTINATION etc USE_SOURCE_PERMISSIONS)

# install all of the 'scripts' from 'staging/scripts' into 'mirror/bin'
install(DIRECTORY ${ZILKER_STAGING_DIR}/scripts/  DESTINATION bin USE_SOURCE_PERMISSIONS)

# 3rdParty libraries/binaries (for platforms that need it)
#   curl, gcrypt, iksemel, xml2, tls, uuid, etc
#
if (ZILKER_INSTALL_LIBS)
    # add curl binary
    install(PROGRAMS ${CROSS_OUTPUT}/bin/curl DESTINATION bin)

    # add socat binary
    if (CONFIG_LIB_SOCAT)
        install(PROGRAMS ${CROSS_OUTPUT}/bin/socat DESTINATION sbin)
    endif()

    # potentially add gnupg binary
    if (CONFIG_LIB_GPG)
        install(PROGRAMS ${CROSS_OUTPUT}/bin/gpg DESTINATION bin)
        install(PROGRAMS ${CROSS_OUTPUT}/bin/gpg-agent DESTINATION bin)
    endif()

    # install 3rdParty libraries and attempt to maintain the soft links
    if (CONFIG_OS_DARWIN)
        # support Macbook
        file(GLOB LIB_EXPORT "${CROSS_OUTPUT}/lib/*.dylib")
    else()
        # other platforms (more Linux-like then Mac)
        file(GLOB LIB_EXPORT "${CROSS_OUTPUT}/lib/*.so*")
        file(GLOB LOCAL_LIB_EXPORT "${CROSS_OUTPUT}/local/lib/*.so*")
    endif()
    install(FILES ${LIB_EXPORT} DESTINATION lib)

    # 3rdParty components for lib-private are built with the 'local' prefix, and their libs
    # (i.e., ${CROSS_OUTPUT}/local/lib) are installed here
    install(FILES ${LOCAL_LIB_EXPORT} DESTINATION lib-private)

endif()

