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

#include <icIpc/pojo.h>
#include <icLog/logging.h>
#include <memory.h>
#include <stdint.h>
#include "icIpc/pojo.h"
#define LOG_TAG "ipcPOJO"

/* private */
struct _Pojo {
    size_t pojoSize;                        //This object's actual size (e.g., for heap allocation)
    pojoDestructor destroy;                 //destroy this object
    pojoCloneFunc clone;                    //clone this object
};

/* Public functions */

extern inline void pojo_destroy__auto(void *p);
extern inline void pojoMapDestroyHelper(void *key, void *value);

void pojo_init(Pojo *p, size_t pojoSize, pojoDestructor destroyFunc, pojoCloneFunc cloneFunc)
{
    _Pojo *_this = calloc(1, sizeof(_Pojo));
    _this->pojoSize = pojoSize;
    _this->destroy = destroyFunc;
    _this->clone = cloneFunc;

    memset(p, 0, pojoSize);
    /*
     * Because our private area pointer is protected by const, it cannot be assigned, but we can subvert typing and
     * write the address directly.
     */
    memcpy((Pojo **) &p->context, &_this, sizeof(_Pojo *));

    if (!destroyFunc)
    {
        icLogWarn(LOG_TAG, "init: destroy not set by object at %p", p);
    }

    if(!cloneFunc)
    {
        icLogWarn(LOG_TAG, "init: clone not set by object at %p", p);
    }
}

void pojo_destroy(Pojo *p)
{
    if (p)
    {
        _Pojo *const _this = p->context;
        if (_this && _this->destroy)
        {
            _this->destroy(p);
        }
        else
        {
            pojo_free(p);
        }
    }
    else
    {
        icLogError(LOG_TAG, "Can't destroy object at %p", p);
    }
}

void *pojo_clone(Pojo *p)
{
    Pojo *clone = NULL;
    if (p)
    {
        _Pojo *const _this = p->context;
        if (_this && _this->clone)
        {
            clone = _this->clone(p);
        }
    }

    if (!clone)
    {
        icLogWarn(LOG_TAG, "clone not supported by object at %p", p);
    }

    return clone;
}

void pojo_free(Pojo *p)
{
    _Pojo *const _this = p->context;
    free(_this);
    memset((void *) &p->context, 0, sizeof(p->context));
}