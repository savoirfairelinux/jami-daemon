/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

#ifndef TLS_VALIDATOR_H
#define TLS_VALIDATOR_H

#include "enumclass_utils.h"

#include <string>
#include <vector>
#include <memory>

// OpenDHT
namespace dht { namespace crypto {
struct Certificate;
}} // namespace dht::crypto

namespace jami { namespace tls {

#if !defined (S_IRWXG)
#define S_IRWXG 00070
#endif /* S_IRWXG */
#if !defined (S_IRGRP)
#define S_IRGRP 00040
#endif /* S_IRGRP */
#if !defined (S_IWGRP)
#define S_IWGRP 00020
#endif /* S_IWGRP */
#if !defined (S_IXGRP)
#define S_IXGRP 00010
#endif /* S_IXGRP */
#if !defined (S_IRWXO)
#define S_IRWXO 00007
#endif /* S_IRWXO */
#if !defined (S_IROTH)
#define S_IROTH 00004
#endif /* S_IROTH */
#if !defined (S_IWOTH)
#define S_IWOTH 00002
#endif /* S_IWOTH */
#if !defined (S_IXOTH)
#define S_IXOTH 00001
#endif /* S_IXOTH */

class TlsValidatorException : public std::runtime_error {
    public:
        TlsValidatorException(const std::string& str) : std::runtime_error(str) {};
};

class TlsValidator {

public:
    /**
     * @enum CertificateCheck All validation fields
     *
     */
    enum class CertificateCheck {
        HAS_PRIVATE_KEY                   , /** This certificate has a build in private key                          */
        EXPIRED                           , /** This certificate is past its expiration date                         */
        STRONG_SIGNING                    , /** This certificate has been signed with a brute-force-able method      */
        NOT_SELF_SIGNED                   , /** This certificate has been self signed                                */
        KEY_MATCH                         , /** The public and private keys provided don't match                     */
        PRIVATE_KEY_STORAGE_PERMISSION    , /** The file hosting the private key isn't correctly secured             */
        PUBLIC_KEY_STORAGE_PERMISSION     , /** The file hosting the public key isn't correctly secured              */
        PRIVATE_KEY_DIRECTORY_PERMISSIONS , /** The folder storing the private key isn't correctly secured           */
        PUBLIC_KEY_DIRECTORY_PERMISSIONS  , /** The folder storing the public key isn't correctly secured            */
        PRIVATE_KEY_STORAGE_LOCATION      , /** Some operating systems have extra policies for certificate storage   */
        PUBLIC_KEY_STORAGE_LOCATION       , /** Some operating systems have extra policies for certificate storage   */
        PRIVATE_KEY_SELINUX_ATTRIBUTES    , /** Some operating systems require keys to have extra attributes         */
        PUBLIC_KEY_SELINUX_ATTRIBUTES     , /** Some operating systems require keys to have extra attributes         */
        EXIST                             , /** The certificate file doesn't exist or is not accessible              */
        VALID                             , /** The file is not a certificate                                        */
        VALID_AUTHORITY                   , /** The claimed authority did not sign the certificate                   */
        KNOWN_AUTHORITY                   , /** Some operating systems provide a list of trusted authorities, use it */
        NOT_REVOKED                       , /** The certificate has been revoked by the authority                    */
        AUTHORITY_MISMATCH                , /** The certificate and authority mismatch                               */
        UNEXPECTED_OWNER                  , /** The certificate has an expected owner                                */
        NOT_ACTIVATED                     , /** The certificate has not been activated yet                           */
        COUNT__,
    };

    /**
     * @enum CertificateDetails Informative fields about a certificate
     */
    enum class CertificateDetails {
        EXPIRATION_DATE                , /** The certificate expiration date                                      */
        ACTIVATION_DATE                , /** The certificate activation date                                      */
        REQUIRE_PRIVATE_KEY_PASSWORD   , /** Does the private key require a password                              */
        PUBLIC_SIGNATURE               ,
        VERSION_NUMBER                 ,
        SERIAL_NUMBER                  ,
        ISSUER                         ,
        SUBJECT_KEY_ALGORITHM          ,
        CN                             ,
        N                              ,
        O                              ,
        SIGNATURE_ALGORITHM            ,
        MD5_FINGERPRINT                ,
        SHA1_FINGERPRINT               ,
        PUBLIC_KEY_ID                  ,
        ISSUER_DN                      ,
        NEXT_EXPECTED_UPDATE_DATE      ,
        OUTGOING_SERVER                , /** The hostname/outgoing server used for this certificate               */
        IS_CA                          ,
        COUNT__
    };

    /**
    * @enum CheckValuesType Categories of possible values for each CertificateCheck
    */
    enum class CheckValuesType {
        BOOLEAN,
        ISO_DATE,
        CUSTOM,
        NUMBER,
        COUNT__,
    };

    /**
     * @enum CheckValue possible values for check
     *
     * All boolean check use PASSED when the test result is positive and
     * FAILED when it is negative. All new check need to keep this convention
     * or ::isValid() result will become unrepresentative of the real state.
     *
     * CUSTOM should be avoided when possible. This enum can be extended when
     * new validated types are required.
     */
    enum class CheckValues {
        PASSED     , /** Equivalent of a boolean "true"                                    */
        FAILED     , /** Equivalent of a boolean "false"                                   */
        UNSUPPORTED, /** The operating system doesn't support or require the check         */
        ISO_DATE   , /** The check value is an ISO 8601 date YYYY-MM-DD[TH24:MM:SS+00:00]  */
        CUSTOM     , /** The check value cannot be represented with a finite set of values */
        NUMBER     ,
        COUNT__,
    };

