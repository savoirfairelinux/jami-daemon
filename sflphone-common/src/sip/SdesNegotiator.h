/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#ifndef __SFL_SDES_NEGOTIATOR_H__
#define __SFL_SDES_NEGOTIATOR_H__

#include <stdexcept>
#include <string>
#include <vector>

namespace sfl
{

/**
 * General exception object that is thrown when
 * an error occured with a regular expression
 * operation.
 */
class parse_error : public std::invalid_argument
{
    public:
        explicit parse_error (const std::string& error) :
                std::invalid_argument (error) {}
};

enum CipherMode {
    AESCounterMode,
    AESF8Mode
};

enum MACMode {
    HMACSHA1
};

enum KeyMethod {
    Inline
    // url, maybe at some point
};

struct CryptoSuiteDefinition {
    char * name;
    int masterKeyLength;
    int masterSaltLength;
    int srtpLifetime;
    int srtcpLifetime;
    CipherMode cipher;
    int encryptionKeyLength;
    MACMode mac;
    int srtpAuthTagLength;
    int srtcpAuthTagLength;
    int srtpAuthKeyLength;
    int srtcpAuthKeyLen;
};

/**
* List of accepted Crypto-Suites
* as defined in RFC4568 (6.2)
*/
const CryptoSuiteDefinition CryptoSuites[3] = {
    { (char*) "AES_CM_128_HMAC_SHA1_80", 128, 112, 48, 31, AESCounterMode, 128, HMACSHA1, 80, 80, 160, 160 },
    { (char*) "AES_CM_128_HMAC_SHA1_32", 128, 112, 48, 31, AESCounterMode, 128, HMACSHA1, 32, 80, 160, 160 },
    { (char*) "F8_128_HMAC_SHA1_80", 128, 112, 48, 31, AESF8Mode, 128, HMACSHA1, 80, 80, 160, 160 }
};


class CryptoAttribute
{

    public:
        CryptoAttribute (std::string tag,
                         std::string cryptoSuite,
                         std::string srtpKeyMethod,
                         std::string srtpKeyInfo,
                         std::string lifetime,
                         std::string mkiValue,
                         std::string mkiLength) :
                tag (tag),
                cryptoSuite (cryptoSuite),
                srtpKeyMethod (srtpKeyMethod),
                srtpKeyInfo (srtpKeyInfo),
                lifetime (lifetime),
                mkiValue (mkiValue),
                mkiLength (mkiLength) {};


        inline std::string getTag() {
            return tag;
        };
        inline std::string getCryptoSuite() {
            return cryptoSuite;
        };
        inline std::string getSrtpKeyMethod() {
            return srtpKeyMethod;
        };
        inline std::string getSrtpKeyInfo() {
            return srtpKeyInfo;
        };
        inline std::string getLifetime() {
            return lifetime;
        };
        inline std::string getMkiValue() {
            return mkiValue;
        };
        inline std::string getMkiLength() {
            return mkiLength;
        };

    private:
        std::string tag;
        std::string cryptoSuite;
        std::string srtpKeyMethod;
        std::string srtpKeyInfo;
        std::string lifetime;
        std::string mkiValue;
        std::string mkiLength;
};

class SdesNegotiator
{
        /**
         * Constructor for an SDES crypto attributes
         * negotiator.
         *
         * @param attribute
         *       A vector of crypto attributes as defined in
         *       RFC4568. This string will be parsed
         *       and a crypto context will be created
         *       from it.
         */

    public:
        SdesNegotiator (const std::vector<CryptoSuiteDefinition>& localCapabilites, const std::vector<std::string>& remoteAttribute);
        ~SdesNegotiator() { };

        bool negotiate (void);

        /**
         * Return crypto suite after negotiation
         */
        std::string getCryptoSuite (void) {
            return _cryptoSuite;
        }

        /**
         * Return key method after negotiation (most likely inline:)
         */
        std::string getKeyMethod (void) {
            return _srtpKeyMethod;
        }

        /**
         * Return crypto suite after negotiation
         */
        std::string getKeyInfo (void) {
            return _srtpKeyInfo;
        }

        /**
         * Return key lifetime after negotiation
         */
        std::string getLifeTime (void) {
            return _lifetime;
        }

        /**
         * Return mki value after negotiation
         */
        std::string getMkiValue (void) {
            return _mkiValue;
        }

        /**
         * Return mki length after negotiation
         */
        std::string getMkiLength (void) {
            return _mkiLength;
        }

    private:
        /**
         * A vector list containing the remote attributes.
         * Multiple crypto lines can be sent, and the
         * prefered method is then chosen from that list.
         */
        std::vector<std::string> _remoteAttribute;

        std::vector<CryptoSuiteDefinition> _localCapabilities;

        /**
         * Selected crypto suite after negotiation
         */
        std::string _cryptoSuite;

        /**
         * Selected key method after negotiation (most likely inline:)
         */
        std::string _srtpKeyMethod;

        /**
         * Selected crypto suite after negotiation
         */
        std::string _srtpKeyInfo;

        /**
         * Selected key lifetime after negotiation
         */
        std::string _lifetime;

        /**
         * Selected mki value after negotiation
         */
        std::string _mkiValue;

        /**
         * Selected mki length after negotiation
         */
        std::string _mkiLength;

        std::vector<CryptoAttribute *> parse (void);
};
}
#endif
