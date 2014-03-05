/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "security_evaluator.h"
#include "logger.h"

// Imports for SSL validation

#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

 #define DATE_LEN 128


#if HAVE_TLS

bool SecurityEvaluator::containsPrivateKey(std::string& pemPath) {

    FILE *keyFile = fopen(pemPath.c_str(), "r");
    RSA *rsa = PEM_read_RSAPrivateKey(keyFile, NULL, NULL, NULL);

    if (rsa == NULL) {
        DEBUG("Bad file, or not containing private key");
        return false;
    } else {
        DEBUG("Valid private key file");
        // Then we should hide fields "Private key file and private key password"
    }
    return true;
}

bool SecurityEvaluator::certificateIsValid(std::string& pemPath) {
    // First check local Certificate Authority file
    FILE *fileCheck = fopen(pemPath.c_str(), "r");
    X509* x509 = PEM_read_X509(fileCheck, nullptr, nullptr, nullptr);
    if (x509 != nullptr)
    {
        char* p = X509_NAME_oneline(X509_get_issuer_name(x509), 0, 0);
        if (p)
        {
            DEBUG("NAME: %s", p);
            OPENSSL_free(p);
        }
        ASN1_TIME *not_before = X509_get_notBefore(x509);
        ASN1_TIME *not_after = X509_get_notAfter(x509);

        char not_after_str[DATE_LEN];
        convertASN1TIME(not_after, not_after_str, DATE_LEN);

        char not_before_str[DATE_LEN];
        convertASN1TIME(not_before, not_before_str, DATE_LEN);

        DEBUG("not_before : %s", not_before_str);
        DEBUG("not_after : %s", not_after);

        // Here perform checks and send callbacks

        return true;
        X509_free(x509);
    } else {
        ERROR("Could not open certificate file");
        return false;
    }
}



void SecurityEvaluator::verifySSLCertificate(std::string& certificatePath, std::string& host, const std::string& port)
{
    BIO *sbio;
    SSL_CTX *ssl_ctx;
    SSL *ssl;
    X509 *server_cert;

    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();

    // Check OpenSSL PRNG
    if(RAND_status() != 1) {
        DEBUG("OpenSSL PRNG not seeded with enough data.");
        EVP_cleanup();
        ERR_free_strings();
        return;
    }

    ssl_ctx = SSL_CTX_new(TLSv1_client_method());

    // Enable certificate validation
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, nullptr);
    // Configure the CA trust store to be used
    if (SSL_CTX_load_verify_locations(ssl_ctx, certificatePath.c_str(), nullptr) != 1) {
        DEBUG("Couldn't load certificate trust store.");
        SSL_CTX_free(ssl_ctx);
        return;
    }

    // Only support secure cipher suites
    //if (SSL_CTX_set_cipher_list(ssl_ctx, SECURE_CIPHER_LIST) != 1)
    //    return;

    // Create the SSL connection
    sbio = BIO_new_ssl_connect(ssl_ctx);
    BIO_get_ssl(sbio, &ssl);
    if(!ssl) {
        DEBUG("Can't locate SSL pointer\n");
        BIO_free_all(sbio);
        return;
    }

	std::string hostWithPort = host + ":" + port;
	DEBUG("Checking certificate for %s", hostWithPort.c_str());

    BIO_set_conn_hostname(sbio, hostWithPort.c_str());
    if(SSL_do_handshake(ssl) <= 0) {
        // SSL Handshake failed
        long verify_err = SSL_get_verify_result(ssl);
        if (verify_err != X509_V_OK) {
            // It failed because the certificate chain validation failed
            DEBUG("Certificate chain validation failed: %s", X509_verify_cert_error_string(verify_err));
        }
        else {
            DEBUG("Boohoohoo, ssl handshake failed");
            // It failed for another reason
            ERR_print_errors_fp(stderr);
        }
        BIO_free_all(sbio);
        return;
    }

    // Recover the server's certificate
    server_cert =  SSL_get_peer_certificate(ssl);
    if (server_cert == nullptr) {
        // The handshake was successful although the server did not provide a certificate
        // Most likely using an insecure anonymous cipher suite... get out!
        BIO_ssl_shutdown(sbio);
        return;
    }


    DEBUG("Hostname validation...");
    // Validate the hostname
    if (validateHostname(host, server_cert) != MatchFound) {
        DEBUG("Hostname validation failed.");
        X509_free(server_cert);
        return;
    }
    DEBUG("Hostname validation passed!");
}