    /**
     * @typedef CheckResult A validated and unvalidated result pair
     *
     * The CheckValue is the most important value of the pair. The string
     * can either be the value of a CheckValues::CUSTOM result or an
     * error code (where applicable).
     */
    using CheckResult = std::pair<CheckValues, std::string>;

    /**
     * Create a TlsValidator for a given certificate
     * @param certificate The certificate path
     * @param privatekey An optional private key file path
     * @param privatekeyPasswd An optional private key password
     * @param caList An optional CA list to use for certificate validation
     */
    TlsValidator(const std::string& certificate,
                 const std::string& privatekey = "",
                 const std::string& privatekeyPasswd = "",
                 const std::string& caList = "");

    TlsValidator(const std::vector<std::vector<uint8_t>>& certificate_chain_raw);

    TlsValidator(const std::vector<uint8_t>& certificate_raw);

    TlsValidator(const std::shared_ptr<dht::crypto::Certificate>&);

    ~TlsValidator();

    bool hasCa() const;

    bool isValid(bool verbose = false);

    // Security checks
    CheckResult hasPrivateKey();
    CheckResult notExpired();
    CheckResult strongSigning();
    CheckResult notSelfSigned();
    CheckResult keyMatch();
    CheckResult privateKeyStoragePermissions();
    CheckResult publicKeyStoragePermissions();
    CheckResult privateKeyDirectoryPermissions();
    CheckResult publicKeyDirectoryPermissions();
    CheckResult privateKeyStorageLocation();
    CheckResult publicKeyStorageLocation();
    CheckResult privateKeySelinuxAttributes();
    CheckResult publicKeySelinuxAttributes();
    CheckResult exist();
    CheckResult valid();
    CheckResult validAuthority();
    CheckResult knownAuthority();
    CheckResult notRevoked();
    CheckResult authorityMatch();
    CheckResult expectedOwner();
    CheckResult activated();

    // Certificate details
    CheckResult getExpirationDate();
    CheckResult getActivationDate();
    CheckResult requirePrivateKeyPassword();
    CheckResult getPublicSignature();
    CheckResult getVersionNumber();
    CheckResult getSerialNumber();
    CheckResult getIssuer();
    CheckResult getSubjectKeyAlgorithm();
    CheckResult getCN();
    CheckResult getN();
    CheckResult getO();
    CheckResult getSignatureAlgorithm();
    CheckResult getMd5Fingerprint();
    CheckResult getSha1Fingerprint();
    CheckResult getPublicKeyId();
    CheckResult getIssuerDN();
    CheckResult outgoingServer();
    CheckResult isCA();

    void setCaTlsValidator(const TlsValidator& validator);

    std::map<std::string,std::string> getSerializedChecks();

    std::map<std::string,std::string> getSerializedDetails();

    std::shared_ptr<dht::crypto::Certificate> getCertificate() const {
        return x509crt_;
    }

private:

    // Enum class names
    static const EnumClassNames<CertificateCheck> CertificateCheckNames;

    static const EnumClassNames<CertificateDetails> CertificateDetailsNames;

    static const EnumClassNames<const CheckValuesType> CheckValuesTypeNames;

    static const EnumClassNames<CheckValues> CheckValuesNames;

    /**
     * Map check to their check method
     */
    static const CallbackMatrix1D<CertificateCheck, TlsValidator, CheckResult> checkCallback;

    /**
     * Map check to their getter method
     */
    static const CallbackMatrix1D<CertificateDetails, TlsValidator, CheckResult> getterCallback;

    /**
     * Valid values for each categories
     */
    static const Matrix2D<CheckValuesType , CheckValues , bool> acceptedCheckValuesResult;

    static const Matrix1D<CertificateCheck, CheckValuesType> enforcedCheckType;

    std::string certificatePath_;
    std::string privateKeyPath_;
    std::string caListPath_ {};

    std::vector<uint8_t> certificateContent_;

    std::shared_ptr<dht::crypto::Certificate> x509crt_;

    bool certificateFileFound_ {false};
    bool certificateFound_ {false};
    bool privateKeyFound_ {false};
    bool privateKeyPassword_ {false};
    bool privateKeyMatch_ {false};

    bool caChecked_ {false};
    unsigned int caValidationOutput_ {0}; // 0 means "no flags set", where flags are ones from gnutls_certificate_status_t

    mutable char copy_buffer[4096];

    /**
     * Helper method to convert a CheckResult into a std::string
     */
    std::string getStringValue(const CertificateCheck check, const CheckResult result);

    // Helper
    unsigned int compareToCa();

public:
#if 0 // TODO reimplement this method. do not use it as it
    /**
     * Verify that the local hostname points to a valid SSL server by
     * establishing a connection to it and by validating its certificate.
     *
     * @param host the DNS domain address that the certificate should feature
     * @return 0 if success, -1 otherwise
     */
    static int verifyHostnameCertificate(const std::string& host,
                                         const uint16_t port);
#endif


}; // TlsValidator

}} // namespace jami::tls

#endif
