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

void SdesNegotiatorTest::testNegotiation()
{
    CPPUNIT_ASSERT(sdesnego->negotiate());
}


