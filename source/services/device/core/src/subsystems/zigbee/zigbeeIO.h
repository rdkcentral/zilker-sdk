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

#ifndef ZILKER_ZIGBEE_IO_H
#define ZILKER_ZIGBEE_IO_H

/* TODO: create unit tests for this */

#include <stddef.h>
#include <stdint.h>

typedef struct _ZigbeeIOContext ZigbeeIOContext;

typedef enum {
    ZIO_READ,
    ZIO_WRITE
} ZigbeeIOMode;

/**
 * Initialize a Zigbee I/O context for payload marshalling.
 * Each call will advance an internal pointer to the next value in the payload until the end.
 * If a request would read/write past the end of the payload buffer, errno will be set
 * to ESPIPE.
 * An attempt to perform the wrong operation (e.g., write to a read buffer) will set errno
 * to EPERM.
 * Writing a string whose length will not fit in a uint8 will set errno to EINVAL.
 *
 * When errno is not 0, the entire operation should be considered invalid, and any read data should be discarded.
 */
ZigbeeIOContext *zigbeeIOInit(uint8_t payload[], size_t payloadLen, ZigbeeIOMode mode);

/**
 * Destroy a ZIO context. This can be safely called immediately after marshalling is complete.
 * @param ctx
 */
void zigbeeIODestroy(ZigbeeIOContext *ctx);

inline void zigbeeIODestroy__auto(ZigbeeIOContext **ctx)
{
    zigbeeIODestroy(*ctx);
}

/**
 * Convenience macro to declare a scope bound ZIO context. The context will be
 * destroyed as soon as it leaves the scope it was declared in.
 */
#define sbZigbeeIOContext AUTO_CLEAN(zigbeeIODestroy__auto) ZigbeeIOContext

/**
 * Get an unsigned char
 */
uint8_t zigbeeIOGetUint8(ZigbeeIOContext *ctx);

/**
 * Put an unsinged char
 * @param ctx
 * @param val
 */
void zigbeeIOPutUint8(ZigbeeIOContext *ctx, uint8_t val);

/**
 * Get a signed char
 */
int8_t zigbeeIOGetInt8(ZigbeeIOContext *ctx);

/**
 * Put a signed char
 * @param ctx
 * @param val
 */
void zigbeeIOPutInt8(ZigbeeIOContext *ctx, int8_t val);

/**
 * Get a short (length <= UINT8_MAX) string
 */
char *zigbeeIOGetString(ZigbeeIOContext *ctx);

/**
 * Read a raw byte array. This should only be used for legacy.
 * @see zigbeeIOGetString for ZCL strings
 */
void zigbeeIOGetBytes(ZigbeeIOContext *ctx, void *buf, uint8_t len);

/**
 * Put a short (length <= UINT8_MAX) string
 * @note Zigbee strings are uint8:len[len] (1+len bytes) long and are not NULL terminated
 * @see ZCLv7 2.6.2.11, 2.6.2.12
 * @param ctx
 * @param str
 */
void zigbeeIOPutString(ZigbeeIOContext *ctx, char *str);

/**
 * Write a raw byte array. This should only be used for legacy.
 * @see zigbeeIOPutString for ZCL strings
 */
void zigbeeIOPutBytes(ZigbeeIOContext *ctx, void *buf, uint8_t len);

/**
 * Get a short (16 bit)
 */
uint16_t zigbeeIOGetUint16(ZigbeeIOContext *ctx);

/**
 * Put a short (16 bit)
 * @param ctx
 * @param val
 */
void zigbeeIOPutUint16(ZigbeeIOContext *ctx, uint16_t val);

/**
 * Get a signed short (16 bit)
 */
int16_t zigbeeIOGetInt16(ZigbeeIOContext *ctx);

/**
 * Put a signed short (16 bit)
 * @param ctx
 * @param val
 */
void zigbeeIOPutInt16(ZigbeeIOContext *ctx, int16_t val);

/**
 * Get an int (32 bit)
 */
uint32_t zigbeeIOGetUint32(ZigbeeIOContext *ctx);

/**
 * Put an int
 * @param ctx
 * @param val
 */
void zigbeeIOPutUint32(ZigbeeIOContext *ctx, uint32_t val);

/**
 * Get a signed int (32 bit)
 */
int32_t zigbeeIOGetInt32(ZigbeeIOContext *ctx);

/**
 * Put a signed int (32 bit)
 * @param ctx
 * @param val
 */
void zigbeeIOPutInt32(ZigbeeIOContext *ctx, int32_t val);

#endif //ZILKER_ZIGBEE_IO_H
