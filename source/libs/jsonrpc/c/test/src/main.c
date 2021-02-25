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
// Created by wboyd747 on 7/11/18.
//

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <errno.h>

#include <libxml2/libxml/tree.h>
#include <jsonrpc/jsonrpc.h>

#define XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

#ifndef unused
#define unused(v) ((void) (v))
#endif

static int test_setup(void** state)
{
    *state = malloc(sizeof(jsonrpc_t));
    if (*state == NULL) return -1;

    return jsonrpc_init(*state);
}

static int test_teardown(void** state)
{
    jsonrpc_destroy(*state);
    free(*state);
    return 0;
}

static cJSON* test_register_method_handler(const cJSON* id, const cJSON* params)
{
    return NULL;
}

static void test_register_method(void** state)
{
    jsonrpc_t* jsonrpc = *state;
    bool ret;

    ret = jsonrpc_register_method(jsonrpc, "my_func", test_register_method_handler);
    assert_true(ret);

    ret = jsonrpc_register_method(jsonrpc, NULL, test_register_method_handler);
    assert_false(ret);

    ret = jsonrpc_register_method(NULL, "my_func", test_register_method_handler);
    assert_false(ret);

    ret = jsonrpc_register_method(jsonrpc, "my_func", NULL);
    assert_false(ret);
}

static void test_create_request(void** state)
{
    cJSON *id, *json, *json_test;

    unused(state);

    id = cJSON_CreateString("my id");

    json = jsonrpc_create_request(id, "my_func", NULL);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "id");
    assert_non_null(json_test);
    assert_non_null(json_test->valuestring);
    assert_string_equal(json_test->valuestring, "my id");

    json_test = cJSON_GetObjectItem(json, "method");
    assert_non_null(json_test);
    assert_non_null(json_test->valuestring);
    assert_string_equal(json_test->valuestring, "my_func");

    json_test = cJSON_GetObjectItem(json, "params");
    assert_null(json_test);

    cJSON_Delete(json);

    json_test = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json_test, "var1", cJSON_CreateNull());
    cJSON_AddItemToObjectCS(json_test, "var2", cJSON_CreateNull());

    json = jsonrpc_create_request(id, "my_func", json_test);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "params");
    assert_non_null(json_test);
    assert_true(cJSON_IsObject(json_test));
    assert_true(cJSON_HasObjectItem(json_test, "var1"));
    assert_true(cJSON_HasObjectItem(json_test, "var2"));

    cJSON_Delete(json);
    cJSON_Delete(id);
}

static void test_create_notification(void** state)
{
    cJSON *json, *json_test;

    unused(state);

    json = jsonrpc_create_notification("my_func", NULL);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "method");
    assert_non_null(json_test);
    assert_non_null(json_test->valuestring);
    assert_string_equal(json_test->valuestring, "my_func");

    json_test = cJSON_GetObjectItem(json, "params");
    assert_null(json_test);

    cJSON_Delete(json);

    json_test = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json_test, "var1", cJSON_CreateNull());
    cJSON_AddItemToObjectCS(json_test, "var2", cJSON_CreateNull());

    json = jsonrpc_create_notification("my_func", json_test);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "params");
    assert_non_null(json_test);
    assert_true(cJSON_IsObject(json_test));
    assert_true(cJSON_HasObjectItem(json_test, "var1"));
    assert_true(cJSON_HasObjectItem(json_test, "var2"));

    cJSON_Delete(json);
}

static void test_response_success(void** state)
{
    cJSON *id, *json, *json_test;

    unused(state);

    id = cJSON_CreateString("my id");

    json_test = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json_test, "var1", cJSON_CreateNull());
    cJSON_AddItemToObjectCS(json_test, "var2", cJSON_CreateNull());

    json = jsonrpc_create_response_success(NULL, NULL);
    assert_null(json);
    assert_int_equal(errno, EINVAL);

    json = jsonrpc_create_response_success(NULL, json_test);
    assert_null(json);
    assert_int_equal(errno, EINVAL);

    json = jsonrpc_create_response_success(id, json_test);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "id");
    assert_non_null(json_test);
    assert_non_null(json_test->valuestring);
    assert_string_equal(json_test->valuestring, "my id");

    json_test = cJSON_GetObjectItem(json, "result");
    assert_non_null(json_test);
    assert_true(cJSON_IsObject(json_test));
    assert_true(cJSON_HasObjectItem(json_test, "var1"));
    assert_true(cJSON_HasObjectItem(json_test, "var2"));

    assert_true(jsonrpc_is_response_success(json));
    assert_false(jsonrpc_is_response_error(json));

    /* Now test the response parsing. */
    int ret;
    const cJSON *rid, *rresult;

    rid = rresult = NULL;

    ret = jsonrpc_get_response_success(NULL, NULL, NULL);
    assert_int_equal(ret, EINVAL);

    ret = jsonrpc_get_response_success(json, NULL, NULL);
    assert_int_equal(ret, EINVAL);

    ret = jsonrpc_get_response_success(json, &rid, NULL);
    assert_int_equal(ret, 0);
    assert_non_null(rid);
    assert_string_equal(id->valuestring, rid->valuestring);
    assert_null(rresult);

    ret = jsonrpc_get_response_success(json, &rid, &rresult);
    assert_int_equal(ret, 0);
    assert_non_null(rid);
    assert_string_equal(id->valuestring, rid->valuestring);
    assert_non_null(rresult);
    assert_true(cJSON_IsObject(rresult));
    assert_true(cJSON_HasObjectItem(rresult, "var1"));
    assert_true(cJSON_HasObjectItem(rresult, "var2"));

    cJSON_Delete(json);
    cJSON_Delete(id);
}

