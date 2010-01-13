/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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


#include "sdesnegotiatorTest.h"

#include <unistd.h>


using std::cout;
using std::endl;


void SdesNegotiatorTest::setUp()
{

    // std::string attr("1 AES_CM_128_HMAC_SHA1_32 srtp inline:16/14/AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwd/2^20/1:32");

    std::string attr("a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:32");

    remoteOffer = new std::vector<std::string>();
    remoteOffer->push_back(attr);

    localCapabilities = new std::vector<sfl::CryptoSuiteDefinition>();
    for(int i = 0; i < 3; i++) {
        localCapabilities->push_back(sfl::CryptoSuites[i]);
    }

    sdesnego = new sfl::SdesNegotiator(*localCapabilities, *remoteOffer);

}


void SdesNegotiatorTest::tearDown()
{

    delete remoteOffer;
    remoteOffer = NULL;

    delete localCapabilities;
    localCapabilities = NULL;

    delete sdesnego;
    sdesnego = NULL;

}

void SdesNegotiatorTest::testTagPattern()
{
    std::string subject = "a=crypto:4"; 

    pattern = new sfl::Pattern("^a=crypto:(?P<tag>[0-9]{1,9})");
    *pattern << subject;

    CPPUNIT_ASSERT(pattern->matches());
    CPPUNIT_ASSERT(pattern->group("tag").compare("4") == 0);

    delete pattern;
    pattern = NULL;
}


void SdesNegotiatorTest::testCryptoSuitePattern()
{
    std::string subject = "AES_CM_128_HMAC_SHA1_80"; 

    pattern = new sfl::Pattern("(?P<cryptoSuite>AES_CM_128_HMAC_SHA1_80|" \
			       "AES_CM_128_HMAC_SHA1_32|"		\
			       "F8_128_HMAC_SHA1_80|"			\
			       "[A-Za-z0-9_]+)");
    *pattern << subject;

    CPPUNIT_ASSERT(pattern->matches());
    CPPUNIT_ASSERT(pattern->group("cryptoSuite").compare("AES_CM_128_HMAC_SHA1_80") == 0);

    delete pattern;
    pattern = NULL;
}


void SdesNegotiatorTest::testKeyParamsPattern()
{

    std::string subject = "inline:d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj|2^20|1:32";

    pattern = new sfl::Pattern("(?P<srtpKeyMethod>inline|[A-Za-z0-9_]+)\\:" \
			       "(?P<srtpKeyInfo>[A-Za-z0-9\x2B\x2F\x3D]+)\\|" \
			       "2\\^(?P<lifetime>[0-9]+)\\|"		\
			       "(?P<mkiValue>[0-9]+)\\:"		\
			       "(?P<mkiLength>[0-9]{1,3})\\;?", "g");

    *pattern << subject;

    pattern->matches();
    CPPUNIT_ASSERT(pattern->group("srtpKeyMethod").compare("inline:"));

    /*
    while (pattern->matches()) {
                
        std::string _srtpKeyMethod = pattern->group ("srtpKeyMethod");
	std::string _srtpKeyInfo = pattern->group ("srtpKeyInfo");
	std::string _lifetime = pattern->group ("lifetime");
	std::string _mkiValue = pattern->group ("mkiValue");
	std::string _mkiLength = pattern->group ("mkiLength");
    }


    CPPUNIT_ASSERT(pattern->group("srtpKeyMethod").compare("inline:"));
    CPPUNIT_ASSERT(pattern->group("srtpKeyInfo").compare("d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj"));
    CPPUNIT_ASSERT(pattern->group("lifetime").compare("20"));
    CPPUNIT_ASSERT(pattern->group("mkivalue").compare("1"));
    CPPUNIT_ASSERT(pattern->group("mkilength").compare("32"));
    */

    delete pattern;
    pattern = NULL;
}


void SdesNegotiatorTest::testNegotiation()
{
    CPPUNIT_ASSERT(sdesnego->negotiate());
    CPPUNIT_ASSERT(sdesnego->getCryptoSuite().compare("AES_CM_128_HMAC_SHA1_80") == 0);
    CPPUNIT_ASSERT(sdesnego->getKeyMethod().compare("inline") == 0);
    CPPUNIT_ASSERT(sdesnego->getKeyInfo().compare("d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj") == 0);
    CPPUNIT_ASSERT(sdesnego->getLifeTime().compare("20") == 0);
    CPPUNIT_ASSERT(sdesnego->getMkiValue().compare("1") == 0);
    CPPUNIT_ASSERT(sdesnego->getMkiLength().compare("32") == 0);
}


