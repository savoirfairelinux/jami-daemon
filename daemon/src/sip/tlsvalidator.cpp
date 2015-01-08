/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *          Vittorio Giovara <vittorio.giovara@savoirfairelinux.com>
 *          Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
 *          Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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

#include "tlsvalidator.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "fileutils.h"
#include "logger.h"

#include <sstream>
#include <iomanip>

#include <cstdio>
#include <cerrno>
#include <cassert>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

const ring::EnumClassNames<TlsValidator::CheckValues> TlsValidator::CheckValuesNames = {{
    /* CheckValues        Name     */
    /* PASSED      */ "PASSED"      ,
    /* FAILED      */ "FAILED"      ,
    /* UNSUPPORTED */ "UNSUPPORTED" ,
    /* ISO_DATE    */ "ISO_DATE"    ,
    /* CUSTOM      */ "CUSTOM"      ,
    /* CUSTOM      */ "DATE"        ,
}};

const ring::CallbackMatrix1D<TlsValidator::CertificateCheck, TlsValidator, TlsValidator::CheckResult> TlsValidator::checkCallback = {{
    /*      CertificateCheck                       Callback                            */
    /*HAS_PRIVATE_KEY                  */ &TlsValidator::hasPrivateKey                  ,
    /*EXPIRED                          */ &TlsValidator::notExpired                     ,
    /*STRONG_SIGNING                   */ &TlsValidator::strongSigning                  ,
    /*NOT_SELF_SIGNED                  */ &TlsValidator::notSelfSigned                  ,
    /*KEY_MATCH                        */ &TlsValidator::keyMatch                       ,
    /*PRIVATE_KEY_STORAGE_PERMISSION   */ &TlsValidator::privateKeyStoragePermissions   ,
    /*PUBLIC_KEY_STORAGE_PERMISSION    */ &TlsValidator::publicKeyStoragePermissions    ,
    /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ &TlsValidator::privateKeyDirectoryPermissions ,
    /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ &TlsValidator::publicKeyDirectoryPermissions  ,
    /*PRIVATE_KEY_STORAGE_LOCATION     */ &TlsValidator::privateKeyStorageLocation      ,
    /*PUBLIC_KEY_STORAGE_LOCATION      */ &TlsValidator::publicKeyStorageLocation       ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ &TlsValidator::privateKeySelinuxAttributes    ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ &TlsValidator::publicKeySelinuxAttributes     ,
    /*OUTGOING_SERVER                  */ &TlsValidator::outgoingServer                 ,
    /*EXIST                            */ &TlsValidator::exist                          ,
    /*VALID                            */ &TlsValidator::valid                          ,
    /*VALID_AUTHORITY                  */ &TlsValidator::validAuthority                 ,
    /*KNOWN_AUTHORITY                  */ &TlsValidator::knownAuthority                 ,
    /*NOT_REVOKED                      */ &TlsValidator::notRevoked                     ,
    /*AUTHORITY_MISMATCH               */ &TlsValidator::authorityMatch                 ,
    /*UNEXPECTED_OWNER                 */ &TlsValidator::expectedOwner                  ,
    /*NOT_ACTIVATED                    */ &TlsValidator::activated                      ,
}};


