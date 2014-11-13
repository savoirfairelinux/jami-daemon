/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
 *          Vittorio Giovara <vittorio.giovara@savoirfairelinux.com>
 *          Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
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

#include <cstdint>
#include <string>
#include "enumclass_utils.h"
#include <gnutls/x509.h>

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
        OUTGOING_SERVER                   , /** The hostname/outgoing server used for this certificate               */
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
     */
    TlsValidator(const std::string& certificate, const std::string& privatekey);

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
    CheckResult outgoingServer();
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

    void setCaTlsValidator(const TlsValidator& validator);

    std::map<std::string,std::string> getSerializedChecks();

    std::map<std::string,std::string> getSerializedDetails();

private:

    // Enum class names
    static const sfl::EnumClassNames<CertificateCheck> CertificateCheckNames;

    static const sfl::EnumClassNames<CertificateDetails> CertificateDetailsNames;

    static const sfl::EnumClassNames<const CheckValuesType> CheckValuesTypeNames;

    static const sfl::EnumClassNames<CheckValues> CheckValuesNames;

    /**
     * Map check to their check method
     */
    static const sfl::CallbackMatrix1D<CertificateCheck, TlsValidator, CheckResult> checkCallback;

    /**
     * Map check to their getter method
     */
    static const sfl::CallbackMatrix1D<CertificateDetails, TlsValidator, CheckResult> getterCallback;

    /**
     * Valid values for each categories
     */
    static const sfl::Matrix2D<CheckValuesType , CheckValues , bool> acceptedCheckValuesResult;

    static const sfl::Matrix1D<CertificateCheck, CheckValuesType> enforcedCheckType;

    std::string certificatePath_;
    std::string privateKeyPath_;
    std::basic_string<unsigned char> certificateContent_;
    std::basic_string<unsigned char> privateKeyContent_;
    gnutls_x509_crt_t x509Certificate_;
    bool certificateFound_;
    bool privateKeyFound_;
    TlsValidator* caCert_;
    bool caChecked_;
    unsigned int caValidationOutput_;

    mutable char copy_buffer[4096];

    /**
     * Helper method to convert a CheckResult into a std::string
     */
    std::string getStringValue(const CertificateCheck check, const CheckResult result);

    // Helper
    unsigned int compareToCa();

    // TODO remove
public:
    /**
     * Check if the given .pem contains a valid private key.
     *
     * @return 0 if success, -1 otherwise
     */
    static int containsPrivateKey(const std::string& pemPath);

    /**
     * Check if the given .pem contains a valid certificate.
     *
     * @return 0 if success, -1 otherwise
     */
    static int certificateIsValid(const std::string& caPath,
                           const std::string& pemPath);

    /**
     * Verify that the local hostname points to a valid SSL server by
     * establishing a connection to it and by validating its certificate.
     *
     * @param host the DNS domain address that the certificate should feature
     * @return 0 if success, -1 otherwise
     */
    static int verifyHostnameCertificate(const std::string& host,
                                  const uint16_t port);


}; // TlsValidator

#endif
