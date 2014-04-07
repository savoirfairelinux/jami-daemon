/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *
 *  Author: Alexandre Lision <alexandre.lision@savoirfairelinux.com>
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

#ifndef SECURITY_EVALUATOR_H
#define SECURITY_EVALUATOR_H

#include <openssl/x509v3.h>
#include <string>

class SecurityEvaluator {

public:

#if HAVE_TLS

        typedef enum {
          SRTP_DISABLED                  ,
          TLS_DISABLED                   ,
          CERTIFICATE_EXPIRED            ,
          CERTIFICATE_SELF_SIGNED        ,
          CA_CERTIFICATE_MISSING         ,
          END_CERTIFICATE_MISSING        ,
          PRIVATE_KEY_MISSING            ,
          CERTIFICATE_MISMATCH           ,
          CERTIFICATE_STORAGE_PERMISSION ,
          CERTIFICATE_STORAGE_FOLDER     ,
          CERTIFICATE_STORAGE_LOCATION   ,
          OUTGOING_SERVER_MISMATCH       ,
          VERIFY_INCOMING_DISABLED       ,
          VERIFY_ANSWER_DISABLED         ,
          REQUIRE_CERTIFICATE_DISABLED   ,
       } SecurityFlaw;

        typedef enum {
            MatchFound,
            MatchNotFound,
            NoSANPresent,
            MalformedCertificate,
            Error
        } HostnameValidationResult;

        /**
         * Check if the given .pem contains a private key
         * This is necessary to show/hide fields in client
         */
        static bool containsPrivateKey(const std::string& pemPath);

        /**
         * Performs check on the given .pem
         * Try to open it, and read issuer name
         * Expiration Date
         */
        static bool certificateIsValid(const std::string& pemPath);

        /**
         * Verify local hostname (General Settings) with the one provided by the serverà
         * certificatePath is the local server .crt, passed as a parameter to create the SSL handshake.
         * This check only works with DNS names, not IP (as stated in everything-you-wanted-to-know-about-openssl.pdf)
         */
        static bool verifyHostnameCertificate(const std::string& certificatePath, const std::string& host, const std::string& port);

#endif


private:

        /**
        * Validates the server's identity by looking for the expected hostname in the
        * server's certificate. As described in RFC 6125, it first tries to find a match
        * in the Subject Alternative Name extension. If the extension is not present in
        * the certificate, it checks the Common Name instead.
        *
        * Returns MatchFound if a match was found.
        * Returns MatchNotFound if no matches were found.
        * Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
        * Returns Error if there was an error.
        */
        static HostnameValidationResult validateHostname(const std::string& hostname, const X509 *server_cert);

        /**
        * Tries to find a match for hostname in the certificate's Common Name field.
        *
        * Returns MatchFound if a match was found.
        * Returns MatchNotFound if no matches were found.
        * Returns MalformedCertificate if the Common Name had a NUL character embedded in it.
        * Returns Error if the Common Name could not be extracted.
        */
        static HostnameValidationResult matchCommonName(const std::string& hostname, const X509 *server_cert);

        /**
        * Tries to find a match for hostname in the certificate's Subject Alternative Name extension.
        *
        * Returns MatchFound if a match was found.
        * Returns MatchNotFound if no matches were found.
        * Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
        * Returns NoSANPresent if the SAN extension was not present in the certificate.
        */
        static HostnameValidationResult matchSubjectAltName(const std::string& hostname, const X509 *server_cert);

        /**
        * Prints certificate in logs
        */
        static void printCertificate(X509* cert);

        /**
         * Convert an extracted time (ASN1_TIME) in ISO-8601
         */
        static int convertASN1TIME(ASN1_TIME *t, char* buf, size_t len);

        SecurityEvaluator() {}
        ~SecurityEvaluator() {}

};

#endif