const ring::CallbackMatrix1D<TlsValidator::CertificateDetails, TlsValidator, TlsValidator::CheckResult> TlsValidator::getterCallback = {{
    /* EXPIRATION_DATE              */  &TlsValidator::getExpirationDate         ,
    /* ACTIVATION_DATE              */  &TlsValidator::getActivationDate         ,
    /* REQUIRE_PRIVATE_KEY_PASSWORD */  &TlsValidator::requirePrivateKeyPassword ,
    /* PUBLIC_SIGNATURE             */  &TlsValidator::getPublicSignature        ,
    /* VERSION_NUMBER               */  &TlsValidator::getVersionNumber          ,
    /* SERIAL_NUMBER                */  &TlsValidator::getSerialNumber           ,
    /* ISSUER                       */  &TlsValidator::getIssuer                 ,
    /* SUBJECT_KEY_ALGORITHM        */  &TlsValidator::getSubjectKeyAlgorithm    ,
    /* CN                           */  &TlsValidator::getCN                     ,
    /* N                            */  &TlsValidator::getN                      ,
    /* O                            */  &TlsValidator::getO                      ,
    /* SIGNATURE_ALGORITHM          */  &TlsValidator::getSignatureAlgorithm     ,
    /* MD5_FINGERPRINT              */  &TlsValidator::getMd5Fingerprint         ,
    /* SHA1_FINGERPRINT             */  &TlsValidator::getSha1Fingerprint        ,
    /* PUBLIC_KEY_ID                */  &TlsValidator::getPublicKeyId            ,
    /* ISSUER_DN                    */  &TlsValidator::getIssuerDN               ,
    /* NEXT_EXPECTED_UPDATE_DATE    */  &TlsValidator::getIssuerDN               , // TODO
}};

const ring::Matrix1D<TlsValidator::CertificateCheck, TlsValidator::CheckValuesType> TlsValidator::enforcedCheckType = {{
    /*      CertificateCheck                    Callback        */
    /*HAS_PRIVATE_KEY                  */ CheckValuesType::BOOLEAN ,
    /*EXPIRED                          */ CheckValuesType::BOOLEAN ,
    /*STRONG_SIGNING                   */ CheckValuesType::BOOLEAN ,
    /*NOT_SELF_SIGNED                  */ CheckValuesType::BOOLEAN ,
    /*KEY_MATCH                        */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_STORAGE_PERMISSION   */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_STORAGE_PERMISSION    */ CheckValuesType::BOOLEAN ,
    /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ CheckValuesType::BOOLEAN ,
    /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_STORAGE_LOCATION     */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_STORAGE_LOCATION      */ CheckValuesType::BOOLEAN ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ CheckValuesType::BOOLEAN ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ CheckValuesType::BOOLEAN ,
//     /*REQUIRE_PRIVATE_KEY_PASSWORD     */ CheckValuesType::BOOLEAN ,
    /*OUTGOING_SERVER                  */ CheckValuesType::CUSTOM  ,
    /*EXIST                            */ CheckValuesType::BOOLEAN ,
    /*VALID                            */ CheckValuesType::BOOLEAN ,
    /*VALID_AUTHORITY                  */ CheckValuesType::BOOLEAN ,
    /*KNOWN_AUTHORITY                  */ CheckValuesType::BOOLEAN ,
    /*NOT_REVOKED                      */ CheckValuesType::BOOLEAN ,
//     /*EXPIRATION_DATE                  */ CheckValuesType::ISO_DATE,
//     /*ACTIVATION_DATE                  */ CheckValuesType::ISO_DATE,
    /*AUTHORITY_MISMATCH               */ CheckValuesType::BOOLEAN ,
    /*UNEXPECTED_OWNER                 */ CheckValuesType::BOOLEAN ,
    /*NOT_ACTIVATED                    */ CheckValuesType::BOOLEAN ,
}};

