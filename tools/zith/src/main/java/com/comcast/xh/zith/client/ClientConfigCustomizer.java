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

import org.apache.commons.io.IOUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.core.io.Resource;
import org.springframework.core.io.support.PathMatchingResourcePatternResolver;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public interface ClientConfigCustomizer
{
    Logger logger = LoggerFactory.getLogger(ClientConfigCustomizer.class);

    void customizeConfig(File configDir);

    static void copyConfigToStorageNamespace(File configDir, String storageNamespace, String classpathPattern)
    {
        File etcDir = new File(configDir, "etc");
        File storageDir = new File(etcDir, "storage");
        File namespaceDir = new File(storageDir, storageNamespace);
        if (!namespaceDir.exists())
        {
            namespaceDir.mkdirs();
        }

        try
        {
            PathMatchingResourcePatternResolver resolver = new PathMatchingResourcePatternResolver();
            for (Resource resource : resolver.getResources("classpath*:" + classpathPattern))
            {
                File outputFile = new File(namespaceDir, resource.getFilename());
                logger.debug("Copying resource " + resource.getURI() + " to " + outputFile.getAbsolutePath());
                try (
                        InputStream is = resource.getInputStream();
                        FileOutputStream fos = new FileOutputStream(outputFile);
                )
                {

                    IOUtils.copy(is, fos);
                } catch (IOException e)
                {
                    throw new RuntimeException("Failed to read config", e);
                }
            }
        } catch (IOException e)
        {
            throw new RuntimeException("Failed to read config", e);
        }
    }
}
