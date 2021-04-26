/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

//
// Created by Boyd, Weston on 3/27/18.
//

#include <stdlib.h>
#include <stdio.h>

#ifdef CONFIG_LIB_NANOMSG
extern int pubsubTest_main(int argc, const char* argv[]);
#endif

extern int rpcTest_main(int argc, const char* argv[]);

int main(int argc, const char* argv[])
{
    printf("Starting RPC Tests....\n");
    if (rpcTest_main(argc, argv) != EXIT_SUCCESS) {
        printf("Failed RPC Tests.\n");
        return EXIT_FAILURE;
    }

#ifdef CONFIG_LIB_NANOMSG
    printf("Starting PubSub Tests....\n");
    if (pubsubTest_main(argc, argv) != EXIT_SUCCESS) {
        printf("Failed PubSub Tests.\n");
        return EXIT_FAILURE;
    }
#endif

    return EXIT_SUCCESS;
}
