/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *          Vittorio Giovara <vittorio.giovara@savoirfairelinux.com>
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

#include <fstream>
#include <iterator>
#include <sstream>

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <cerrno>
#include <dirent.h>
#include <cassert>
#include <ctime>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/abstract.h>

#include "logger.h"
#include "tlsvalidator.h"

static unsigned char *crypto_cert_read(const std::string& path,const std::basic_string<unsigned char>& content, size_t *out_len, gnutls_x509_crt_t* cert_out);

const sfl::EnumClassNames<TlsValidator::CheckValues> TlsValidator::CheckValuesNames = {{
    /* CheckValues        Name     */
    /* PASSED      */ "PASSED"      ,
    /* FAILED      */ "FAILED"      ,
    /* UNSUPPORTED */ "UNSUPPORTED" ,
    /* ISO_DATE    */ "ISO_DATE"    ,
    /* CUSTOM      */ "CUSTOM"      ,
}};;

const sfl::CallbackMatrix1D<TlsValidator::CertificateCheck, TlsValidator, TlsValidator::CheckResult> TlsValidator::checkCallback = {{
    /*      CertificateCheck                       Callback                       */
    /*HAS_PRIVATE_KEY                */ &TlsValidator::hasPrivateKey               ,
    /*EXPIRED                        */ &TlsValidator::notExpired                  ,
    /*STRONG_SIGNING                 */ &TlsValidator::strongSigning               ,
    /*NOT_SELF_SIGNED                */ &TlsValidator::notSelfSigned               ,
    /*KEY_MATCH                      */ &TlsValidator::keyMatch                    ,
    /*PRIVATE_KEY_STORAGE_PERMISSION */ &TlsValidator::privateKeyStoragePermission ,
    /*PUBLIC_KEY_STORAGE_PERMISSION  */ &TlsValidator::publicKeyStoragePermission  ,
    /*PRIVATE_KEY_FOLDER_PERMISSION  */ &TlsValidator::privateKeyFolderPermission  ,
    /*PUBLIC_KEY_FOLDER_PERMISSION   */ &TlsValidator::publicKeyFolderPermission   ,
    /*PRIVATE_KEY_STORAGE_LOCATION   */ &TlsValidator::privateKeyStorageLocation   ,
    /*PUBLIC_KEY_STORAGE_LOCATION    */ &TlsValidator::publicKeyStorageLocation    ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES */ &TlsValidator::privateKeySelinuxAttributes ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES  */ &TlsValidator::publicKeySelinuxAttributes  ,
    /*REQUIRE_PRIVATE_KEY_PASSWORD   */ &TlsValidator::requirePrivateKeyPassword   ,
    /*OUTGOING_SERVER                */ &TlsValidator::outgoingServer              ,
    /*EXIST                          */ &TlsValidator::exist                       ,
    /*VALID                          */ &TlsValidator::valid                       ,
    /*VALID_AUTHORITY                */ &TlsValidator::validAuthority              ,
    /*KNOWN_AUTHORITY                */ &TlsValidator::knownAuthority              ,
    /*NOT_REVOKED                    */ &TlsValidator::notRevoked                  ,
    /*EXPIRATION_DATE                */ &TlsValidator::expirationDate              ,
    /*ACTIVATION_DATE                */ &TlsValidator::activationDate              ,
    /*AUTHORITY_MISMATCH             */ &TlsValidator::authorityMatch              ,
    /*UNEXPECTED_OWNER               */ &TlsValidator::expectedOwner               ,
    /*NOT_ACTIVATED                  */ &TlsValidator::activated                   ,
}};

