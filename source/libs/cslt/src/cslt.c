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
// Created by Boyd, Weston on 4/16/18.
//

#include <stdio.h>
#include <pthread.h>

#include <icTypes/icHashMap.h>
#include <string.h>
#include <errno.h>

#include <cslt/cslt.h>
#include <cslt/sheens.h>
#include <cslt/icrules.h>

#include <icTypes/icLinkedList.h>

#include "cslt_internal.h"
#include "sheens/sheens_transcoders.h"
#include "icrule/icrule_transcoders.h"

static icHashMap* factory_map = NULL;
static pthread_mutex_t mutex;

void cslt_init(icHashMap* transcoder_settings)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &attr);

    pthread_mutex_lock(&mutex);
    if (factory_map == NULL) {
        icHashMap* settings = NULL;

        factory_map = hashMapCreate();

        if (transcoder_settings != NULL) {
            // Initialize our currently supported transcoders
            settings = hashMapGet(transcoder_settings,
                                  TRANSCODER_NAME_SHEENS,
                                  (uint16_t) strlen(TRANSCODER_NAME_SHEENS));
        }

        sheens_transcoder_init(settings);

        if (transcoder_settings != NULL) {
            settings = hashMapGet(transcoder_settings,
                                  TRANSCODER_NAME_ICRULES,
                                  (uint16_t) strlen(TRANSCODER_NAME_ICRULES));
        }

        icrule_transcoder_init(settings);
    }
    pthread_mutex_unlock(&mutex);
}

const cslt_factory_t* cslt_get_transcode_factory(const char* encoder)
{
    cslt_factory_t* factory;

    if ((encoder == NULL) || (strlen(encoder) == 0)) {
        errno = EINVAL;
        return NULL;
    }

    pthread_mutex_lock(&mutex);
    factory = hashMapGet(factory_map, (void*) encoder, FACTORY_HASHMAP_KEYLEN(encoder));
    pthread_mutex_unlock(&mutex);

    if (factory == NULL) {
        errno = ENOTSUP;
    }

    return factory;
}

const cslt_transcoder_t* cslt_get_transcoder_by_name(const char* decoder,
                                                     const char* encoder)
{
    cslt_factory_t* factory;
    cslt_transcoder_t* transcoder = NULL;

    if ((decoder == NULL) || (strlen(decoder) == 0) ||
        (encoder == NULL) || (strlen(encoder) == 0)) {
        errno = EINVAL;
        return NULL;
    }

    pthread_mutex_lock(&mutex);
    factory = hashMapGet(factory_map, (void*) encoder, FACTORY_HASHMAP_KEYLEN(encoder));
    if (factory) {
        transcoder = hashMapGet(factory->transcode_map,
                                (void*) decoder,
                                FACTORY_HASHMAP_KEYLEN(decoder));
    }
    pthread_mutex_unlock(&mutex);

    if (transcoder == NULL) {
        errno = ENOTSUP;
    }

    return transcoder;
}

