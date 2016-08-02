/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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

#include "sdes_negotiator.h"
#include "pattern.h"

#include <memory>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>

#include <cstdio>

namespace ring {

SdesNegotiator::SdesNegotiator(const std::vector<CryptoSuiteDefinition>& localCapabilites) :
    localCapabilities_(localCapabilites)
{}

std::vector<CryptoAttribute>
SdesNegotiator::parse(const std::vector<std::string>& attributes)
{
    // The patterns below try to follow
    // the ABNF grammar rules described in
    // RFC4568 section 9.2 with the general
    // syntax :
    //a=crypto:tag 1*WSP crypto-suite 1*WSP key-params *(1*WSP session-param)

    std::unique_ptr<Pattern> generalSyntaxPattern, tagPattern, cryptoSuitePattern,
        keyParamsPattern;

    try {
        // used to match white space (which are used as separator)
        generalSyntaxPattern.reset(new Pattern("[\x20\x09]+", true));

        tagPattern.reset(new Pattern("^(?P<tag>[0-9]{1,9})", false));

        cryptoSuitePattern.reset(new Pattern(
            "(?P<cryptoSuite>AES_CM_128_HMAC_SHA1_80|" \
            "AES_CM_128_HMAC_SHA1_32|" \
            "F8_128_HMAC_SHA1_80|" \
            "[A-Za-z0-9_]+)", false)); // srtp-crypto-suite-ext

        keyParamsPattern.reset(new Pattern(
            "(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
            "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)"     \
            "(\\|2\\^(?P<lifetime>[0-9]+)\\|"         \
            "(?P<mkiValue>[0-9]+)\\:"             \
            "(?P<mkiLength>[0-9]{1,3})\\;?)?", true));

    } catch (const CompileError& exception) {
        throw ParseError("A compile exception occurred on a pattern.");
    }

    // Take each line from the vector
    // and parse its content

    std::vector<CryptoAttribute> cryptoAttributeVector;

    for (const auto &item : attributes) {

        // Split the line into its component
        // that we will analyze further down.
        std::vector<std::string> sdesLine;

        generalSyntaxPattern->updateSubject(item);

        try {
            sdesLine = generalSyntaxPattern->split();

            if (sdesLine.size() < 3)
                throw ParseError("Missing components in SDES line");
        } catch (const MatchError& exception) {
            throw ParseError("Error while analyzing the SDES line.");
        }

        // Check if the attribute starts with a=crypto
        // and get the tag for this line
        tagPattern->updateSubject(sdesLine.at(0));

        std::string tag;

        if (tagPattern->matches()) {
            try {
                tag = tagPattern->group("tag");
            } catch (const MatchError& exception) {
                throw ParseError("Error while parsing the tag field");
            }
        } else
            return cryptoAttributeVector;

        // Check if the crypto suite is valid and retreive
        // its value.
        cryptoSuitePattern->updateSubject(sdesLine.at(1));

        std::string cryptoSuite;

        if (cryptoSuitePattern->matches()) {
            try {
                cryptoSuite = cryptoSuitePattern->group("cryptoSuite");
            } catch (const MatchError& exception) {
                throw ParseError("Error while parsing the crypto-suite field");
            }
        } else
            return cryptoAttributeVector;

        // Parse one or more key-params field.
        keyParamsPattern->updateSubject(sdesLine.at(2));

        std::string srtpKeyInfo;
        std::string srtpKeyMethod;
        std::string lifetime;
        std::string mkiLength;
        std::string mkiValue;

        try {
            while (keyParamsPattern->matches()) {
                srtpKeyMethod = keyParamsPattern->group("srtpKeyMethod");
                srtpKeyInfo = keyParamsPattern->group("srtpKeyInfo");
                lifetime = keyParamsPattern->group("lifetime");
                mkiValue = keyParamsPattern->group("mkiValue");
                mkiLength = keyParamsPattern->group("mkiLength");
            }
        } catch (const MatchError& exception) {
            throw ParseError("Error while parsing the key-params field");
        }

        // Add the new CryptoAttribute to the vector
        cryptoAttributeVector.emplace_back(
            tag,
            cryptoSuite,
            srtpKeyMethod,
            srtpKeyInfo,
            lifetime,
            mkiValue,
            mkiLength
        );
    }
    return cryptoAttributeVector;
}

CryptoAttribute
SdesNegotiator::negotiate(const std::vector<std::string>& attributes) const
{
    try {
        auto cryptoAttributeVector(parse(attributes));
        for (const auto& iter_offer : cryptoAttributeVector) {
            for (const auto& iter_local : localCapabilities_) {
                if (iter_offer.getCryptoSuite() == iter_local.name)
                    return iter_offer;
            }
        }
    }
    catch (const ParseError& exception) {}
    catch (const MatchError& exception) {}
    return {};
}

} // namespace ring
