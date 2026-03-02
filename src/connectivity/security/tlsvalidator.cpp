/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "tlsvalidator.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <opendht/infohash.h> // for toHex
#include <dhtnet/certstore.h>

#include "fileutils.h"
#include "logger.h"
#include "security_const.h"
#include "string_utils.h"

#include <sstream>

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

#include <filesystem>

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
        /* SUBJECT_KEY                  */ &TlsValidator::getSubjectKey,
        /* CN                           */ &TlsValidator::getCN,
        /* UID                          */ &TlsValidator::getUID,
        /* N                            */ &TlsValidator::getN,
        /* O                            */ &TlsValidator::getO,
        /* SIGNATURE_ALGORITHM          */ &TlsValidator::getSignatureAlgorithm,
        /* MD5_FINGERPRINT              */ &TlsValidator::getMd5Fingerprint,
        /* SHA1_FINGERPRINT             */ &TlsValidator::getSha1Fingerprint,
        /* PUBLIC_KEY_ID                */ &TlsValidator::getPublicKeyId,
        /* ISSUER_DN                    */ &TlsValidator::getIssuerDN,
        /* ISSUER_CN                    */ &TlsValidator::getIssuerCN,
        /* ISSUER_UID                   */ &TlsValidator::getIssuerUID,
        /* ISSUER_N                     */ &TlsValidator::getIssuerN,
        /* ISSUER_O                     */ &TlsValidator::getIssuerO,
        /* NEXT_EXPECTED_UPDATE_DATE    */ &TlsValidator::getIssuerDN, // TODO
        /* OUTGOING_SERVER              */ &TlsValidator::outgoingServer,
        /* IS_CA                        */ &TlsValidator::isCA,
    }};

