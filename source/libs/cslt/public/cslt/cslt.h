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
// Created by Boyd, Weston on 4/17/18.
//

#ifndef ZILKER_CSLT_H
#define ZILKER_CSLT_H

#include <stddef.h>
#include <stdbool.h>

#include <icTypes/icHashMap.h>

/** An individual transcoder that describes what
 * it can decode from and encode to.
 */
typedef struct cslt_transcoder {
    /** The supported decoder for this transcoder. */
    const char* decoder;

    /** The supported encoder for this transcoder. */
    const char* encoder;

    /** Verify that this transcoder can support the
     * supplied schema.
     *
     * @param schema The schema to be transcoded.
     * (Example: iControl legacy Rules)
     * @return True if the schema is supported by this
     * transcoder, otherwise false.
     */
    bool (*is_valid) (const char* schema);

    /** Transcode the supplied source schema to a
     * new schema.
     *
     * @param src The source schema that will be decoded.
     * @param dst The destination buffer for the new encoded
     * schema. If the dst points to a NULL pointer then a
     * new buffer will be created by the transcoder and supplied
     * back to the caller. If the dst points to a non-NULL pointer
     * and size is greater than zero then supplied buffer will be
     * used up to the provided size.
     * @param size The number of bytes available to write into the
     * dst pointed memory. If dst points to NULL then this parameter
     * is ignored.
     * @return The total number of bytes written. If dst point to
     * a NULL pointer, and src points to a passthru schema then
     * the returned value may be zero. On error -1 will be returned
     * and errno will be filled in.
     * EINVAL: An invalid parameter was supplied.
     * ENOMEM: Memory failed to allocate internally.
     * EBADMSG: The supplied schema contained an error that caused
     * the transcoder to fail.
     * E2BIG: The final encoded schema was to big for the supplied
     * dst memory with the provided size. This error may only
     * occur when both a valid dst memory address and a size greater
     * than zero is provided.
     */
    int (*transcode) (const char* src, char** dst, size_t size);

    /**
     * The version of the transcoder
     */
    int transcoder_version;
} cslt_transcoder_t;

/** An abstract factory that will provide various
 * transcoders that all encode to a specific
 * schema.
 */
typedef struct cslt_factory {
    /** The encoder this factory provides for. */
    const char* encoder;

    /** Container for all transcoders ordered by their decoders. */
    icHashMap* transcode_map;
} cslt_factory_t;

/** Initialize the schema languange transcoder library. A map
 * of settings unique to each transcoder may be provided in
 * order to properly initialize.
 *
 * @param transcoder_settings A map pointing to the settings
 * for each transcoder. The settings are optional and defaults
 * will always be utilized if no settings are provided. See
 * the specific transcoder headers for what settings are
 * supported.
 */
void cslt_init(icHashMap* transcoder_settings);

/** Retrieve a factory for a specific schema encoder to
 * produce new transcode instances of varying decode types.
 *
 * @param encoder The encoder needed for transcoding.
 * @return A factory that may produce new transcoders of
 * varying decode types. NULL if no factory was found, and
 * errno will be supplied.
 * EINVAL: Invalid parameter supplied.
 * ENOTSUP: The encoder is not supported.
 */
const cslt_factory_t* cslt_get_transcode_factory(const char* encoder);

/** Retrieve a specific transcoder.
 *
 * @param decoder The schema name that will be decoded.
 * @param encoder The schema name that will be encoded.
 * @return A schema transcoded that may be used to transcode
 * schemas. NULL if an error occurs, or no transcoder found.
 * EINVAL: Invalid parameter supplied.
 * ENOTSUP: The transcoder is not supported.
 */
const cslt_transcoder_t* cslt_get_transcoder_by_name(const char* decoder,
                                                     const char* encoder);

/** Retrieve a transcoder from the factory that supports a
 * provided schema.
 *
 * @param factory A transcoder factory that will search for the
 * supported schemas.
 * @param schema A schema that wants to be transcoded.
 * @return A transcoder that supports the supplied schema.
 * NULL if an error occurs, or no transcoder found.
 * EINVAL: Invalid parameter supplied.
 * ENOTSUP: The transcoder is not supported.
 */
