/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savarda <alexandre.savard@savoirfairelinux.com>
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
#include <time.h>

#include "rtptest.h"
#include "audio/audiortp/AudioRtpSession.h"
#include "audio/audiortp/AudioSymmetricRtpSession.h"

#include <unistd.h>

void RtpTest::setUp() {

	pjsipInit();

	CallID cid = "123456";

	sipcall = new SIPCall(cid, Call::Incoming, _pool);

	sipcall->setLocalIp("127.0.0.1");
	sipcall->setLocalAudioPort(RANDOM_LOCAL_PORT);
	sipcall->setLocalExternAudioPort(RANDOM_LOCAL_PORT);
}

bool RtpTest::pjsipInit() {
	// Create memory cache for pool
	pj_caching_pool_init(&_cp, &pj_pool_factory_default_policy, 0);

	// Create memory pool for application.
	_pool = pj_pool_create(&_cp.factory, "rtpTest", 4000, 4000, NULL);

	if (!_pool) {
		_debug ("----- RtpTest: Could not initialize pjsip memory pool ------");
		return PJ_ENOMEM;
	}

	return true;
}

void RtpTest::testRtpInitClose() {
	_debug ("-------------------- RtpTest::testRtpInitClose --------------------\n");

	audiortp = new AudioRtpFactory();

	try {
		_debug ("-------- Open Rtp Session ----------");
		audiortp->initAudioRtpConfig(sipcall);
		audiortp->initAudioRtpSession(sipcall);
		//AudioCodecType codecType = PAYLOAD_CODEC_ULAW;
		//AudioCodec* audioCodec = Manager::instance().getCodecDescriptorMap().instantiateCodec(codecType);
		//audiortp->start(audioCodec);

	} catch (...) {
		_debug ("!!! Exception occured while Oppenning Rtp !!!");
		CPPUNIT_ASSERT(false);

	}

	CPPUNIT_ASSERT (audiortp != NULL);

	sleep(1);

	_debug ("------ RtpTest::testRtpClose() ------");

	try {
		_debug ("------ Close Rtp Session -------");
		audiortp->stop();

	} catch (...) {

		_debug ("!!! Exception occured while closing Rtp !!!");
		CPPUNIT_ASSERT(false);

	}

	delete audiortp;

	audiortp = NULL;
}

void RtpTest::tearDown() {
	delete sipcall;
	sipcall = NULL;
}