const Matrix1D<TlsValidator::CertificateCheck, TlsValidator::CheckValuesType> TlsValidator::enforcedCheckType = {{
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
    /* SUBJECT_KEY                  */ libjami::Certificate::DetailsNames::SUBJECT_KEY,
    /* CN                           */ libjami::Certificate::DetailsNames::CN,
    /* UID                          */ libjami::Certificate::DetailsNames::UID,
    /* N                            */ libjami::Certificate::DetailsNames::N,
    /* O                            */ libjami::Certificate::DetailsNames::O,
    /* SIGNATURE_ALGORITHM          */ libjami::Certificate::DetailsNames::SIGNATURE_ALGORITHM,
    /* MD5_FINGERPRINT              */ libjami::Certificate::DetailsNames::MD5_FINGERPRINT,
    /* SHA1_FINGERPRINT             */ libjami::Certificate::DetailsNames::SHA1_FINGERPRINT,
    /* PUBLIC_KEY_ID                */ libjami::Certificate::DetailsNames::PUBLIC_KEY_ID,
    /* ISSUER_DN                    */ libjami::Certificate::DetailsNames::ISSUER_DN,
    /* ISSUER_CN                    */ libjami::Certificate::DetailsNames::ISSUER_CN,
    /* ISSUER_UID                   */ libjami::Certificate::DetailsNames::ISSUER_UID,
    /* ISSUER_N                     */ libjami::Certificate::DetailsNames::ISSUER_N,
    /* ISSUER_O                     */ libjami::Certificate::DetailsNames::ISSUER_O,
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

const Matrix2D<TlsValidator::CheckValuesType, TlsValidator::CheckValues, bool> TlsValidator::acceptedCheckValuesResult = {
    {
        /*   Type          PASSED    FAILED   UNSUPPORTED   ISO_DATE    CUSTOM    NUMBER */
        /* BOOLEAN  */ {{true, true, true, false, false, false}},
        /* ISO_DATE */ {{false, false, true, true, false, false}},
        /* CUSTOM   */ {{false, false, true, false, true, false}},
        /* NUMBER   */ {{false, false, true, false, false, true}},
    }};

TlsValidator::TlsValidator(const dhtnet::tls::CertificateStore& certStore,
                           const std::vector<std::vector<uint8_t>>& certificateChainRaw)
    : TlsValidator(certStore,
                   std::make_shared<dht::crypto::Certificate>(certificateChainRaw.begin(), certificateChainRaw.end()))
{}

TlsValidator::TlsValidator(const dhtnet::tls::CertificateStore& certStore,
                           const std::string& certificate,
                           const std::string& privatekey,
                           const std::string& privatekeyPasswd,
                           const std::string& caList)
    : certStore_(certStore)
    , certificatePath_(certificate)
    , privateKeyPath_(privatekey)
    , caListPath_(caList)
    , certificateFound_(false)
    , copy_buffer()
{
    std::vector<uint8_t> certificate_raw;
    try {
        certificate_raw = fileutils::loadFile(certificatePath_);
        certificateFileFound_ = true;
    } catch (const std::exception& e) {
        JAMI_WARNING("Could not load certificate: {}", e.what());
    }

    if (not certificate_raw.empty()) {
        try {
            x509crt_ = std::make_shared<dht::crypto::Certificate>(certificate_raw);
            certificateContent_ = x509crt_->getPacked();
            certificateFound_ = true;
        } catch (const std::exception& e) {
            JAMI_WARNING("Could not extract certificate: {}", e.what());
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

TlsValidator::TlsValidator(const dhtnet::tls::CertificateStore& certStore, const std::vector<uint8_t>& certificate_raw)
    : certStore_(certStore)
    , copy_buffer()
{
    try {
        x509crt_ = std::make_shared<dht::crypto::Certificate>(certificate_raw);
        certificateContent_ = x509crt_->getPacked();
        certificateFound_ = true;
    } catch (const std::exception& e) {
        throw TlsValidatorException("Unable to load certificate");
    }
}

TlsValidator::TlsValidator(const dhtnet::tls::CertificateStore& certStore,
                           const std::shared_ptr<dht::crypto::Certificate>& crt)
    : certStore_(certStore)
    , certificateFound_(true)
    , copy_buffer()
{
    try {
        if (not crt)
            throw std::invalid_argument("Certificate must be set");
        x509crt_ = crt;
        certificateContent_ = x509crt_->getPacked();
    } catch (const std::exception& e) {
        throw TlsValidatorException("Unable to load certificate");
    }
}

TlsValidator::~TlsValidator() {}

/**
 * This method convert results into validated strings
 *
 * @todo The date should be validated, this is currently not an issue
 */
std::string
TlsValidator::getStringValue([[maybe_unused]] const TlsValidator::CertificateCheck check,
                             const TlsValidator::CheckResult& result)
{
    assert(acceptedCheckValuesResult[enforcedCheckType[check]][result.first]);

    switch (result.first) {
    case CheckValues::PASSED:
    case CheckValues::FAILED:
    case CheckValues::UNSUPPORTED:
        return std::string(CheckValuesNames[result.first]);
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
        return std::string(CheckValuesNames[CheckValues::FAILED]);
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
                    JAMI_WARNING("Check failed: {}", CertificateCheckNames[check]);
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
        ret[std::string(CertificateCheckNames[CertificateCheck::EXIST])] = getStringValue(CertificateCheck::EXIST,
                                                                                          exist());
    } else {
        for (const CertificateCheck check : Matrix0D<CertificateCheck>())
            ret[std::string(CertificateCheckNames[check])] = getStringValue(check, (this->*(checkCallback[check]))());
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
        for (const CertificateDetails& det : Matrix0D<CertificateDetails>()) {
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
            }
            ret[std::string(CertificateDetailsNames[det])] = val;
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
    return TlsValidator::TlsValidator::CheckResult(err == GNUTLS_E_SUCCESS ? TlsValidator::CheckValues::CUSTOM
                                                                           : TlsValidator::CheckValues::UNSUPPORTED,
                                                   err == GNUTLS_E_SUCCESS ? std::string(copy_buffer, size) : "");
}

constexpr static const int TIMETYPE_SIZE = 12;
/**
 * Convert a time_t to an ISO date string
 */
static TlsValidator::CheckResult
formatDate(const time_t time)
{
    char buffer[TIMETYPE_SIZE];
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
                                         dht::toHex(reinterpret_cast<uint8_t*>(copy_buffer), resultSize));
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

    auto root_cas = certStore_.getTrustedCertificates();
    auto err = gnutls_x509_trust_list_add_cas(trust, root_cas.data(), root_cas.size(), 0);
    if (err)
        JAMI_WARN("gnutls_x509_trust_list_add_cas failed: %s", gnutls_strerror(err));

    if (not caListPath_.empty()) {
        if (std::filesystem::is_directory(caListPath_))
            gnutls_x509_trust_list_add_trust_dir(trust, caListPath_.c_str(), nullptr, GNUTLS_X509_FMT_PEM, 0, 0);
        else
            gnutls_x509_trust_list_add_trust_file(trust, caListPath_.c_str(), nullptr, GNUTLS_X509_FMT_PEM, 0, 0);
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
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_EXPIRED ? CheckValues::FAILED : CheckValues::PASSED,
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
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_NOT_ACTIVATED ? CheckValues::FAILED
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
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_INSECURE_ALGORITHM ? CheckValues::FAILED
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
    return TlsValidator::CheckResult(privateKeyMatch_ ? CheckValues::PASSED : CheckValues::FAILED, "");
}

TlsValidator::CheckResult
TlsValidator::privateKeyStoragePermissions()
{
    struct stat statbuf {};
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
    struct stat statbuf {};
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
    namespace fs = std::filesystem;

    if (privateKeyPath_.empty())
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    fs::path dir = fs::path(privateKeyPath_).parent_path();
    if (dir.empty())
        dir = fs::path("."); // mimic dirname() behavior when no separator

    std::error_code ec;
    auto st = fs::status(dir, ec);
    if (ec)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    if (!fs::is_directory(st))
        return TlsValidator::CheckResult(CheckValues::FAILED, "");

    auto perm = st.permissions();

    bool ownerRead = (perm & fs::perms::owner_read) != fs::perms::none;
    bool ownerExec = (perm & fs::perms::owner_exec) != fs::perms::none;
    bool groupAny = (perm & (fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec))
                    != fs::perms::none;
    bool othersAny = (perm & (fs::perms::others_read | fs::perms::others_write | fs::perms::others_exec))
                     != fs::perms::none;

    bool ok = ownerRead && ownerExec && !groupAny && !othersAny;
    return TlsValidator::CheckResult(ok ? CheckValues::PASSED : CheckValues::FAILED, "");
}

TlsValidator::CheckResult
TlsValidator::publicKeyDirectoryPermissions()
{
    namespace fs = std::filesystem;

    if (certificatePath_.empty())
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    fs::path dir = fs::path(certificatePath_).parent_path();
    if (dir.empty())
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    std::error_code ec;
    auto st = fs::status(dir, ec);
    if (ec)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    if (!fs::is_directory(st))
        return TlsValidator::CheckResult(CheckValues::FAILED, "");

    auto perm = st.permissions();

    bool ownerRead = (perm & fs::perms::owner_read) != fs::perms::none;
    bool ownerExec = (perm & fs::perms::owner_exec) != fs::perms::none;
    bool groupAny = (perm & (fs::perms::group_read | fs::perms::group_write | fs::perms::group_exec))
                    != fs::perms::none;
    bool othersAny = (perm & (fs::perms::others_read | fs::perms::others_write | fs::perms::others_exec))
                     != fs::perms::none;

    bool ok = ownerRead && ownerExec && !groupAny && !othersAny;
    return TlsValidator::CheckResult(ok ? CheckValues::PASSED : CheckValues::FAILED, "");
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
    return TlsValidator::CheckResult(privateKeyPassword_ ? CheckValues::PASSED : CheckValues::FAILED, "");
}
/**
 * The CA and certificate provide conflicting ownership information
 */
TlsValidator::CheckResult
TlsValidator::expectedOwner()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_UNEXPECTED_OWNER ? CheckValues::FAILED
                                                                                  : CheckValues::PASSED,
                                     "");
}

/**
 * The file has been found
 */
TlsValidator::CheckResult
TlsValidator::exist()
{
    return TlsValidator::CheckResult((certificateFound_ or certificateFileFound_) ? CheckValues::PASSED
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
    return TlsValidator::CheckResult(certificateFound_ ? CheckValues::PASSED : CheckValues::FAILED, "");
}

/**
 * The provided authority is invalid
 */
TlsValidator::CheckResult
TlsValidator::validAuthority()
{
    // TODO Merge with either above or below
    return TlsValidator::CheckResult((compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND) ? CheckValues::FAILED
                                                                                    : CheckValues::PASSED,
                                     "");
}

/**
 * Check if the authority match the certificate
 */
TlsValidator::CheckResult
TlsValidator::authorityMatch()
{
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_CA ? CheckValues::FAILED
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
    return TlsValidator::CheckResult(compareToCa() & GNUTLS_CERT_SIGNER_NOT_FOUND ? CheckValues::FAILED
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
                                             || (compareToCa() & GNUTLS_CERT_REVOCATION_DATA_ISSUED_IN_FUTURE)
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
        auto icrt = certStore_.findIssuer(x509crt_);
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
    unsigned key_length = 0;
    gnutls_pk_algorithm_t algo = (gnutls_pk_algorithm_t) gnutls_x509_crt_get_pk_algorithm(x509crt_->cert, &key_length);

    if (algo < 0)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    const char* name = gnutls_pk_get_name(algo);

    if (!name)
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, "");

    if (key_length)
        return TlsValidator::CheckResult(CheckValues::CUSTOM, fmt::format("{} ({} bits)", name, key_length));
    else
        return TlsValidator::CheckResult(CheckValues::CUSTOM, name);
}

/**
 * The subject public key
 */
TlsValidator::CheckResult
TlsValidator::getSubjectKey()
{
    try {
        std::vector<uint8_t> data;
        x509crt_->getPublicKey().pack(data);
        return TlsValidator::CheckResult(CheckValues::CUSTOM, dht::toHex(data));
    } catch (const dht::crypto::CryptoException& e) {
        return TlsValidator::CheckResult(CheckValues::UNSUPPORTED, e.what());
    }
}

/**
 * The 'CN' section of a DN (RFC4514)
 */
TlsValidator::CheckResult
TlsValidator::getCN()
{
    // TODO split, cache
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn_by_oid(x509crt_->cert, GNUTLS_OID_X520_COMMON_NAME, 0, 0, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 * The 'UID' section of a DN (RFC4514)
 */
TlsValidator::CheckResult
TlsValidator::getUID()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_dn_by_oid(x509crt_->cert, GNUTLS_OID_LDAP_UID, 0, 0, copy_buffer, &resultSize);
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
    int err = gnutls_x509_crt_get_dn_by_oid(x509crt_->cert, GNUTLS_OID_X520_NAME, 0, 0, copy_buffer, &resultSize);
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
    gnutls_sign_algorithm_t algo = (gnutls_sign_algorithm_t) gnutls_x509_crt_get_signature_algorithm(x509crt_->cert);

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
    int err = gnutls_x509_crt_get_fingerprint(x509crt_->cert, GNUTLS_DIG_MD5, copy_buffer, &resultSize);
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
    int err = gnutls_x509_crt_get_fingerprint(x509crt_->cert, GNUTLS_DIG_SHA1, copy_buffer, &resultSize);
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

    return checkBinaryError(err, reinterpret_cast<char*>(unsigned_copy_buffer), resultSize);
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
 *  If the certificate is not self signed, return the issuer CN
 */
TlsValidator::CheckResult
TlsValidator::getIssuerCN()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_dn_by_oid(x509crt_->cert,
                                                   GNUTLS_OID_X520_COMMON_NAME,
                                                   0,
                                                   0,
                                                   copy_buffer,
                                                   &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 *  If the certificate is not self signed, return the issuer UID
 */
TlsValidator::CheckResult
TlsValidator::getIssuerUID()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_dn_by_oid(x509crt_->cert, GNUTLS_OID_LDAP_UID, 0, 0, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 *  If the certificate is not self signed, return the issuer N
 */
TlsValidator::CheckResult
TlsValidator::getIssuerN()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_dn_by_oid(x509crt_->cert, GNUTLS_OID_X520_NAME, 0, 0, copy_buffer, &resultSize);
    return checkError(err, copy_buffer, resultSize);
}

/**
 *  If the certificate is not self signed, return the issuer O
 */
TlsValidator::CheckResult
TlsValidator::getIssuerO()
{
    size_t resultSize = sizeof(copy_buffer);
    int err = gnutls_x509_crt_get_issuer_dn_by_oid(x509crt_->cert,
                                                   GNUTLS_OID_X520_ORGANIZATION_NAME,
                                                   0,
                                                   0,
                                                   copy_buffer,
                                                   &resultSize);
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
