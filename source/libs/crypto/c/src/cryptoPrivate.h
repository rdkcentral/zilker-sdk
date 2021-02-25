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

#ifndef ZILKER_CRYPTOPRIVATE_H
#define ZILKER_CRYPTOPRIVATE_H

/**
 * This is a private header for crypto modules that need to share
 * definitions for publicly opaque types. Be sure to exclude this from all public headers.
 */

#include <openssl/x509.h>

struct X509Cert
{
    X509 *cert;
};

#endif //ZILKER_CRYPTOPRIVATE_H
