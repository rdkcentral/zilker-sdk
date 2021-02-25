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

package com.comcast.xh.zith.template;

import freemarker.template.Configuration;
import freemarker.template.Template;
import freemarker.template.TemplateException;
import freemarker.template.TemplateExceptionHandler;
import org.apache.commons.io.output.FileWriterWithEncoding;

import java.io.File;
import java.io.IOException;
import java.io.StringWriter;
import java.io.Writer;
import java.util.Map;

public class TemplateEngine
{
    private static final TemplateEngine instance = new TemplateEngine();

    public static TemplateEngine getInstance()
    {
        return instance;
    }

    private static Configuration cfg;

    static
    {
        cfg = new Configuration(Configuration.VERSION_2_3_28);
        cfg.setClassForTemplateLoading(TemplateEngine.class, "/templates");
        cfg.setDefaultEncoding("UTF-8");
        cfg.setTemplateExceptionHandler(TemplateExceptionHandler.RETHROW_HANDLER);
        cfg.setLogTemplateExceptions(false);
        cfg.setWrapUncheckedExceptions(true);
        cfg.setNumberFormat("computer");
    }

    private void processTemplate(String templatePath, Map<String,Object> model, Writer out) throws TemplateException
    {
        try
        {
            /* Get the template (uses cache internally) */
            Template temp = cfg.getTemplate(templatePath);

            temp.process(model, out);
        } catch (IOException e)
        {
            throw new TemplateException("Failed to load template " + templatePath, e, null);
        }
    }

    public String loadClasspathTemplate(String templatePath, Map<String,Object> model) throws TemplateException
    {
        StringWriter out = new StringWriter();
        processTemplate(templatePath, model, out);
        return out.toString();
    }

    public void writeClasspathTemplate(String templatePath, Map<String,Object> model, File outputFile) throws TemplateException
    {
        try (FileWriterWithEncoding out = new FileWriterWithEncoding(outputFile, "UTF-8"))
        {
            processTemplate(templatePath, model, out);
        } catch (IOException e)
        {
            throw new TemplateException("Failed to write output file", e, null);
        }

    }
}
