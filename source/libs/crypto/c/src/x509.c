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

#include <icCrypto/x509.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <stdbool.h>
#include <icLog/logging.h>
#include <string.h>
#include "util.h"
#include "cryptoPrivate.h"
#include <icTypes/sbrm.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <errno.h>
#include <icTypes/icStringBuffer.h>

#define LOG_TAG "crypto/x509"

#define INDENT_SPACES 4

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define X509_get0_notBefore(c) X509_get_notBefore(c)
#define X509_get0_notAfter(c) X509_get_notAfter(c)
#define ASN1_STRING_get0_data(s) ASN1_STRING_data(s)
#endif

#define RFC2253_AVG_LENGTH 50

extern inline void x509GeneralNameDestroy__auto(X509GeneralName **name);
extern inline void x509CertDestroy__auto(X509Cert **cert);

static void x509DirNameDestroy(X509DirName *dn);

static char *x509NameToString(const X509_NAME *x509Name);

static char *asn1TimeToString(const ASN1_TIME *time)
{
    if (time == NULL)
    {
        return NULL;
    }

    BIO *mem = BIO_new(BIO_s_mem());

    ASN1_TIME_print(mem, time);

    char *out = getMemBIOString(mem);

    BIO_free(mem);

    return out;
}

static char *getX509NameByNID(const X509_NAME *x509Name, int nid)
{
    char *str = NULL;

    /* Older openSSL implementations discard the const qualifier but we want our interface to be explicit-const */
    X509_NAME *name = (X509_NAME *) x509Name;
    int cnIdx = X509_NAME_get_index_by_NID(name, nid, -1);

    if (cnIdx != -1)
    {
        X509_NAME_ENTRY *entry = X509_NAME_get_entry(name, cnIdx);
        ASN1_STRING *asn1CN = X509_NAME_ENTRY_get_data(entry);

        if (asn1CN != NULL)
        {
            const char *tmp = ASN1_STRING_get0_data(asn1CN);
            if (tmp != NULL)
            {
                str = strdup(tmp);
            }
        }
    }

    return str;
}

static void putStringInX509Name(X509GeneralName *name, const ASN1_STRING *certName)
{
    if (name == NULL || certName == NULL)
    {
        return;
    }

    BIO *bio = BIO_new(BIO_s_mem());
    ASN1_STRING_print(bio, certName);
    char *tmp = getMemBIOString(bio);
    memcpy((char *) &name->printableName, &tmp, sizeof(tmp));

    BIO_free(bio);
}

static X509DirName *createX509DirName(const X509_NAME *directoryName)
{
    if (directoryName == NULL)
    {
        return NULL;
    }

    X509DirName *dirName = calloc(1, sizeof(X509DirName));

    /*
     * Each of these memcpy operations is a type pun to assign const pointers.
     * I.e., the address stored in tmp is copied into dirName->x, as if
     * the expressions are dirName->x = getX509NameByNID(…)
     */

    char *tmp = getX509NameByNID(directoryName, NID_commonName);
    memcpy((void *) &dirName->commonName, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_countryName);
    memcpy((void *) &dirName->country, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_localityName);
    memcpy((void *) &dirName->locality, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_dnQualifier);
    memcpy((void *) &dirName->dnQualifier, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_organizationalUnitName);
    memcpy((void *) &dirName->orgUnit, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_serialNumber);
    memcpy((void *) &dirName->serialNumber, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_domainComponent);
    memcpy((void *) &dirName->domainComponent, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_stateOrProvinceName);
    memcpy((void *) &dirName->state, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_userId);
    memcpy((void *) &dirName->userId, &tmp, sizeof(tmp));

    tmp = getX509NameByNID(directoryName, NID_organizationName);
    memcpy((void *) &dirName->org, &tmp, sizeof(tmp));

    return dirName;
}

static X509GeneralName *createWrappedX509String(X509GeneralNameType type, const ASN1_STRING *printable)
{
    if (printable == NULL || type == CRYPTO_X509_NAME_INVALID)
    {
        return NULL;
    }

    X509GeneralName *x509Name = calloc(1, sizeof(X509GeneralName));
    memcpy((X509GeneralNameType *) &x509Name->type, &type, sizeof(x509Name->type));

    putStringInX509Name(x509Name, printable);

    return x509Name;
}

