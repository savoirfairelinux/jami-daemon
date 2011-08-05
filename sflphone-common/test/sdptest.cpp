/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include "sdptest.h"
#include <iostream>
#include <cstring>

#include "audio/codecs/audiocodec.h"


enum session_type
{
    REMOTE_OFFER,
    LOCAL_OFFER,
};

static const char *sdp_answer1 = "v=0\r\n"
                           "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
                           "s= \r\n"
                           "c=IN IP4 host.example.com\r\n"
                           "t=0 0\r\n"
                           "m=audio 49920 RTP/AVP 0\r\n"
                           "a=rtpmap:0 PCMU/8000\r\n"
                           "m=video 0 RTP/AVP 31\r\n"
                           "m=video 53002 RTP/AVP 32\r\n"
                           "a=rtpmap:32 MPV/90000\r\n";

static const char *sdp_offer1 = "v=0\r\n"
                          "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
                          "s= \r\n"
                          "c=IN IP4 host.example.com\r\n"
                          "t=0 0\r\n"
                          "m=audio 49920 RTP/AVP 0\r\n"
                          "a=rtpmap:0 PCMU/8000\r\n"
                          "m=video 0 RTP/AVP 31\r\n"
                          "m=video 53002 RTP/AVP 32\r\n"
                          "a=rtpmap:32 MPV/90000\r\n";

static const char *sdp_answer2 = "v=0\r\n"
                           "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
                           "s= \r\n"
                           "c=IN IP4 host.example.com\r\n"
                           "t=0 0\r\n"
                           "m=audio 49920 RTP/AVP 3 97 9\r\n"
                           "a=rtpmap:3 GSM/8000\r\n"
		                   "a=rtpmap:97 iLBC/8000\r\n"
		                   "a=rtpmap:9 G722/8000\r\n"
                           "m=video 0 RTP/AVP 31\r\n"
                           "m=video 53002 RTP/AVP 32\r\n"
                           "a=rtpmap:32 MPV/90000\r\n";

static const char *sdp_offer2 = "v=0\r\n"
                          "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
                          "s= \r\n"
                          "c=IN IP4 host.example.com\r\n"
                          "t=0 0\r\n"
                          "m=audio 49920 RTP/AVP 3 97 9\r\n"
                          "a=rtpmap:3 GSM/8000\r\n"
						  "a=rtpmap:97 iLBC/8000\r\n"
                          "a=rtpmap:9 G722/8000\r\n"
                          "m=video 0 RTP/AVP 31\r\n"
                          "m=video 53002 RTP/AVP 32\r\n"
                          "a=rtpmap:32 MPV/90000\r\n";

static const char *sdp_reinvite = "v=0\r\n"
                            "o=bob 2890844730 2890844730 IN IP4 host.example.com\r\n"
                            "s= \r\n"
                            "c=IN IP4 host.exampleReinvite.com\r\n"
                            "t=0 0\r\n"
                            "m=audio 42445 RTP/AVP 0\r\n"
                            "a=rtpmap:0 PCMU/8000\r\n"
                            "m=video 0 RTP/AVP 31\r\n"
                            "m=video 53002 RTP/AVP 32\r\n"
                            "a=rtpmap:32 MPV/90000\r\n";


void SDPTest::setUp()
{
	pj_caching_pool_init (&_poolCache, &pj_pool_factory_default_policy, 0);

	_testPool = pj_pool_create (&_poolCache.factory, "sdptest", 4000, 4000, NULL);

	_session = new Sdp(_testPool);
}

void SDPTest::tearDown()
{
	delete _session;
	_session = NULL;

    pj_pool_release(_testPool);
}


void SDPTest::testInitialOfferFirstCodec ()
{
	std::cout << "------------ SDPTest::testInitialOfferFirstCodec --------------" << std::endl;

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getLocalIP() == "");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "");

	CodecOrder codecSelection;
    pjmedia_sdp_session *remoteAnswer;

	codecSelection.push_back(PAYLOAD_CODEC_ULAW);
	codecSelection.push_back(PAYLOAD_CODEC_ALAW);
	codecSelection.push_back(PAYLOAD_CODEC_G722);

	_session->setLocalIP("127.0.0.1");
	_session->setLocalPublishedAudioPort(49567);

    _session->createOffer(codecSelection);

    // pjmedia_sdp_parse(_testPool, test[0].offer_answer[0].sdp2, strlen(test[0].offer_answer[0].sdp2), &remoteAnswer);
    pjmedia_sdp_parse(_testPool, (char*)sdp_answer1, strlen(sdp_answer1), &remoteAnswer);

    _session->receivingAnswerAfterInitialOffer(remoteAnswer);

    _session->startNegotiation();

    _session->updateInternalState();

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(_session->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(_session->getSessionMedia()->getMimeSubtype() == "PCMU");

}

