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

/*-----------------------------------------------
 * obfuscation.h
 *
 * attempt to hide data into a larger buffer.  uses
 * masking and random generation to keep subsequent
 * obfuscations as dis-similar as possible.
 * designed internally, so please holler if you have
 * suggestions on making this better...
 *
 * NOTE: this is NOT encryption, so use at your own risk!
 *
 * Author: jelderton -  8/18/16.
 *-----------------------------------------------*/

#ifndef ZILKER_OBFUSCATION_H
#define ZILKER_OBFUSCATION_H

#include <stdint.h>

/*
 * obfuscate some data into a buffer that can be saved or used
 * for other purposes (note: may want to base64 or uuencode the
 * result as the output will be binary).
 * the algorithm requires a 'passphrase' to serve as a key - which
 * is combined with the input data and then sprinkled across a
 * buffer of random bytes.
 *
 * caller must free the returned buffer, and take note of the in/out
 * parameter 'outputLen' - describing the length of the returned buffer.
 *
 * @param passphrase - well-known password to use while encoding 'input'
 * @param passLen - length of the password string
 * @param input - data to obfuscate
 * @param inputLen - length of the data to obfuscate
 * @param outputLen - length of the returned buffer.
 */
char *obfuscate(const char *passphrase, uint32_t passLen,
                const char *input, uint32_t inputLen,
                uint32_t *outputLen);

/*
 * given an obfuscated buffer, extract and return the original value.
 * requires the same 'passphrase' used when encoding the buffer.
 *
 * caller must free the returned buffer, and take note of the in/out
 * parameter - describing the length of the returned buffer.
 *
 * @param passphrase - well-known password used during obfuscation
 * @param passLen - length of the password string
 * @param input - data to unobfuscate
 * @param inputLen - length of the data to unobfuscate
 * @param outputLen - length of the returned buffer.
 */
char *unobfuscate(const char *passphrase, uint32_t passLen,
                  const char *input, uint32_t inputLen,
                  uint32_t *outputLen);

#endif // ZILKER_OBFUSCATION_H
