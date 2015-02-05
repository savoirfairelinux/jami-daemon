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
#include "security_const.h"

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

namespace ring {

//Map the internal ring Enum class of the exported names

const EnumClassNames<TlsValidator::CheckValues> TlsValidator::CheckValuesNames = {{
    /* CheckValues                        Name                         */
    /* PASSED      */ DRing::Certificate::CheckValuesNames::PASSED      ,
    /* FAILED      */ DRing::Certificate::CheckValuesNames::FAILED      ,
    /* UNSUPPORTED */ DRing::Certificate::CheckValuesNames::UNSUPPORTED ,
    /* ISO_DATE    */ DRing::Certificate::CheckValuesNames::ISO_DATE    ,
    /* CUSTOM      */ DRing::Certificate::CheckValuesNames::CUSTOM      ,
    /* CUSTOM      */ DRing::Certificate::CheckValuesNames::DATE        ,
}};

const CallbackMatrix1D<TlsValidator::CertificateCheck, TlsValidator, TlsValidator::CheckResult> TlsValidator::checkCallback = {{
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
    /*EXIST                            */ &TlsValidator::exist                          ,
    /*VALID                            */ &TlsValidator::valid                          ,
    /*VALID_AUTHORITY                  */ &TlsValidator::validAuthority                 ,
    /*KNOWN_AUTHORITY                  */ &TlsValidator::knownAuthority                 ,
    /*NOT_REVOKED                      */ &TlsValidator::notRevoked                     ,
    /*AUTHORITY_MISMATCH               */ &TlsValidator::authorityMatch                 ,
    /*UNEXPECTED_OWNER                 */ &TlsValidator::expectedOwner                  ,
    /*NOT_ACTIVATED                    */ &TlsValidator::activated                      ,
}};

const CallbackMatrix1D<TlsValidator::CertificateDetails, TlsValidator, TlsValidator::CheckResult> TlsValidator::getterCallback = {{
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
    /* OUTGOING_SERVER              */  &TlsValidator::outgoingServer            ,
}};

const Matrix1D<TlsValidator::CertificateCheck, TlsValidator::CheckValuesType> TlsValidator::enforcedCheckType = {{
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
    /*EXIST                            */ CheckValuesType::BOOLEAN ,
    /*VALID                            */ CheckValuesType::BOOLEAN ,
    /*VALID_AUTHORITY                  */ CheckValuesType::BOOLEAN ,
    /*KNOWN_AUTHORITY                  */ CheckValuesType::BOOLEAN ,
    /*NOT_REVOKED                      */ CheckValuesType::BOOLEAN ,
    /*AUTHORITY_MISMATCH               */ CheckValuesType::BOOLEAN ,
    /*UNEXPECTED_OWNER                 */ CheckValuesType::BOOLEAN ,
    /*NOT_ACTIVATED                    */ CheckValuesType::BOOLEAN ,
}};

const EnumClassNames<TlsValidator::CertificateCheck> TlsValidator::CertificateCheckNames = {{
    /*      CertificateCheck                                   Name                                         */
    /*HAS_PRIVATE_KEY                  */ DRing::Certificate::ChecksNames::HAS_PRIVATE_KEY                   ,
    /*EXPIRED                          */ DRing::Certificate::ChecksNames::EXPIRED                           ,
    /*STRONG_SIGNING                   */ DRing::Certificate::ChecksNames::STRONG_SIGNING                    ,
    /*NOT_SELF_SIGNED                  */ DRing::Certificate::ChecksNames::NOT_SELF_SIGNED                   ,
    /*KEY_MATCH                        */ DRing::Certificate::ChecksNames::KEY_MATCH                         ,
    /*PRIVATE_KEY_STORAGE_PERMISSION   */ DRing::Certificate::ChecksNames::PRIVATE_KEY_STORAGE_PERMISSION    ,
    /*PUBLIC_KEY_STORAGE_PERMISSION    */ DRing::Certificate::ChecksNames::PUBLIC_KEY_STORAGE_PERMISSION     ,
    /*PRIVATEKEY_DIRECTORY_PERMISSIONS */ DRing::Certificate::ChecksNames::PRIVATE_KEY_DIRECTORY_PERMISSIONS ,
    /*PUBLICKEY_DIRECTORY_PERMISSIONS  */ DRing::Certificate::ChecksNames::PUBLIC_KEY_DIRECTORY_PERMISSIONS  ,
    /*PRIVATE_KEY_STORAGE_LOCATION     */ DRing::Certificate::ChecksNames::PRIVATE_KEY_STORAGE_LOCATION      ,
    /*PUBLIC_KEY_STORAGE_LOCATION      */ DRing::Certificate::ChecksNames::PUBLIC_KEY_STORAGE_LOCATION       ,
    /*PRIVATE_KEY_SELINUX_ATTRIBUTES   */ DRing::Certificate::ChecksNames::PRIVATE_KEY_SELINUX_ATTRIBUTES    ,
    /*PUBLIC_KEY_SELINUX_ATTRIBUTES    */ DRing::Certificate::ChecksNames::PUBLIC_KEY_SELINUX_ATTRIBUTES     ,
    /*EXIST                            */ DRing::Certificate::ChecksNames::EXIST                             ,
    /*VALID                            */ DRing::Certificate::ChecksNames::VALID                             ,
    /*VALID_AUTHORITY                  */ DRing::Certificate::ChecksNames::VALID_AUTHORITY                   ,
    /*KNOWN_AUTHORITY                  */ DRing::Certificate::ChecksNames::KNOWN_AUTHORITY                   ,
    /*NOT_REVOKED                      */ DRing::Certificate::ChecksNames::NOT_REVOKED                       ,
    /*AUTHORITY_MISMATCH               */ DRing::Certificate::ChecksNames::AUTHORITY_MISMATCH                ,
    /*UNEXPECTED_OWNER                 */ DRing::Certificate::ChecksNames::UNEXPECTED_OWNER                  ,
    /*NOT_ACTIVATED                    */ DRing::Certificate::ChecksNames::NOT_ACTIVATED                     ,
}};

const EnumClassNames<TlsValidator::CertificateDetails> TlsValidator::CertificateDetailsNames = {{
    /* EXPIRATION_DATE              */ DRing::Certificate::DetailsNames::EXPIRATION_DATE              ,
    /* ACTIVATION_DATE              */ DRing::Certificate::DetailsNames::ACTIVATION_DATE              ,
    /* REQUIRE_PRIVATE_KEY_PASSWORD */ DRing::Certificate::DetailsNames::REQUIRE_PRIVATE_KEY_PASSWORD ,
    /* PUBLIC_SIGNATURE             */ DRing::Certificate::DetailsNames::PUBLIC_SIGNATURE             ,
    /* VERSION_NUMBER               */ DRing::Certificate::DetailsNames::VERSION_NUMBER               ,
    /* SERIAL_NUMBER                */ DRing::Certificate::DetailsNames::SERIAL_NUMBER                ,
    /* ISSUER                       */ DRing::Certificate::DetailsNames::ISSUER                       ,
    /* SUBJECT_KEY_ALGORITHM        */ DRing::Certificate::DetailsNames::SUBJECT_KEY_ALGORITHM        ,
    /* CN                           */ DRing::Certificate::DetailsNames::CN                           ,
    /* N                            */ DRing::Certificate::DetailsNames::N                            ,
    /* O                            */ DRing::Certificate::DetailsNames::O                            ,
    /* SIGNATURE_ALGORITHM          */ DRing::Certificate::DetailsNames::SIGNATURE_ALGORITHM          ,
    /* MD5_FINGERPRINT              */ DRing::Certificate::DetailsNames::MD5_FINGERPRINT              ,
    /* SHA1_FINGERPRINT             */ DRing::Certificate::DetailsNames::SHA1_FINGERPRINT             ,
    /* PUBLIC_KEY_ID                */ DRing::Certificate::DetailsNames::PUBLIC_KEY_ID                ,
    /* ISSUER_DN                    */ DRing::Certificate::DetailsNames::ISSUER_DN                    ,
    /* NEXT_EXPECTED_UPDATE_DATE    */ DRing::Certificate::DetailsNames::NEXT_EXPECTED_UPDATE_DATE    ,
    /* OUTGOING_SERVER              */ DRing::Certificate::DetailsNames::OUTGOING_SERVER              ,

}};

const EnumClassNames<const TlsValidator::CheckValuesType> TlsValidator::CheckValuesTypeNames = {{
    /*   Type                            Name                          */
    /* BOOLEAN  */ DRing::Certificate::ChecksValuesTypesNames::BOOLEAN  ,
    /* ISO_DATE */ DRing::Certificate::ChecksValuesTypesNames::ISO_DATE ,
    /* CUSTOM   */ DRing::Certificate::ChecksValuesTypesNames::CUSTOM   ,
    /* NUMBER   */ DRing::Certificate::ChecksValuesTypesNames::NUMBER   ,
}};

const Matrix2D<TlsValidator::CheckValuesType , TlsValidator::CheckValues , bool> TlsValidator::acceptedCheckValuesResult = {{
    /*   Type          PASSED    FAILED   UNSUPPORTED   ISO_DATE    CUSTOM    NUMBER */
    /* BOOLEAN  */  {{  true   ,  true  ,    true     ,  false    ,  false   ,false }},
    /* ISO_DATE */  {{  false  ,  false ,    true     ,  true     ,  false  , false }},
    /* CUSTOM   */  {{  false  ,  false ,    true     ,  false    ,  true   , false }},
    /* NUMBER   */  {{  false  ,  false ,    true     ,  false    ,  false  , true  }},
}};


TlsValidator::TlsValidator(const std::string& certificate, const std::string& privatekey) :
certificatePath_(certificate), privateKeyPath_(privatekey), certificateFound_(false), caCert_(nullptr),
caChecked_(false)
{
    int err = gnutls_global_init();
    if (err != GNUTLS_E_SUCCESS)
        throw TlsValidatorException(gnutls_strerror(err));

    try {
        x509crt_ = {fileutils::loadFile(certificatePath_)};
        certificateContent_ = x509crt_.getPacked();
        certificateFound_ = true;
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
    for (const CertificateCheck check : Matrix0D<CertificateCheck>()) {
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
        for (const CertificateCheck check : Matrix0D<CertificateCheck>())
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
 * Set an authority
 */
void TlsValidator::setCaTlsValidator(const TlsValidator& validator)
{
    caChecked_ = false;
    caCert_ = (TlsValidator*)(&validator);
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


} //namespace ring