const sfl::Matrix1D<TlsValidator::CertificateCheck, TlsValidator::CheckValuesType> TlsValidator::enforcedCheckType = {{
    /*      CertificateCheck                    Callback        */
    /*HAS_PRIVATE_KEY                */ CheckValuesType::BOOLEAN ,
    /*EXPIRED                        */ CheckValuesType::BOOLEAN ,
    /*STRONG_SIGNING                 */ CheckValuesType::BOOLEAN ,
    /*NOT_SELF_SIGNED                */ CheckValuesType::BOOLEAN ,
    /*KEY_MATCH                      */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_STORAGE_PERMISSION */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_STORAGE_PERMISSION  */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_FOLDER_PERMISSION  */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_FOLDER_PERMISSION   */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_STORAGE_LOCATION   */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_STORAGE_LOCATION    */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES  */ CheckValuesType::BOOLEAN ,
    /*REQUIRE_PRIVATE_KEY_PASSWORD   */ CheckValuesType::BOOLEAN ,
    /*OUTGOING_SERVER                */ CheckValuesType::CUSTOM  ,
    /*EXIST                          */ CheckValuesType::BOOLEAN ,
    /*VALID                          */ CheckValuesType::BOOLEAN ,
    /*VALID_AUTHORITY                */ CheckValuesType::BOOLEAN ,
    /*KNOWN_AUTHORITY                */ CheckValuesType::BOOLEAN ,
    /*NOT_REVOKED                    */ CheckValuesType::BOOLEAN ,
    /*EXPIRATION_DATE                */ CheckValuesType::ISO_DATE,
    /*ACTIVATION_DATE                */ CheckValuesType::ISO_DATE,
    /*AUTHORITY_MISMATCH             */ CheckValuesType::BOOLEAN ,
    /*UNEXPECTED_OWNER               */ CheckValuesType::BOOLEAN ,
    /*NOT_ACTIVATED                  */ CheckValuesType::BOOLEAN ,
}};

const sfl::EnumClassNames<TlsValidator::CertificateCheck> TlsValidator::CertificateCheckNames = {{
    /*      CertificateCheck                       Name                 */
    /*HAS_PRIVATE_KEY                */ "HAS_PRIVATE_KEY"                ,
    /*EXPIRED                        */ "EXPIRED"                        ,
    /*STRONG_SIGNING                 */ "STRONG_SIGNING"                 ,
    /*NOT_SELF_SIGNED                */ "NOT_SELF_SIGNED"                ,
    /*KEY_MATCH                      */ "KEY_MATCH"                      ,
    /*PRIVATE_KEY_STORAGE_PERMISSION */ "PRIVATE_KEY_STORAGE_PERMISSION" ,
    /*PUBLIC_KEY_STORAGE_PERMISSION  */ "PUBLIC_KEY_STORAGE_PERMISSION"  ,
    /*PRIVATE_KEY_FOLDER_PERMISSION  */ "PRIVATE_KEY_FOLDER_PERMISSION"  ,
    /*PUBLIC_KEY_FOLDER_PERMISSION   */ "PUBLIC_KEY_FOLDER_PERMISSION"   ,
    /*PRIVATE_KEY_STORAGE_LOCATION   */ "PRIVATE_KEY_STORAGE_LOCATION"   ,
    /*PUBLIC_KEY_STORAGE_LOCATION    */ "PUBLIC_KEY_STORAGE_LOCATION"    ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES */ "PRIVATE_KEY_SELINUX_ATTRIBUTES" ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES  */ "PUBLIC_KEY_SELINUX_ATTRIBUTES"  ,
    /*REQUIRE_PRIVATE_KEY_PASSWORD   */ "REQUIRE_PRIVATE_KEY_PASSWORD"   , // TODO move to certificateDetails()
    /*OUTGOING_SERVER                */ "OUTGOING_SERVER"                ,
    /*EXIST                          */ "EXIST"                          ,
    /*VALID                          */ "VALID"                          ,
    /*VALID_AUTHORITY                */ "VALID_AUTHORITY"                ,
    /*KNOWN_AUTHORITY                */ "KNOWN_AUTHORITY"                ,
    /*NOT_REVOKED                    */ "NOT_REVOKED"                    ,
    /*EXPIRATION_DATE                */ "EXPIRATION_DATE"                , // TODO move to certificateDetails()
    /*ACTIVATION_DATE                */ "ACTIVATION_DATE"                , // TODO move to certificateDetails()
    /*AUTHORITY_MISMATCH             */ "AUTHORITY_MISMATCH"             ,
    /*UNEXPECTED_OWNER               */ "UNEXPECTED_OWNER"               ,
    /*NOT_ACTIVATED                  */ "NOT_ACTIVATED"                  ,
}};

