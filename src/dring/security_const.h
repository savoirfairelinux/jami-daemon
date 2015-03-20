/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
#ifndef DRING_SECURITY_H
#define DRING_SECURITY_H

namespace DRing {

namespace Certificate {

/**
* Those constantes are used by the ConfigurationManager.validateCertificate method
*/
namespace ChecksNames {
    constexpr static char HAS_PRIVATE_KEY                  [] = "HAS_PRIVATE_KEY"                 ;
    constexpr static char EXPIRED                          [] = "EXPIRED"                         ;
    constexpr static char STRONG_SIGNING                   [] = "STRONG_SIGNING"                  ;
    constexpr static char NOT_SELF_SIGNED                  [] = "NOT_SELF_SIGNED"                 ;
    constexpr static char KEY_MATCH                        [] = "KEY_MATCH"                       ;
    constexpr static char PRIVATE_KEY_STORAGE_PERMISSION   [] = "PRIVATE_KEY_STORAGE_PERMISSION"  ;
    constexpr static char PUBLIC_KEY_STORAGE_PERMISSION    [] = "PUBLIC_KEY_STORAGE_PERMISSION"   ;
    constexpr static char PRIVATE_KEY_DIRECTORY_PERMISSIONS[] = "PRIVATEKEY_DIRECTORY_PERMISSIONS";
    constexpr static char PUBLIC_KEY_DIRECTORY_PERMISSIONS [] = "PUBLICKEY_DIRECTORY_PERMISSIONS" ;
    constexpr static char PRIVATE_KEY_STORAGE_LOCATION     [] = "PRIVATE_KEY_STORAGE_LOCATION"    ;
    constexpr static char PUBLIC_KEY_STORAGE_LOCATION      [] = "PUBLIC_KEY_STORAGE_LOCATION"     ;
    constexpr static char PRIVATE_KEY_SELINUX_ATTRIBUTES   [] = "PRIVATE_KEY_SELINUX_ATTRIBUTES"  ;
    constexpr static char PUBLIC_KEY_SELINUX_ATTRIBUTES    [] = "PUBLIC_KEY_SELINUX_ATTRIBUTES"   ;
    constexpr static char EXIST                            [] = "EXIST"                           ;
    constexpr static char VALID                            [] = "VALID"                           ;
    constexpr static char VALID_AUTHORITY                  [] = "VALID_AUTHORITY"                 ;
    constexpr static char KNOWN_AUTHORITY                  [] = "KNOWN_AUTHORITY"                 ;
    constexpr static char NOT_REVOKED                      [] = "NOT_REVOKED"                     ;
    constexpr static char AUTHORITY_MISMATCH               [] = "AUTHORITY_MISMATCH"              ;
    constexpr static char UNEXPECTED_OWNER                 [] = "UNEXPECTED_OWNER"                ;
    constexpr static char NOT_ACTIVATED                    [] = "NOT_ACTIVATED"                   ;
} //namespace DRing::Certificate::CheckValuesNames

/**
* Those constants are used by the ConfigurationManager.getCertificateDetails method
*/
namespace DetailsNames {
    constexpr static char EXPIRATION_DATE             [] = "EXPIRATION_DATE"              ;
    constexpr static char ACTIVATION_DATE             [] = "ACTIVATION_DATE"              ;
    constexpr static char REQUIRE_PRIVATE_KEY_PASSWORD[] = "REQUIRE_PRIVATE_KEY_PASSWORD" ;
    constexpr static char PUBLIC_SIGNATURE            [] = "PUBLIC_SIGNATURE"             ;
    constexpr static char VERSION_NUMBER              [] = "VERSION_NUMBER"               ;
    constexpr static char SERIAL_NUMBER               [] = "SERIAL_NUMBER"                ;
    constexpr static char ISSUER                      [] = "ISSUER"                       ;
    constexpr static char SUBJECT_KEY_ALGORITHM       [] = "SUBJECT_KEY_ALGORITHM"        ;
    constexpr static char CN                          [] = "CN"                           ;
    constexpr static char N                           [] = "N"                            ;
    constexpr static char O                           [] = "O"                            ;
    constexpr static char SIGNATURE_ALGORITHM         [] = "SIGNATURE_ALGORITHM"          ;
    constexpr static char MD5_FINGERPRINT             [] = "MD5_FINGERPRINT"              ;
    constexpr static char SHA1_FINGERPRINT            [] = "SHA1_FINGERPRINT"             ;
    constexpr static char PUBLIC_KEY_ID               [] = "PUBLIC_KEY_ID"                ;
    constexpr static char ISSUER_DN                   [] = "ISSUER_DN"                    ;
    constexpr static char NEXT_EXPECTED_UPDATE_DATE   [] = "NEXT_EXPECTED_UPDATE_DATE"    ;
    constexpr static char OUTGOING_SERVER             [] = "OUTGOING_SERVER"              ;
} //namespace DRing::Certificate::CheckValuesNames

/**
* Those constants are used by the ConfigurationManager.getCertificateDetails and
* ConfigurationManager.validateCertificate methods
*/
namespace ChecksValuesTypesNames {
    constexpr static char BOOLEAN [] = "BOOLEAN"  ;
    constexpr static char ISO_DATE[] = "ISO_DATE" ;
    constexpr static char CUSTOM  [] = "CUSTOM"   ;
    constexpr static char NUMBER  [] = "NUMBER"   ;
} //namespace DRing::Certificate::CheckValuesNames

/**
* Those constantes are used by the ConfigurationManager.validateCertificate method
*/
namespace CheckValuesNames {
    constexpr static char PASSED     [] = "PASSED"     ;
    constexpr static char FAILED     [] = "FAILED"     ;
    constexpr static char UNSUPPORTED[] = "UNSUPPORTED";
    constexpr static char ISO_DATE   [] = "ISO_DATE"   ;
    constexpr static char CUSTOM     [] = "CUSTOM"     ;
    constexpr static char DATE       [] = "DATE"       ;
} //namespace DRing::Certificate::CheckValuesNames

} //namespace DRing::Certificate

} //namespace DRing

#endif
