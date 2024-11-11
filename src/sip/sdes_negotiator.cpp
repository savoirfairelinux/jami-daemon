/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sdes_negotiator.h"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <regex>

#include <cstdio>

namespace jami {

std::vector<CryptoAttribute>
SdesNegotiator::parse(const std::vector<std::string>& attributes)
{
    // The patterns below try to follow
    // the ABNF grammar rules described in
    // RFC4568 section 9.2 with the general
    // syntax :
    // a=crypto:tag 1*WSP crypto-suite 1*WSP key-params *(1*WSP session-param)

    // used to match white space (which are used as separator)

    static const std::regex generalSyntaxPattern {"[\x20\x09]+"};

    static const std::regex tagPattern {"^([0-9]{1,9})"};

    static const std::regex cryptoSuitePattern {"(AES_CM_128_HMAC_SHA1_80|"
                                                "AES_CM_128_HMAC_SHA1_32|"
                                                "F8_128_HMAC_SHA1_80|"
                                                "[A-Za-z0-9_]+)"}; // srtp-crypto-suite-ext

    static const std::regex keyParamsPattern {"(inline|[A-Za-z0-9_]+)\\:"
                                              "([A-Za-z0-9\x2B\x2F\x3D]+)"
                                              "((\\|2\\^)([0-9]+)\\|"
                                              "([0-9]+)\\:"
                                              "([0-9]{1,3})\\;?)?"};

    // Take each line from the vector
    // and parse its content

    std::vector<CryptoAttribute> cryptoAttributeVector;

    for (const auto& item : attributes) {
        // Split the line into its component that we will analyze further down.
        // Additional white space is added to better split the content
        // Result is stored in the sdsLine

        std::vector<std::string> sdesLine;
        std::smatch sm_generalSyntaxPattern;

        std::sregex_token_iterator iter(item.begin(), item.end(), generalSyntaxPattern, -1), end;
        for (; iter != end; ++iter)
            sdesLine.push_back(*iter);

        if (sdesLine.size() < 3) {
            throw ParseError("Missing components in SDES line");
        }

        // Check if the attribute starts with a=crypto
        // and get the tag for this line

        std::string tag;
        std::smatch sm_tagPattern;

        if (std::regex_search(sdesLine.at(0), sm_tagPattern, tagPattern)) {
            tag = sm_tagPattern[1];

        } else {
            throw ParseError("No Matching Found in Tag Attribute");
        }

        // Check if the crypto suite is valid and retrieve
        // its value.

        std::string cryptoSuite;
        std::smatch sm_cryptoSuitePattern;

        if (std::regex_search(sdesLine.at(1), sm_cryptoSuitePattern, cryptoSuitePattern)) {
            cryptoSuite = sm_cryptoSuitePattern[1];

        } else {
            throw ParseError("No Matching Found in CryptoSuite Attribute");
        }

        // Parse one or more key-params field.
        // Group number is used to locate different paras

        std::string srtpKeyInfo;
        std::string srtpKeyMethod;
        std::string lifetime;
        std::string mkiLength;
        std::string mkiValue;
        std::smatch sm_keyParamsPattern;

        if (std::regex_search(sdesLine.at(2), sm_keyParamsPattern, keyParamsPattern)) {
            srtpKeyMethod = sm_keyParamsPattern[1];
            srtpKeyInfo = sm_keyParamsPattern[2];
            lifetime = sm_keyParamsPattern[5];
            mkiValue = sm_keyParamsPattern[6];
            mkiLength = sm_keyParamsPattern[7];

        } else {
            throw ParseError("No Matching Found in Key-params Attribute");
        }

        // Add the new CryptoAttribute to the vector
        cryptoAttributeVector.emplace_back(std::move(tag),
                                           std::move(cryptoSuite),
                                           std::move(srtpKeyMethod),
                                           std::move(srtpKeyInfo),
                                           std::move(lifetime),
                                           std::move(mkiValue),
                                           std::move(mkiLength));
    }
    return cryptoAttributeVector;
}

CryptoAttribute
SdesNegotiator::negotiate(const std::vector<std::string>& attributes)
{
    try {
        auto cryptoAttributeVector(parse(attributes));
        for (const auto& iter_offer : cryptoAttributeVector) {
            for (const auto& iter_local : CryptoSuites) {
                if (iter_offer.getCryptoSuite() == iter_local.name)
                    return iter_offer;
            }
        }
    } catch (const ParseError& exception) {
    }
    return {};
}

} // namespace jami