static void test_response_error(void** state)
{
    cJSON *id, *json, *json_test, *json_error;
    int code = 12344321;

    unused(state);

    id = cJSON_CreateString("my id");

    json = jsonrpc_create_response_error(NULL, code, NULL, NULL);
    assert_null(json);
    assert_int_equal(errno, EINVAL);

    json = jsonrpc_create_response_error(id, code, NULL, NULL);
    assert_null(json);
    assert_int_equal(errno, EINVAL);

    json = jsonrpc_create_response_error(id, code, "my message", NULL);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "id");
    assert_non_null(json_test);
    assert_non_null(json_test->valuestring);
    assert_string_equal(json_test->valuestring, "my id");

    json_error = cJSON_GetObjectItem(json, "error");
    assert_non_null(json_error);
    assert_true(cJSON_IsObject(json_error));
    assert_true(cJSON_HasObjectItem(json_error, "code"));
    assert_true(cJSON_HasObjectItem(json_error, "message"));
    assert_false(cJSON_HasObjectItem(json_error, "data"));

    json_test = cJSON_GetObjectItem(json_error, "code");
    assert_non_null(json_test);
    assert_true(cJSON_IsNumber(json_test));
    assert_int_equal(json_test->valueint, 12344321);

    json_test = cJSON_GetObjectItem(json_error, "message");
    assert_non_null(json_test);
    assert_true(cJSON_IsString(json_test));
    assert_string_equal(json_test->valuestring, "my message");

    cJSON_Delete(json);

    json_test = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json_test, "var1", cJSON_CreateNull());
    cJSON_AddItemToObjectCS(json_test, "var2", cJSON_CreateNull());

    json = jsonrpc_create_response_error(id, code, "my message", json_test);
    assert_non_null(json);

    json_test = cJSON_GetObjectItem(json, "id");
    assert_non_null(json_test);
    assert_non_null(json_test->valuestring);
    assert_string_equal(json_test->valuestring, "my id");

    json_error = cJSON_GetObjectItem(json, "error");
    assert_non_null(json_error);
    assert_true(cJSON_IsObject(json_error));
    assert_true(cJSON_HasObjectItem(json_error, "code"));
    assert_true(cJSON_HasObjectItem(json_error, "message"));
    assert_true(cJSON_HasObjectItem(json_error, "data"));

    json_test = cJSON_GetObjectItem(json_error, "code");
    assert_non_null(json_test);
    assert_true(cJSON_IsNumber(json_test));
    assert_int_equal(json_test->valueint, 12344321);

    json_test = cJSON_GetObjectItem(json_error, "message");
    assert_non_null(json_test);
    assert_true(cJSON_IsString(json_test));
    assert_string_equal(json_test->valuestring, "my message");

    json_test = cJSON_GetObjectItem(json_error, "data");
    assert_non_null(json_test);
    assert_true(cJSON_IsObject(json_test));
    assert_true(cJSON_HasObjectItem(json_test, "var1"));
    assert_true(cJSON_HasObjectItem(json_test, "var2"));

    assert_true(jsonrpc_is_response_error(json));
    assert_false(jsonrpc_is_response_success(json));

    /* Now test the response parsing. */
    int ret, rcode;
    const cJSON *rid, *rdata;
    const char* rmsg;

    rid = rdata = NULL;
    rmsg = NULL;
    rcode = -1;

    ret = jsonrpc_get_response_error(NULL, NULL, NULL, NULL, NULL);
    assert_int_equal(ret, EINVAL);

    ret = jsonrpc_get_response_error(json, NULL, NULL, NULL, NULL);
    assert_int_equal(ret, EINVAL);

    ret = jsonrpc_get_response_error(json, &rid, NULL, NULL, NULL);
    assert_int_equal(ret, EINVAL);

    ret = jsonrpc_get_response_error(json, &rid, &rcode, NULL, NULL);
    assert_int_equal(ret, 0);
    assert_non_null(rid);
    assert_string_equal(id->valuestring, rid->valuestring);
    assert_int_equal(rcode, 12344321);
    assert_null(rmsg);
    assert_null(rdata);

    ret = jsonrpc_get_response_error(json, &rid, &rcode, &rmsg, NULL);
    assert_int_equal(ret, 0);
    assert_non_null(rid);
    assert_string_equal(id->valuestring, rid->valuestring);
    assert_int_equal(rcode, 12344321);
    assert_non_null(rmsg);
    assert_string_equal(rmsg, "my message");
    assert_null(rdata);

    ret = jsonrpc_get_response_error(json, &rid, &code, &rmsg, &rdata);
    assert_int_equal(ret, 0);
    assert_non_null(rid);
    assert_string_equal(id->valuestring, rid->valuestring);
    assert_int_equal(rcode, 12344321);
    assert_non_null(rmsg);
    assert_string_equal(rmsg, "my message");
    assert_non_null(rdata);
    assert_true(cJSON_IsObject(rdata));
    assert_true(cJSON_HasObjectItem(rdata, "var1"));
    assert_true(cJSON_HasObjectItem(rdata, "var2"));

    cJSON_Delete(json);
    cJSON_Delete(id);
}

