/*
 *  Copyright (C) 2004-2026 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "manager.h"
#include "jamidht/jamiaccount.h"
#include "media/media_attribute.h"
#include "sip/sipcall.h"
#include "../../test_runner.h"
#include "jami.h"
#include "jami/callmanager_interface.h"
#include "media_const.h"
#include "common.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <string>

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

// A browser-style WebRTC audio offer: BUNDLE, rtcp-mux, ICE credentials,
// DTLS fingerprint and an Opus payload. The candidate points to a port
// nobody listens on: this test validates signaling only, media checks are
// covered by the webrtc_interop harness.
static const char* const kExternalOffer
    = "v=0\r\n"
      "o=- 3899017587 3899017587 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=msid-semantic: WMS *\r\n"
      "m=audio 10024 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 127.0.0.1\r\n"
      "a=rtcp:10024 IN IP4 127.0.0.1\r\n"
      "a=candidate:1 1 UDP 2130706431 127.0.0.1 10024 typ host\r\n"
      "a=ice-ufrag:extufrag\r\n"
      "a=ice-pwd:extpwd012345678901234567\r\n"
      "a=fingerprint:sha-256 "
      "7B:8B:F0:65:5F:78:E2:51:3B:AC:6F:F3:3F:46:1B:35:DC:B8:5F:64:1A:24:C2:43:F0:A1:58:D0:A1:2C:19:08\r\n"
      "a=setup:actpass\r\n"
      "a=mid:0\r\n"
      "a=sendrecv\r\n"
      "a=rtcp-mux\r\n"
      "a=rtpmap:111 opus/48000/2\r\n"
      "a=fmtp:111 minptime=10;useinbandfec=1\r\n";

class ExternalMediaCallTest : public CppUnit::TestFixture
{
public:
    ExternalMediaCallTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ExternalMediaCallTest() { libjami::fini(); }
    static std::string name() { return "ExternalMediaCall"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testOutgoingExternalMediaCall();

    CPPUNIT_TEST_SUITE(ExternalMediaCallTest);
    CPPUNIT_TEST(testOutgoingExternalMediaCall);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ExternalMediaCallTest, ExternalMediaCallTest::name());

void
ExternalMediaCallTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
ExternalMediaCallTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
ExternalMediaCallTest::testOutgoingExternalMediaCall()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string bobCallId;
    std::string bobRemoteSdp;
    std::string aliceRemoteSdp;
    std::string aliceCallState;
    std::atomic<int> callStopped {0};

    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            if (accountId == bobId)
                bobCallId = callId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::RemoteSdpReceived>(
        [&](const std::string& accountId, const std::string&, const std::string& sdp) {
            if (accountId == bobId)
                bobRemoteSdp = sdp;
            else if (accountId == aliceId)
                aliceRemoteSdp = sdp;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId, const std::string&, const std::string& state, signed) {
            if (accountId == aliceId)
                aliceCallState = state;
            if (state == "OVER")
                callStopped += 1;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    // Alice places a call carrying the external (browser) SDP offer.
    auto aliceCallId = libjami::placeCallWithExternalMedia(aliceId, bobUri, kExternalOffer);
    CPPUNIT_ASSERT(not aliceCallId.empty());

    // Bob receives the call, along with the raw remote SDP offer.
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return not bobCallId.empty() and not bobRemoteSdp.empty(); }));
    CPPUNIT_ASSERT(bobRemoteSdp.find("ice-ufrag:extufrag") != std::string::npos);
    CPPUNIT_ASSERT(bobRemoteSdp.find("opus/48000/2") != std::string::npos);

    // Bob answers with regular local media.
    std::vector<std::map<std::string, std::string>> answerMedia;
    answerMedia.emplace_back(std::map<std::string, std::string> {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                                                  libjami::Media::MediaAttributeValue::AUDIO},
                                                                 {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                                                                 {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}});
    libjami::acceptWithMedia(bobId, bobCallId, answerMedia);

    // Alice receives Bob's SDP answer through the RemoteSdpReceived signal.
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return not aliceRemoteSdp.empty(); }));
    CPPUNIT_ASSERT(aliceRemoteSdp.find("ice-ufrag") != std::string::npos);
    CPPUNIT_ASSERT(aliceRemoteSdp.find("fingerprint") != std::string::npos);
    CPPUNIT_ASSERT(aliceRemoteSdp.find("opus/48000/2") != std::string::npos);

    // The call is established at the SIP level.
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceCallState == "CURRENT"; }));

    libjami::hangUp(aliceId, aliceCallId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped >= 2; }));

    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ExternalMediaCallTest::name());