SecurityEvaluator::HostnameValidationResult SecurityEvaluator::validateHostname(std::string& hostname, const X509 *server_cert) {
    HostnameValidationResult result;

    if(hostname.c_str() == nullptr || (server_cert == nullptr)) {
        DEBUG("hostname.c_str() == nullptr || (server_cert == nullptr)");
        return Error;
    }

    // First try the Subject Alternative Names extension
    result = matchSubjectAltName(hostname, server_cert);
    if (result == NoSANPresent) {
        // Extension was not found: try the Common Name
        result = matchCommonName(hostname, server_cert);
    }

    return result;
}

SecurityEvaluator::HostnameValidationResult SecurityEvaluator::matchCommonName(std::string& hostname, const X509 *server_cert) {
    int common_name_loc = -1;
    X509_NAME_ENTRY *common_name_entry = nullptr;
    ASN1_STRING *common_name_asn1 = nullptr;
    char *common_name_str = nullptr;

    // Find the position of the CN field in the Subject field of the certificate
    common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name((X509 *) server_cert), NID_commonName, -1);
    if (common_name_loc < 0) {
        return Error;
    }

    // Extract the CN field
    common_name_entry = X509_NAME_get_entry(X509_get_subject_name((X509 *) server_cert), common_name_loc);
    if (common_name_entry == nullptr) {
        return Error;
    }

    // Convert the CN field to a C string
    common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
    if (common_name_asn1 == nullptr) {
        return Error;
    }
    common_name_str = (char *) ASN1_STRING_data(common_name_asn1);

    // Make sure there isn't an embedded NUL character in the CN
    if (ASN1_STRING_length(common_name_asn1) != strlen(common_name_str)) {
        return MalformedCertificate;
    }
    DEBUG("hostname = %s and extracted name is %s", hostname.c_str(), common_name_str);
    // Compare expected hostname with the CN
    if (strcasecmp(hostname.c_str(), common_name_str) == 0) {
        return MatchFound;
    }
    else {
        return MatchNotFound;
    }
}

SecurityEvaluator::HostnameValidationResult SecurityEvaluator::matchSubjectAltName(std::string& hostname, const X509 *server_cert) {
    HostnameValidationResult result = MatchNotFound;
    int i;
    int san_names_nb = -1;
    STACK_OF(GENERAL_NAME) *san_names = nullptr;

    // Try to extract the names within the SAN extension from the certificate
    san_names = static_cast<STACK_OF(GENERAL_NAME)*>(X509_get_ext_d2i((X509 *) server_cert, NID_subject_alt_name, nullptr, nullptr));
    if (san_names == nullptr) {
        return NoSANPresent;
    }
    san_names_nb = sk_GENERAL_NAME_num(san_names);

    // Check each name within the extension
    for (i=0; i<san_names_nb; i++) {
        const GENERAL_NAME *current_name = sk_GENERAL_NAME_value(san_names, i);

        if (current_name->type == GEN_DNS) {
            // Current name is a DNS name, let's check it
            char *dns_name = (char *) ASN1_STRING_data(current_name->d.dNSName);

            // Make sure there isn't an embedded NUL character in the DNS name
            if (ASN1_STRING_length(current_name->d.dNSName) != strlen(dns_name)) {
                result = MalformedCertificate;
                break;
            }
            else { // Compare expected hostname with the DNS name
                if (strcasecmp(hostname.c_str(), dns_name) == 0) {
                    result = MatchFound;
                    break;
                }
            }
        }
    }
    sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

    return result;
}

int SecurityEvaluator::convertASN1TIME(ASN1_TIME *t, char* buf, size_t len)
{
    int rc;
    BIO *b = BIO_new(BIO_s_mem());
    rc = ASN1_TIME_print(b, t);
    if (rc <= 0) {
        ERROR("fetchdaemon", "ASN1_TIME_print failed or wrote no data.");
        BIO_free(b);
        return EXIT_FAILURE;
    }
    rc = BIO_gets(b, buf, len);
    if (rc <= 0) {
        ERROR("fetchdaemon", "BIO_gets call failed to transfer contents to buf");
        BIO_free(b);
        return EXIT_FAILURE;
    }
    BIO_free(b);
    return EXIT_SUCCESS;
}

void SecurityEvaluator::printCertificate(X509* cert)
{
    char subj[1024];
    char issuer[1024];
    X509_NAME_oneline(X509_get_subject_name(cert), subj, 1024);
    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, 1024);
    DEBUG("Certificate: %s", subj);
    DEBUG("\tIssuer: %s", issuer);
}

#endif