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

#ifndef ZILKER_POJO_H
#define ZILKER_POJO_H

#include <stddef.h>

/**
 * @brief Object for C
 * This is a simple Object implementation for C. It has a private descriptor that can't be overwritten without lying,
 * and provides a consistent interface for clone and destroy operations. Currently it is used for IPC objects, but can
 * be used as a general form for anything.
 *
 * @note This carries on the historical "Plain Old Java Object" name as seen throughout libs/ipc for legibility.
 */

typedef struct Pojo Pojo;

/* Private */
typedef struct _Pojo _Pojo;

typedef void (*pojoDestructor)(Pojo *pojo);
typedef void *(*pojoCloneFunc)(Pojo *pojo);

struct Pojo {
    _Pojo * const context;
};

/**
 * Initialize an object context.
 * @param p A pointer to your object
 * @param pojoSize The result of sizeof(myObject)
 * @param destroyFunc A function that frees your object. It will receive a pointer to your object.
 * @param cloneFunc
 * @note Callers are responsible for allocating (and freeing, if required) p any way they prefer.
 */
void pojo_init(Pojo *p, size_t pojoSize, pojoDestructor destroyFunc, pojoCloneFunc cloneFunc);

/**
 * Destroy a pojo.
 * @param p A pointer to your object.
 */
void pojo_destroy(Pojo *p);

/**
 * Clone a pojo.
 * @return the cloned object.
 * @note if the object doesn't implement clone, it will return NULL
 */
void *pojo_clone(Pojo *p);

/**
 * Support function for generated code that is called by pojo_destroy that may incidentally be called directly.
 * This ensures any private data is freed.
 * @note You do not need to manually call this.
 */
void pojo_free(Pojo *p);

/**
 * Convenience function to auto-destroy an an RPC message whose pointer is on the stack.
 */
inline void pojo_destroy__auto(void *p)
{
    void **tmp = (void **) p;
    pojo_destroy(*tmp);
}

/**
 * Map helper to destroy a map containing a set of pojo objects.
 * @param key
 * @param value
 */
inline void pojoMapDestroyHelper(void *key, void *value)
{
    pojo_destroy(value);
}

#endif //ZILKER_POJO_H