const ring::EnumClassNames<TlsValidator::CertificateCheck> TlsValidator::CertificateCheckNames = {{
    /*      CertificateCheck                       Name                 */
    /*HAS_PRIVATE_KEY                  */ "HAS_PRIVATE_KEY"                ,
    /*EXPIRED                          */ "EXPIRED"                        ,
    /*STRONG_SIGNING                   */ "STRONG_SIGNING"                 ,
    /*NOT_SELF_SIGNED                  */ "NOT_SELF_SIGNED"                ,
    /*KEY_MATCH                        */ "KEY_MATCH"                      ,
    /*PRIVATE_KEY_STORAGE_PERMISSION   */ "PRIVATE_KEY_STORAGE_PERMISSION" ,
    /*PUBLIC_KEY_STORAGE_PERMISSION    */ "PUBLIC_KEY_STORAGE_PERMISSION"  ,
    /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ "PRIVATEKEY_DIRECTORY_PERMISSIONS"  ,
    /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ "PUBLICKEY_DIRECTORY_PERMISSIONS"   ,
    /*PRIVATE_KEY_STORAGE_LOCATION     */ "PRIVATE_KEY_STORAGE_LOCATION"   ,
    /*PUBLIC_KEY_STORAGE_LOCATION      */ "PUBLIC_KEY_STORAGE_LOCATION"    ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ "PRIVATE_KEY_SELINUX_ATTRIBUTES" ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ "PUBLIC_KEY_SELINUX_ATTRIBUTES"  ,
//     /*REQUIRE_PRIVATE_KEY_PASSWORD     */ "REQUIRE_PRIVATE_KEY_PASSWORD"   , // TODO move to certificateDetails()
    /*OUTGOING_SERVER                  */ "OUTGOING_SERVER"                ,
    /*EXIST                            */ "EXIST"                          ,
    /*VALID                            */ "VALID"                          ,
    /*VALID_AUTHORITY                  */ "VALID_AUTHORITY"                ,
    /*KNOWN_AUTHORITY                  */ "KNOWN_AUTHORITY"                ,
    /*NOT_REVOKED                      */ "NOT_REVOKED"                    ,
//     /*EXPIRATION_DATE                  */ "EXPIRATION_DATE"                , // TODO move to certificateDetails()
//     /*ACTIVATION_DATE                  */ "ACTIVATION_DATE"                , // TODO move to certificateDetails()
    /*AUTHORITY_MISMATCH               */ "AUTHORITY_MISMATCH"             ,
    /*UNEXPECTED_OWNER                 */ "UNEXPECTED_OWNER"               ,
    /*NOT_ACTIVATED                    */ "NOT_ACTIVATED"                  ,
}};

const ring::EnumClassNames<TlsValidator::CertificateDetails> TlsValidator::CertificateDetailsNames = {{
    /* EXPIRATION_DATE              */ "EXPIRATION_DATE"              ,
    /* ACTIVATION_DATE              */ "ACTIVATION_DATE"              ,
    /* REQUIRE_PRIVATE_KEY_PASSWORD */ "REQUIRE_PRIVATE_KEY_PASSWORD" ,
    /* PUBLIC_SIGNATURE             */ "PUBLIC_SIGNATURE"             ,
    /* VERSION_NUMBER               */ "VERSION_NUMBER"               ,
    /* SERIAL_NUMBER                */ "SERIAL_NUMBER"                ,
    /* ISSUER                       */ "ISSUER"                       ,
    /* SUBJECT_KEY_ALGORITHM        */ "SUBJECT_KEY_ALGORITHM"        ,
    /* CN                           */ "CN"                           ,
    /* N                            */ "N"                            ,
    /* O                            */ "O"                            ,
    /* SIGNATURE_ALGORITHM          */ "SIGNATURE_ALGORITHM"          ,
    /* MD5_FINGERPRINT              */ "MD5_FINGERPRINT"              ,
    /* SHA1_FINGERPRINT             */ "SHA1_FINGERPRINT"             ,
    /* PUBLIC_KEY_ID                */ "PUBLIC_KEY_ID"                ,
    /* ISSUER_DN                    */ "ISSUER_DN"                    ,
    /* NEXT_EXPECTED_UPDATE_DATE    */ "NEXT_EXPECTED_UPDATE_DATE"    ,
}};

const ring::EnumClassNames<const TlsValidator::CheckValuesType> TlsValidator::CheckValuesTypeNames = {{
    /*   Type        Name    */
    /* BOOLEAN  */ "BOOLEAN"  ,
    /* ISO_DATE */ "ISO_DATE" ,
    /* CUSTOM   */ "CUSTOM"   ,
    /* NUMBER   */ "NUMBER"   ,
}};

