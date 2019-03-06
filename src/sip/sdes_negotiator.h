/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
#ifndef __SDES_NEGOTIATOR_H__
#define __SDES_NEGOTIATOR_H__

#include "media/media_codec.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ring {

/**
 * Exception object that is thrown when
 * an error occurred while mathing a
 * pattern to an expression.
 */
class MatchError : public std::invalid_argument {
    public:
        MatchError(const std::string& error) :
            std::invalid_argument(error) {}
};

/**
 * General exception object that is thrown when
 * an error occurred with a regular expression
 * operation.
 */
class ParseError : public std::invalid_argument {
    public:
        explicit ParseError(const std::string& error) :
            std::invalid_argument(error) {}
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
    const char *name;
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

static std::vector<CryptoSuiteDefinition> CryptoSuites = {
    { "AES_CM_128_HMAC_SHA1_80",
      128, 112, 48, 31, AESCounterMode, 128, HMACSHA1, 80, 80, 160, 160 },

    { "AES_CM_128_HMAC_SHA1_32",
      128, 112, 48, 31, AESCounterMode, 128, HMACSHA1, 32, 80, 160, 160 },

    { "F8_128_HMAC_SHA1_80",
      128, 112, 48, 31, AESF8Mode, 128, HMACSHA1, 80, 80, 160, 160 }
};

class SdesNegotiator {
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
        SdesNegotiator() {}
        SdesNegotiator(const std::vector<CryptoSuiteDefinition>& capabilites);

        ring::CryptoAttribute
        negotiate(const std::vector<std::string>& attributes) const;

        inline explicit operator bool() const {
            return not localCapabilities_.empty();
        }

    private:
        static std::vector<CryptoAttribute>
        parse(const std::vector<std::string>& attributes);

        /**
         * A vector list containing the remote attributes.
         * Multiple crypto lines can be sent, and the
         * preferred method is then chosen from that list.
         */
        std::vector<CryptoSuiteDefinition> localCapabilities_;
};

} // namespace ring

#endif // __SDES_NEGOTIATOR_H__
