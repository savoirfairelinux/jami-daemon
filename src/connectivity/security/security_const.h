/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Proulx <philippe.proulx@savoirfairelinux.com>
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
#ifndef LIBJAMI_SECURITY_H
#define LIBJAMI_SECURITY_H

#include "def.h"

namespace libjami {

namespace Certificate {

namespace Status {
constexpr static char UNDEFINED[] = "UNDEFINED";
constexpr static char ALLOWED[] = "ALLOWED";
constexpr static char BANNED[] = "BANNED";
} // namespace Status

namespace TrustStatus {
constexpr static char UNTRUSTED[] = "UNTRUSTED";
constexpr static char TRUSTED[] = "TRUSTED";
} // namespace TrustStatus

/**
 * Those constantes are used by the ConfigurationManager.validateCertificate method
 */
namespace ChecksNames {
constexpr static char HAS_PRIVATE_KEY[] = "HAS_PRIVATE_KEY";
constexpr static char EXPIRED[] = "EXPIRED";
constexpr static char STRONG_SIGNING[] = "STRONG_SIGNING";
constexpr static char NOT_SELF_SIGNED[] = "NOT_SELF_SIGNED";
constexpr static char KEY_MATCH[] = "KEY_MATCH";
constexpr static char PRIVATE_KEY_STORAGE_PERMISSION[] = "PRIVATE_KEY_STORAGE_PERMISSION";
constexpr static char PUBLIC_KEY_STORAGE_PERMISSION[] = "PUBLIC_KEY_STORAGE_PERMISSION";
constexpr static char PRIVATE_KEY_DIRECTORY_PERMISSIONS[] = "PRIVATEKEY_DIRECTORY_PERMISSIONS";
constexpr static char PUBLIC_KEY_DIRECTORY_PERMISSIONS[] = "PUBLICKEY_DIRECTORY_PERMISSIONS";
constexpr static char PRIVATE_KEY_STORAGE_LOCATION[] = "PRIVATE_KEY_STORAGE_LOCATION";
constexpr static char PUBLIC_KEY_STORAGE_LOCATION[] = "PUBLIC_KEY_STORAGE_LOCATION";
constexpr static char PRIVATE_KEY_SELINUX_ATTRIBUTES[] = "PRIVATE_KEY_SELINUX_ATTRIBUTES";
constexpr static char PUBLIC_KEY_SELINUX_ATTRIBUTES[] = "PUBLIC_KEY_SELINUX_ATTRIBUTES";
constexpr static char EXIST[] = "EXIST";
constexpr static char VALID[] = "VALID";
constexpr static char VALID_AUTHORITY[] = "VALID_AUTHORITY";
constexpr static char KNOWN_AUTHORITY[] = "KNOWN_AUTHORITY";
constexpr static char NOT_REVOKED[] = "NOT_REVOKED";
constexpr static char AUTHORITY_MISMATCH[] = "AUTHORITY_MISMATCH";
constexpr static char UNEXPECTED_OWNER[] = "UNEXPECTED_OWNER";
constexpr static char NOT_ACTIVATED[] = "NOT_ACTIVATED";
} // namespace ChecksNames

/**
 * Those constants are used by the ConfigurationManager.getCertificateDetails method
 */
namespace DetailsNames {
constexpr static char EXPIRATION_DATE[] = "EXPIRATION_DATE";
constexpr static char ACTIVATION_DATE[] = "ACTIVATION_DATE";
constexpr static char REQUIRE_PRIVATE_KEY_PASSWORD[] = "REQUIRE_PRIVATE_KEY_PASSWORD";
constexpr static char PUBLIC_SIGNATURE[] = "PUBLIC_SIGNATURE";
constexpr static char VERSION_NUMBER[] = "VERSION_NUMBER";
constexpr static char SERIAL_NUMBER[] = "SERIAL_NUMBER";
constexpr static char ISSUER[] = "ISSUER";
constexpr static char SUBJECT_KEY_ALGORITHM[] = "SUBJECT_KEY_ALGORITHM";
constexpr static char CN[] = "CN";
constexpr static char N[] = "N";
constexpr static char O[] = "O";
constexpr static char SIGNATURE_ALGORITHM[] = "SIGNATURE_ALGORITHM";
constexpr static char MD5_FINGERPRINT[] = "MD5_FINGERPRINT";
constexpr static char SHA1_FINGERPRINT[] = "SHA1_FINGERPRINT";
constexpr static char PUBLIC_KEY_ID[] = "PUBLIC_KEY_ID";
constexpr static char ISSUER_DN[] = "ISSUER_DN";
constexpr static char NEXT_EXPECTED_UPDATE_DATE[] = "NEXT_EXPECTED_UPDATE_DATE";
constexpr static char OUTGOING_SERVER[] = "OUTGOING_SERVER";
constexpr static char IS_CA[] = "IS_CA";
} // namespace DetailsNames

/**
 * Those constants are used by the ConfigurationManager.getCertificateDetails and
 * ConfigurationManager.validateCertificate methods
 */
namespace ChecksValuesTypesNames {
constexpr static char BOOLEAN[] = "BOOLEAN";
constexpr static char ISO_DATE[] = "ISO_DATE";
constexpr static char CUSTOM[] = "CUSTOM";
constexpr static char NUMBER[] = "NUMBER";
} // namespace ChecksValuesTypesNames

/**
 * Those constantes are used by the ConfigurationManager.validateCertificate method
 */
namespace CheckValuesNames {
constexpr static char PASSED[] = "PASSED";
constexpr static char FAILED[] = "FAILED";
constexpr static char UNSUPPORTED[] = "UNSUPPORTED";
constexpr static char ISO_DATE[] = "ISO_DATE";
constexpr static char CUSTOM[] = "CUSTOM";
constexpr static char DATE[] = "DATE";
} // namespace CheckValuesNames

} // namespace Certificate

namespace TlsTransport {
constexpr static char TLS_PEER_CERT[] = "TLS_PEER_CERT";
constexpr static char TLS_PEER_CA_NUM[] = "TLS_PEER_CA_NUM";
constexpr static char TLS_PEER_CA_[] = "TLS_PEER_CA_";
constexpr static char TLS_CIPHER[] = "TLS_CIPHER";
} // namespace TlsTransport

} // namespace libjami

#endif
