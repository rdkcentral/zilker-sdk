
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

 package com.icontrol.generate.service.io;

import com.icontrol.generate.service.Main;
import freemarker.template.Configuration;
import freemarker.template.Template;
import freemarker.template.TemplateException;
import freemarker.template.TemplateExceptionHandler;

import java.io.*;
import java.util.Map;

public class FreemarkerHelper
{
    private static Configuration cfg;

    static
    {
        cfg = new Configuration(Configuration.VERSION_2_3_28);
        cfg.setClassForTemplateLoading(Main.class, "templates");
        cfg.setDefaultEncoding("UTF-8");
        cfg.setTemplateExceptionHandler(TemplateExceptionHandler.RETHROW_HANDLER);
        cfg.setLogTemplateExceptions(false);
        cfg.setWrapUncheckedExceptions(true);
        cfg.setNumberFormat("computer");
    }

    static String processTemplateToString(String templateName, Map<String,Object> model) throws IOException
    {
        /* Get the template (uses cache internally) */
        Template temp = cfg.getTemplate(templateName);

        /* Merge data-model with template */

        Writer out = new StringWriter();
        try
        {
            temp.process(model, out);
        } catch (TemplateException e)
        {
            throw new IOException("Failed to process template " + templateName, e);
        }

        return out.toString();
    }

    static void processTemplateToFile(File baseDir, String outDir, String fileName, String templateName, Map<String,Object> model) throws IOException
    {
        File fileOutDir = new File(baseDir, outDir);
        File outFile = new File(fileOutDir, fileName);

        /* Get the template (uses cache internally) */
        Template temp = cfg.getTemplate(templateName);

        /* Merge data-model with template */

        Writer out = new FileWriter(outFile);
        try
        {
            temp.process(model, out);
        } catch (TemplateException e)
        {
            throw new IOException("Failed to process template " + templateName, e);
        }

    }
}
