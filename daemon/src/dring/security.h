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

namespace DRing {

   namespace Certificate {

      /**
       * Those constantes are used by the ConfigurationManager.validateCertificate method
       */
      namespace ChecksNames {
         constexpr static const char* HAS_PRIVATE_KEY                   = "HAS_PRIVATE_KEY"                 ;
         constexpr static const char* EXPIRED                           = "EXPIRED"                         ;
         constexpr static const char* STRONG_SIGNING                    = "STRONG_SIGNING"                  ;
         constexpr static const char* NOT_SELF_SIGNED                   = "NOT_SELF_SIGNED"                 ;
         constexpr static const char* KEY_MATCH                         = "KEY_MATCH"                       ;
         constexpr static const char* PRIVATE_KEY_STORAGE_PERMISSION    = "PRIVATE_KEY_STORAGE_PERMISSION"  ;
         constexpr static const char* PUBLIC_KEY_STORAGE_PERMISSION     = "PUBLIC_KEY_STORAGE_PERMISSION"   ;
         constexpr static const char* PRIVATE_KEY_DIRECTORY_PERMISSIONS = "PRIVATEKEY_DIRECTORY_PERMISSIONS";
         constexpr static const char* PUBLIC_KEY_DIRECTORY_PERMISSIONS  = "PUBLICKEY_DIRECTORY_PERMISSIONS" ;
         constexpr static const char* PRIVATE_KEY_STORAGE_LOCATION      = "PRIVATE_KEY_STORAGE_LOCATION"    ;
         constexpr static const char* PUBLIC_KEY_STORAGE_LOCATION       = "PUBLIC_KEY_STORAGE_LOCATION"     ;
         constexpr static const char* PRIVATE_KEY_SELINUX_ATTRIBUTES    = "PRIVATE_KEY_SELINUX_ATTRIBUTES"  ;
         constexpr static const char* PUBLIC_KEY_SELINUX_ATTRIBUTES     = "PUBLIC_KEY_SELINUX_ATTRIBUTES"   ;
         constexpr static const char* OUTGOING_SERVER                   = "OUTGOING_SERVER"                 ;
         constexpr static const char* EXIST                             = "EXIST"                           ;
         constexpr static const char* VALID                             = "VALID"                           ;
         constexpr static const char* VALID_AUTHORITY                   = "VALID_AUTHORITY"                 ;
         constexpr static const char* KNOWN_AUTHORITY                   = "KNOWN_AUTHORITY"                 ;
         constexpr static const char* NOT_REVOKED                       = "NOT_REVOKED"                     ;
         constexpr static const char* AUTHORITY_MISMATCH                = "AUTHORITY_MISMATCH"              ;
         constexpr static const char* UNEXPECTED_OWNER                  = "UNEXPECTED_OWNER"                ;
         constexpr static const char* NOT_ACTIVATED                     = "NOT_ACTIVATED"                   ;
      };

      /**
       * Those constants are used by the ConfigurationManager.getCertificateDetails method
       */
      namespace DetailsNames {
         constexpr static const char* EXPIRATION_DATE              = "EXPIRATION_DATE"              ;
         constexpr static const char* ACTIVATION_DATE              = "ACTIVATION_DATE"              ;
         constexpr static const char* REQUIRE_PRIVATE_KEY_PASSWORD = "REQUIRE_PRIVATE_KEY_PASSWORD" ;
         constexpr static const char* PUBLIC_SIGNATURE             = "PUBLIC_SIGNATURE"             ;
         constexpr static const char* VERSION_NUMBER               = "VERSION_NUMBER"               ;
         constexpr static const char* SERIAL_NUMBER                = "SERIAL_NUMBER"                ;
         constexpr static const char* ISSUER                       = "ISSUER"                       ;
         constexpr static const char* SUBJECT_KEY_ALGORITHM        = "SUBJECT_KEY_ALGORITHM"        ;
         constexpr static const char* CN                           = "CN"                           ;
         constexpr static const char* N                            = "N"                            ;
         constexpr static const char* O                            = "O"                            ;
         constexpr static const char* SIGNATURE_ALGORITHM          = "SIGNATURE_ALGORITHM"          ;
         constexpr static const char* MD5_FINGERPRINT              = "MD5_FINGERPRINT"              ;
         constexpr static const char* SHA1_FINGERPRINT             = "SHA1_FINGERPRINT"             ;
         constexpr static const char* PUBLIC_KEY_ID                = "PUBLIC_KEY_ID"                ;
         constexpr static const char* ISSUER_DN                    = "ISSUER_DN"                    ;
         constexpr static const char* NEXT_EXPECTED_UPDATE_DATE    = "NEXT_EXPECTED_UPDATE_DATE"    ;
      };

      /**
       * Those constants are used by the ConfigurationManager.getCertificateDetails and
       * ConfigurationManager.validateCertificate methods
       */
      namespace ChecksValuesTypesNames {
         constexpr static const char* BOOLEAN  = "BOOLEAN"  ;
         constexpr static const char* ISO_DATE = "ISO_DATE" ;
         constexpr static const char* CUSTOM   = "CUSTOM"   ;
         constexpr static const char* NUMBER   = "NUMBER"   ;
      }

      /**
       * Those constantes are used by the ConfigurationManager.validateCertificate method
       */
       namespace CheckValuesNames {
         constexpr static const char* PASSED      = "PASSED"     ;
         constexpr static const char* FAILED      = "FAILED"     ;
         constexpr static const char* UNSUPPORTED = "UNSUPPORTED";
         constexpr static const char* ISO_DATE    = "ISO_DATE"   ;
         constexpr static const char* CUSTOM      = "CUSTOM"     ;
         constexpr static const char* DATE        = "DATE"       ;
      }
   }
}