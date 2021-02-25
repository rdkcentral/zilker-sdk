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

#ifndef ZILKER_PASSTHRU_TRANSCODER_H
#define ZILKER_PASSTHRU_TRANSCODER_H

/** A simple passthru transcoder that will make *dst equal
 * to src.
 *
 * The passthru will copy src to destination if the memory
 * pointed to by dst is non-NULL. Otherwise, it will assign
 * src to *dst to prevent memory copies.
 *
 * @param src The source schema to be decoded.
 * @param dst The destination memory to encode. If *dst is NULL
 * then passthru will assign *dst to src. Thus preventing a
 * memory copy.
 * @param size The number of bytes available in
 * the supplied destination buffer. If the size is
 * SHEEN_MSGSIZE, or *dst is non-NULL then size will
 * be ignored.
 * @return The number of bytes written on success, or
 * -1 on error and errno will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 * E2BIG: The final encoded schema was to big for the supplied
 * dst memory with the provided size.
 */
int passthru_transcode(const char* src, char** dst, size_t size);

#endif //ZILKER_PASSTHRU_TRANSCODER_H
