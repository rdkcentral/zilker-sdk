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

import java.util.Base64;

public class AttributeData
{
    private int id;
    private int type;
    private boolean success;
    private int dataLen;
    private String data;
    private Integer mfgId = null;

    public AttributeData(int id, int type, byte[] data)
    {
        this(id, type, Base64.getEncoder().encodeToString(data));
        this.dataLen = data.length;
    }

    public AttributeData(int id, int type, String zhalData)
    {
        this.id = id;
        this.type = type;
        this.success = true;
        this.dataLen = -1;
        this.data = zhalData;
    }

    public AttributeData(int id, int type, byte[] data, Integer mfgId)
    {
        this(id, type, Base64.getEncoder().encodeToString(data), mfgId);
        this.dataLen = data.length;
    }

    public AttributeData(int id, int type, String zhalData, Integer mfgId)
    {
        this.id = id;
        this.type = type;
        this.success = true;
        this.dataLen = -1;
        this.data = zhalData;
        this.mfgId = mfgId;
    }

    public Integer getMfgId()
    {
        return mfgId;
    }

    public int getId()
    {
        return id;
    }

    public int getType()
    {
        return type;
    }

    public boolean isSuccess()
    {
        return success;
    }

    public int getDataLen()
    {
        if (data != null)
        {
            if (dataLen == -1)
            {
                dataLen = Base64.getDecoder().decode(data).length;
            }
        }
        else
        {
            return 0;
        }
        return dataLen;
    }

    public String getData()
    {
        return data;
    }

    public byte[] getRawData()
    {
        return Base64.getDecoder().decode(data);
    }
}