const sfl::EnumClassNames<const TlsValidator::CheckValuesType> TlsValidator::CheckValuesTypeNames = {{
    /*   Type        Name    */
    /* BOOLEAN  */ "BOOLEAN"  ,
    /* ISO_DATE */ "ISO_DATE" ,
    /* CUSTOM   */ "CUSTOM"   ,
}};

const sfl::Matrix2D<TlsValidator::CheckValuesType , TlsValidator::CheckValues , bool> TlsValidator::acceptedCheckValuesResult = {{
    /*   Type          PASSED    FAILED   UNSUPPORTED   ISO_DATE    CUSTOM  */
    /* BOOLEAN  */  {{  true   ,  true  ,    true     ,  false    ,  false  }},
    /* ISO_DATE */  {{  false  ,  false ,    true     ,  true     ,  false  }},
    /* CUSTOM   */  {{  false  ,  false ,    true     ,  false    ,  true   }},
}};


TlsValidator::TlsValidator(const std::string& certificate, const std::string& privatekey) :
certificatePath_(certificate), privateKeyPath_(privatekey), caCert_(nullptr), caChecked_(false)
{
    int err = gnutls_global_init();
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init GnuTLS - %s", gnutls_strerror(err));
    }

    std::fstream certF(certificatePath_);
    certificateFound_ = certF.is_open();
    if (certificateFound_) {
        certificateContent_ = std::basic_string<unsigned char>(std::istreambuf_iterator<char>(certF), std::istreambuf_iterator<char>());
    }
    certF.close();

    std::fstream pkF(privateKeyPath_);
    privateKeyFound_ = pkF.is_open();
    if (privateKeyFound_) {
        privateKeyContent_ = std::basic_string<unsigned char>(std::istreambuf_iterator<char>(certF), std::istreambuf_iterator<char>());
    }
    pkF.close();

    size_t len = 0;

    // All users of 'x509Certificate' need to return UNSUPPORTED certificateFound
    // is false. This is necessary because the callbacks can be called directly
    // It is irrelevant for the serializeAll() method as it does the check
    // before calling the callbacks methods.
    if (certificateFound_)
        crypto_cert_read(certificatePath_,certificateContent_,&len,&x509Certificate_);
}

TlsValidator::~TlsValidator()
{
    // TODO once the private key is used more extensively
    // the current implementation never load/need the x509PK
    // gnutls_x509_privkey_deinit(key);

    if (certificateFound_) {
        gnutls_x509_crt_deinit(x509Certificate_);
    }

    // TODO figure out when to call this. Static destructor?
    // gnutls_global_deinit();
}

/**
 * This method convert results into validated strings
 *
 * @todo The date should be validated, this is currently not an issue
 */
std::string TlsValidator::getStringValue(const TlsValidator::CertificateCheck check, const TlsValidator::CheckResult result)
{
    assert(acceptedCheckValuesResult[enforcedCheckType[check]][result.first]);

    switch(result.first) {
        case CheckValues::PASSED:
        case CheckValues::FAILED:
        case CheckValues::UNSUPPORTED:
            return CheckValuesNames[result.first];
            break;
        case CheckValues::ISO_DATE:
            // TODO validate date
            // return CheckValues::FAILED;
            return result.second;
        case CheckValues::CUSTOM:
            return result.second;
    };

    // Consider any other case (such as forced int->CheckValues casting) as failed
    return CheckValuesNames[CheckValues::FAILED];
}

/**
 * Check if all boolean check passed
 * return true if there was no ::FAILED checks
 *
 * Checks functions are not "const", so this function isn't
 */
bool TlsValidator::isValid(bool verbose)
{
    for (const CertificateCheck check : sfl::Matrix0D<CertificateCheck>()) {
        if (enforcedCheckType[check] == CheckValuesType::BOOLEAN) {
            if ((this->*(checkCallback[check]))().first == CheckValues::FAILED) {
                if (verbose)
                    WARN("Check failed: %s",CertificateCheckNames[check]);
                return false;
            }
        }
    }
    return true;
}

/**
 * Convert all checks results into a string map
 */
