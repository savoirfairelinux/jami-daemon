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


enum session_type {
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
    pj_caching_pool_init(&poolCache_, &pj_pool_factory_default_policy, 0);

    testPool_ = pj_pool_create(&poolCache_.factory, "sdptest", 4000, 4000, NULL);

    session_ = new Sdp(testPool_);
}

void SDPTest::tearDown()
{
    delete session_;
    session_ = NULL;
    pj_pool_release(testPool_);
}

void SDPTest::receiveAnswerAfterInitialOffer(const pjmedia_sdp_session* remote)
{
    assert(pjmedia_sdp_neg_get_state(session_->negotiator_) == PJMEDIA_SDP_NEG_STATE_LOCAL_OFFER);
    assert(pjmedia_sdp_neg_set_remote_answer(session_->memPool_, session_->negotiator_, remote) == PJ_SUCCESS);
    assert(pjmedia_sdp_neg_get_state(session_->negotiator_) == PJMEDIA_SDP_NEG_STATE_WAIT_NEGO);
}

void SDPTest::testInitialOfferFirstCodec()
{
    std::cout << "------------ SDPTest::testInitialOfferFirstCodec --------------" << std::endl;

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getLocalIP() == "");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "");

    CodecOrder codecSelection;
    pjmedia_sdp_session *remoteAnswer;

    codecSelection.push_back(PAYLOAD_CODEC_ULAW);
    codecSelection.push_back(PAYLOAD_CODEC_ALAW);
    codecSelection.push_back(PAYLOAD_CODEC_G722);

    session_->setLocalIP("127.0.0.1");
    session_->setLocalPublishedAudioPort(49567);

    session_->createOffer(codecSelection);

    // pjmedia_sdp_parse(testPool_, test[0].offer_answer[0].sdp2, strlen(test[0].offer_answer[0].sdp2), &remoteAnswer);
    pjmedia_sdp_parse(testPool_, (char*)sdp_answer1, strlen(sdp_answer1), &remoteAnswer);

    receiveAnswerAfterInitialOffer(remoteAnswer);
    session_->startNegotiation();

    session_->setMediaTransportInfoFromRemoteSdp();

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(session_->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(session_->getSessionMedia()->getMimeSubtype() == "PCMU");

}

void SDPTest::testInitialAnswerFirstCodec()
{
    std::cout << "------------ SDPTest::testInitialAnswerFirstCodec -------------" << std::endl;

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getLocalIP() == "");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "");

    CodecOrder codecSelection;
    pjmedia_sdp_session *remoteOffer;

    codecSelection.push_back(PAYLOAD_CODEC_ULAW);
    codecSelection.push_back(PAYLOAD_CODEC_ALAW);
    codecSelection.push_back(PAYLOAD_CODEC_G722);

    pjmedia_sdp_parse(testPool_, (char*)sdp_offer1, strlen(sdp_offer1), &remoteOffer);

    session_->setLocalIP("127.0.0.1");
    session_->setLocalPublishedAudioPort(49567);

    session_->receiveOffer(remoteOffer, codecSelection);

    session_->startNegotiation();

    session_->setMediaTransportInfoFromRemoteSdp();

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(session_->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(session_->getSessionMedia()->getMimeSubtype() == "PCMU");

}

void SDPTest::testInitialOfferLastCodec()
{
    std::cout << "------------ SDPTest::testInitialOfferLastCodec --------------------" << std::endl;

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getLocalIP() == "");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "");

    CodecOrder codecSelection;
    pjmedia_sdp_session *remoteAnswer;

    codecSelection.push_back(PAYLOAD_CODEC_ULAW);
    codecSelection.push_back(PAYLOAD_CODEC_ALAW);
    codecSelection.push_back(PAYLOAD_CODEC_G722);

    session_->setLocalIP("127.0.0.1");
    session_->setLocalPublishedAudioPort(49567);

    session_->createOffer(codecSelection);

    // pjmedia_sdp_parse(testPool_, test[0].offer_answer[0].sdp2, strlen(test[0].offer_answer[0].sdp2), &remoteAnswer);
    pjmedia_sdp_parse(testPool_, (char*)sdp_answer2, strlen(sdp_answer2), &remoteAnswer);

    receiveAnswerAfterInitialOffer(remoteAnswer);
    session_->startNegotiation();

    session_->setMediaTransportInfoFromRemoteSdp();

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(session_->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(session_->getSessionMedia()->getMimeSubtype() == "G722");

}

void SDPTest::testInitialAnswerLastCodec()
{
    std::cout << "------------ SDPTest::testInitialAnswerLastCodec ------------" << std::endl;

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getLocalIP() == "");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "");

    CodecOrder codecSelection;
    pjmedia_sdp_session *remoteOffer;

    codecSelection.push_back(PAYLOAD_CODEC_ULAW);
    codecSelection.push_back(PAYLOAD_CODEC_ALAW);
    codecSelection.push_back(PAYLOAD_CODEC_G722);

    pjmedia_sdp_parse(testPool_, (char*)sdp_offer2, strlen(sdp_offer2), &remoteOffer);

    session_->setLocalIP("127.0.0.1");
    session_->setLocalPublishedAudioPort(49567);

    session_->receiveOffer(remoteOffer, codecSelection);

    session_->startNegotiation();

    session_->setMediaTransportInfoFromRemoteSdp();

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(session_->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(session_->getSessionMedia()->getMimeSubtype() == "G722");

}


void SDPTest::testReinvite()
{
    std::cout << "------------ SDPTest::testReinvite --------------------" << std::endl;

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 0);
    CPPUNIT_ASSERT(session_->getLocalIP() == "");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "");

    CodecOrder codecSelection;
    pjmedia_sdp_session *remoteAnswer;
    pjmedia_sdp_session *reinviteOffer;

    codecSelection.push_back(PAYLOAD_CODEC_ULAW);
    codecSelection.push_back(PAYLOAD_CODEC_ALAW);
    codecSelection.push_back(PAYLOAD_CODEC_G722);

    session_->setLocalIP("127.0.0.1");
    session_->setLocalPublishedAudioPort(49567);

    session_->createOffer(codecSelection);

    // pjmedia_sdp_parse(testPool_, test[0].offer_answer[0].sdp2, strlen(test[0].offer_answer[0].sdp2), &remoteAnswer);
    pjmedia_sdp_parse(testPool_, (char*)sdp_answer1, strlen(sdp_answer1), &remoteAnswer);

    receiveAnswerAfterInitialOffer(remoteAnswer);
    session_->startNegotiation();

    session_->setMediaTransportInfoFromRemoteSdp();

    CPPUNIT_ASSERT(session_->getLocalPublishedAudioPort() == 49567);
    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 49920);
    CPPUNIT_ASSERT(session_->getLocalIP() == "127.0.0.1");
    CPPUNIT_ASSERT(session_->getRemoteIP() == "host.example.com");
    CPPUNIT_ASSERT(session_->getSessionMedia()->getMimeSubtype() == "PCMU");

    pjmedia_sdp_parse(testPool_, (char*) sdp_reinvite, strlen(sdp_reinvite), &reinviteOffer);

    session_->receiveOffer(reinviteOffer, codecSelection);

    session_->startNegotiation();

    session_->setMediaTransportInfoFromRemoteSdp();

    CPPUNIT_ASSERT(session_->getRemoteAudioPort() == 42445);
    CPPUNIT_ASSERT(session_->getRemoteIP() == "host.exampleReinvite.com");
}
