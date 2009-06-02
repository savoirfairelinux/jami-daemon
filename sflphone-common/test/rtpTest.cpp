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
#include <assert.h>
#include <string>
#include <cstring>
#include <math.h>
#include <dlfcn.h>
#include <iostream>
#include <sstream>


#include "rtpTest.h"

#include <unistd.h>


using std::cout;
using std::endl;


void RtpTest::setUp(){

  _debug("------ Set up rtp test------\n");

    Manager::instance().initConfigFile();
    Manager::instance().init();

    pjsipInit();

    CallID cid = "123456";

    sipcall = new SIPCall(cid, Call::Incoming, _pool);
    
    sipcall->setLocalIp("127.0.0.1");
    sipcall->setLocalAudioPort(RANDOM_LOCAL_PORT);
    sipcall->setLocalExternAudioPort(RANDOM_LOCAL_PORT);

    
}

bool RtpTest::pjsipInit(){

    // Create memory cache for pool
    pj_caching_pool_init(&_cp, &pj_pool_factory_default_policy, 0);

    // Create memory pool for application. 
    _pool = pj_pool_create(&_cp.factory, "rtpTest", 4000, 4000, NULL);

    if (!_pool) {
        _debug("----- RtpTest: Could not initialize pjsip memory pool ------\n");
        return PJ_ENOMEM;
    }

}


void RtpTest::testRtpInitClose()
{

    audiortp = new AudioRtp();

    _debug("------ void RtpTest::testRtpInit() ------\n");
    try {

        _debug("-------- Open Rtp Session ----------\n");
        CPPUNIT_ASSERT(audiortp->createNewSession(sipcall) == 0);

    } catch(...) {
        
        _debug("!!! Exception occured while Oppenning Rtp !!!\n");
	
    }

    CPPUNIT_ASSERT(audiortp != NULL);
    
    _debug("------ Finilize Rtp Initialization ------ \n");


    _debug("------ RtpTest::testRtpClose() ------\n");

    try {
      _debug("------ Close Rtp Session -------\n");  
        CPPUNIT_ASSERT(audiortp->closeRtpSession());

    } catch(...) {

        _debug("!!! Exception occured while closing Rtp !!!\n");

    }

    delete audiortp;  audiortp = NULL;

}

void RtpTest::testRtpThread()
{

    _debug("------ void RtpTest::testRtpThread ------\n");

    

    if(rtpthread != 0){
        _debug("!!! Rtp Thread already exists..., stopping it\n"); 
	delete rtpthread;  rtpthread = 0;
    }

    CPPUNIT_ASSERT(rtpthread == 0);
    // CPPUNIT_ASSERT(rtpthread->_sym == NULL);

    try {

        rtpthread = new AudioRtpRTX(sipcall, true);
	
    } catch(...) {

        _debug("!!! Exception occured while instanciating AudioRtpRTX !!!\n");

    }

    CPPUNIT_ASSERT(rtpthread == 0);

    delete rtpthread;  rtpthread = 0;
}


void RtpTest::tearDown(){

    delete sipcall;   sipcall = NULL;
}