const cslt_transcoder_t* cslt_get_transcoder(const cslt_factory_t* factory,
                                             const char* schema);

/** Transcode a schema to the new format.
 *
 * The dst will always point to NULL, and thus
 * a new buffer will be provided for the caller
 * on return. If the transcode is a passthru then
 * zero bytes written will be returned and
 * *dst will be set to src. This eliminates
 * any memory copies.
 *
 * @param transcoder The transcoder to be used.
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode
 * the new schema into.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 */
int cslt_transcode(const cslt_transcoder_t* transcoder,
                   const char* src,
                   char** dst);

/** Transcode a schema to the new format.
 *
 * The caller must provide a valid memory destination, and
 * a enough space in the buffer to fill in the new
 * encoded schema.
 *
 * @param transcoder The transcoder to be used.
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode
 * the new schema into.
 * @param size The number of bytes available in
 * the supplied destination buffer.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 * E2BIG: The final encoded schema was to big for the supplied
 * dst memory with the provided size.
 */
int cslt_transcode_preallocated(const cslt_transcoder_t* transcoder,
                                const char* src,
                                char* dst,
                                size_t size);

/** Transcode a schema by looking up the transcoder
 * internally for a given encoder/decoder.
 *
 * This is a one-shot convenience routine to help
 * transcode a schema.
 *
 * The dst will always point to NULL, and thus
 * a new buffer will be provided for the caller
 * on return. If the transcode is a passthru then
 * zero bytes written will be returned and
 * *dst will be set to src. This eliminates
 * any memory copies.
 *
 * @param decoder The schema name that will be decoded.
 * @param encoder The schema name that will be encoded.
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode
 * the new schema into.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 */
int cslt_transcode_by_name(const char* decoder,
                           const char* encoder,
                           const char* src,
                           char** dst);

/** Transcode a schema by looking up the transcoder
 * internally for a given encoder/decoder.
 *
 * This is a one-shot convenience routine to help
 * transcode a schema.
 *
 * The caller must provide a valid memory destination, and
 * a enough space in the buffer to fill in the new
 * encoded schema.
 *
 * @param decoder The schema name that will be decoded.
 * @param encoder The schema name that will be encoded.
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode
 * the new schema into.
 * @param size The number of bytes available in
 * the supplied destination buffer.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 * E2BIG: The final encoded schema was to big for the supplied
 * dst memory with the provided size.
 */
int cslt_transcode_by_name_preallocated(const char* decoder,
                                        const char* encoder,
                                        const char* src,
                                        char* dst,
                                        size_t size);

/** ranscode a schema by looking up the transcoder
 * internally for a given encoder that is capable of
 * decoding the supplied schema.
 *
 * This is a one-shot convenience routine to help
 * transcode a schema.
 *
 * The dst will always point to NULL, and thus
 * a new buffer will be provided for the caller
 * on return. If the transcode is a passthru then
 * zero bytes written will be returned and
 * *dst will be set to src. This eliminates
 * any memory copies.
 *
 * @param encoder The schema name that will be encoded.
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode
 * the new schema into.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 */
int cslt_transcode_by_schema(const char* encoder,
                             const char* src,
                             char** dst);

/** Transcode a schema by looking up the transcoder
 * internally for a given encoder that is capable of
 * decoding the supplied schema.
 *
 * This is a one-shot convenience routine to help
 * transcode a schema.
 *
 * The caller must provide a valid memory destination, and
 * a enough space in the buffer to fill in the new
 * encoded schema.
 *
 * @param encoder The schema name that will be encoded.
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode
 * the new schema into.
 * @param size The number of bytes available in
 * the supplied destination buffer.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 * E2BIG: The final encoded schema was to big for the supplied
 * dst memory with the provided size.
 */
int cslt_transcode_by_schema_preallocated(const char* encoder,
                                          const char* src,
                                          char* dst,
                                          size_t size);

#endif //ZILKER_CSLT_H