static X509GeneralName *createWrappedX509DirName(X509_NAME *name)
{
    X509GeneralName *x509Name = calloc(1, sizeof(X509GeneralName));
    X509GeneralNameType tmpType = CRYPTO_X509_NAME_DIRNAME;
    memcpy((X509GeneralNameType *) &x509Name->type, &tmpType, sizeof(x509Name->type));

    X509DirName *tmpDirName = createX509DirName(name);
    memcpy((X509DirName *) &x509Name->dirName, &tmpDirName, sizeof(x509Name->dirName));

    char *tmp = x509NameToString(name);
    memcpy((char *) &x509Name->printableName, &tmp, sizeof(tmp));

    return x509Name;
}

char *x509GetValidNotBefore(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    return asn1TimeToString(X509_get0_notBefore(x509->cert));
}

char *x509GetValidNotAfter(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    return asn1TimeToString(X509_get0_notAfter(x509->cert));
}

char *x509GetSubjectCN(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    X509_NAME *subject = X509_get_subject_name(x509->cert);

    return getX509NameByNID(subject, NID_commonName);
}

char *x509GetIssuerCN(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    X509_NAME *issuer = X509_get_issuer_name(x509->cert);

    return getX509NameByNID(issuer, NID_commonName);
}

static char *x509NameToString(const X509_NAME *x509Name)
{
    if (x509Name == NULL)
    {
        return NULL;
    }

    char *name = NULL;

    BIO *mem = BIO_new(BIO_s_mem());

    if (X509_NAME_print_ex(mem, (X509_NAME *) x509Name, INDENT_SPACES, XN_FLAG_RFC2253) != -1)
    {
        name = getMemBIOString(mem);
    }
    else
    {
        icLogWarn(LOG_TAG, "Unable to format X509 name");
    }

    BIO_free(mem);

    return name;
}

char *x509GetSubjectName(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    const X509_NAME *subject = X509_get_subject_name(x509->cert);

    return x509NameToString(subject);
}

char *x509GetIssuerName(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    const X509_NAME *issuer = X509_get_issuer_name(x509->cert);

    return x509NameToString(issuer);
}

char *x509GetPEM(const X509Cert *x509)
{
    if (x509 == NULL || x509->cert == NULL)
    {
        return NULL;
    }

    char *pemCert = NULL;
    BIO *mem = BIO_new(BIO_s_mem());

    if (PEM_write_bio_X509(mem, x509->cert) == true)
    {
        pemCert = getMemBIOString(mem);
    }
    else
    {
        icLogWarn(LOG_TAG, "Unable to write x509 to PEM");
    }

    BIO_free(mem);

    return pemCert;
}

X509Cert *x509CertLoadPEM(const char *path)
{
    X509Cert *cert = NULL;

    if (doesNonEmptyFileExist(path) == true)
    {
        cert = calloc(1, sizeof(X509Cert));
        AUTO_CLEAN(fclose__auto) FILE *certFile = fopen(path, "r");
        if (certFile != NULL)
        {
            cert->cert = PEM_read_X509(certFile, NULL, NULL, NULL);
        }
        else
        {
            AUTO_CLEAN(free_generic__auto) char *errstr = strerrorSafe(errno);
            icLogWarn(LOG_TAG, "Unable to open file at %s: %s", path, errstr);
        }

        if (certFile != NULL && cert->cert == NULL)
        {
            icLogError(LOG_TAG, "Unable to decode PEM cert file '%s'", path);
            x509CertDestroy(cert);
            cert = NULL;
        }
    }

    return cert;
}

void x509CertDestroy(X509Cert *cert)
{
    if (cert != NULL)
    {
        X509_free(cert->cert);
    }
    free(cert);
}

X509GeneralName *x509GetSubject(const X509Cert *x509)
{
    if (x509 == NULL)
    {
        return NULL;
    }

    return createWrappedX509DirName(X509_get_subject_name(x509->cert));
}

X509GeneralName *x509GetIssuer(const X509Cert *x509)
{
    if (x509 == NULL)
    {
        return NULL;
    }

    return createWrappedX509DirName(X509_get_issuer_name(x509->cert));
}