std::map<std::string,std::string> TlsValidator::serializeAll()
{
    std::map<std::string,std::string> ret;
    if (not certificateFound_) {
        // Instead of checking `certificateFound` everywhere, handle it once
        ret[CertificateCheckNames[CertificateCheck::EXIST]]
            = getStringValue(CertificateCheck::EXIST,exist());
    }
    else {
//         ret[CertificateCheckNames[CertificateCheck::HAS_PRIVATE_KEY]] = getStringValue(CertificateCheck::HAS_PRIVATE_KEY,embedPrivateKey());
        for ( const CertificateCheck check : sfl::Matrix0D<CertificateCheck>() )
            ret[CertificateCheckNames[check]] = getStringValue(check,(this->*(checkCallback[check]))());
    }

    return ret;
}

/**
 * Load the content of a certificate file and return the data pointer to it.
 */
static unsigned char *crypto_cert_read(const std::string& path,const std::basic_string<unsigned char>& content, size_t *out_len, gnutls_x509_crt_t* cert_out)
{
    unsigned char *data = nullptr;
    gnutls_datum_t dt;
    size_t fsize = 0;
    int err;

    dt.data = (unsigned char*)(content.c_str());
    if (!dt.data)
        return nullptr;

    dt.size = content.size();
    if (gnutls_x509_crt_init(cert_out) != GNUTLS_E_SUCCESS) {
        ERROR("Not enough memory for certificate.");
        goto out;
    }

    err = gnutls_x509_crt_import(*cert_out, &dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(*cert_out, &dt, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not import certificate %s - %s", path.c_str(), gnutls_strerror(err));
        goto out;
    }

    *out_len = 10000;
    data = (unsigned char *)malloc(*out_len);
    if (!data) {
        *out_len = 0;
        goto out;
    }
    err = gnutls_x509_crt_export(*cert_out, GNUTLS_X509_FMT_DER, data, out_len);
    if (err != GNUTLS_E_SUCCESS) {
        free(data);
        data = nullptr;
        *out_len = 0;
        ERROR("Certificate %s could not be exported - %s.\n",
              path.c_str(), gnutls_strerror(err));
    }

out:
    return data;
}

/**
 * Load all root CAs present in the system.
 * Normally we should use gnutls_certificate_set_x509_system_trust(), but it requires
 * GnuTLS 3.0 or later. As a workaround we iterate on the system trusted store folder
 * and load every certificate available there.
 *
 * @fixme Port to GnuTLS 3.0 as it is now the minimum required version
 */
static int crypto_cert_load_trusted(gnutls_certificate_credentials_t cred)
{
    DIR *trust_store;
    struct dirent *trust_ca;
    struct stat statbuf;
    int err, res = -1;
    char ca_file[512];

    trust_store = opendir("/etc/ssl/certs/");

    if (!trust_store) {
        ERROR("Failed to open system trusted store.");
        goto out;
    }
    while ((trust_ca = readdir(trust_store)) != nullptr) {
        /* Prepare the string and check it is a regular file. */
        err = snprintf(ca_file, sizeof(ca_file), "/etc/ssl/certs/%s", trust_ca->d_name);
        if (err < 0) {
            ERROR("snprintf() error");
            goto out;
        } else if (err >= sizeof(ca_file)) {
            ERROR("File name too long '%s'.", trust_ca->d_name);
            goto out;
        }
        err = stat(ca_file, &statbuf);
        if (err < 0) {
            ERROR("Failed to stat file '%s'.", ca_file);
            goto out;
        }
        if (!S_ISREG(statbuf.st_mode))
            continue;

        /* Load the root CA. */
        err = gnutls_certificate_set_x509_trust_file(cred, ca_file, GNUTLS_X509_FMT_PEM);
        if (err == 0) {
            WARN("No trusted certificates found - %s", gnutls_strerror(err));
        } else if (err < 0) {
            ERROR("Could not load trusted certificates - %s", gnutls_strerror(err));
            goto out;
        }
    }

    res = 0;
out:
    closedir(trust_store);
    return res;
}

/**
 * Print the Subject, the Issuer and the Verification status of a given certificate.
 *
 * @todo Move to "certificateDetails()" once completed
 */
static int crypto_cert_print_issuer(gnutls_x509_crt_t cert,
                                    gnutls_x509_crt_t issuer)
{
    char name[512];
    char issuer_name[512];
    size_t name_size;
    size_t issuer_name_size;

    issuer_name_size = sizeof(issuer_name);
    gnutls_x509_crt_get_issuer_dn(cert, issuer_name,
                                  &issuer_name_size);

    name_size = sizeof(name);
    gnutls_x509_crt_get_dn(cert, name, &name_size);

    DEBUG("Subject: %s", name);
    DEBUG("Issuer: %s", issuer_name);

    if (issuer != nullptr) {
        issuer_name_size = sizeof(issuer_name);
        gnutls_x509_crt_get_dn(issuer, issuer_name, &issuer_name_size);

        DEBUG("Verified against: %s", issuer_name);
    }

    return 0;
}

/**
 * Deprecated, kept to keep the patch small
 */
int TlsValidator::containsPrivateKey(const std::string& pemPath)
{
    // TODO remove the old API
}

/**
 * Deprecated, kept to keep the patch small
 */
int TlsValidator::certificateIsValid(const std::string& caPath, const std::string& certPath)
{
    // TODO remove the old API
}

/**
 * Check if a certificate has been signed with the authority
 */
unsigned int TlsValidator::compareToCa()
{
    // Those check can only be applied when a valid CA is present
    if (certificateFound_ or (not caCert_) or caCert_->valid().first == CheckValues::FAILED)
        return GNUTLS_CERT_SIGNER_NOT_FOUND;

    // Don't check unless the certificate changed
    if (caChecked_)
        return caValidationOutput_;

    const int err = gnutls_x509_crt_verify(x509Certificate_, &caCert_->x509Certificate_, 1, 0, &caValidationOutput_);

    if (err)
        return GNUTLS_CERT_SIGNER_NOT_FOUND;

    return caValidationOutput_;
}

/**
 * Verify if a hostname is valid
 *
 * @warning This function is blocking
 *
 * Mainly based on Fedora Defensive Coding tutorial
 * https://docs.fedoraproject.org/en-US/Fedora_Security_Team/html/Defensive_Coding/sect-Defensive_Coding-TLS-Client-GNUTLS.html
 */
int TlsValidator::verifyHostnameCertificate(const std::string& host, const uint16_t port)
{
    int err, arg, res = -1;
    unsigned int status = (unsigned) -1;
    const char *errptr = nullptr;
    gnutls_session_t session = nullptr;
    gnutls_certificate_credentials_t cred = nullptr;
    unsigned int certslen = 0;
    const gnutls_datum_t *certs = nullptr;
    gnutls_x509_crt_t cert = nullptr;

    char buf[4096];
    int sockfd;
    struct sockaddr_in name;
    struct hostent *hostinfo;
    const int one = 1;
    fd_set fdset;
    struct timeval tv;

    if (!host.size() || !port) {
        ERROR("Wrong parameters used - host %s, port %d.", host.c_str(), port);
        return res;
    }

    /* Create the socket. */
    sockfd = socket (PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ERROR("Could not create socket.");
        return res;
    }
    /* Set non-blocking so we can dected timeouts. */
    arg = fcntl(sockfd, F_GETFL, nullptr);
    if (arg < 0)
        goto out;
    arg |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, arg) < 0)
        goto out;

    /* Give the socket a name. */
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(port);
    hostinfo = gethostbyname(host.c_str());
    if (hostinfo == nullptr) {
        ERROR("Unknown host %s.", host.c_str());
        goto out;
    }
    name.sin_addr = *(struct in_addr *)hostinfo->h_addr;
    /* Connect to the address specified in name struct. */
    err = connect(sockfd, (struct sockaddr *)&name, sizeof(name));
    if (err < 0) {
        /* Connection in progress, use select to see if timeout is reached. */
        if (errno == EINPROGRESS) {
            do {
                FD_ZERO(&fdset);
                FD_SET(sockfd, &fdset);
                tv.tv_sec = 10;     // 10 second timeout
                tv.tv_usec = 0;
                err = select(sockfd + 1, nullptr, &fdset, nullptr, &tv);
                if (err < 0 && errno != EINTR) {
                    ERROR("Could not connect to hostname %s at port %d",
                          host.c_str(), port);
                    goto out;
                } else if (err > 0) {
                    /* Select returned, if so_error is clean we are ready. */
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                    if (so_error) {
                        ERROR("Connection delayed.");
                        goto out;
                    }
                    break;  // exit do-while loop
                } else {
                    ERROR("Connection timeout.");
                    goto out;
                }
            } while(1);
        } else {
            ERROR("Could not connect to hostname %s at port %d", host.c_str(), port);
            goto out;
        }
    }
    /* Set the socked blocking again. */
    arg = fcntl(sockfd, F_GETFL, nullptr);
    if (arg < 0)
        goto out;
    arg &= ~O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, arg) < 0)
        goto out;

    /* Disable Nagle algorithm that slows down the SSL handshake. */
    err = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (err < 0) {
        ERROR("Could not set TCP_NODELAY.");
        goto out;
    }


    /* Load the trusted CA certificates. */
    err = gnutls_certificate_allocate_credentials(&cred);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not allocate credentials - %s", gnutls_strerror(err));
        goto out;
    }
    err = crypto_cert_load_trusted(cred);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not load credentials.");
        goto out;
    }

    /* Create the session object. */
    err = gnutls_init(&session, GNUTLS_CLIENT);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init session -%s\n", gnutls_strerror(err));
        goto out;
    }

    /* Configure the cipher preferences. The default set should be good enough. */
    err = gnutls_priority_set_direct(session, "NORMAL", &errptr);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not set up ciphers - %s (%s)", gnutls_strerror(err), errptr);
        goto out;
    }

    /* Install the trusted certificates. */
    err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cred);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not set up credentials - %s", gnutls_strerror(err));
        goto out;
    }

    /* Associate the socket with the session object and set the server name. */
    gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t) (uintptr_t) sockfd);
    err = gnutls_server_name_set(session, GNUTLS_NAME_DNS, host.c_str(), host.size());
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not set server name - %s", gnutls_strerror(err));
        goto out;
    }

    /* Establish the connection. */
    err = gnutls_handshake(session);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Handshake failed - %s", gnutls_strerror(err));
        goto out;
    }
    /* Obtain the server certificate chain. The server certificate
     * itself is stored in the first element of the array. */
    certs = gnutls_certificate_get_peers(session, &certslen);
    if (certs == nullptr || certslen == 0) {
        ERROR("Could not obtain peer certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* Validate the certificate chain. */
    err = gnutls_certificate_verify_peers2(session, &status);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not verify the certificate chain - %s", gnutls_strerror(err));
        goto out;
    }
    if (status != 0) {
        gnutls_datum_t msg;
#if GNUTLS_VERSION_AT_LEAST_3_1_4
        int type = gnutls_certificate_type_get(session);
        err = gnutls_certificate_verification_status_print(status, type, &out, 0);
#else
        err = -1;
#endif
        if (err == 0) {
            ERROR("Certificate validation failed - %s\n", msg.data);
            gnutls_free(msg.data);
            goto out;
        } else {
            ERROR("Certificate validation failed with code 0x%x.", status);
            goto out;
        }
    }

    /* Match the peer certificate against the hostname.
     * We can only obtain a set of DER-encoded certificates from the
     * session object, so we have to re-parse the peer certificate into
     * a certificate object. */

    err = gnutls_x509_crt_init(&cert);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* The peer certificate is the first certificate in the list. */
    err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read peer certificate - %s", gnutls_strerror(err));
        goto out;
    }
    /* Finally check if the hostnames match. */
    err = gnutls_x509_crt_check_hostname(cert, host.c_str());
    if (err == 0) {
        ERROR("Hostname %s does not match certificate.", host.c_str());
        goto out;
    }

    /* Try sending and receiving some data through. */
    snprintf(buf, sizeof(buf), "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host.c_str());
    err = gnutls_record_send(session, buf, strlen(buf));
    if (err < 0) {
        ERROR("Send failed - %s", gnutls_strerror(err));
        goto out;
    }
    err = gnutls_record_recv(session, buf, sizeof(buf));
    if (err < 0) {
        ERROR("Recv failed - %s", gnutls_strerror(err));
        goto out;
    }

    DEBUG("Hostname %s seems to point to a valid server.", host.c_str());
    res = 0;
out:
    if (session) {
        gnutls_bye(session, GNUTLS_SHUT_RDWR);
        gnutls_deinit(session);
    }
    if (cert)
        gnutls_x509_crt_deinit(cert);
    if (cred)
        gnutls_certificate_free_credentials(cred);
    close(sockfd);
    return res;
}

