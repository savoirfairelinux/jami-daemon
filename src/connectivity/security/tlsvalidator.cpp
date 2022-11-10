/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *  Author: Vittorio Giovara <vittorio.giovara@savoirfairelinux.com>
 *  Author: Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
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
 */

#include "tlsvalidator.h"

#include "certstore.h"

#include "fileutils.h"
#include "logger.h"
#include "security_const.h"

#include <sstream>
#include <iomanip>

#include <cstdio>
#include <cerrno>
#include <cassert>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>

#ifndef _MSC_VER
#include <libgen.h>
#endif

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#else
#ifndef _MSC_VER
#define close(x) closesocket(x)
#endif
#endif
#include <unistd.h>
#include <fcntl.h>

#ifdef _MSC_VER
#include "windirent.h"
#endif

namespace jami {
namespace tls {

// Map the internal ring Enum class of the exported names

const EnumClassNames<TlsValidator::CheckValues> TlsValidator::CheckValuesNames = {{
    /* CheckValues                        Name                         */
    /* PASSED      */ libjami::Certificate::CheckValuesNames::PASSED,
    /* FAILED      */ libjami::Certificate::CheckValuesNames::FAILED,
    /* UNSUPPORTED */ libjami::Certificate::CheckValuesNames::UNSUPPORTED,
    /* ISO_DATE    */ libjami::Certificate::CheckValuesNames::ISO_DATE,
    /* CUSTOM      */ libjami::Certificate::CheckValuesNames::CUSTOM,
    /* CUSTOM      */ libjami::Certificate::CheckValuesNames::DATE,
}};

const CallbackMatrix1D<TlsValidator::CertificateCheck, TlsValidator, TlsValidator::CheckResult>
    TlsValidator::checkCallback = {{
        /*      CertificateCheck                       Callback                            */
        /*HAS_PRIVATE_KEY                  */ &TlsValidator::hasPrivateKey,
        /*EXPIRED                          */ &TlsValidator::notExpired,
        /*STRONG_SIGNING                   */ &TlsValidator::strongSigning,
        /*NOT_SELF_SIGNED                  */ &TlsValidator::notSelfSigned,
        /*KEY_MATCH                        */ &TlsValidator::keyMatch,
        /*PRIVATE_KEY_STORAGE_PERMISSION   */ &TlsValidator::privateKeyStoragePermissions,
        /*PUBLIC_KEY_STORAGE_PERMISSION    */ &TlsValidator::publicKeyStoragePermissions,
        /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ &TlsValidator::privateKeyDirectoryPermissions,
        /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ &TlsValidator::publicKeyDirectoryPermissions,
        /*PRIVATE_KEY_STORAGE_LOCATION     */ &TlsValidator::privateKeyStorageLocation,
        /*PUBLIC_KEY_STORAGE_LOCATION      */ &TlsValidator::publicKeyStorageLocation,
        /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ &TlsValidator::privateKeySelinuxAttributes,
        /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ &TlsValidator::publicKeySelinuxAttributes,
        /*EXIST                            */ &TlsValidator::exist,
        /*VALID                            */ &TlsValidator::valid,
        /*VALID_AUTHORITY                  */ &TlsValidator::validAuthority,
        /*KNOWN_AUTHORITY                  */ &TlsValidator::knownAuthority,
        /*NOT_REVOKED                      */ &TlsValidator::notRevoked,
        /*AUTHORITY_MISMATCH               */ &TlsValidator::authorityMatch,
        /*UNEXPECTED_OWNER                 */ &TlsValidator::expectedOwner,
        /*NOT_ACTIVATED                    */ &TlsValidator::activated,
    }};

const CallbackMatrix1D<TlsValidator::CertificateDetails, TlsValidator, TlsValidator::CheckResult>
    TlsValidator::getterCallback = {{
        /* EXPIRATION_DATE              */ &TlsValidator::getExpirationDate,
        /* ACTIVATION_DATE              */ &TlsValidator::getActivationDate,
        /* REQUIRE_PRIVATE_KEY_PASSWORD */ &TlsValidator::requirePrivateKeyPassword,
        /* PUBLIC_SIGNATURE             */ &TlsValidator::getPublicSignature,
        /* VERSION_NUMBER               */ &TlsValidator::getVersionNumber,
        /* SERIAL_NUMBER                */ &TlsValidator::getSerialNumber,
        /* ISSUER                       */ &TlsValidator::getIssuer,
        /* SUBJECT_KEY_ALGORITHM        */ &TlsValidator::getSubjectKeyAlgorithm,
        /* CN                           */ &TlsValidator::getCN,
        /* N                            */ &TlsValidator::getN,
        /* O                            */ &TlsValidator::getO,
        /* SIGNATURE_ALGORITHM          */ &TlsValidator::getSignatureAlgorithm,
        /* MD5_FINGERPRINT              */ &TlsValidator::getMd5Fingerprint,
        /* SHA1_FINGERPRINT             */ &TlsValidator::getSha1Fingerprint,
        /* PUBLIC_KEY_ID                */ &TlsValidator::getPublicKeyId,
        /* ISSUER_DN                    */ &TlsValidator::getIssuerDN,
        /* NEXT_EXPECTED_UPDATE_DATE    */ &TlsValidator::getIssuerDN, // TODO
        /* OUTGOING_SERVER              */ &TlsValidator::outgoingServer,
        /* IS_CA                        */ &TlsValidator::isCA,
    }};

const Matrix1D<TlsValidator::CertificateCheck, TlsValidator::CheckValuesType>
    TlsValidator::enforcedCheckType = {{
        /*      CertificateCheck                    Callback        */
        /*HAS_PRIVATE_KEY                  */ CheckValuesType::BOOLEAN,
        /*EXPIRED                          */ CheckValuesType::BOOLEAN,
        /*STRONG_SIGNING                   */ CheckValuesType::BOOLEAN,
        /*NOT_SELF_SIGNED                  */ CheckValuesType::BOOLEAN,
        /*KEY_MATCH                        */ CheckValuesType::BOOLEAN,
        /*PRIVATE_KEY_STORAGE_PERMISSION   */ CheckValuesType::BOOLEAN,
        /*PUBLIC_KEY_STORAGE_PERMISSION    */ CheckValuesType::BOOLEAN,
        /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ CheckValuesType::BOOLEAN,
        /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ CheckValuesType::BOOLEAN,
        /*PRIVATE_KEY_STORAGE_LOCATION     */ CheckValuesType::BOOLEAN,
        /*PUBLIC_KEY_STORAGE_LOCATION      */ CheckValuesType::BOOLEAN,
        /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ CheckValuesType::BOOLEAN,
        /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ CheckValuesType::BOOLEAN,
        /*EXIST                            */ CheckValuesType::BOOLEAN,
        /*VALID                            */ CheckValuesType::BOOLEAN,
        /*VALID_AUTHORITY                  */ CheckValuesType::BOOLEAN,
        /*KNOWN_AUTHORITY                  */ CheckValuesType::BOOLEAN,
        /*NOT_REVOKED                      */ CheckValuesType::BOOLEAN,
        /*AUTHORITY_MISMATCH               */ CheckValuesType::BOOLEAN,
        /*UNEXPECTED_OWNER                 */ CheckValuesType::BOOLEAN,
        /*NOT_ACTIVATED                    */ CheckValuesType::BOOLEAN,
    }};

const EnumClassNames<TlsValidator::CertificateCheck> TlsValidator::CertificateCheckNames = {{
    /*      CertificateCheck                                   Name */
    /*HAS_PRIVATE_KEY                  */ libjami::Certificate::ChecksNames::HAS_PRIVATE_KEY,
    /*EXPIRED                          */ libjami::Certificate::ChecksNames::EXPIRED,
    /*STRONG_SIGNING                   */ libjami::Certificate::ChecksNames::STRONG_SIGNING,
    /*NOT_SELF_SIGNED                  */ libjami::Certificate::ChecksNames::NOT_SELF_SIGNED,
    /*KEY_MATCH                        */ libjami::Certificate::ChecksNames::KEY_MATCH,
    /*PRIVATE_KEY_STORAGE_PERMISSION   */ libjami::Certificate::ChecksNames::PRIVATE_KEY_STORAGE_PERMISSION,
    /*PUBLIC_KEY_STORAGE_PERMISSION    */ libjami::Certificate::ChecksNames::PUBLIC_KEY_STORAGE_PERMISSION,
    /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ libjami::Certificate::ChecksNames::PRIVATE_KEY_DIRECTORY_PERMISSIONS,
    /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ libjami::Certificate::ChecksNames::PUBLIC_KEY_DIRECTORY_PERMISSIONS,
    /*PRIVATE_KEY_STORAGE_LOCATION     */ libjami::Certificate::ChecksNames::PRIVATE_KEY_STORAGE_LOCATION,
    /*PUBLIC_KEY_STORAGE_LOCATION      */ libjami::Certificate::ChecksNames::PUBLIC_KEY_STORAGE_LOCATION,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ libjami::Certificate::ChecksNames::PRIVATE_KEY_SELINUX_ATTRIBUTES,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ libjami::Certificate::ChecksNames::PUBLIC_KEY_SELINUX_ATTRIBUTES,
    /*EXIST                            */ libjami::Certificate::ChecksNames::EXIST,
    /*VALID                            */ libjami::Certificate::ChecksNames::VALID,
    /*VALID_AUTHORITY                  */ libjami::Certificate::ChecksNames::VALID_AUTHORITY,
    /*KNOWN_AUTHORITY                  */ libjami::Certificate::ChecksNames::KNOWN_AUTHORITY,
    /*NOT_REVOKED                      */ libjami::Certificate::ChecksNames::NOT_REVOKED,
    /*AUTHORITY_MISMATCH               */ libjami::Certificate::ChecksNames::AUTHORITY_MISMATCH,
    /*UNEXPECTED_OWNER                 */ libjami::Certificate::ChecksNames::UNEXPECTED_OWNER,
    /*NOT_ACTIVATED                    */ libjami::Certificate::ChecksNames::NOT_ACTIVATED,
}};

const EnumClassNames<TlsValidator::CertificateDetails> TlsValidator::CertificateDetailsNames = {{
    /* EXPIRATION_DATE              */ libjami::Certificate::DetailsNames::EXPIRATION_DATE,
    /* ACTIVATION_DATE              */ libjami::Certificate::DetailsNames::ACTIVATION_DATE,
    /* REQUIRE_PRIVATE_KEY_PASSWORD */ libjami::Certificate::DetailsNames::REQUIRE_PRIVATE_KEY_PASSWORD,
    /* PUBLIC_SIGNATURE             */ libjami::Certificate::DetailsNames::PUBLIC_SIGNATURE,
    /* VERSION_NUMBER               */ libjami::Certificate::DetailsNames::VERSION_NUMBER,
    /* SERIAL_NUMBER                */ libjami::Certificate::DetailsNames::SERIAL_NUMBER,
    /* ISSUER                       */ libjami::Certificate::DetailsNames::ISSUER,
    /* SUBJECT_KEY_ALGORITHM        */ libjami::Certificate::DetailsNames::SUBJECT_KEY_ALGORITHM,
    /* CN                           */ libjami::Certificate::DetailsNames::CN,
    /* N                            */ libjami::Certificate::DetailsNames::N,
    /* O                            */ libjami::Certificate::DetailsNames::O,
    /* SIGNATURE_ALGORITHM          */ libjami::Certificate::DetailsNames::SIGNATURE_ALGORITHM,
    /* MD5_FINGERPRINT              */ libjami::Certificate::DetailsNames::MD5_FINGERPRINT,
    /* SHA1_FINGERPRINT             */ libjami::Certificate::DetailsNames::SHA1_FINGERPRINT,
    /* PUBLIC_KEY_ID                */ libjami::Certificate::DetailsNames::PUBLIC_KEY_ID,
    /* ISSUER_DN                    */ libjami::Certificate::DetailsNames::ISSUER_DN,
    /* NEXT_EXPECTED_UPDATE_DATE    */ libjami::Certificate::DetailsNames::NEXT_EXPECTED_UPDATE_DATE,
    /* OUTGOING_SERVER              */ libjami::Certificate::DetailsNames::OUTGOING_SERVER,
    /* IS_CA                        */ libjami::Certificate::DetailsNames::IS_CA,

}};

const EnumClassNames<const TlsValidator::CheckValuesType> TlsValidator::CheckValuesTypeNames = {{
    /*   Type                            Name                          */
    /* BOOLEAN  */ libjami::Certificate::ChecksValuesTypesNames::BOOLEAN,
    /* ISO_DATE */ libjami::Certificate::ChecksValuesTypesNames::ISO_DATE,
    /* CUSTOM   */ libjami::Certificate::ChecksValuesTypesNames::CUSTOM,
    /* NUMBER   */ libjami::Certificate::ChecksValuesTypesNames::NUMBER,
}};

const Matrix2D<TlsValidator::CheckValuesType, TlsValidator::CheckValues, bool>
    TlsValidator::acceptedCheckValuesResult = {{
        /*   Type          PASSED    FAILED   UNSUPPORTED   ISO_DATE    CUSTOM    NUMBER */
        /* BOOLEAN  */ {{true, true, true, false, false, false}},
        /* ISO_DATE */ {{false, false, true, true, false, false}},
        /* CUSTOM   */ {{false, false, true, false, true, false}},
        /* NUMBER   */ {{false, false, true, false, false, true}},
    }};

TlsValidator::TlsValidator(const std::vector<std::vector<uint8_t>>& crtChain)
    : TlsValidator(std::make_shared<dht::crypto::Certificate>(crtChain.begin(), crtChain.end()))
{}

TlsValidator::TlsValidator(const std::string& certificate,
                           const std::string& privatekey,
                           const std::string& privatekeyPasswd,
                           const std::string& caList)
    : certificatePath_(certificate)
    , privateKeyPath_(privatekey)
    , caListPath_(caList)
    , certificateFound_(false)
{
    std::vector<uint8_t> certificate_raw;
    try {
        certificate_raw = fileutils::loadFile(certificatePath_);
        certificateFileFound_ = true;
    } catch (const std::exception& e) {
    }

    if (not certificate_raw.empty()) {
        try {
            x509crt_ = std::make_shared<dht::crypto::Certificate>(certificate_raw);
            certificateContent_ = x509crt_->getPacked();
            certificateFound_ = true;
        } catch (const std::exception& e) {
        }
    }

    try {
        auto privateKeyContent = fileutils::loadFile(privateKeyPath_);
        dht::crypto::PrivateKey key_tmp(privateKeyContent, privatekeyPasswd);
        privateKeyFound_ = true;
        privateKeyPassword_ = not privatekeyPasswd.empty();
        privateKeyMatch_ = key_tmp.getPublicKey().getId() == x509crt_->getId();
    } catch (const dht::crypto::DecryptError& d) {
        // If we encounter a DecryptError, it means the private key exists and is encrypted,
        // otherwise we would get some other exception.
        JAMI_WARN("decryption error: %s", d.what());
        privateKeyFound_ = true;
        privateKeyPassword_ = true;
    } catch (const std::exception& e) {
        JAMI_WARN("creation failed: %s", e.what());
    }
}

TlsValidator::TlsValidator(const std::vector<uint8_t>& certificate_raw)
{
    try {
        x509crt_ = std::make_shared<dht::crypto::Certificate>(certificate_raw);
        certificateContent_ = x509crt_->getPacked();
        certificateFound_ = true;
    } catch (const std::exception& e) {
        throw TlsValidatorException("Can't load certificate");
    }
}

TlsValidator::TlsValidator(const std::shared_ptr<dht::crypto::Certificate>& crt)
    : certificateFound_(true)
{
    try {
        if (not crt)
            throw std::invalid_argument("Certificate must be set");
        x509crt_ = crt;
        certificateContent_ = x509crt_->getPacked();
    } catch (const std::exception& e) {
        throw TlsValidatorException("Can't load certificate");
    }
}

TlsValidator::~TlsValidator() {}

/**
 * This method convert results into validated strings
 *
 * @todo The date should be validated, this is currently not an issue
 */
std::string
TlsValidator::getStringValue(const TlsValidator::CertificateCheck check,
                             const TlsValidator::CheckResult result)
{
    assert(acceptedCheckValuesResult[enforcedCheckType[check]][result.first]);

    switch (result.first) {
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
bool
TlsValidator::isValid(bool verbose)
{
    for (const CertificateCheck check : Matrix0D<CertificateCheck>()) {
        if (enforcedCheckType[check] == CheckValuesType::BOOLEAN) {
            if (((this->*(checkCallback[check]))()).first == CheckValues::FAILED) {
                if (verbose)
                    JAMI_WARN("Check failed: %s", CertificateCheckNames[check]);
                return false;
            }
        }
    }
    return true;
}

/**
 * Convert all checks results into a string map
 */
std::map<std::string, std::string>
TlsValidator::getSerializedChecks()
{
    std::map<std::string, std::string> ret;
    if (not certificateFound_) {
        // Instead of checking `certificateFound` everywhere, handle it once
        ret[CertificateCheckNames[CertificateCheck::EXIST]] = getStringValue(CertificateCheck::EXIST,
                                                                             exist());
    } else {
        for (const CertificateCheck check : Matrix0D<CertificateCheck>())
            ret[CertificateCheckNames[check]] = getStringValue(check,
                                                               (this->*(checkCallback[check]))());
    }

    return ret;
}

/**
 * Get a map with all common certificate details
 */
std::map<std::string, std::string>
TlsValidator::getSerializedDetails()
{
    std::map<std::string, std::string> ret;
    if (certificateFound_) {
        for (const CertificateDetails det : Matrix0D<CertificateDetails>()) {
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
 * Helper method to return UNSUPPORTED when an error is detected
 */
static TlsValidator::CheckResult
checkError(int err, char* copy_buffer, size_t size)
{
    return TlsValidator::TlsValidator::CheckResult(err == GNUTLS_E_SUCCESS
                                                       ? TlsValidator::CheckValues::CUSTOM
                                                       : TlsValidator::CheckValues::UNSUPPORTED,
                                                   err == GNUTLS_E_SUCCESS
                                                       ? std::string(copy_buffer, size)
                                                       : "");
}

/**
 * Some fields, such as the binary signature need to be converted to an
 * ASCII-hexadecimal representation before being sent to DBus as it will cause the
 * process to assert
 */
static std::string
binaryToHex(const uint8_t* input, size_t input_sz)
{
    std::ostringstream ret;
    ret << std::hex;
    for (size_t i = 0; i < input_sz; i++)
        ret << std::setfill('0') << std::setw(2) << (unsigned) input[i];
    return ret.str();
}

/**
 * Convert a time_t to an ISO date string
 */
static TlsValidator::CheckResult
formatDate(const time_t time)
{
    char buffer[12];
    struct tm* timeinfo = localtime(&time);
    strftime(buffer, sizeof(buffer), "%F", timeinfo);
    return TlsValidator::CheckResult(TlsValidator::CheckValues::ISO_DATE, buffer);
}

/**
 * Helper method to return UNSUPPORTED when an error is detected
 *
 * This method also convert the output to binary
 */
static TlsValidator::CheckResult
checkBinaryError(int err, char* copy_buffer, size_t resultSize)
{
    if (err == GNUTLS_E_SUCCESS)
        return TlsValidator::CheckResult(TlsValidator::CheckValues::CUSTOM,
                                         binaryToHex(reinterpret_cast<uint8_t*>(copy_buffer),
                                                     resultSize));
    else
        return TlsValidator::CheckResult(TlsValidator::CheckValues::UNSUPPORTED, "");
}

/**
 * Check if a certificate has been signed with the authority
 */
unsigned int
TlsValidator::compareToCa()
{
    // Don't check unless the certificate changed
    if (caChecked_)
        return caValidationOutput_;

    // build the CA trusted list
    gnutls_x509_trust_list_t trust;
    gnutls_x509_trust_list_init(&trust, 0);

    auto root_cas = CertificateStore::instance().getTrustedCertificates();
    auto err = gnutls_x509_trust_list_add_cas(trust, root_cas.data(), root_cas.size(), 0);
    if (err)
        JAMI_WARN("gnutls_x509_trust_list_add_cas failed: %s", gnutls_strerror(err));

    if (not caListPath_.empty()) {
        if (fileutils::isDirectory(caListPath_))
            gnutls_x509_trust_list_add_trust_dir(trust,
                                                 caListPath_.c_str(),
                                                 nullptr,
                                                 GNUTLS_X509_FMT_PEM,
                                                 0,
                                                 0);
        else
            gnutls_x509_trust_list_add_trust_file(trust,
                                                  caListPath_.c_str(),
                                                  nullptr,
                                                  GNUTLS_X509_FMT_PEM,
                                                  0,
                                                  0);
    }

    // build the certificate chain
    auto crts = x509crt_->getChain();
    err = gnutls_x509_trust_list_verify_crt2(trust,
                                             crts.data(),
                                             crts.size(),
                                             nullptr,
                                             0,
                                             GNUTLS_PROFILE_TO_VFLAGS(GNUTLS_PROFILE_MEDIUM),
                                             &caValidationOutput_,
                                             nullptr);

    gnutls_x509_trust_list_deinit(trust, true);

    if (err) {
        JAMI_WARN("gnutls_x509_trust_list_verify_crt2 failed: %s", gnutls_strerror(err));
        return GNUTLS_CERT_SIGNER_NOT_FOUND;
    }

    caChecked_ = true;
    return caValidationOutput_;
}

#if 0 // disabled, see .h for reason
/**
 * Verify if a hostname is valid
 *
 * @warning This function is blocking
 *
 * Mainly based on Fedora Defensive Coding tutorial
 * https://docs.fedoraproject.org/en-US/Fedora_Security_Team/1/html/Defensive_Coding/sect-Defensive_Coding-TLS-Client-GNUTLS.html
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
        JAMI_ERR("Wrong parameters used - host %s, port %d.", host.c_str(), port);
        return res;
    }

    /* Create the socket. */
    sockfd = socket (PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        JAMI_ERR("Could not create socket.");
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
        JAMI_ERR("Unknown host %s.", host.c_str());
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
                    JAMI_ERR("Could not connect to hostname %s at port %d",
                          host.c_str(), port);
                    goto out;
                } else if (err > 0) {
                    /* Select returned, if so_error is clean we are ready. */
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

                    if (so_error) {
                        JAMI_ERR("Connection delayed.");
                        goto out;
                    }
                    break;  // exit do-while loop
                } else {
                    JAMI_ERR("Connection timeout.");
                    goto out;
                }
            } while(1);
        } else {
            JAMI_ERR("Could not connect to hostname %s at port %d", host.c_str(), port);
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
        JAMI_ERR("Could not set TCP_NODELAY.");
        goto out;
    }


    /* Load the trusted CA certificates. */
    err = gnutls_certificate_allocate_credentials(&cred);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not allocate credentials - %s", gnutls_strerror(err));
        goto out;
    }
    err = gnutls_certificate_set_x509_system_trust(cred);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not load credentials.");
        goto out;
    }

    /* Create the session object. */
    err = gnutls_init(&session, GNUTLS_CLIENT);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not init session -%s\n", gnutls_strerror(err));
        goto out;
    }

