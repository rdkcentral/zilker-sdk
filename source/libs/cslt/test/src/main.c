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

#include "test_internal.h"

#include "../../src/passthru_transcoder.h"
#include "../../src/cslt_internal.h"

#define TESTDEC_VALUE "This is test decode syntax."
#define TESTENC_VALUE "Encode syntax, yes we have!"

void save_file(const char* filename, const void* data, size_t size)
{
    size_t ret;
    FILE* fout = fopen(filename, "w");

    assert_non_null(fout);

    ret = fwrite(data, 1, size, fout);
    assert_int_equal(ret, size);

    fflush(fout);
    fclose(fout);
}

static bool testenc2testenc_is_valid(const char* schema)
{
    if ((schema == NULL) || (strlen(schema) == 0)) return false;

    return (strcmp(schema, TESTENC_VALUE) == 0);
}

static bool testdec2testenc_is_valid(const char* schema)
{
    if ((schema == NULL) || (strlen(schema) == 0)) return false;

    return (strcmp(schema, TESTDEC_VALUE) == 0);
}

static int testdec2testenc_transcode(const char* src, char** dst, size_t size)
{
    size_t written_bytes;

    if ((src == NULL) || (strlen(src) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((dst == NULL) || (size == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((*dst == NULL) || (size != SHEEN_MSGSIZE)) {
        errno = EINVAL;
        return -1;
    }

    if (!testdec2testenc_is_valid(src)) {
        errno = EBADMSG;
        return -1;
    }

    written_bytes = strlen(TESTENC_VALUE) + 1;

    if (written_bytes <= size) {
        memcpy(*dst, TESTENC_VALUE, written_bytes);
    } else {
        written_bytes = size;
        memcpy(*dst, TESTENC_VALUE, size);
    }

    return (int) written_bytes;
}

// Unit tests
static int schematrans_test_factory(void)
{
    const cslt_factory_t* factory;
    const cslt_transcoder_t* transcoder;

    // Test Getting the transcoder factory
    factory = cslt_get_transcode_factory("whatwhat");

    if (factory != NULL) return EXIT_FAILURE;

    // Test Getting the transcoder factory
    factory = cslt_get_transcode_factory("testenc");

    if (factory == NULL) return EXIT_FAILURE;
    if (strcmp(factory->encoder, "testenc") != 0) return EXIT_FAILURE;
    if (factory->transcode_map == NULL) return EXIT_FAILURE;
    if (hashMapCount(factory->transcode_map) == 0) return EXIT_FAILURE;

    // Test getting the transcoder by explicit name
    transcoder = cslt_get_transcoder_by_name("whatwhat", "testenc");
    if (transcoder != NULL) return EXIT_FAILURE;

    // Test getting the transcoder by explicit name
    transcoder = cslt_get_transcoder_by_name("testdec", "whatwhat");
    if (transcoder != NULL) return EXIT_FAILURE;

    // Test getting the transcoder by explicit name
    transcoder = cslt_get_transcoder_by_name("", "testenc");
    if (transcoder != NULL) return EXIT_FAILURE;

    // Test getting the transcoder by explicit name
    transcoder = cslt_get_transcoder_by_name("testdec", NULL);
    if (transcoder != NULL) return EXIT_FAILURE;

    // Test getting the transcoder by explicit name
    transcoder = cslt_get_transcoder_by_name("testdec", "testenc");

    if (transcoder == NULL) return EXIT_FAILURE;
    if (transcoder->decoder == NULL) return EXIT_FAILURE;
    if (transcoder->encoder == NULL) return EXIT_FAILURE;
    if (strcmp(transcoder->decoder, "testdec") != 0) return EXIT_FAILURE;
    if (strcmp(transcoder->encoder, "testenc") != 0) return EXIT_FAILURE;
    if (transcoder->is_valid == NULL) return EXIT_FAILURE;
    if (transcoder->transcode == NULL) return EXIT_FAILURE;

    // Test getting the dec->enc transcoder
    transcoder = cslt_get_transcoder(factory, TESTDEC_VALUE);

    if (transcoder == NULL) return EXIT_FAILURE;
    if (transcoder->decoder == NULL) return EXIT_FAILURE;
    if (transcoder->encoder == NULL) return EXIT_FAILURE;
    if (strcmp(transcoder->decoder, "testdec") != 0) return EXIT_FAILURE;
    if (strcmp(transcoder->encoder, "testenc") != 0) return EXIT_FAILURE;
    if (transcoder->is_valid == NULL) return EXIT_FAILURE;
    if (transcoder->transcode == NULL) return EXIT_FAILURE;

    // Test getting the passthru transcoder
    transcoder = cslt_get_transcoder(factory, TESTENC_VALUE);

    if (transcoder == NULL) return EXIT_FAILURE;
    if (transcoder->decoder == NULL) return EXIT_FAILURE;
    if (transcoder->encoder == NULL) return EXIT_FAILURE;
    if (strcmp(transcoder->decoder, "testenc") != 0) return EXIT_FAILURE;
    if (strcmp(transcoder->encoder, "testenc") != 0) return EXIT_FAILURE;
    if (transcoder->is_valid == NULL) return EXIT_FAILURE;
    if (transcoder->transcode == NULL) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static int schematrans_test_is_valid(void)
{
    const cslt_transcoder_t* transcoder;

    transcoder = cslt_get_transcoder_by_name("testdec", "testenc");

    if (transcoder == NULL) return EXIT_FAILURE;
    if (!transcoder->is_valid(TESTDEC_VALUE)) return EXIT_FAILURE;
    if (transcoder->is_valid(TESTENC_VALUE)) return EXIT_FAILURE;

    transcoder = cslt_get_transcoder_by_name("testenc", "testenc");

    if (transcoder == NULL) return EXIT_FAILURE;
    if (!transcoder->is_valid(TESTENC_VALUE)) return EXIT_FAILURE;
    if (transcoder->is_valid(TESTDEC_VALUE)) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static int schematrans_test_transcode(void)
{
    const cslt_transcoder_t* transcoder;
    char* buffer = NULL;
    int ret;
    size_t size;

    transcoder = cslt_get_transcoder_by_name("testdec", "testenc");
    if (transcoder == NULL) return EXIT_FAILURE;

    ret = cslt_transcode_preallocated(transcoder, NULL, buffer, 0);
    if (ret != -1) return EXIT_FAILURE;

    ret = cslt_transcode_preallocated(transcoder, "", buffer, 0);
    if (ret != -1) return EXIT_FAILURE;

    ret = cslt_transcode_preallocated(transcoder, TESTDEC_VALUE, buffer, 0);
    if (ret != -1) return EXIT_FAILURE;

    size = strlen(TESTDEC_VALUE) - 5;
    buffer = malloc(size);

    ret = cslt_transcode_preallocated(transcoder, TESTDEC_VALUE, buffer, 0);
    if (ret != -1) return EXIT_FAILURE;

    ret = cslt_transcode_preallocated(transcoder, TESTDEC_VALUE, buffer, size);
    if (ret == -1) return EXIT_FAILURE;
    if (strncmp(TESTENC_VALUE, buffer, size) != 0) return EXIT_FAILURE;

    free(buffer);

    size = strlen(TESTDEC_VALUE) + 1;
    buffer = malloc(size);

    ret = cslt_transcode_preallocated(transcoder, TESTDEC_VALUE, buffer, size);
    if (ret == -1) return EXIT_FAILURE;
    if (strcmp(TESTENC_VALUE, buffer) != 0) return EXIT_FAILURE;

    free(buffer);

    transcoder = cslt_get_transcoder_by_name("testenc", "testenc");
    if (transcoder == NULL) return EXIT_FAILURE;

    size = strlen(TESTENC_VALUE) + 1;
    buffer = malloc(size);

    ret = cslt_transcode_preallocated(transcoder, TESTENC_VALUE, buffer, size);
    if (ret == -1) return EXIT_FAILURE;
    if (strcmp(TESTENC_VALUE, buffer) != 0) return EXIT_FAILURE;

    free(buffer);

    ret = cslt_transcode_by_name_preallocated("", "testenc", TESTDEC_VALUE, buffer, size);
    if (ret != -1) return EXIT_FAILURE;

    ret = cslt_transcode_by_name_preallocated(NULL, "testenc", TESTDEC_VALUE, buffer, size);
    if (ret != -1) return EXIT_FAILURE;

    ret = cslt_transcode_by_name_preallocated("testdec", "", TESTDEC_VALUE, buffer, size);
    if (ret != -1) return EXIT_FAILURE;

    ret = cslt_transcode_by_name_preallocated("testdec", "testenc", TESTENC_VALUE, buffer, size);
    if (ret != -1) return EXIT_FAILURE;

    size = strlen(TESTENC_VALUE) + 1;
    buffer = malloc(size);

    ret = cslt_transcode_by_name_preallocated("testdec", "testenc", TESTDEC_VALUE, buffer, size);
    if (ret == -1) return EXIT_FAILURE;
    if (strcmp(TESTENC_VALUE, buffer) != 0) return EXIT_FAILURE;

    free(buffer);

    size = strlen(TESTENC_VALUE) + 1;
    buffer = malloc(size);

    ret = cslt_transcode_by_schema_preallocated("testenc", TESTENC_VALUE, buffer, size);
    if (ret == -1) return EXIT_FAILURE;
    if (strcmp(TESTENC_VALUE, buffer) != 0) return EXIT_FAILURE;

    memset(buffer, 0, size);

    ret = cslt_transcode_by_schema_preallocated("testenc", TESTDEC_VALUE, buffer, size);
    if (ret == -1) return EXIT_FAILURE;
    if (strcmp(TESTENC_VALUE, buffer) != 0) return EXIT_FAILURE;

    ret = cslt_transcode_by_schema_preallocated("testenc", "what what", buffer, size);
    if (ret != -1) return EXIT_FAILURE;

    free(buffer);

    return EXIT_SUCCESS;
}

// Main setup
static cslt_factory_t test_factory = {
        .encoder = "testenc"
};

static const cslt_transcoder_t testdec2testenc_transcoder = {
        .decoder = "testdec",
        .encoder = "testenc",

        .is_valid = testdec2testenc_is_valid,
        .transcode = testdec2testenc_transcode
};

static const cslt_transcoder_t passthru_transcoder = {
        .decoder = "testenc",
        .encoder = "testenc",

        .is_valid = testenc2testenc_is_valid,
        .transcode = passthru_transcode
};

int sheens_test(void);

/* Setup our test transcoder and factory */
int main(int argc, const char* argv[])
{
    // Just use default settings
    cslt_init(NULL);

//    cslt_register_factory(&test_factory);
//    cslt_register_transcoder(&testdec2testenc_transcoder);
//    cslt_register_transcoder(&passthru_transcoder);

#if 0
    if (schematrans_test_factory() != EXIT_SUCCESS) return EXIT_FAILURE;
    if (schematrans_test_is_valid() != EXIT_SUCCESS) return EXIT_FAILURE;
    if (schematrans_test_transcode() != EXIT_SUCCESS) return EXIT_FAILURE;
#endif

    return sheens_test();
}
