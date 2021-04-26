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


# Convert a source file to a literal string, removing comments. A macro named like the file name will be defined
# that holds the file's text.

for f in "$@"; do
DEST_DIR=$(dirname "${f}")
FILENAME=`basename "${f}"`
DEST_FILENAME="${DEST_DIR}/${FILENAME}.h"
SYM=`echo "${FILENAME}" | awk '{ print toupper($0) }' | tr . _`_BLOB

echo "-- Processing ${f} -> ${DEST_FILENAME}"

cat <<EOF > "${DEST_FILENAME}"
#ifndef ${SYM}_H
#define ${SYM}_H

#define ${SYM} \\
EOF

# Escape double quotes and collapse spaces for C literal strings
# Whenever a {{identifier}} is seen, convert it to "identifier" to allow the C preprocessor to sub in any macros.
sed 's/\"/\\"/g;s/  */ /g;s/{{\([A-Za-z0-9_]*\)}}/\"\1\"/g' ${f} | \
awk 'BEGIN { continuation = "" }
     {
        # Look for and discard comments
        simple = index($0, "//")
        if ((cstart = index($0, "/*")) != 0 || simple != 0)
        {
            if (simple != 0)
            {
               $0 = substr($0, 1, simple - 1)
            }
            else
            {
                # Look for */, which may not be on this line.
                pre = substr($0, 1, cstart - 1)
                rest = substr($0, cstart + 2)
                cend = index(rest, "*/")
                while (cend == 0)
                {
                    if (getline <= 0)
                    {
                        print("EOF reached before comment end or error: ", ERRNO) > "/dev/stderr"
                    }

                    rest = rest $0
                    cend = index(rest, "*/")
                }

                $0 = pre substr(rest, cend + 2)
            }
        }
        if ($0 != "")
        {
            # Finish off the previous line then add the linefeed, so the last line has no macro continuation
            printf("%s\"%s\\n\"", continuation, $0)
            continuation = "\\\n"
        }
     }' >> "${DEST_FILENAME}"
echo -e "\n#endif //${SYM}_H" >> "${DEST_FILENAME}"

done
