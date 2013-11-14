/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include <cstddef>
#include <stdio.h>
#include <sstream>
#include <cstddef>
#include <ccrtp/rtp.h>
#include <string>
#include <cstring>
#include <math.h>
#include <dlfcn.h>
#include <iostream>
#include <sstream>

#include "sdesnegotiatortest.h"
#include "sip/pattern.h"
#include "sip/sdes_negotiator.h"

#include <unistd.h>
#include "test_utils.h"
#include "logger.h"

using std::cout;
using std::endl;

void SdesNegotiatorTest::testTagPattern()
{
    TITLE();
    std::string subject = "a=crypto:4";

    sfl::Pattern pattern("^a=crypto:(?P<tag>[0-9]{1,9})", false);
    pattern.updateSubject(subject);

    CPPUNIT_ASSERT(pattern.matches());
    CPPUNIT_ASSERT(pattern.group("tag").compare("4") == 0);
}


void SdesNegotiatorTest::testCryptoSuitePattern()
{
    TITLE();
    std::string subject = "AES_CM_128_HMAC_SHA1_80";

    sfl::Pattern pattern("(?P<cryptoSuite>AES_CM_128_HMAC_SHA1_80|" \
                               "AES_CM_128_HMAC_SHA1_32|"		\
                               "F8_128_HMAC_SHA1_80|"			\
                               "[A-Za-z0-9_]+)", false);
    pattern.updateSubject(subject);

    CPPUNIT_ASSERT(pattern.matches());
    CPPUNIT_ASSERT(pattern.group("cryptoSuite").compare("AES_CM_128_HMAC_SHA1_80") == 0);
}


void SdesNegotiatorTest::testKeyParamsPattern()
{
    TITLE();

    std::string subject = "inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:32";

    sfl::Pattern pattern("(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
                               "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)\\|" \
                               "(2\\^(?P<lifetime>[0-9]+)\\|"		\
                               "(?P<mkiValue>[0-9]+)\\:"		\
                               "(?P<mkiLength>[0-9]{1,3})\\;?)?", true);

    pattern.updateSubject(subject);

    pattern.matches();
    CPPUNIT_ASSERT(pattern.group("srtpKeyMethod").compare("inline:"));
    CPPUNIT_ASSERT(pattern.group("srtpKeyInfo").compare("d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj")
                   == 0);
    CPPUNIT_ASSERT(pattern.group("lifetime").compare("20") == 0);
    CPPUNIT_ASSERT(pattern.group("mkiValue").compare("1") == 0);
    CPPUNIT_ASSERT(pattern.group("mkiLength").compare("32") == 0);
}


void SdesNegotiatorTest::testKeyParamsPatternWithoutMKI()
{
    TITLE();

    std::string subject("inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj");

    sfl::Pattern pattern("(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
                               "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)" \
                               "(\\|2\\^(?P<lifetime>[0-9]+)\\|"                \
                               "(?P<mkiValue>[0-9]+)\\:"                \
                               "(?P<mkiLength>[0-9]{1,3})\\;?)?", true);

    pattern.updateSubject(subject);
    pattern.matches();
    CPPUNIT_ASSERT(pattern.group("srtpKeyMethod").compare("inline:"));
    CPPUNIT_ASSERT(pattern.group("srtpKeyInfo").compare("d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj")
                   == 0);
}


/**
 * Make sure that all the fields can be extracted
 * properly from the syntax.
 */
void SdesNegotiatorTest::testNegotiation()
{
    TITLE();

    // Add a new SDES crypto line to be processed.
    std::vector<std::string> remoteOffer;
    remoteOffer.push_back("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd|2^20|1:32");
    remoteOffer.push_back("a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj|2^20|1:32");

    // Register the local capabilities.
    std::vector<sfl::CryptoSuiteDefinition> localCapabilities;

    for (int i = 0; i < 3; ++i)
        localCapabilities.push_back(sfl::CryptoSuites[i]);

    sfl::SdesNegotiator sdesnego(localCapabilities, remoteOffer);

    CPPUNIT_ASSERT(sdesnego.negotiate());
}

/**
 * Make sure that unproperly formatted crypto lines are rejected.
 */
void SdesNegotiatorTest::testComponent()
{
    TITLE();

    // Register the local capabilities.
    std::vector<sfl::CryptoSuiteDefinition> capabilities;

    // Support all the CryptoSuites
    for (int i = 0; i < 3; i++)
        capabilities.push_back(sfl::CryptoSuites[i]);

    // Make sure that if a component is missing, negotiate will fail
    std::string cryptoLine("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:|2^20|1:32");
    std::vector<std::string> cryptoOffer;
    cryptoOffer.push_back(cryptoLine);

    sfl::SdesNegotiator negotiator(capabilities, cryptoOffer);
    CPPUNIT_ASSERT(!negotiator.negotiate());
}

/**
 * Make sure that most simple case does not fail.
 */
void SdesNegotiatorTest::testMostSimpleCase()
{
    TITLE();

    // Register the local capabilities.
    std::vector<sfl::CryptoSuiteDefinition> capabilities;

    // Support all the CryptoSuites
    for (int i = 0; i < 3; i++)
        capabilities.push_back(sfl::CryptoSuites[i]);

    // Make sure taht this case works (since it's default for most application)
    std::string cryptoLine("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd");
    std::vector<std::string> cryptoOffer;
    cryptoOffer.push_back(cryptoLine);

    sfl::SdesNegotiator negotiator(capabilities, cryptoOffer);

    CPPUNIT_ASSERT(negotiator.negotiate());

    CPPUNIT_ASSERT(negotiator.getCryptoSuite() == "AES_CM_128_HMAC_SHA1_80");
    CPPUNIT_ASSERT(negotiator.getKeyMethod() == "inline");
    CPPUNIT_ASSERT(negotiator.getKeyInfo() == "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd");
    CPPUNIT_ASSERT(negotiator.getLifeTime().empty());
    CPPUNIT_ASSERT(negotiator.getMkiValue().empty());
    CPPUNIT_ASSERT(negotiator.getMkiLength().empty());
    CPPUNIT_ASSERT(negotiator.getAuthTagLength() == "80");
}


void SdesNegotiatorTest::test32ByteKeyLength()
{
    TITLE();

    // Register the local capabilities.
    std::vector<sfl::CryptoSuiteDefinition> capabilities;

    //Support all the CryptoSuites
    for (int i = 0; i < 3; i++)
        capabilities.push_back(sfl::CryptoSuites[i]);

    std::string cryptoLine("a=crypto:1 AES_CM_128_HMAC_SHA1_32 inline:AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd");
    std::vector<std::string> cryptoOffer;
    cryptoOffer.push_back(cryptoLine);

    sfl::SdesNegotiator negotiator(capabilities, cryptoOffer);

    CPPUNIT_ASSERT(negotiator.negotiate());

    CPPUNIT_ASSERT(negotiator.getCryptoSuite() == "AES_CM_128_HMAC_SHA1_32");
    CPPUNIT_ASSERT(negotiator.getKeyMethod() == "inline");
    CPPUNIT_ASSERT(negotiator.getKeyInfo() == "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd");
    CPPUNIT_ASSERT(negotiator.getLifeTime().empty());
    CPPUNIT_ASSERT(negotiator.getMkiValue().empty());
    CPPUNIT_ASSERT(negotiator.getMkiLength().empty());
    CPPUNIT_ASSERT(negotiator.getAuthTagLength() == "32");
}

