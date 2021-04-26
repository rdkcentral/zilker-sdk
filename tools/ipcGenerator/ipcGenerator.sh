#!/usr/bin/env bash
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

set -e
set -x

function usage()
{
   echo "Usage: ipcGenerator.sh [-s sourceDir] [-d baseDownloadUrl] [-f] [-j] [-l]"
   echo "Options:"
   echo "       -f force update"
   echo "       -j force Java"
   echo "       -l use local build/artifact (default)"
   exit 1
}

SOURCE_DIR="${ZILKER_SDK_TOP}"
FORCE_JAVA="n"
MAVEN_LOCAL="y"
FORCE_UPDATE="n"
while getopts ":s:d:o:fjlh" opt; do
   case ${opt} in
      s )
         SOURCE_DIR="$OPTARG"
         OUTPUT_DIR="${SOURCE_DIR}/build/generated"
         ;;
      d )
         BASE_DOWNLOAD_URL="$OPTARG"
         MAVEN_LOCAL="n"
         ;;
      o )
         OUTPUT_DIR="$OPTARG"
         ;;
      f )
         FORCE_UPDATE="y"
         ;;
      j)
         FORCE_JAVA="y"
         ;;
      l)
         MAVEN_LOCAL="y"
         ;;
      h )
         usage
         ;;
      \? )
         echo "Invalid option: $OPTARG" 1>&2
         usage
         ;;
      : )
         echo "Invalid option: $OPTARG requires an argument" 1>&2
         usage
         ;;
  esac
done
shift $((OPTIND -1))

# Get IPC_GEN_VERSION from 
IPC_GEN_VERSION=$(grep ^ipcGeneratorVersion ${SOURCE_DIR}/version.properties | awk '{ print $3 }')

# Use Java unless we are on a machine where we can run our native generator
USE_JAVA=1

if [ "$FORCE_JAVA" = "n" ]; then
    # Only 64 bit linux can run the native generator
    if [[ `uname -s` == Linux*  && `uname -m` == x86_64 ]]; then
        USE_JAVA=0
    fi
fi

if [ ${USE_JAVA} -eq 0 ]; then
     EXE_FILE="${OUTPUT_DIR}/ipcGenerator-${IPC_GEN_VERSION}"
     EXE_COMMAND="${EXE_FILE}"
     DOWNLOAD_FILE="ipcGenerator-${IPC_GEN_VERSION}.zip"
else
     EXE_FILE="${OUTPUT_DIR}/ipcGenerator-${IPC_GEN_VERSION}.jar"
     EXE_COMMAND="java -jar ${EXE_FILE}"
     DOWNLOAD_FILE="ipcGenerator-${IPC_GEN_VERSION}.jar"
fi

if [ "$FORCE_UPDATE" = "y" ]; then
   # Removing the file should force an update
   rm -f "${EXE_FILE}"
fi

# Download/build the generator if we don't have it
if [ ! -s "${EXE_FILE}" ]; then
   if [ ! -d "${OUTPUT_DIR}" ]; then
      mkdir -p ${OUTPUT_DIR}
   fi
   if [ "$MAVEN_LOCAL" = "n" ]; then
      wget -O "${OUTPUT_DIR}/${DOWNLOAD_FILE}" \
         "${BASE_DOWNLOAD_URL}/com/comcast/ipcGenerator/${IPC_GEN_VERSION}/${DOWNLOAD_FILE}"
   else
      if [ ! -f "${SOURCE_DIR}/tools/ipcGenerator/build/libs/${DOWNLOAD_FILE}" ]; then
         pushd ${SOURCE_DIR};
         gradle :ipcGenerator:build;
         popd;
      fi
      cp -v "${SOURCE_DIR}/tools/ipcGenerator/build/libs/${DOWNLOAD_FILE}" "${OUTPUT_DIR}/${DOWNLOAD_FILE}"
   fi
   if [[ ${DOWNLOAD_FILE} == *.zip ]]; then
      unzip "${OUTPUT_DIR}/${DOWNLOAD_FILE}" -d "${OUTPUT_DIR}"
      rm "${OUTPUT_DIR}/${DOWNLOAD_FILE}"
   fi
   chmod 755 "${EXE_FILE}"
fi

DEF_DIR="${SOURCE_DIR}/tools/ipcGenerator/definitions"

# Check if we need to regenerate.  We store a combined sha256 of the binary used to generate and all the definitions
HASH_CMD="sha256sum"
RUN_IPC_GEN="Y"
if type ${HASH_CMD} >/dev/null 2>&1; then
   OUTPUT_SHA="${OUTPUT_DIR}/gen.hash"
   DEFS_SHA=$(find "${DEF_DIR}" -name "*.xml" \
      | LC_ALL=C sort | xargs ${HASH_CMD} | ${HASH_CMD} | awk '{ print $1 }')
   GEN_SHA=$(${HASH_CMD} "${EXE_FILE}")
   TOTAL_SHA=$(echo "${DEFS_SHA} ${GEN_SHA}" | ${HASH_CMD} | awk '{ print $1 }')

   if [ -f "${OUTPUT_SHA}" ]; then
      EXISTING_SHA=$(cat "${OUTPUT_SHA}")
      if [ "${TOTAL_SHA}" = "${EXISTING_SHA}" ]; then
         echo "ipcGenerator.sh: No changes detected, skipping generation..."
         RUN_IPC_GEN="N"
      else
         echo "ipcGenerator.sh: Hash mismatch, existing=${EXISTING_SHA} current=${TOTAL_SHA}"
      fi
   fi
fi

if [ "${RUN_IPC_GEN}" = "Y" ]; then
   # Run the generator
   ${EXE_COMMAND} -f "${DEF_DIR}" -d "${OUTPUT_DIR}"

   #Store off the SHA
   if [ ! -z "${TOTAL_SHA}" ]; then
      echo "${TOTAL_SHA}" > "${OUTPUT_SHA}"
   fi
fi