icLinkedList *x509GetSubjectAltNames(const X509Cert *x509)
{
    icLinkedList *sans = NULL;

    if (x509 == NULL)
    {
        return NULL;
    }

    if (X509_get_version(x509->cert) >= 2)
    {
        GENERAL_NAMES *names = X509_get_ext_d2i(x509->cert, NID_subject_alt_name, NULL, NULL);
        if (names != NULL)
        {
            sans = linkedListCreate();
            for (int i = 0; i < sk_GENERAL_NAME_num(names); i++)
            {
                const GENERAL_NAME *name = sk_GENERAL_NAME_value(names, i);
                X509GeneralName *x509Name = NULL;
                switch(name->type)
                {
                    case GEN_DIRNAME:
                        x509Name = createWrappedX509DirName(name->d.directoryName);
                        break;

                    case GEN_DNS:
                        x509Name = createWrappedX509String(CRYPTO_X509_NAME_DNS, name->d.dNSName);
                        break;

                    case GEN_URI:
                        x509Name = createWrappedX509String(CRYPTO_X509_NAME_URI, name->d.uniformResourceIdentifier);
                        break;

                    case GEN_EMAIL:
                        x509Name = createWrappedX509String(CRYPTO_X509_NAME_EMAIL, name->d.rfc822Name);
                        break;

                    case GEN_IPADD:
                        /* TODO possibly stuff into an in_addr */
                    default:
                    {
                        icLogWarn(LOG_TAG, "%s: general name type %d not supported", __func__, name->type);
                        x509Name = calloc(1, sizeof(X509GeneralName));
                        X509GeneralNameType tmpType = CRYPTO_X509_NAME_UNSUPPORTED;
                        memcpy((X509GeneralNameType *) &x509Name->type, &tmpType, sizeof(x509Name->type));

                        char *tmp = strdup("(not available)");
                        memcpy((char *) &x509Name->printableName, &tmp, sizeof(tmp));
                        break;
                    }
                }

                if (x509Name != NULL)
                {
                    linkedListAppend(sans, x509Name);
                }
            }
        }
        sk_GENERAL_NAME_pop_free(names, GENERAL_NAME_free);
    }

    return sans;
}

char *x509GeneralNamesFormat(const icLinkedList *names)
{
    if (names == NULL)
    {
        return NULL;
    }

    sbIcLinkedListIterator *it = linkedListIteratorCreate((icLinkedList *) names);
    uint32_t count = linkedListCount((icLinkedList * ) names);
    char *formatted = NULL;

    if (count > 0)
    {
        AUTO_CLEAN(stringBufferDestroy__auto)
            icStringBuffer *buf = stringBufferCreate(count * RFC2253_AVG_LENGTH);

        while(linkedListIteratorHasNext(it) == true)
        {
            const X509GeneralName *name = linkedListIteratorGetNext(it);
            AUTO_CLEAN(free_generic__auto)
                char *formattedName = x509GeneralNameFormat(name);

            stringBufferAppend(buf, formattedName);
            stringBufferAppend(buf, "\n");
        }

        formatted = stringBufferToString(buf);
    }

    return formatted;
}

char *x509GeneralNameFormat(const X509GeneralName *name)
{
    if (name == NULL)
    {
        return NULL;
    }

    return stringBuilder("%s:\t%s",
                         x509GeneralNameTypeToString(name->type),
                         stringCoalesceAlt(name->printableName, ""));
}

const char *x509GeneralNameTypeToString(const X509GeneralNameType type)
{
    const char *typeName = "invalid";

    switch(type)
    {

        case CRYPTO_X509_NAME_INVALID:
            break;

        case CRYPTO_X509_NAME_DIRNAME:
            typeName = "dirName";
            break;

        case CRYPTO_X509_NAME_EMAIL:
            typeName = "email";
            break;

        case CRYPTO_X509_NAME_DNS:
            typeName = "DNS";
            break;

        case CRYPTO_X509_NAME_URI:
            typeName = "URI";
            break;

        case CRYPTO_X509_NAME_UNSUPPORTED:
        default:
            typeName = "not supported";
            break;
    }

    return typeName;
}

void x509GeneralNameDestroy(X509GeneralName *name)
{
    if (name != NULL)
    {
        x509DirNameDestroy((X509DirName *) name->dirName);
        free((char *) name->printableName);
        free(name);
    }
}

static void x509DirNameDestroy(X509DirName *dn)
{
    if (dn != NULL)
    {
        free((char *) dn->commonName);
        free((char *) dn->country);
        free((char *) dn->dnQualifier);
        free((char *) dn->orgUnit);
        free((char *) dn->locality);
        free((char *) dn->org);
        free((char *) dn->serialNumber);
        free((char *) dn->state);
        free((char *) dn->userId);
        free((char *) dn->domainComponent);
        free(dn);
    }
}