const ring::Matrix2D<TlsValidator::CheckValuesType , TlsValidator::CheckValues , bool> TlsValidator::acceptedCheckValuesResult = {{
    /*   Type          PASSED    FAILED   UNSUPPORTED   ISO_DATE    CUSTOM    NUMBER */
    /* BOOLEAN  */  {{  true   ,  true  ,    true     ,  false    ,  false   ,false }},
    /* ISO_DATE */  {{  false  ,  false ,    true     ,  true     ,  false  , false }},
    /* CUSTOM   */  {{  false  ,  false ,    true     ,  false    ,  true   , false }},
    /* NUMBER   */  {{  false  ,  false ,    true     ,  false    ,  false  , true  }},
}};


TlsValidator::TlsValidator(const std::string& certificate, const std::string& privatekey) :
certificatePath_(certificate), privateKeyPath_(privatekey), caCert_(nullptr), caChecked_(false)
{
    int err = gnutls_global_init();
    if (err != GNUTLS_E_SUCCESS)
        throw TlsValidatorException(gnutls_strerror(err));

    try {
        x509crt_ = {fileutils::loadFile(certificatePath_)};
        certificateContent_ = x509crt_.getPacked();
    } catch (const std::exception& e) {
        throw TlsValidatorException("Can't load certificate");
    }

    try {
        privateKeyContent_ = fileutils::loadFile(privateKeyPath_);
        dht::crypto::PrivateKey key_tmp(privateKeyContent_);
        privateKeyFound_ = true;
    } catch (const std::exception& e) {
        privateKeyContent_.clear();
    }
}

TlsValidator::~TlsValidator()
{
    gnutls_global_deinit();
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
        case CheckValues::ISO_DATE:
            // TODO validate date
            // return CheckValues::FAILED;
            return result.second;
        case CheckValues::NUMBER:
            // TODO Validate numbers
        case CheckValues::CUSTOM:
            return result.second;
        default:
            // Consider any other case (such as forced int->CheckValues casting) as failed
            return CheckValuesNames[CheckValues::FAILED];
    };
}

/**
 * Check if all boolean check passed
 * return true if there was no ::FAILED checks
 *
 * Checks functions are not "const", so this function isn't
 */
bool TlsValidator::isValid(bool verbose)
{
    for (const CertificateCheck check : ring::Matrix0D<CertificateCheck>()) {
        if (enforcedCheckType[check] == CheckValuesType::BOOLEAN) {
            if (((this->*(checkCallback[check]))()).first == CheckValues::FAILED) {
                if (verbose)
                    RING_WARN("Check failed: %s", CertificateCheckNames[check]);
                return false;
            }
        }
    }
    return true;
}

/**
 * Convert all checks results into a string map
 */
std::map<std::string,std::string> TlsValidator::getSerializedChecks()
{
    std::map<std::string,std::string> ret;
    if (not certificateFound_) {
        // Instead of checking `certificateFound` everywhere, handle it once
        ret[CertificateCheckNames[CertificateCheck::EXIST]]
            = getStringValue(CertificateCheck::EXIST, exist());
    }
    else {
        for (const CertificateCheck check : ring::Matrix0D<CertificateCheck>())
            ret[CertificateCheckNames[check]] = getStringValue(check,(this->*(checkCallback[check]))());
    }

    return ret;
}

/**
 * Get a map with all common certificate details
 */
std::map<std::string,std::string> TlsValidator::getSerializedDetails()
{
    std::map<std::string,std::string> ret;
    if (certificateFound_) {
        for (const CertificateDetails det : ring::Matrix0D<CertificateDetails>()) {
            const CheckResult r = (this->*(getterCallback[det]))();
            std::string val;
            // TODO move this to a fuction
            switch (r.first) {
                case CheckValues::PASSED:
                case CheckValues::FAILED:
                case CheckValues::UNSUPPORTED:
                    val = CheckValuesNames[r.first];
                    break;
                case CheckValues::ISO_DATE:
                    // TODO validate date
                case CheckValues::NUMBER:
                    // TODO Validate numbers
                case CheckValues::CUSTOM:
                default:
                    val = r.second;
                    break;
            };
            ret[CertificateDetailsNames[det]] = val;
        }
    }
    return ret;
}

