/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <stdio.h>
#include <sstream>
#include <ccrtp/rtp.h>
#include <assert.h>
#include <string>
#include <cstring>
#include <math.h>
#include <dlfcn.h>
#include <iostream>
#include <sstream>

#include "sdesnegotiatortest.h"

#include <unistd.h>
#include "global.h"


using std::cout;
using std::endl;


void SdesNegotiatorTest::testTagPattern()
{
    _debug ("-------------------- SdesNegotiatorTest::testTagPattern --------------------\n");

    std::string subject = "a=crypto:4";

    pattern = new sfl::Pattern ("^a=crypto:(?P<tag>[0-9]{1,9})");
    *pattern << subject;

    CPPUNIT_ASSERT (pattern->matches());
    CPPUNIT_ASSERT (pattern->group ("tag").compare ("4") == 0);

    delete pattern;
    pattern = NULL;
}


void SdesNegotiatorTest::testCryptoSuitePattern()
{
    _debug ("-------------------- SdesNegotiatorTest::testCryptoSuitePattern --------------------\n");

    std::string subject = "AES_CM_128_HMAC_SHA1_80";

    pattern = new sfl::Pattern ("(?P<cryptoSuite>AES_CM_128_HMAC_SHA1_80|" \
                                "AES_CM_128_HMAC_SHA1_32|"		\
                                "F8_128_HMAC_SHA1_80|"			\
                                "[A-Za-z0-9_]+)");
    *pattern << subject;

    CPPUNIT_ASSERT (pattern->matches());
    CPPUNIT_ASSERT (pattern->group ("cryptoSuite").compare ("AES_CM_128_HMAC_SHA1_80") == 0);

    delete pattern;
    pattern = NULL;
}


void SdesNegotiatorTest::testKeyParamsPattern()
{
    _debug ("-------------------- SdesNegotiatorTest::testKeyParamsPattern --------------------\n");

    std::string subject = "inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:32";

    pattern = new sfl::Pattern ("(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
                                "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)\\|" \
                                "(2\\^(?P<lifetime>[0-9]+)\\|"		\
                                "(?P<mkiValue>[0-9]+)\\:"		\
                                "(?P<mkiLength>[0-9]{1,3})\\;?)?", "g");

    *pattern << subject;

    pattern->matches();
    CPPUNIT_ASSERT (pattern->group ("srtpKeyMethod").compare ("inline:"));
    CPPUNIT_ASSERT (pattern->group ("srtpKeyInfo").compare ("d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj")
                    == 0);
    CPPUNIT_ASSERT (pattern->group ("lifetime").compare ("20") == 0);
    CPPUNIT_ASSERT (pattern->group ("mkiValue").compare ("1") == 0);
    CPPUNIT_ASSERT (pattern->group ("mkiLength").compare ("32") == 0);

    delete pattern;
    pattern = NULL;
}


void SdesNegotiatorTest::testKeyParamsPatternWithoutMKI()
{
    _debug ("-------------------- SdesNegotiatorTest::testKeyParamsPatternWithoutMKI --------------------\n");

    std::string subject = "inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";

    pattern = new sfl::Pattern ("(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
                                "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)" \
                                "(\\|2\\^(?P<lifetime>[0-9]+)\\|"                \
                                "(?P<mkiValue>[0-9]+)\\:"                \
                                "(?P<mkiLength>[0-9]{1,3})\\;?)?", "g");

    *pattern << subject;
    pattern->matches();
    CPPUNIT_ASSERT (pattern->group ("srtpKeyMethod").compare ("inline:"));
    CPPUNIT_ASSERT (pattern->group ("srtpKeyInfo").compare ("d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj")
                    == 0);

    delete pattern;
    pattern = NULL;
}


/**
 * Make sure that all the fields can be extracted
 * properly from the syntax.
 */
