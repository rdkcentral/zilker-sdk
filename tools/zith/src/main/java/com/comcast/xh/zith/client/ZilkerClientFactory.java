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

package com.comcast.xh.zith.client;

import org.apache.commons.lang.SystemUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.lang.reflect.Constructor;

public class ZilkerClientFactory
{
    private static final Logger logger = LoggerFactory.getLogger(ZilkerClientFactory.class);
    private static Class<? extends NativeZilkerClient> classToUse = NativeZilkerClient.class;

    public static NativeZilkerClient getNativeZilkerClient()
    {
        if (classToUse == null)
        {
            // Native only on Linux
            if (SystemUtils.IS_OS_LINUX || SystemUtils.IS_OS_MAC)
            {
                try
                {
                    NativeZilkerClient.findZilkerBinary(NativeZilkerClient.determineZilkerTop());
                    classToUse = NativeZilkerClient.class;
                    logger.debug("Detected and using native zilker client");
                } catch (Exception e)
                {
                    logger.debug("Native zilker client not found");
                }
            }
        }


        if (classToUse != null)
        {
            try
            {
                return classToUse.newInstance();
            } catch (Exception e)
            {
                throw new RuntimeException(e);
            }
        }

        throw new RuntimeException("Could not find compatible client to use");
    }
}
