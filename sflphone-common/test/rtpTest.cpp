/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savarda <emmanuel.milou@savoirfairelinux.com>
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


#include "rtpTest.h"

#include <unistd.h>


using std::cout;
using std::endl;


void RtpTest::setUp(){

    Manager::instance().initConfigFile();
    Manager::instance().init();

    CallID cid = "123456";
        
    audiortp = new AudioRtp();

    sipcall = new SIPCall(cid, Call::Incoming, NULL);
}

void RtpTest::testRtpInit()
{

    _debug("------ void RtpTest::testRtpInit() ------\n");
    try {

        _debug("-------- Open Rtp Session ----------\n");
        CPPUNIT_ASSERT(audiortp->createNewSession(sipcall) == 0);

    } catch(...) {
        
        _debug("!!! Exception occured while Oppenning Rtp \n");
	
    }

}
    

void RtpTest::testRtpClose()
{

  _debug("------ RtpTest::testRtpClose() ------");

    try {
      _debug("------ Close Rtp Session -------\n");  
        CPPUNIT_ASSERT(audiortp->closeRtpSession());

    } catch(...) {

        _debug("!!! Exception occured while closing Rtp \n");

    }

}


void RtpTest::tearDown(){

    delete audiortp;  audiortp = NULL;
}
