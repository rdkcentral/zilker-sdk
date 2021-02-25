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


# Get our external IP
EXTERNAL_IP=`ip route get 8.8.8.8 | awk -F"src " 'NR==1{split($2,a," ");print a[1]}'`

# Forward multicast to unicast, then java code will take the unicast and stick it back on multicast on the host
socat UDP4-RECVFROM:12575,ip-add-membership=225.0.0.50:127.0.0.1,fork,reuseaddr UDP4-SENDTO:host.docker.internal:12576 &

# zhal event forwarding.  zhal in container is only listening on localhost, this will forward from the exposed port
# to localhost in the container
socat UDP4-RECVFROM:8711,bind=${EXTERNAL_IP},fork,reuseaddr UDP4-SENDTO:localhost:8711 &

# zhal IPC forward.  zhal in container tries to talk to localhost, forward this to the host
socat TCP-LISTEN:18443,bind=localhost,fork,reuseaddr TCP:host.docker.internal:18443 &

# Forward IPC.  IPCs are only listening on localhost, this will forward from the expose port to localhost in the container
while read port; do
  socat TCP-LISTEN:${port},bind=${EXTERNAL_IP},fork,reuseaddr TCP:localhost:${port} &
done </zilker-client-helpers/ipcPorts.txt

# Forward HTTP and XMPP from localhost to host.docker.internal
socat TCP-LISTEN:8443,bind=localhost,fork,reuseaddr TCP:host.docker.internal:8443 &
socat TCP-LISTEN:5222,bind=localhost,fork,reuseaddr TCP:host.docker.internal:5222 &

# Start zilker using exec
echo "Starting zilker..."
exec /zilker-client/mirror/sbin/zilker "$@"