/**
 * Set an authority
 */
void TlsValidator::setCaTlsValidator(const TlsValidator& validator)
{
    caChecked_ = false;
    caCert_ = (TlsValidator*)(&validator);
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

    RING_DBG("Subject: %s", name);
    RING_DBG("Issuer: %s", issuer_name);

    if (issuer != nullptr) {
        issuer_name_size = sizeof(issuer_name);
        gnutls_x509_crt_get_dn(issuer, issuer_name, &issuer_name_size);

        RING_DBG("Verified against: %s", issuer_name);
    }

    return 0;
}

/**
 * Helper method to return UNSUPPORTED when an error is detected
 */
static TlsValidator::CheckResult checkError(int err, char* copy_buffer, size_t size)
{
    return TlsValidator::TlsValidator::CheckResult(
        err == GNUTLS_E_SUCCESS ?
            TlsValidator::CheckValues::CUSTOM : TlsValidator::CheckValues::UNSUPPORTED,
        err == GNUTLS_E_SUCCESS ?
            std::string(copy_buffer, size) : ""
    );
}

/**
 * Some fields, such as the binary signature need to be converted to an
 * ASCII-hexadecimal representation before being sent to DBus as it will cause the
 * process to assert
 */
static std::string binaryToHex(const char* input, size_t input_sz)
{
    std::ostringstream ret;
    for (size_t i=0; i<input_sz; i++)
        ret << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << (unsigned)input[i];
    return ret.str();
}

/**
 * Convert a time_t to an ISO date string
 */
static TlsValidator::CheckResult formatDate(const time_t time)
{
    char buffer[12];
    struct tm* timeinfo = localtime(&time);
    strftime(buffer, sizeof(buffer), "%F\0", timeinfo);
    return TlsValidator::CheckResult(TlsValidator::CheckValues::ISO_DATE, buffer);
}

/**
 * Helper method to return UNSUPPORTED when an error is detected
 *
 * This method also convert the output to binary
 */