/**
 * Check if the Validator have access to a private key
 */
TlsValidator::CheckResult TlsValidator::hasPrivateKey()
{
    if (privateKeyFound_)
        return TlsValidator::CheckResult(CheckValues::PASSED,"");
    gnutls_datum_t dt;
    gnutls_x509_privkey_t key;
    size_t bufsize;
    int err, res = -1;

    dt.data = const_cast<unsigned char*>(certificateContent_.c_str());
    if (!dt.data)
        return CheckResult(CheckValues::FAILED,"");
    dt.size = certificateContent_.size();



    err = gnutls_x509_privkey_init(&key);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not init key - %s", gnutls_strerror(err));
        return CheckResult(CheckValues::FAILED,"");
    }

    err = gnutls_x509_privkey_import(key, &dt, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_privkey_import(key, &dt, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        ERROR("Could not read key - %s from %s", gnutls_strerror(err),dt.data);
        return CheckResult(CheckValues::FAILED,std::string("Could not read key - ") +  gnutls_strerror(err));
    }

    res = 0;
    DEBUG("Key from %s seems valid.", certificatePath_.c_str());
    return CheckResult(CheckValues::PASSED,"");
}

/**
 * Check if the certificate is not expired
 *
 * The double negative is used because all boolean checks need to have
 * a consistent return value semantic
 *
 * @fixme Handle both "with ca" and "without ca" case
 */
