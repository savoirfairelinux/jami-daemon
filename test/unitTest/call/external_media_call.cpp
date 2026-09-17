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
static const std::string kExternalOffer = [] {
    std::string offer
        = "v=0\r\n"
      "o=- 3899017587 3899017587 IN IP4 127.0.0.1\r\n"
      "s=-\r\n"
      "t=0 0\r\n"
      "a=group:BUNDLE 0\r\n"
      "a=msid-semantic: WMS *\r\n"
      "m=audio 10024 UDP/TLS/RTP/SAVPF 111\r\n"
      "c=IN IP4 127.0.0.1\r\n"
      "a=rtcp:10024 IN IP4 127.0.0.1\r\n";
    for (unsigned i = 0; i < 96; ++i) {
        offer += "a=candidate:" + std::to_string(i + 1) + " 1 UDP 2130706431 127.0.0.1 "
                 + std::to_string(10024 + i) + " typ host\r\n";
    }
    offer +=
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
    return offer;
}();

static std::string
makeExternalAnswer(std::string offer)
{
    auto replace = [&offer](std::string_view from, std::string_view to) {
        if (auto pos = offer.find(from); pos != std::string::npos)
            offer.replace(pos, from.size(), to);
    };
    replace("a=setup:actpass", "a=setup:active");
    replace("a=ice-ufrag:", "a=ice-ufrag:web");
    replace("a=ice-pwd:", "a=ice-pwd:web");
    return offer;
}

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
    CPPUNIT_ASSERT(kExternalOffer.size() > 4096);

    std::mutex mtx;
    std::condition_variable cv;
    auto waitFor = [&](auto predicate) {
        std::unique_lock lk {mtx};
        return cv.wait_for(lk, 30s, std::move(predicate));
    };
    auto readState = [&](const auto& value) {
        std::lock_guard lk {mtx};
        return value;
    };
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string bobCallId;
    std::string bobRemoteSdp;
    std::string aliceRemoteSdp;
    std::string aliceReoffer;
    std::string aliceCallState;
    std::atomic<int> callStopped {0};

    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            std::lock_guard lk {mtx};
            if (accountId == bobId)
                bobCallId = callId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::RemoteSdpReceived>(
        [&](const std::string& accountId, const std::string&, const std::string& sdp) {
            std::lock_guard lk {mtx};
            if (accountId == bobId)
                bobRemoteSdp = sdp;
            else if (accountId == aliceId) {
                if (aliceRemoteSdp.empty())
                    aliceRemoteSdp = sdp;
                else
                    aliceReoffer = sdp;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId, const std::string&, const std::string& state, signed) {
            std::lock_guard lk {mtx};
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
    CPPUNIT_ASSERT(waitFor([&] { return not bobCallId.empty() and not bobRemoteSdp.empty(); }));
    const auto receivedBobCallId = readState(bobCallId);
    const auto receivedBobSdp = readState(bobRemoteSdp);
    CPPUNIT_ASSERT(receivedBobSdp.find("ice-ufrag:extufrag") != std::string::npos);
    CPPUNIT_ASSERT(receivedBobSdp.find("opus/48000/2") != std::string::npos);

    // Bob answers with regular local media.
    std::vector<std::map<std::string, std::string>> answerMedia;
    answerMedia.emplace_back(std::map<std::string, std::string> {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                                                  libjami::Media::MediaAttributeValue::AUDIO},
                                                                 {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                                                                 {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}});
    libjami::acceptWithMedia(bobId, receivedBobCallId, answerMedia);

    // Alice receives Bob's SDP answer through the RemoteSdpReceived signal.
    CPPUNIT_ASSERT(waitFor([&] { return not aliceRemoteSdp.empty(); }));
    const auto receivedAliceSdp = readState(aliceRemoteSdp);
    CPPUNIT_ASSERT(receivedAliceSdp.find("ice-ufrag") != std::string::npos);
    CPPUNIT_ASSERT(receivedAliceSdp.find("fingerprint") != std::string::npos);
    CPPUNIT_ASSERT(receivedAliceSdp.find("opus/48000/2") != std::string::npos);

    // The call is established at the SIP level.
    CPPUNIT_ASSERT(waitFor([&] { return aliceCallState == "CURRENT"; }));
    std::this_thread::sleep_for(3s);

    auto updatedMedia = answerMedia;
    updatedMedia.emplace_back(
        libjami::MediaMap {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                            libjami::Media::MediaAttributeValue::VIDEO},
                           {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                           {libjami::Media::MediaAttributeKey::MUTED, "false"},
                           {libjami::Media::MediaAttributeKey::SOURCE, "test"},
                           {libjami::Media::MediaAttributeKey::LABEL, "video_0"}});
    CPPUNIT_ASSERT(libjami::requestMediaChange(bobId, receivedBobCallId, updatedMedia));

    CPPUNIT_ASSERT(waitFor([&] { return not aliceReoffer.empty(); }));
    const auto receivedAliceReoffer = readState(aliceReoffer);
    CPPUNIT_ASSERT(receivedAliceReoffer.find("ice-ufrag") != std::string::npos);
    CPPUNIT_ASSERT(libjami::answerMediaChangeWithExternalSdp(
        aliceId, aliceCallId, makeExternalAnswer(receivedAliceReoffer)));

    auto bobCall = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(receivedBobCallId));
    CPPUNIT_ASSERT(bobCall);
    bool answerReceived = false;
    for (unsigned i = 0; i < 100 and not answerReceived; ++i) {
        std::this_thread::sleep_for(100ms);
        answerReceived = Sdp::toString(bobCall->getSDP().getActiveRemoteSdpSession()).find("a=ice-ufrag:web")
                         != std::string::npos;
    }
    CPPUNIT_ASSERT(answerReceived);

    libjami::hangUp(aliceId, aliceCallId);
    CPPUNIT_ASSERT(waitFor([&] { return callStopped >= 2; }));

    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ExternalMediaCallTest::name());
