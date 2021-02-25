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

#include <setjmp.h>
#include <unistd.h>
#include <stdarg.h>
#include <cmocka.h>
#include <string.h>

#include <icCrypto/x509.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>

#ifndef RES_DIR
#error "no RES_DIR defined"
#endif

#define TEST_CERT RES_DIR "/sanCert.pem"

static void test_x509SubjectAltNames(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    icLinkedList *names = x509GetSubjectAltNames(cert);

    sbIcLinkedListIterator *it = linkedListIteratorCreate(names);
    bool foundDN = false;
    while(linkedListIteratorHasNext(it))
    {
        X509GeneralName *name = linkedListIteratorGetNext(it);
        if (name->type == CRYPTO_X509_NAME_DIRNAME)
        {
            assert_non_null(name->dirName);
            assert_non_null(name->dirName->org);
            assert_string_equal(name->dirName->org, "Comcast");

            assert_non_null(name->dirName->orgUnit);
            assert_string_equal(name->dirName->orgUnit, "Xfinity Back Office");

            assert_non_null(name->dirName->commonName);
            assert_string_equal(name->dirName->commonName, "142587");
            foundDN = true;
            break;
        }
    }

    assert_true(foundDN);

    linkedListDestroy(names, (linkedListItemFreeFunc) x509GeneralNameDestroy);
}

static void test_x509SubjectName(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    AUTO_CLEAN(x509GeneralNameDestroy__auto)
        X509GeneralName *subjName = x509GetSubject(cert);
    assert_int_equal(subjName->type, CRYPTO_X509_NAME_DIRNAME);

    const X509DirName *subject = subjName->dirName;

    assert_non_null(subject);
    assert_non_null(subject->commonName);
    assert_string_equal(subject->commonName, "nodeAddressTest.xfinityhome.com");

    assert_non_null(subject->org);
    assert_string_equal(subject->org, "Comcast");

    assert_non_null(subject->orgUnit);
    assert_string_equal(subject->orgUnit, "1E9VSCiCbutgExdd9kBAg76BaR2BGfXWMr");

    assert_non_null(subject->country);
    assert_string_equal(subject->country, "US");

    assert_non_null(subject->userId);
    assert_string_equal(subject->userId, "test-system-ID");

    assert_non_null(subject->state);
    assert_string_equal(subject->state, "PA");

    assert_non_null(subject->locality);
    assert_string_equal(subject->locality, "Philadelphia");
}

static void test_x509IssuerName(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    AUTO_CLEAN(x509GeneralNameDestroy__auto)
        X509GeneralName *issName = x509GetIssuer(cert);
    assert_int_equal(issName->type, CRYPTO_X509_NAME_DIRNAME);

    const X509DirName *issuer = issName->dirName;

    assert_non_null(issuer);
    assert_non_null(issuer->commonName);
    assert_string_equal(issuer->commonName, "STAGE ONLY - Xfinity Subscriber Issuing RSA ICA");
}

static void test_x509FormatSANs(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);

    assert_non_null(cert);

    icLinkedList *sans = x509GetSubjectAltNames(cert);
    assert_non_null(sans);

    AUTO_CLEAN(free_generic__auto) char *formattedSans = x509GeneralNamesFormat(sans);

    linkedListDestroy(sans, (linkedListItemFreeFunc) x509GeneralNameDestroy);

    printf("SANs:\n%s\n", formattedSans);
}

static void test_x509GetPEM(void **state)
{
    AUTO_CLEAN(x509CertDestroy__auto) X509Cert *cert = x509CertLoadPEM(TEST_CERT);
    AUTO_CLEAN(free_generic__auto) char *testCertPEM = readFileContents(TEST_CERT);

    AUTO_CLEAN(free_generic__auto) char *reEncoded = x509GetPEM(cert);
    assert_string_equal(reEncoded, testCertPEM);
}

int main(int argc, char *argv[])
{
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_x509SubjectAltNames),
                    cmocka_unit_test(test_x509SubjectName),
                    cmocka_unit_test(test_x509IssuerName),
                    cmocka_unit_test(test_x509FormatSANs),
                    cmocka_unit_test(test_x509GetPEM)
            };

    int rc = cmocka_run_group_tests(tests, NULL, NULL);

    return rc;
}
