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


    audiortp->_RTXThread->computeCodecFrameSize(320,8000);

      // computeNbByteAudioLayer
    
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

    audiortp = new AudioRtp();

    _debug("-------- Open Rtp Session ----------\n");
    try {

        CPPUNIT_ASSERT(audiortp->createNewSession(sipcall) == 0);

    } catch(...) {
        
        _debug("!!! Exception occured while Oppenning Rtp !!!\n");
	
    }

    _debug("------ void RtpTest::testRtpThread ------\n");  

    CPPUNIT_ASSERT(audiortp->_RTXThread->computeCodecFrameSize(160,8000) == 20.0f);
    CPPUNIT_ASSERT(audiortp->_RTXThread->computeCodecFrameSize(320,16000) == 20.0f);
    CPPUNIT_ASSERT(audiortp->_RTXThread->computeCodecFrameSize(882,44100) == 20.0f);

    // 20 ms at 44.1 khz corespond to 882 samples (1764 byte)
    CPPUNIT_ASSERT(audiortp->_RTXThread->computeNbByteAudioLayer(20.f) == 1764);
    
    _debug("------ Close Rtp Session -------\n");
    try {

        CPPUNIT_ASSERT(audiortp->closeRtpSession());

    } catch(...) {

        _debug("!!! Exception occured while closing Rtp !!!\n");

    }

    delete audiortp;  audiortp = NULL;
}



void RtpTest::testRtpResampling()
{

    int nbSample = 50;
    int rsmpl_nbSample = 0;

    SFLDataFormat *data = new SFLDataFormat[1024];
    SFLDataFormat *rsmpl_data = new SFLDataFormat[1024];

    for (int i = 0; i < nbSample; i++)
        data[i] = i;


    audiortp = new AudioRtp();

    _debug("-------- Open Rtp Session ----------\n");
    try {

        CPPUNIT_ASSERT(audiortp->createNewSession(sipcall) == 0);

    } catch(...) {
        
        _debug("!!! Exception occured while Oppenning Rtp !!!\n");
	
    }

    _debug("------ void RtpTest::testRtpResampling ------\n");  

    CPPUNIT_ASSERT(0 == 0);
    rsmpl_nbSample = audiortp->_RTXThread->reSampleData(data, rsmpl_data, 8000, nbSample, UP_SAMPLING);
    _debug("ORIGINAL DATA SET\n");
    for (int i = 0; i < nbSample; i++)
        printf("  %i=>%i  ", i, data[i]);
    
    _debug("RESAMPLED DATA SET\n");
    for (int i = 0; i < rsmpl_nbSample; i++ )
        printf("  %i=>%i  ", i, rsmpl_data[i]);

    printf("\n");
    
    
    _debug("------ Close Rtp Session -------\n");
    try {

        CPPUNIT_ASSERT(audiortp->closeRtpSession());

    } catch(...) {

        _debug("!!! Exception occured while closing Rtp !!!\n");

    }

    delete audiortp;  audiortp = NULL;
}


void RtpTest::tearDown(){

    delete sipcall;   sipcall = NULL;
}