    /* Configure the cipher preferences. The default set should be good enough. */
    err = gnutls_priority_set_direct(session, "NORMAL", &errptr);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not set up ciphers - %s (%s)", gnutls_strerror(err), errptr);
        goto out;
    }

    /* Install the trusted certificates. */
    err = gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, cred);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not set up credentials - %s", gnutls_strerror(err));
        goto out;
    }

    /* Associate the socket with the session object and set the server name. */
    gnutls_transport_set_ptr(session, (gnutls_transport_ptr_t) (uintptr_t) sockfd);
    err = gnutls_server_name_set(session, GNUTLS_NAME_DNS, host.c_str(), host.size());
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not set server name - %s", gnutls_strerror(err));
        goto out;
    }

    /* Establish the connection. */
    err = gnutls_handshake(session);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Handshake failed - %s", gnutls_strerror(err));
        goto out;
    }
    /* Obtain the server certificate chain. The server certificate
     * itself is stored in the first element of the array. */
    certs = gnutls_certificate_get_peers(session, &certslen);
    if (certs == nullptr || certslen == 0) {
        JAMI_ERR("Could not obtain peer certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* Validate the certificate chain. */
    err = gnutls_certificate_verify_peers2(session, &status);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not verify the certificate chain - %s", gnutls_strerror(err));
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
            JAMI_ERR("Certificate validation failed - %s\n", msg.data);
            gnutls_free(msg.data);
            goto out;
        } else {
            JAMI_ERR("Certificate validation failed with code 0x%x.", status);
            goto out;
        }
    }

    /* Match the peer certificate against the hostname.
     * We can only obtain a set of DER-encoded certificates from the
     * session object, so we have to re-parse the peer certificate into
     * a certificate object. */

    err = gnutls_x509_crt_init(&cert);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not init certificate - %s", gnutls_strerror(err));
        goto out;
    }

    /* The peer certificate is the first certificate in the list. */
    err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_PEM);
    if (err != GNUTLS_E_SUCCESS)
        err = gnutls_x509_crt_import(cert, certs, GNUTLS_X509_FMT_DER);
    if (err != GNUTLS_E_SUCCESS) {
        JAMI_ERR("Could not read peer certificate - %s", gnutls_strerror(err));
        goto out;
    }
    /* Finally check if the hostnames match. */
    err = gnutls_x509_crt_check_hostname(cert, host.c_str());
    if (err == 0) {
        JAMI_ERR("Hostname %s does not match certificate.", host.c_str());
        goto out;
    }

    /* Try sending and receiving some data through. */
    snprintf(buf, sizeof(buf), "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", host.c_str());
    err = gnutls_record_send(session, buf, strlen(buf));
    if (err < 0) {
        JAMI_ERR("Send failed - %s", gnutls_strerror(err));
        goto out;
    }
    err = gnutls_record_recv(session, buf, sizeof(buf));
    if (err < 0) {
        JAMI_ERR("Recv failed - %s", gnutls_strerror(err));
        goto out;
    }

    JAMI_DBG("Hostname %s seems to point to a valid server.", host.c_str());
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
#endif