TlsValidator::CheckResult TlsValidator::notExpired()
{
    // time_t expirationTime = gnutls_x509_crt_get_expiration_time(cert);
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_EXPIRED
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * If the activation value is in the past
 *
 * @fixme Handle both "with ca" and "without ca" case
 */
TlsValidator::CheckResult TlsValidator::activated()
{
    // time_t activationTime = gnutls_x509_crt_get_activation_time(cert);
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_NOT_ACTIVATED 
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * If the algorithm used to sign the certificate is considered weak by modern
 * standard
 */
TlsValidator::CheckResult TlsValidator::strongSigning()
{
    // Doesn't seem to have the same value as
    // certtool  --infile /home/etudiant/Téléchargements/mynsauser.pem --key-inf
    // TODO figure out why
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_INSECURE_ALGORITHM
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * The certificate is not self signed
 */
TlsValidator::CheckResult TlsValidator::notSelfSigned()
{
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

/**
 * The provided key can be used along with the certificate
 */
TlsValidator::CheckResult TlsValidator::keyMatch()
{
    // TODO encrypt and decrypt a small string to check
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

TlsValidator::CheckResult TlsValidator::privateKeyStoragePermission()
{
    struct stat statbuf;
    int err = stat(privateKeyPath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");

    return TlsValidator::CheckResult(
        (statbuf.st_mode & S_IFREG) && /* Regular file only */
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode ^ S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

TlsValidator::CheckResult TlsValidator::publicKeyStoragePermission()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");

    return TlsValidator::CheckResult(
        (statbuf.st_mode & S_IFREG) && /* Regular file only */
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode ^ S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

TlsValidator::CheckResult TlsValidator::privateKeyFolderPermission()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");

    return TlsValidator::CheckResult(
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode & S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

TlsValidator::CheckResult TlsValidator::publicKeyFolderPermission()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");

    return TlsValidator::CheckResult(
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode & S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * Certificate should be located in specific path on some operating systems
 */
TlsValidator::CheckResult TlsValidator::privateKeyStorageLocation()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

/**
 * Certificate should be located in specific path on some operating systems
 */
TlsValidator::CheckResult TlsValidator::publicKeyStorageLocation()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

/**
 * SELinux provide additional key protection mechanism
 */
TlsValidator::CheckResult TlsValidator::privateKeySelinuxAttributes()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

/**
 * SELinux provide additional key protection mechanism
 */
TlsValidator::CheckResult TlsValidator::publicKeySelinuxAttributes()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

/**
 * If the key need decryption
 *
 * Double factor authentication is recommended
 */
TlsValidator::CheckResult TlsValidator::requirePrivateKeyPassword()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");
}

/**
 * The expected outgoing server domain
 *
 * @todo Move to "certificateDetails()" method once completed
 * @todo extract information for the certificate
 */
TlsValidator::CheckResult TlsValidator::outgoingServer()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::CUSTOM,"");
}

/**
 * The CA and certificate provide conflicting ownership information
 */
TlsValidator::CheckResult TlsValidator::expectedOwner()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_UNEXPECTED_OWNER?CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * The file has been found
 */
TlsValidator::CheckResult TlsValidator::exist()
{
    return TlsValidator::CheckResult(certificateFound_?CheckValues::PASSED:CheckValues::FAILED,"");
}

/**
 * The certificate is invalid compared to the authority
 *
 * @todo Handle case when there is facultative authority, such as DHT
 */
TlsValidator::CheckResult TlsValidator::valid()
{
    // TODO this is wrong
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_INVALID?CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * The provided authority is invalid
 */
TlsValidator::CheckResult TlsValidator::validAuthority()
{
    // TODO Merge with either above or bellow
    return TlsValidator::CheckResult((!caCert_) || (compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND)?CheckValues::FAILED:CheckValues::PASSED,"");
    //                                   ^--- When no authority is present, then it is not invalid, it is not there at all
}

/**
 * Check if the authority match the certificate
 */
TlsValidator::CheckResult TlsValidator::authorityMatch()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_CA?CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * When an account require an authority known by the system (like /usr/share/ssl/certs)
 * then the whole chain of trust need be to checked
 *
 * @fixme port crypto_cert_load_trusted
 * @fixme add account settings
 * @todo implement the check
 */
TlsValidator::CheckResult TlsValidator::knownAuthority()
{
    // TODO SFLphone need a new boolean account setting "require trusted authority" or something defaulting to true
    // using GNUTLS_CERT_SIGNER_NOT_FOUND is a temporary placeholder as it is close enough
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND?CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * Check if the certificate has been revoked
 */
TlsValidator::CheckResult TlsValidator::notRevoked()
{
    return TlsValidator::CheckResult((compareToCa() & GNUTLS_CERT_REVOKED) || (compareToCa() & GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE)?CheckValues::FAILED:CheckValues::PASSED,"");
}

/**
 * Get the expiration date
 *
 * @todo Move to "certificateDetails()" method once completed
 */
TlsValidator::CheckResult TlsValidator::expirationDate()
{
    if (not certificateFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");

    time_t expiration = gnutls_x509_crt_get_expiration_time(x509Certificate_);
    char buffer [12];
    struct tm* timeinfo = localtime(&expiration);
    strftime (buffer,sizeof(buffer),"%F\0",timeinfo);
    return TlsValidator::CheckResult(CheckValues::ISO_DATE,buffer);
}

/**
 * Get the activation date
 *
 * @todo Move to "certificateDetails()" method once completed
 */
TlsValidator::CheckResult TlsValidator::activationDate()
{
    if (not certificateFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED,"");

    time_t expiration = gnutls_x509_crt_get_activation_time(x509Certificate_);
    char buffer [12];
    struct tm* timeinfo = localtime(&expiration);
    strftime (buffer,sizeof(buffer),"%F\0",timeinfo);
    return TlsValidator::CheckResult(CheckValues::ISO_DATE,buffer);
}

/**
 * A certificate authority has been provided
 */
bool TlsValidator::hasCa() const
{
    return caCert_ != nullptr and caCert_->certificateFound_;
}

/**
 * Set an authority
 */
void TlsValidator::setCaTlsValidator(const TlsValidator& validator)
{
    caChecked_ = false;
    caCert_ = (TlsValidator*)(&validator);
}
