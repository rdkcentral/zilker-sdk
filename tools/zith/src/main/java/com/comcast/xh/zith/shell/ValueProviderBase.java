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

package com.comcast.xh.zith.shell;

import org.springframework.core.MethodParameter;
import org.springframework.shell.CompletionContext;
import org.springframework.shell.CompletionProposal;
import org.springframework.shell.standard.ValueProviderSupport;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

public abstract class ValueProviderBase extends ValueProviderSupport
{
    protected abstract Collection<String> getAllPossibleValues(MethodParameter parameter);

    @Override
    public List<CompletionProposal> complete(MethodParameter parameter, CompletionContext completionContext, String[] hints)
    {
        List<CompletionProposal> result = new ArrayList<>();
        String prefix = completionContext.currentWordUpToCursor();
        if (prefix == null)
        {
            prefix = "";
        }
        for (String value : getAllPossibleValues(parameter))
        {
            if (value.startsWith(prefix))
            {
                // Escape spaces, even though Spring is supposed to take care of this
                result.add(new CompletionProposal(value.replace(" ", "\\ ")));
            }
        }
        return result;
    }
}