const cslt_transcoder_t* cslt_get_transcoder(const cslt_factory_t* factory,
                                             const char* schema)
{
    cslt_transcoder_t* transcoder = NULL;
    icHashMapIterator* iterator;

    if (factory == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if ((schema == NULL) || (strlen(schema) == 0)) {
        errno = EINVAL;
        return NULL;
    }

    pthread_mutex_lock(&mutex);
    iterator = hashMapIteratorCreate(factory->transcode_map);

    while (hashMapIteratorHasNext(iterator)) {
        char* decoder = NULL;
        uint16_t keylen = 0;

        if (hashMapIteratorGetNext(iterator,
                                   (void**) &decoder, &keylen,
                                   (void**) &transcoder)) {
            if (transcoder &&
                transcoder->is_valid &&
                transcoder->is_valid(schema)) {
                break;
            }
        }

        transcoder = NULL;
    }
    pthread_mutex_unlock(&mutex);

    hashMapIteratorDestroy(iterator);

    if (transcoder == NULL) {
        errno = ENOTSUP;
    }

    return transcoder;
}

static int cslt_transcode_internal(const cslt_transcoder_t* transcoder,
                                   const char* src,
                                   char** dst,
                                   size_t size)
{
    if ((src == NULL) || (strlen(src) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((dst == NULL) || (size == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((*dst == NULL) && (size != SHEEN_MSGSIZE)) {
        errno = EINVAL;
        return -1;
    }

    if (transcoder == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (transcoder->transcode == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    if (transcoder->is_valid && !transcoder->is_valid(src)) {
        errno = EBADMSG;
        return -1;
    }

    return transcoder->transcode(src, dst, size);
}
int cslt_transcode_preallocated(const cslt_transcoder_t* transcoder,
                                const char* src,
                                char* dst,
                                size_t size)
{
    return cslt_transcode_internal(transcoder, src, &dst, size);
}

int cslt_transcode(const cslt_transcoder_t* transcoder,
                   const char* src,
                   char** dst)
{
    if (dst != NULL) *dst = NULL;

    return cslt_transcode_internal(transcoder, src, dst, SHEEN_MSGSIZE);

}

static int cslt_transcode_by_name_internal(const char* decoder,
                                           const char* encoder,
                                           const char* src,
                                           char** dst,
                                           size_t size)
{
    const cslt_transcoder_t* transcoder;

    if ((decoder == NULL) || (strlen(decoder) == 0) ||
        (encoder == NULL) || (strlen(encoder) == 0)) {
        errno = EINVAL;
        return -1;
    }

    transcoder = cslt_get_transcoder_by_name(decoder, encoder);

    if ((transcoder == NULL) || (transcoder->is_valid == NULL)) {
        errno = ENOTSUP;
        return -1;
    }

    return cslt_transcode_internal(transcoder, src, dst, size);
}

int cslt_transcode_by_name(const char* decoder,
                           const char* encoder,
                           const char* src,
                           char** dst)
{
    if (dst != NULL) *dst = NULL;

    return cslt_transcode_by_name_internal(decoder, encoder, src, dst, SHEEN_MSGSIZE);
}

int cslt_transcode_by_name_preallocated(const char* decoder,
                                        const char* encoder,
                                        const char* src,
                                        char* dst,
                                        size_t size)
{
    return cslt_transcode_by_name_internal(decoder, encoder, src, &dst, size);
}

static int cslt_transcode_by_schema_internal(const char* encoder,
                                             const char* src,
                                             char** dst,
                                             size_t size)
{
    const cslt_factory_t* factory;
    const cslt_transcoder_t* transcoder;

    if ((encoder == NULL) || (strlen(encoder) == 0)) {
        errno = EINVAL;
        return -1;
    }

    factory = cslt_get_transcode_factory(encoder);

    if (factory == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    transcoder = cslt_get_transcoder(factory, src);

    if (transcoder == NULL) {
        errno = ENOTSUP;
        return -1;
    }

    return cslt_transcode_internal(transcoder, src, dst, size);
}

int cslt_transcode_by_schema(const char* encoder,
                             const char* src,
                             char** dst)
{
    if (dst != NULL) *dst = NULL;

    return cslt_transcode_by_schema_internal(encoder, src, dst, SHEEN_MSGSIZE);
}

int cslt_transcode_by_schema_preallocated(const char* encoder,
                                          const char* src,
                                          char* dst,
                                          size_t size)
{
    return cslt_transcode_by_schema_internal(encoder, src, &dst, size);
}

/* Private to namespace*/
void cslt_register_factory(cslt_factory_t* factory)
{
    if (factory) {
        const cslt_factory_t* found_factory;

        pthread_mutex_lock(&mutex);
        found_factory = hashMapGet(factory_map,
                                   (void*) factory->encoder,
                                   FACTORY_HASHMAP_KEYLEN(factory->encoder));
        if (found_factory == NULL) {
            factory->transcode_map = hashMapCreate();
            hashMapPut(factory_map,
                       (void*) factory->encoder,
                       FACTORY_HASHMAP_KEYLEN(factory->encoder),
                       (void*) factory);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void cslt_register_transcoder(const cslt_transcoder_t* transcoder)
{
    if (transcoder) {
        const cslt_factory_t* factory;

        pthread_mutex_lock(&mutex);
        factory = hashMapGet(factory_map,
                             (void*) transcoder->encoder,
                             FACTORY_HASHMAP_KEYLEN(transcoder->encoder));
        if (factory) {
            const cslt_transcoder_t* found_transcoder;

            found_transcoder = hashMapGet(factory->transcode_map,
                                          (void*) transcoder->decoder,
                                          FACTORY_HASHMAP_KEYLEN(transcoder->decoder));
            if (found_transcoder == NULL) {
                hashMapPut(factory->transcode_map,
                           (void*) transcoder->decoder,
                           FACTORY_HASHMAP_KEYLEN(transcoder->decoder),
                           (void*) transcoder);
            }
        }
        pthread_mutex_unlock(&mutex);
    }
}