void SDPTest::testInitialAnswerFirstCodec ()
{
	std::cout << "------------ SDPTest::testInitialAnswerFirstCodec -------------" << std::endl;

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getLocalIP() == "");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "");

	CodecOrder codecSelection;
    pjmedia_sdp_session *remoteOffer;

	codecSelection.push_back(PAYLOAD_CODEC_ULAW);
	codecSelection.push_back(PAYLOAD_CODEC_ALAW);
	codecSelection.push_back(PAYLOAD_CODEC_G722);

    pjmedia_sdp_parse(_testPool, (char*)sdp_offer1, strlen(sdp_offer1), &remoteOffer);

    _session->setLocalIP("127.0.0.1");
	_session->setLocalPublishedAudioPort(49567);

    _session->receiveOffer(remoteOffer, codecSelection);

    _session->startNegotiation();

    _session->updateInternalState();

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(_session->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(_session->getSessionMedia()->getMimeSubtype() == "PCMU");

}

void SDPTest::testInitialOfferLastCodec ()
{
	std::cout << "------------ SDPTest::testInitialOfferLastCodec --------------------" << std::endl;

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getLocalIP() == "");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "");

	CodecOrder codecSelection;
    pjmedia_sdp_session *remoteAnswer;

	codecSelection.push_back(PAYLOAD_CODEC_ULAW);
	codecSelection.push_back(PAYLOAD_CODEC_ALAW);
	codecSelection.push_back(PAYLOAD_CODEC_G722);

	_session->setLocalIP("127.0.0.1");
	_session->setLocalPublishedAudioPort(49567);

    _session->createOffer(codecSelection);

    // pjmedia_sdp_parse(_testPool, test[0].offer_answer[0].sdp2, strlen(test[0].offer_answer[0].sdp2), &remoteAnswer);
    pjmedia_sdp_parse(_testPool, (char*)sdp_answer2, strlen(sdp_answer2), &remoteAnswer);

    _session->receivingAnswerAfterInitialOffer(remoteAnswer);

    _session->startNegotiation();

    _session->updateInternalState();

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(_session->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(_session->getSessionMedia()->getMimeSubtype() == "G722");

}

void SDPTest::testInitialAnswerLastCodec ()
{
	std::cout << "------------ SDPTest::testInitialAnswerLastCodec ------------" << std::endl;

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getLocalIP() == "");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "");

	CodecOrder codecSelection;
    pjmedia_sdp_session *remoteOffer;

	codecSelection.push_back(PAYLOAD_CODEC_ULAW);
	codecSelection.push_back(PAYLOAD_CODEC_ALAW);
	codecSelection.push_back(PAYLOAD_CODEC_G722);

    pjmedia_sdp_parse(_testPool, (char*)sdp_offer2, strlen(sdp_offer2), &remoteOffer);

    _session->setLocalIP("127.0.0.1");
	_session->setLocalPublishedAudioPort(49567);

    _session->receiveOffer(remoteOffer, codecSelection);

    _session->startNegotiation();

    _session->updateInternalState();

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(_session->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(_session->getSessionMedia()->getMimeSubtype() == "G722");

}


void SDPTest::testReinvite ()
{
	std::cout << "------------ SDPTest::testReinvite --------------------" << std::endl;

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(_session->getLocalIP() == "");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "");

	CodecOrder codecSelection;
    pjmedia_sdp_session *remoteAnswer;
    pjmedia_sdp_session *reinviteOffer;

	codecSelection.push_back(PAYLOAD_CODEC_ULAW);
	codecSelection.push_back(PAYLOAD_CODEC_ALAW);
	codecSelection.push_back(PAYLOAD_CODEC_G722);

	_session->setLocalIP("127.0.0.1");
	_session->setLocalPublishedAudioPort(49567);

    _session->createOffer(codecSelection);

    // pjmedia_sdp_parse(_testPool, test[0].offer_answer[0].sdp2, strlen(test[0].offer_answer[0].sdp2), &remoteAnswer);
    pjmedia_sdp_parse(_testPool, (char*)sdp_answer1, strlen(sdp_answer1), &remoteAnswer);

    _session->receivingAnswerAfterInitialOffer(remoteAnswer);

    _session->startNegotiation();

    _session->updateInternalState();

    CPPUNIT_ASSERT(_session->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(_session->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(_session->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(_session->getSessionMedia()->getMimeSubtype() == "PCMU");

    pjmedia_sdp_parse(_testPool, (char*)sdp_reinvite, strlen(sdp_reinvite), &reinviteOffer);

    _session->receiveOffer(reinviteOffer, codecSelection);

    _session->startNegotiation();

    _session->updateInternalState();

    CPPUNIT_ASSERT(_session->getRemoteAudioPort() == 42445);
    CPPUNIT_ASSERT(_session->getRemoteIP() == "host.exampleReinvite.com");

}