void SdesNegotiatorTest::testNegotiation()
{
    _debug ("-------------------- SdesNegotiatorTest::testNegotiation --------------------\n");

    // Add a new SDES crypto line to be processed.
    remoteOffer = new std::vector<std::string>();
    remoteOffer->push_back (std::string ("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd|2^20|1:32"));
    remoteOffer->push_back (std::string ("a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:NzB4d1BINUAvLEw6UzF3WSJ+PSdFcGdUJShpX1Zj|2^20|1:32"));

    // Register the local capabilities.
    localCapabilities = new std::vector<sfl::CryptoSuiteDefinition>();

    for (int i = 0; i < 3; i++) {
        localCapabilities->push_back (sfl::CryptoSuites[i]);
    }

    sdesnego = new sfl::SdesNegotiator (*localCapabilities, *remoteOffer);

    CPPUNIT_ASSERT (sdesnego->negotiate());
    // CPPUNIT_ASSERT(sdesnego->getKeyInfo().compare("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd|2^20|1:32")==0);

    delete remoteOffer;
    remoteOffer = NULL;

    delete localCapabilities;
    localCapabilities = NULL;

    delete sdesnego;
    sdesnego = NULL;
}

/**
 * Make sure that unproperly formatted crypto lines are rejected.
 */
void SdesNegotiatorTest::testComponent()
{
    _debug ("-------------------- SdesNegotiatorTest::testComponent --------------------\n");

    // Register the local capabilities.
    std::vector<sfl::CryptoSuiteDefinition> * capabilities = new std::vector<sfl::CryptoSuiteDefinition>();

    //Support all the CryptoSuites
    for (int i = 0; i < 3; i++) {
        capabilities->push_back (sfl::CryptoSuites[i]);
    }

    // Make sure that if a component is missing, negotiate will fail
    std::string cryptoLine ("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:|2^20|1:32");
    std::vector<std::string> * cryptoOffer = new std::vector<std::string>();
    cryptoOffer->push_back (cryptoLine);

    sfl::SdesNegotiator * negotiator = new sfl::SdesNegotiator (*capabilities, *cryptoOffer);

    CPPUNIT_ASSERT (negotiator->negotiate() == false);
}



/**
 * Make sure that most simple case does not fail.
 */
void SdesNegotiatorTest::testMostSimpleCase()
{
    _debug ("-------------------- SdesNegotiatorTest::testMostSimpleCase --------------------\n");

    // Register the local capabilities.
    std::vector<sfl::CryptoSuiteDefinition> * capabilities = new std::vector<sfl::CryptoSuiteDefinition>();

    //Support all the CryptoSuites
    for (int i = 0; i < 3; i++) {
        capabilities->push_back (sfl::CryptoSuites[i]);
    }

    // Make sure taht this case works (since it's default for most application)
    std::string cryptoLine ("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd");
    std::vector<std::string> * cryptoOffer = new std::vector<std::string>();
    cryptoOffer->push_back (cryptoLine);

    sfl::SdesNegotiator * negotiator = new sfl::SdesNegotiator (*capabilities, *cryptoOffer);

    CPPUNIT_ASSERT (negotiator->negotiate() == true);

    CPPUNIT_ASSERT (negotiator->getCryptoSuite().compare ("AES_CM_128_HMAC_SHA1_80") == 0);
    CPPUNIT_ASSERT (negotiator->getKeyMethod().compare ("inline") == 0);
    CPPUNIT_ASSERT (negotiator->getKeyInfo().compare ("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd") == 0);
    CPPUNIT_ASSERT (negotiator->getLifeTime().compare ("") == 0);
    CPPUNIT_ASSERT (negotiator->getMkiValue().compare ("") == 0);
    CPPUNIT_ASSERT (negotiator->getMkiLength().compare ("") == 0);

    delete capabilities;
    capabilities = NULL;
    delete cryptoOffer;
    cryptoOffer = NULL;
    delete negotiator;
    negotiator = NULL;
}
