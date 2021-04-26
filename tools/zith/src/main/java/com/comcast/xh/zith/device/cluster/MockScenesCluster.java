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

package com.comcast.xh.zith.device.cluster;

//
// Created by tlea on 1/4/19.
//

public class MockScenesCluster extends MockCluster
{
    public MockScenesCluster(String eui64, int endpointId)
    {
        super("scenes", eui64, endpointId, 0x05, true);

        addAttribute(0, 32);
        addAttribute(1, 32);
        addAttribute(2, 33);
        addAttribute(3, 16);
        addAttribute(4, 24);
    }
}
