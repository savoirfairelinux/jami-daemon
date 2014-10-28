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
#ifndef SECURITY_EVALUATOR_H
#define SECURITY_EVALUATOR_H

#include <stdint.h>
#include <string>
#include <functional>

//SFLphone
#include "enumclass_utils.h"

// Gnutls
#include <gnutls/x509.h>


class TlsValidator {

public:
    /**
     * @enum CertificateChecks All validation fields
     *
     */
    enum class CertificateChecks {
        EMBED_PRIVATE_KEY              , /** This certificate has a build in private key                          */
        CERTIFICATE_EXPIRED            , /** This certificate is past its expiration date                         */
        WEAK_SIGNING                   , /** This certificate has been signed with a brute-force-able method      */
        CERTIFICATE_SELF_SIGNED        , /** This certificate has been self signed                                */
        PRIVATE_KEY_MISSING            , /** No private key is available for this certificate                     */
        CERTIFICATE_KEY_MISMATCH       , /** The public and private keys provided don't match                     */
        PRIVATE_KEY_STORAGE_PERMISSION , /** The file hosting the private key isn't correctly secured             */
        PUBLIC_KEY_STORAGE_PERMISSION  , /** The file hosting the public key isn't correctly secured              */
        PRIVATE_KEY_FOLDER_PERMISSION  , /** The folder storing the private key isn't correctly secured           */
        PUBLIC_KEY_FOLDER_PERMISSION   , /** The folder storing the public key isn't correctly secured            */
        PRIVATE_KEY_STORAGE_LOCATION   , /** Some operating systems have extra policies for certificate storage   */
        PUBLIC_KEY_STORAGE_LOCATION    , /** Some operating systems have extra policies for certificate storage   */
        PRIVATE_KEY_SELINUX_ATTRIBUTES , /** Some operating systems require keys to have extra attributes         */
        PUBLIC_KEY_SELINUX_ATTRIBUTES  , /** Some operating systems require keys to have extra attributes         */
        REQUIRE_PRIVATE_KEY_PASSWORD   , /** Does the private key require a password                              */
        OUTGOING_SERVER                , /** The hostname/outgoing server used for this certificate               */
        MISSING_CERTIFICATE            , /** The certificate file doesn't exist or is not accessible              */
        INVALID_CERTIFICATE            , /** The file is not a certificate                                        */
        INVALID_AUTHORITY              , /** The claimed authority did not sign the certificate                   */
        UNKOWN_AUTHORITY               , /** Some operating systems provide a list of trusted authorities, use it */
        REVOKED                        , /** The certificate has been revoked by the authority                    */
        CERTIFICATE_EXPIRATION_DATE    , /** The certificate expiration date                                      */
        CERTIFICATE_ACTIVATION_DATE    , /** The certificate activation date                                      */
        CERTIFICATE_AUTHORITY_MISMATCH , /** The certificate and authority mismatch                               */
        UNEXPECTED_OWNER               , /** The certificate has an expected owner                                */
        NOT_ACTIVATED                  , /** The certificate has not been activated yet                           */
        COUNT__,
    };

    /**
    * @enum ChecksValuesType Categories of possible values for each CertificateChecks
    */
    enum class ChecksValuesType {
        BOOLEAN,
        ISO_DATE,
        CUSTOM,
        COUNT__,
    };

    /**
     * @enum ChecksValues possible values for checks
     */
    enum class ChecksValues {
        PASSED     , /** The check has been successful (good)                      */
        FAILED     , /** The check failed (bad)                                    */
        UNSUPPORTED, /** The operating system doesn't support or require the check */
        ISO_DATE   , /** The check value is an ISO date                            */
        CUSTOM     , /** The check value is a custom string                        */
        COUNT__,
    };

    /**
     * @typedef CheckResult a pair of formal and optional custom result for a check
     */
    typedef std::pair<ChecksValues,std::string> CheckResult;

    /**
     * Create a TlsValidator for a given certificate
     * @param certificate The certificate path
     * @param privatekey An optional private key file path
     */
    TlsValidator(const std::string& certificate, const std::string& privatekey);

    ~TlsValidator();

    // Getter
    bool hasCa() const;

    bool isValid(bool verbose = false);

    // Checks
    CheckResult hasPrivateKey();
    CheckResult notExpired();
    CheckResult weakSigning();
    CheckResult notSelfSigned();
    CheckResult privateKeyExist();
    CheckResult keyMatch();
    CheckResult privateKeyStoragePermission();
    CheckResult publicKeyStoragePermission();
    CheckResult privateKeyFolderPermission();
    CheckResult publicKeyFolderPermission();
    CheckResult privateKeyStorageLocation();
    CheckResult publicKeyStorageLocation();
    CheckResult privateKeySelinuxAttributes();
    CheckResult publicKeySelinuxAttributes();
    CheckResult requirePrivateKeyPassword();
    CheckResult outgoingServer();
    CheckResult exist();
    CheckResult validCertificate();
    CheckResult invalidAuthority();
    CheckResult unkownAuthority();
    CheckResult revoked();
    CheckResult expirationDate();
    CheckResult activationDate();
    CheckResult authorityMatch();
    CheckResult expectedOwner();
    CheckResult activated();

    // Setter
    void setCaTlsValidator(const TlsValidator& validator);

    // Mutator
    std::map<std::string,std::string> serializeAll();

private:

    // Enum class names
    static const sfl::EnumClassNames< CertificateChecks > CertificateChecksNames;

    static const sfl::EnumClassNames< const ChecksValuesType > ChecksValuesTypeNames;

    static const sfl::EnumClassNames< ChecksValues > ChecksValuesNames;

    /**
     * Map checks to their getter method
     */
    static const sfl::CallbackMatrix1D< CertificateChecks, TlsValidator, CheckResult> checkCallback;

    /**
     * Valid values for each categories
     */
    static const sfl::Matrix2D< ChecksValuesType , ChecksValues , bool > acceptedChecksValuesResult;

    // Attributes
    static const sfl::Matrix1D<  CertificateChecks, ChecksValuesType > enforcedCheckType;
    std::string certificatePath;
    std::string privateKeyPath;
    std::basic_string<unsigned char> certificateContent;
    std::basic_string<unsigned char> privateKeyContent;
    gnutls_x509_crt_t x509Certificate;
    bool certificateFound;
    bool privateKeyFound;
    TlsValidator* caCert;
    bool caChecked;
    unsigned int caValidationOutput;

    /**
     * Helper method to convert a CheckResult into a std::string
     */
    std::string getStringValue(const CertificateChecks check, const CheckResult result);

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