static TlsValidator::CheckResult checkBinaryError(int err, char* copy_buffer, size_t resultSize)
{
    if (err == GNUTLS_E_SUCCESS)
        return TlsValidator::CheckResult(TlsValidator::CheckValues::CUSTOM, binaryToHex(copy_buffer, resultSize));
    else
        return TlsValidator::CheckResult(TlsValidator::CheckValues::UNSUPPORTED, "");
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

    const int err = gnutls_x509_crt_verify(
        x509crt_.cert, &caCert_->x509crt_.cert, 1, 0, &caValidationOutput_);

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
        RING_ERR("Wrong parameters used - host %s, port %d.", host.c_str(), port);
        return res;
    }

    /* Create the socket. */
    sockfd = socket (PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        RING_ERR("Could not create socket.");
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
        RING_ERR("Unknown host %s.", host.c_str());
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
                    RING_ERR("Could not connect to hostname %s at port %d",
                          host.c_str(), port);
                    goto out;
                } else if (err > 0) {
                    /* Select returned, if so_error is clean we are ready. */
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                    if (so_error) {
                        RING_ERR("Connection delayed.");
                        goto out;
                    }
                    break;  // exit do-while loop
                } else {
                    RING_ERR("Connection timeout.");
                    goto out;
                }
            } while(1);
        } else {
            RING_ERR("Could not connect to hostname %s at port %d", host.c_str(), port);
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
        RING_ERR("Could not set TCP_NODELAY.");
        goto out;
    }


    /* Load the trusted CA certificates. */
    err = gnutls_certificate_allocate_credentials(&cred);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not allocate credentials - %s", gnutls_strerror(err));
        goto out;
    }
    err = gnutls_certificate_set_x509_system_trust(cred);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not load credentials.");
        goto out;
    }

    /* Create the session object. */
    err = gnutls_init(&session, GNUTLS_CLIENT);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not init session -%s\n", gnutls_strerror(err));
        goto out;
    }

    /* Configure the cipher preferences. The default set should be good enough. */
    err = gnutls_priority_set_direct(session, "NORMAL", &errptr);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not set up ciphers - %s (%s)", gnutls_strerror(err), errptr);
        goto out;
    }

    /* Install the trusted certificates. */
    err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cred);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not set up credentials - %s", gnutls_strerror(err));
        goto out;
    }

    /* Associate the socket with the session object and set the server name. */
    gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t) (uintptr_t) sockfd);
    err = gnutls_server_name_set(session, GNUTLS_NAME_DNS, host.c_str(), host.size());
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not set server name - %s", gnutls_strerror(err));
        goto out;
    }

    /* Establish the connection. */
    err = gnutls_handshake(session);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Handshake failed - %s", gnutls_strerror(err));
        goto out;
    }
    /* Obtain the server certificate chain. The server certificate
     * itself is stored in the first element of the array. */
    certs = gnutls_certificate_get_peers(session, &certslen);
    if (certs == nullptr || certslen == 0) {
        RING_ERR("Could not obtain peer certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* Validate the certificate chain. */
    err = gnutls_certificate_verify_peers2(session, &status);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not verify the certificate chain - %s", gnutls_strerror(err));
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
            RING_ERR("Certificate validation failed - %s\n", msg.data);
            gnutls_free(msg.data);
            goto out;
        } else {
            RING_ERR("Certificate validation failed with code 0x%x.", status);
            goto out;
        }
    }

    /* Match the peer certificate against the hostname.
     * We can only obtain a set of DER-encoded certificates from the
     * session object, so we have to re-parse the peer certificate into
     * a certificate object. */

    err = gnutls_x509_crt_init(&cert);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not init certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* The peer certificate is the first certificate in the list. */
    err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        RING_ERR("Could not read peer certificate - %s", gnutls_strerror(err));
        goto out;
    }
    /* Finally check if the hostnames match. */
    err = gnutls_x509_crt_check_hostname(cert, host.c_str());
    if (err == 0) {
        RING_ERR("Hostname %s does not match certificate.", host.c_str());
        goto out;
    }

    /* Try sending and receiving some data through. */
    snprintf(buf, sizeof(buf), "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host.c_str());
    err = gnutls_record_send(session, buf, strlen(buf));
    if (err < 0) {
        RING_ERR("Send failed - %s", gnutls_strerror(err));
        goto out;
    }
    err = gnutls_record_recv(session, buf, sizeof(buf));
    if (err < 0) {
        RING_ERR("Recv failed - %s", gnutls_strerror(err));
        goto out;
    }

    RING_DBG("Hostname %s seems to point to a valid server.", host.c_str());
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
        return TlsValidator::CheckResult(CheckValues::PASSED, "");

    try {
        dht::crypto::PrivateKey key_tmp(certificateContent_);
    } catch (const std::exception& e) {
        return CheckResult(CheckValues::FAILED, e.what());
    }

    RING_DBG("Key from %s seems valid.", certificatePath_.c_str());
    return CheckResult(CheckValues::PASSED, "");
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
        ? CheckValues::FAILED:CheckValues::PASSED, "");
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
        ? CheckValues::FAILED:CheckValues::PASSED, "");
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
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

/**
 * The certificate is not self signed
 */