/**
 * Check if the Validator have access to a private key
 */
TlsValidator::CheckResult
TlsValidator::hasPrivateKey()
{
    if (privateKeyFound_)
        return TlsValidator::CheckResult(CheckValues::PASSED, "");

    try {
        dht::crypto::PrivateKey key_tmp(certificateContent_);
    } catch (const std::exception& e) {
        return CheckResult(CheckValues::FAILED, e.what());
    }

    JAMI_DBG("Key from %s seems valid.", certificatePath_.c_str());
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
TlsValidator::CheckResult
TlsValidator::notExpired()
{
    if (exist().first == CheckValues::FAILED)
        TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    // time_t expirationTime = gnutls_x509_crt_get_expiration_time(cert);
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_EXPIRED ? CheckValues::FAILED
                                                                         : CheckValues::PASSED,
                                     "");
}

/**
 * If the activation value is in the past
 *
 * @fixme Handle both "with ca" and "without ca" case
 */
TlsValidator::CheckResult
TlsValidator::activated()
{
    if (exist().first == CheckValues::FAILED)
        TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    // time_t activationTime = gnutls_x509_crt_get_activation_time(cert);
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_NOT_ACTIVATED
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * If the algorithm used to sign the certificate is considered weak by modern
 * standard
 */
TlsValidator::CheckResult
TlsValidator::strongSigning()
{
    if (exist().first == CheckValues::FAILED)
        TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    // Doesn't seem to have the same value as
    // certtool  --infile /home/etudiant/Téléchargements/mynsauser.pem --key-inf
    // TODO figure out why
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_INSECURE_ALGORITHM
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * The certificate is not self signed
 */
TlsValidator::CheckResult
TlsValidator::notSelfSigned()
{
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * The provided key can be used along with the certificate
 */
TlsValidator::CheckResult
TlsValidator::keyMatch()
{
    if (exist().first == CheckValues::FAILED)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    if (not privateKeyFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
    return TlsValidator::CheckResult(privateKeyMatch_ ? CheckValues::PASSED : CheckValues::FAILED,
                                     "");
}

TlsValidator::CheckResult
TlsValidator::privateKeyStoragePermissions()
{
    struct stat statbuf;
    int err = stat(privateKeyPath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

// clang-format off
    return TlsValidator::CheckResult(
        (statbuf.st_mode & S_IFREG) && /* Regular file only */
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ( (statbuf.st_mode & S_IRUSR) /* write is not relevant */     && !(statbuf.st_mode & S_IXUSR))
        /* Group */ && (!(statbuf.st_mode & S_IRGRP) && !(statbuf.st_mode & S_IWGRP) && !(statbuf.st_mode & S_IXGRP))
        /* Other */ && (!(statbuf.st_mode & S_IROTH) && !(statbuf.st_mode & S_IWOTH) && !(statbuf.st_mode & S_IXOTH))
        ? CheckValues::PASSED:CheckValues::FAILED, "");
// clang-format on
}

TlsValidator::CheckResult
TlsValidator::publicKeyStoragePermissions()
{
    struct stat statbuf;
    int err = stat(certificatePath_.c_str(), &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

// clang-format off
    return TlsValidator::CheckResult(
        (statbuf.st_mode & S_IFREG) && /* Regular file only */
        /*                          READ                      WRITE                            EXECUTE          */
        /* Owner */    ( (statbuf.st_mode & S_IRUSR) /* write is not relevant */   && !(statbuf.st_mode & S_IXUSR))
        /* Group */ && ( /* read is not relevant */   !(statbuf.st_mode & S_IWGRP) && !(statbuf.st_mode & S_IXGRP))
        /* Other */ && ( /* read is not relevant */   !(statbuf.st_mode & S_IWOTH) && !(statbuf.st_mode & S_IXOTH))
        ? CheckValues::PASSED:CheckValues::FAILED, "");
// clang-format on
}

TlsValidator::CheckResult
TlsValidator::privateKeyDirectoryPermissions()
{
    if (privateKeyPath_.empty())
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

#ifndef _MSC_VER
    auto path = std::unique_ptr<char, decltype(free)&>(strdup(privateKeyPath_.c_str()), free);
    const char* dir = dirname(path.get());
#else
    char* dir;
    _splitpath(certificatePath_.c_str(), nullptr, dir, nullptr, nullptr);
#endif

    struct stat statbuf;
    int err = stat(dir, &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(
        /*                          READ                      WRITE EXECUTE             */
        /* Owner */ (
            (statbuf.st_mode & S_IRUSR) /* write is not relevant */ && (statbuf.st_mode & S_IXUSR))
                /* Group */
                && (!(statbuf.st_mode & S_IRGRP) && !(statbuf.st_mode & S_IWGRP)
                    && !(statbuf.st_mode & S_IXGRP))
                /* Other */
                && (!(statbuf.st_mode & S_IROTH) && !(statbuf.st_mode & S_IWOTH)
                    && !(statbuf.st_mode & S_IXOTH))
                && S_ISDIR(statbuf.st_mode)
            ? CheckValues::PASSED
            : CheckValues::FAILED,
        "");
}

TlsValidator::CheckResult
TlsValidator::publicKeyDirectoryPermissions()
{
#ifndef _MSC_VER
    auto path = std::unique_ptr<char, decltype(free)&>(strdup(certificatePath_.c_str()), free);
    const char* dir = dirname(path.get());
#else
    char* dir;
    _splitpath(certificatePath_.c_str(), nullptr, dir, nullptr, nullptr);
#endif

    struct stat statbuf;
    int err = stat(dir, &statbuf);
    if (err)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    return TlsValidator::CheckResult(
        /*                          READ                      WRITE EXECUTE             */
        /* Owner */ (
            (statbuf.st_mode & S_IRUSR) /* write is not relevant */ && (statbuf.st_mode & S_IXUSR))
                /* Group */
                && (!(statbuf.st_mode & S_IRGRP) && !(statbuf.st_mode & S_IWGRP)
                    && !(statbuf.st_mode & S_IXGRP))
                /* Other */
                && (!(statbuf.st_mode & S_IROTH) && !(statbuf.st_mode & S_IWOTH)
                    && !(statbuf.st_mode & S_IXOTH))
                && S_ISDIR(statbuf.st_mode)
            ? CheckValues::PASSED
            : CheckValues::FAILED,
        "");
}

/**
 * Certificate should be located in specific path on some operating systems
 */
TlsValidator::CheckResult
TlsValidator::privateKeyStorageLocation()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * Certificate should be located in specific path on some operating systems
 */
TlsValidator::CheckResult
TlsValidator::publicKeyStorageLocation()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * SELinux provide additional key protection mechanism
 */
TlsValidator::CheckResult
TlsValidator::privateKeySelinuxAttributes()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * SELinux provide additional key protection mechanism
 */
TlsValidator::CheckResult
TlsValidator::publicKeySelinuxAttributes()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
}

/**
 * If the key need decryption
 *
 * Double factor authentication is recommended
 */
TlsValidator::CheckResult
TlsValidator::requirePrivateKeyPassword()
{
    return TlsValidator::CheckResult(privateKeyPassword_ ? CheckValues::PASSED : CheckValues::FAILED,
                                     "");
}
/**
 * The CA and certificate provide conflicting ownership information
 */
TlsValidator::CheckResult
TlsValidator::expectedOwner()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_UNEXPECTED_OWNER
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * The file has been found
 */
TlsValidator::CheckResult
TlsValidator::exist()
{
    return TlsValidator::CheckResult((certificateFound_ or certificateFileFound_)
                                         ? CheckValues::PASSED
                                         : CheckValues::FAILED,
                                     "");
}

/**
 * The certificate is invalid compared to the authority
 *
 * @todo Handle case when there is facultative authority, such as DHT
 */
TlsValidator::CheckResult
TlsValidator::valid()
{
    return TlsValidator::CheckResult(certificateFound_ ? CheckValues::PASSED : CheckValues::FAILED,
                                     "");
}

/**
 * The provided authority is invalid
 */
TlsValidator::CheckResult
TlsValidator::validAuthority()
{
    // TODO Merge with either above or bellow
    return TlsValidator::CheckResult((compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND)
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * Check if the authority match the certificate
 */
TlsValidator::CheckResult
TlsValidator::authorityMatch()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_CA
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * When an account require an authority known by the system (like /usr/share/ssl/certs)
 * then the whole chain of trust need be to checked
 *
 * @fixme port crypto_cert_load_trusted
 * @fixme add account settings
 * @todo implement the check
 */
TlsValidator::CheckResult
TlsValidator::knownAuthority()
{
    // TODO need a new boolean account setting "require trusted authority" or something defaulting
    // to true using GNUTLS_CERT_SIGNER_NOT_FOUND is a temporary placeholder as it is close enough
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * Check if the certificate has been revoked
 */
TlsValidator::CheckResult
TlsValidator::notRevoked()
{
    return TlsValidator::CheckResult((compareToCa() & GNUTLS_CERT_REVOKED)
                                             || (compareToCa()
                                                 & GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE)
                                         ? CheckValues::FAILED
                                         : CheckValues::PASSED,
                                     "");
}

/**
 * A certificate authority has been provided
 */
bool
TlsValidator::hasCa() const
{
    return (x509crt_ and x509crt_->issuer); /* or
            (caCert_ != nullptr and caCert_->certificateFound_);*/
}

//
// Certificate details
//

// TODO gnutls_x509_crl_get_this_update

/**
 * An hexadecimal representation of the signature
 */
TlsValidator::CheckResult
TlsValidator::getPublicSignature()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_signature(x509crt_->cert, copy_buffer, &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * Return the certificate version
 */
TlsValidator::CheckResult
TlsValidator::getVersionNumber()
{
    int version = gnutls_x509_crt_get_version(x509crt_->cert);
    if (version < 0)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    std::ostringstream convert;
    convert << version;

    return TlsValidator::CheckResult(CheckValues::NUMBER, convert.str());
}

/**
 * Return the certificate serial number
 */
TlsValidator::CheckResult
TlsValidator::getSerialNumber()
{
    // gnutls_x509_crl_iter_crt_serial
    // gnutls_x509_crt_get_authority_key_gn_serial
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_serial(x509crt_->cert, copy_buffer, &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * If the certificate is not self signed, return the issuer
 */
TlsValidator::CheckResult
TlsValidator::getIssuer()
{
    if (not x509crt_->issuer) {
        auto icrt = CertificateStore::instance().findIssuer(x509crt_);
        if (icrt)
            return TlsValidator::CheckResult(CheckValues::CUSTOM, icrt->getId().toString());
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");
    }
    return TlsValidator::CheckResult(CheckValues::CUSTOM, x509crt_->issuer->getId().toString());
}

/**
 * The algorithm used to sign the certificate details (rather than the certificate itself)
 */
TlsValidator::CheckResult
TlsValidator::getSubjectKeyAlgorithm()
{
    gnutls_pk_algorithm_t algo = (gnutls_pk_algorithm_t)
        gnutls_x509_crt_get_pk_algorithm(x509crt_->cert, nullptr);

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
TlsValidator::CheckResult
TlsValidator::getCN()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn_by_oid(x509crt_->cert,
                                            GNUTLS_OID_X520_COMMON_NAME,
                                            0,
                                            0,
                                            copy_buffer,
                                            &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * The 'N' section of a DN (RFC4514)
 */
TlsValidator::CheckResult
TlsValidator::getN()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn_by_oid(x509crt_->cert,
                                            GNUTLS_OID_X520_NAME,
                                            0,
                                            0,
                                            copy_buffer,
                                            &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * The 'O' section of a DN (RFC4514)
 */
TlsValidator::CheckResult
TlsValidator::getO()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn_by_oid(x509crt_->cert,
                                            GNUTLS_OID_X520_ORGANIZATION_NAME,
                                            0,
                                            0,
                                            copy_buffer,
                                            &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * Return the algorithm used to sign the Key
 *
 * For example: RSA
 */
TlsValidator::CheckResult
TlsValidator::getSignatureAlgorithm()
{
    gnutls_sign_algorithm_t algo = (gnutls_sign_algorithm_t) gnutls_x509_crt_get_signature_algorithm(
        x509crt_->cert);

    if (algo < 0)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    const char* algoName = gnutls_sign_get_name(algo);
    return TlsValidator::CheckResult(CheckValues::CUSTOM, algoName);
}

/**
 *Compute the key fingerprint
 *
 * This need to be used along with getSha1Fingerprint() to avoid collisions
 */
TlsValidator::CheckResult
TlsValidator::getMd5Fingerprint()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_fingerprint(x509crt_->cert,
                                              GNUTLS_DIG_MD5,
                                              copy_buffer,
                                              &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * Compute the key fingerprint
 *
 * This need to be used along with getMd5Fingerprint() to avoid collisions
 */
TlsValidator::CheckResult
TlsValidator::getSha1Fingerprint()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_fingerprint(x509crt_->cert,
                                              GNUTLS_DIG_SHA1,
                                              copy_buffer,
                                              &resultSize);
    return checkBinaryError(err, copy_buffer, resultSize);
}

/**
 * Return an hexadecimal identifier
 */
TlsValidator::CheckResult
TlsValidator::getPublicKeyId()
{
    static unsigned char unsigned_copy_buffer[4096];
    size_t resultSize = sizeof(unsigned_copy_buffer);
    int err = gnutls_x509_crt_get_key_id(x509crt_->cert, 0, unsigned_copy_buffer, &resultSize);

    // TODO check for GNUTLS_E_SHORT_MEMORY_BUFFER and increase the buffer size
    // TODO get rid of the cast, display a HEX or something, need research

    return checkBinaryError(err, (char*) unsigned_copy_buffer, resultSize);
}
// gnutls_x509_crt_get_authority_key_id

/**
 *  If the certificate is not self signed, return the issuer DN (RFC4514)
 */
TlsValidator::CheckResult
TlsValidator::getIssuerDN()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_dn(x509crt_->cert, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * Get the expiration date
 *
 * @todo Move to "certificateDetails()" method once completed
 */
TlsValidator::CheckResult
TlsValidator::getExpirationDate()
{
    if (not certificateFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    time_t expiration = gnutls_x509_crt_get_expiration_time(x509crt_->cert);

    return formatDate(expiration);
}

/**
 * Get the activation date
 *
 * @todo Move to "certificateDetails()" method once completed
 */
TlsValidator::CheckResult
TlsValidator::getActivationDate()
{
    if (not certificateFound_)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    time_t expiration = gnutls_x509_crt_get_activation_time(x509crt_->cert);

    return formatDate(expiration);
}

/**
 * The expected outgoing server domain
 *
 * @todo Move to "certificateDetails()" method once completed
 * @todo extract information for the certificate
 */
TlsValidator::CheckResult
TlsValidator::outgoingServer()
{
    // TODO
    return TlsValidator::CheckResult(CheckValues::CUSTOM, "");
}

/**
 * If the certificate is not self signed, return the issuer
 */
TlsValidator::CheckResult
TlsValidator::isCA()
{
    return TlsValidator::CheckResult(CheckValues::CUSTOM, x509crt_->isCA() ? TRUE_STR : FALSE_STR);
}

} // namespace tls
} // namespace jami