#define TEST_EXECUTE_ID "im_numba_one"

static cJSON* test_execute_one_method_handler(const cJSON* id, const cJSON* params)
{
    cJSON* json;

    assert_non_null(id);
    assert_non_null(params);

    assert_string_equal(id->valuestring, TEST_EXECUTE_ID);

    json = cJSON_GetObjectItem(params, "var1");
    assert_non_null(json);
    assert_string_equal(json->valuestring, "blah1");

    json = cJSON_GetObjectItem(params, "var2");
    assert_non_null(json);
    assert_string_equal(json->valuestring, "blah2");

    return jsonrpc_create_response_success(id, cJSON_CreateNull());
}

static cJSON* test_execute_two_method_handler(const cJSON* id, const cJSON* params)
{
    cJSON* json;

    assert_null(id);

    json = cJSON_GetObjectItem(params, "var3");
    assert_non_null(json);
    assert_string_equal(json->valuestring, "blah3");

    json = cJSON_GetObjectItem(params, "var4");
    assert_non_null(json);
    assert_string_equal(json->valuestring, "blah4");

    return NULL;
}

static void test_execute(void** state)
{
    jsonrpc_t* jsonrpc = *state;
    cJSON *one, *two, *three, *id, *response;
    const cJSON *id_response, *result_response;
    bool ret;
    int result;

    ret = jsonrpc_register_method(jsonrpc, "one", test_execute_one_method_handler);
    assert_true(ret);

    ret = jsonrpc_register_method(jsonrpc, "two", test_execute_two_method_handler);
    assert_true(ret);

    id = cJSON_CreateString(TEST_EXECUTE_ID);

    one = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(one, "var1", cJSON_CreateString("blah1"));
    cJSON_AddItemToObjectCS(one, "var2", cJSON_CreateString("blah2"));

    two = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(two, "var3", cJSON_CreateString("blah3"));
    cJSON_AddItemToObjectCS(two, "var4", cJSON_CreateString("blah4"));

    three = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(three, "var5", cJSON_CreateString("blah7"));
    cJSON_AddItemToObjectCS(three, "var6", cJSON_CreateString("blah8"));

    one = jsonrpc_create_request(id, "one", one);
    two = jsonrpc_create_notification("two", two);
    three = jsonrpc_create_request(id, "three", three);

    result = jsonrpc_execute(jsonrpc, one, &response);
    assert_int_equal(result, 0);
    assert_non_null(response);
    assert_true(jsonrpc_is_response_success(response));

    jsonrpc_get_response_success(response, &id_response, &result_response);
    assert_non_null(id_response);
    assert_non_null(result_response);
    assert_string_equal(id_response->valuestring, TEST_EXECUTE_ID);
    assert_true(cJSON_IsNull(result_response));

    cJSON_Delete(response);

    result = jsonrpc_execute(jsonrpc, two, &response);
    assert_int_equal(result, 0);
    assert_null(response);

    result = jsonrpc_execute(jsonrpc, three, &response);
    assert_int_not_equal(result, 0);

    cJSON_Delete(id);
    cJSON_Delete(one);
    cJSON_Delete(two);
    cJSON_Delete(three);
}

int main(int argc, char* argv[])
{
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_register_method, test_setup, test_teardown),
            cmocka_unit_test(test_create_request),
            cmocka_unit_test(test_create_notification),
            cmocka_unit_test(test_response_success),
            cmocka_unit_test(test_response_error),
            cmocka_unit_test_setup_teardown(test_execute, test_setup, test_teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