TlsValidator::CheckResult TlsValidator::notSelfSigned()
{
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * The provided key can be used along with the certificate
 */
TlsValidator::CheckResult TlsValidator::keyMatch()
{
    // TODO encrypt and decrypt a small string to check
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

TlsValidator::CheckResult TlsValidator::privateKeyStoragePermissions()
{
    struct stat statbuf;
    int err = stat(privateKeyPath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(
        (statbuf.st_mode & S_IFREG) && /* Regular file only */
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode ^ S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

TlsValidator::CheckResult TlsValidator::publicKeyStoragePermissions()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(
        (statbuf.st_mode & S_IFREG) && /* Regular file only */
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode ^ S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

TlsValidator::CheckResult TlsValidator::privateKeyDirectoryPermissions()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode & S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

TlsValidator::CheckResult TlsValidator::publicKeyDirectoryPermissions()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ((statbuf.st_mode & S_IRUSR) /* write is not relevant */    && (statbuf.st_mode & S_IXUSR))
        /* Group */ && ((statbuf.st_mode ^ S_IRGRP) && (statbuf.st_mode ^ S_IWGRP) && (statbuf.st_mode ^ S_IXGRP))
        /* Other */ && ((statbuf.st_mode ^ S_IROTH) && (statbuf.st_mode ^ S_IWOTH) && (statbuf.st_mode ^ S_IXOTH))
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

/**
 * Certificate should be located in specific path on some operating systems
 */
TlsValidator::CheckResult TlsValidator::privateKeyStorageLocation()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * Certificate should be located in specific path on some operating systems
 */
TlsValidator::CheckResult TlsValidator::publicKeyStorageLocation()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * SELinux provide additional key protection mechanism
 */
TlsValidator::CheckResult TlsValidator::privateKeySelinuxAttributes()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * SELinux provide additional key protection mechanism
 */
TlsValidator::CheckResult TlsValidator::publicKeySelinuxAttributes()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * If the key need decryption
 *
 * Double factor authentication is recommended
 */
TlsValidator::CheckResult TlsValidator::requirePrivateKeyPassword()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
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
    return TlsValidator::CheckResult(CheckValues::CUSTOM, "");
}

/**
 * The CA and certificate provide conflicting ownership information
 */
TlsValidator::CheckResult TlsValidator::expectedOwner()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_UNEXPECTED_OWNER
        ? CheckValues::FAILED : CheckValues::PASSED, "");
}

/**
 * The file has been found
 */
TlsValidator::CheckResult TlsValidator::exist()
{
    return TlsValidator::CheckResult(certificateFound_ ? CheckValues::PASSED : CheckValues::FAILED, "");
}

/**
 * The certificate is invalid compared to the authority
 *
 * @todo Handle case when there is facultative authority, such as DHT
 */
TlsValidator::CheckResult TlsValidator::valid()
{
    // TODO this is wrong
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_INVALID
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

/**
 * The provided authority is invalid
 */
TlsValidator::CheckResult TlsValidator::validAuthority()
{
    // TODO Merge with either above or bellow
    return TlsValidator::CheckResult((!caCert_) || (compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND)
                                      // ^--- When no authority is present, then it is not invalid, it is not there at all
        ? CheckValues::FAILED:CheckValues::PASSED, "");
}

/**
 * Check if the authority match the certificate
 */
TlsValidator::CheckResult TlsValidator::authorityMatch()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_CA
        ? CheckValues::FAILED : CheckValues::PASSED, "");
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
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND
        ? CheckValues::FAILED : CheckValues::PASSED, "");
}

/**
 * Check if the certificate has been revoked
 */
TlsValidator::CheckResult TlsValidator::notRevoked()
{
    return TlsValidator::CheckResult(
        (compareToCa() & GNUTLS_CERT_REVOKED) || (compareToCa() & GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE)
        ? CheckValues::FAILED : CheckValues::PASSED, "");
}

/**
 * A certificate authority has been provided
 */
bool TlsValidator::hasCa() const
{
    return caCert_ != nullptr and caCert_->certificateFound_;
}

//
// Certificate details
//

// TODO gnutls_x509_crl_get_this_update

/**
 * An hexadecimal representation of the signature
 */
TlsValidator::CheckResult TlsValidator::getPublicSignature()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_signature(x509crt_.cert, copy_buffer, &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * Return the certificate version
 */
TlsValidator::CheckResult TlsValidator::getVersionNumber()
{
    int version = gnutls_x509_crt_get_version(x509crt_.cert);
    if (version < 0)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    std::ostringstream convert;
    convert << version;

    return TlsValidator::CheckResult(CheckValues::NUMBER, convert.str());
}

/**
 * Return the certificate serial number
 */
TlsValidator::CheckResult TlsValidator::getSerialNumber()
{
// gnutls_x509_crl_iter_crt_serial
// gnutls_x509_crt_get_authority_key_gn_serial
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_serial(x509crt_.cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * If the certificate is not self signed, return the issuer
 */
TlsValidator::CheckResult TlsValidator::getIssuer()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_unique_id(x509crt_.cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * The algorithm used to sign the certificate details (rather than the certificate itself)
 */
TlsValidator::CheckResult TlsValidator::getSubjectKeyAlgorithm()
{
    gnutls_pk_algorithm_t algo = (gnutls_pk_algorithm_t) gnutls_x509_crt_get_pk_algorithm(
        x509crt_.cert, nullptr);

    if (algo < 0)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    const char* name = gnutls_pk_get_name(algo);

    if (!name)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(CheckValues::CUSTOM, name);
}

/**
 * The 'CN' section of a DN (RFC4514)
 */
TlsValidator::CheckResult TlsValidator::getCN()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn(x509crt_.cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * The 'N' section of a DN (RFC4514)
 */
TlsValidator::CheckResult TlsValidator::getN()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn(x509crt_.cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * The 'O' section of a DN (RFC4514)
 */
TlsValidator::CheckResult TlsValidator::getO()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn(x509crt_.cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * Return the algorithm used to sign the Key
 *
 * For example: RSA
 */
TlsValidator::CheckResult TlsValidator::getSignatureAlgorithm()
{
    gnutls_sign_algorithm_t algo = (gnutls_sign_algorithm_t) gnutls_x509_crt_get_signature_algorithm(x509crt_.cert);

    if (algo < 0)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    const char* algoName =  gnutls_sign_get_name(algo);
    return TlsValidator::CheckResult(CheckValues::CUSTOM, algoName);
}

/**
 *Compute the key fingerprint
 *
 * This need to be used along with getSha1Fingerprint() to avoid collisions
 */
TlsValidator::CheckResult TlsValidator::getMd5Fingerprint()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_fingerprint(x509crt_.cert, GNUTLS_DIG_MD5, copy_buffer, &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * Compute the key fingerprint
 *
 * This need to be used along with getMd5Fingerprint() to avoid collisions
 */
TlsValidator::CheckResult TlsValidator::getSha1Fingerprint()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_fingerprint(x509crt_.cert, GNUTLS_DIG_SHA1, copy_buffer, &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * Return an hexadecimal identifier
 */
TlsValidator::CheckResult TlsValidator::getPublicKeyId()
{
    size_t resultSize = sizeof(copy_buffer);
    static unsigned char unsigned_copy_buffer[4096];
    int err = gnutls_x509_crt_get_key_id(x509crt_.cert,0,unsigned_copy_buffer,&resultSize);

    // TODO check for GNUTLS_E_SHORT_MEMORY_BUFFER and increase the buffer size
    // TODO get rid of the cast, display a HEX or something, need research

    return checkBinaryError(err, (char*) unsigned_copy_buffer, resultSize);
}
// gnutls_x509_crt_get_authority_key_id

/**
 *  If the certificate is not self signed, return the issuer DN (RFC4514)
 */
TlsValidator::CheckResult TlsValidator::getIssuerDN()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_dn(x509crt_.cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * Get the expiration date
 *
 * @todo Move to "certificateDetails()" method once completed
 */
TlsValidator::CheckResult TlsValidator::getExpirationDate()
{
    if (not certificateFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    time_t expiration = gnutls_x509_crt_get_expiration_time(x509crt_.cert);

    return formatDate(expiration);
}

/**
 * Get the activation date
 *
 * @todo Move to "certificateDetails()" method once completed
 */
TlsValidator::CheckResult TlsValidator::getActivationDate()
{
    if (not certificateFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    time_t expiration = gnutls_x509_crt_get_activation_time(x509crt_.cert);

    return formatDate(expiration);
}
