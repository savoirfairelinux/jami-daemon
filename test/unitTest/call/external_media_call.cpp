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
#include "conference.h"
#include "jamidht/jamiaccount.h"
#include "jamidht/conversation_module.h"
#include "media/audio/audio_rtp_session.h"
#include "media/audio/ringbufferpool.h"
#ifdef ENABLE_VIDEO
#include "media/video/video_rtp_session.h"
#endif
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

static const std::string kExternalVideoOffer = [] {
    auto offer = kExternalOffer;
    const auto bundle = offer.find("a=group:BUNDLE 0\r\n");
    offer.replace(bundle, std::string("a=group:BUNDLE 0\r\n").size(), "a=group:BUNDLE 0 1\r\n");
    offer += "m=video 10025 UDP/TLS/RTP/SAVPF 96\r\n"
             "c=IN IP4 127.0.0.1\r\n"
             "a=rtcp:10025 IN IP4 127.0.0.1\r\n"
             "a=candidate:1 1 UDP 2130706431 127.0.0.1 10025 typ host\r\n"
             "a=ice-ufrag:extufrag\r\n"
             "a=ice-pwd:extpwd012345678901234567\r\n"
             "a=fingerprint:sha-256 "
             "7B:8B:F0:65:5F:78:E2:51:3B:AC:6F:F3:3F:46:1B:35:DC:B8:5F:64:1A:24:C2:43:F0:A1:58:D0:A1:2C:19:08\r\n"
             "a=setup:actpass\r\n"
             "a=mid:1\r\n"
             "a=sendrecv\r\n"
             "a=rtcp-mux\r\n"
             "a=rtpmap:96 H264/90000\r\n"
             "a=fmtp:96 profile-level-id=428029;packetization-mode=1\r\n";
    return offer;
}();

static std::string
makeExternalAnswer(std::string offer)
{
    auto replace = [&offer](std::string_view from, std::string_view to) {
        for (auto pos = offer.find(from); pos != std::string::npos; pos = offer.find(from, pos + to.size()))
            offer.replace(pos, from.size(), to);
    };
    replace("a=setup:actpass", "a=setup:active");
    replace("a=ice-ufrag:", "a=ice-ufrag:web");
    replace("a=ice-pwd:", "a=ice-pwd:web");
    return offer;
}

static std::string
makeReturnOffer(std::string offer)
{
    auto replaceAll = [&offer](std::string_view from, std::string_view to) {
        for (auto pos = offer.find(from); pos != std::string::npos; pos = offer.find(from, pos + to.size()))
            offer.replace(pos, from.size(), to);
    };
    replaceAll("3899017587 IN", "3899017588 IN");
    replaceAll("extufrag", "returnufrag");
    replaceAll("extpwd012345678901234567", "returnpwd012345678901234567");
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
    void testRejectUnmigratedExternalMedia();
    void testOutgoingExternalMediaCall();
    void testPromoteExternalMediaCall();
    void testPromoteExternalVideoCall();
    void testPromotionCancellation();
    void testReturnExternalMediaAudio();
    void testReturnExternalMediaVideo();
    void testNativeCallMediaReadiness();
    void testBrowserConferenceParticipantAudio();
    void testBrowserConferenceParticipantVideo();
    void runPromotion(bool cancel, bool video = false, bool demote = false);

    CPPUNIT_TEST_SUITE(ExternalMediaCallTest);
    CPPUNIT_TEST(testRejectUnmigratedExternalMedia);
    CPPUNIT_TEST(testOutgoingExternalMediaCall);
    CPPUNIT_TEST(testPromoteExternalMediaCall);
#ifdef ENABLE_VIDEO
    CPPUNIT_TEST(testPromoteExternalVideoCall);
#endif
    CPPUNIT_TEST(testPromotionCancellation);
    CPPUNIT_TEST(testReturnExternalMediaAudio);
#ifdef ENABLE_VIDEO
    CPPUNIT_TEST(testReturnExternalMediaVideo);
#endif
    CPPUNIT_TEST(testNativeCallMediaReadiness);
    CPPUNIT_TEST(testBrowserConferenceParticipantAudio);
#ifdef ENABLE_VIDEO
    CPPUNIT_TEST(testBrowserConferenceParticipantVideo);
#endif
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
ExternalMediaCallTest::testRejectUnmigratedExternalMedia()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    CPPUNIT_ASSERT(account);
    auto call = std::make_shared<SIPCall>(account,
                                          "external-media-guard",
                                          Call::CallType::OUTGOING,
                                          std::vector<libjami::MediaMap> {
                                              {{libjami::Media::MediaAttributeKey::EXTERNAL_SDP, kExternalOffer}}});
    auto conf = std::make_shared<Conference>(account, "browser-host-guard");
    CPPUNIT_ASSERT(call->hasExternalMedia());
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"),
                         libjami::getExternalMediaPromotionState(aliceId, call->getCallId()));
    CPPUNIT_ASSERT(!call->isNativeMediaReady());
    CPPUNIT_ASSERT(call->getRtpSessionList().empty());
    std::vector<libjami::MediaMap> media {
        {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
         {libjami::Media::MediaAttributeKey::ENABLED, "true"},
         {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}}};
    CPPUNIT_ASSERT(not call->requestMediaChange(media));
    CPPUNIT_ASSERT(not Manager::instance().addSubCall(*call, *conf));
    CPPUNIT_ASSERT(call->hasExternalMedia());
    CPPUNIT_ASSERT(call->getRtpSessionList().empty());
    CPPUNIT_ASSERT(not call->isConferenceParticipant());
    CPPUNIT_ASSERT(conf->getSubCalls().empty());
}

void
ExternalMediaCallTest::testOutgoingExternalMediaCall()
{
    add_confirmed_contact(aliceId, bobId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    CPPUNIT_ASSERT(kExternalOffer.size() > 4096);

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
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
            if (accountId == bobId)
                bobCallId = callId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::RemoteSdpReceived>(
        [&](const std::string& accountId, const std::string&, const std::string& sdp) {
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
    std::this_thread::sleep_for(3s);
    CPPUNIT_ASSERT_EQUAL(std::string("AUDIO_ONLY"), libjami::getExternalMediaPromotionState(aliceId, aliceCallId));

    auto aliceCall = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(aliceCallId));
    CPPUNIT_ASSERT(aliceCall);
    CPPUNIT_ASSERT(aliceCall->hasExternalMedia());
    CPPUNIT_ASSERT(aliceCall->getRtpSessionList().empty());
    CPPUNIT_ASSERT(not aliceCall->requestMediaChange(answerMedia));
    CPPUNIT_ASSERT(aliceCall->hasExternalMedia());
    CPPUNIT_ASSERT(aliceCall->getRtpSessionList().empty());
    CPPUNIT_ASSERT(not libjami::addNativeToBrowserConference(aliceId, aliceCallId, "missing-host"));
    CPPUNIT_ASSERT(not aliceCall->isConferenceParticipant());

    auto updatedMedia = answerMedia;
    updatedMedia.emplace_back(
        libjami::MediaMap {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
                           {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                           {libjami::Media::MediaAttributeKey::MUTED, "false"},
                           {libjami::Media::MediaAttributeKey::SOURCE, "test"},
                           {libjami::Media::MediaAttributeKey::LABEL, "video_0"}});
    CPPUNIT_ASSERT(libjami::requestMediaChange(bobId, bobCallId, updatedMedia));

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return not aliceReoffer.empty(); }));
    CPPUNIT_ASSERT(aliceReoffer.find("ice-ufrag") != std::string::npos);
    CPPUNIT_ASSERT(aliceReoffer.find("a=mid:0\r\n") != std::string::npos);
    CPPUNIT_ASSERT(aliceReoffer.find("a=mid:audio_0\r\n") == std::string::npos);
    CPPUNIT_ASSERT(aliceReoffer.find("m=audio") != std::string::npos);
    CPPUNIT_ASSERT(aliceReoffer.find("UDP/TLS/RTP/SAVPF 111") != std::string::npos);
    CPPUNIT_ASSERT(aliceReoffer.find("a=rtpmap:111 opus/48000/2\r\n") != std::string::npos);
    CPPUNIT_ASSERT(aliceReoffer.find("a=rtpmap:104 opus/48000/2\r\n") == std::string::npos);
    CPPUNIT_ASSERT(
        libjami::answerMediaChangeRequestWithExternalMedia(aliceId, aliceCallId, makeExternalAnswer(aliceReoffer)));

    auto bobCall = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(bobCallId));
    CPPUNIT_ASSERT(bobCall);
    // Browser tracks are muted outside the daemon, but native peers still
    // need the same SIP mute notification as for daemon-owned capture.
    for (bool muted : {true, false}) {
        CPPUNIT_ASSERT(libjami::muteLocalMedia(aliceId, aliceCallId,
                                             libjami::Media::Details::MEDIA_TYPE_AUDIO, muted));
        bool received = false;
        for (unsigned i = 0; i < 100 && !received; ++i) {
            std::this_thread::sleep_for(100ms);
            received = bobCall->isPeerMuted() == muted;
        }
        CPPUNIT_ASSERT(received);
        const auto audioStreams = aliceCall->getAudioStreams();
        CPPUNIT_ASSERT_EQUAL(size_t(1), audioStreams.size());
        CPPUNIT_ASSERT_EQUAL(muted, audioStreams.begin()->second);
        CPPUNIT_ASSERT(aliceCall->getRtpSessionList().empty());
    }
    bool answerReceived = false;
    for (unsigned i = 0; i < 100 and not answerReceived; ++i) {
        std::this_thread::sleep_for(100ms);
        answerReceived = Sdp::toString(bobCall->getSDP().getActiveRemoteSdpSession()).find("a=ice-ufrag:web")
                         != std::string::npos;
    }
    CPPUNIT_ASSERT(answerReceived);
    CPPUNIT_ASSERT_EQUAL(std::string("UNSUPPORTED_MEDIA"),
                         libjami::getExternalMediaPromotionState(aliceId, aliceCallId));
    CPPUNIT_ASSERT(aliceCall->hasExternalMedia());

    libjami::hangUp(aliceId, aliceCallId);
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return callStopped >= 2; }));

    libjami::unregisterSignalHandlers();
}

void
ExternalMediaCallTest::testPromoteExternalMediaCall()
{
    runPromotion(false);
}

void
ExternalMediaCallTest::testPromoteExternalVideoCall()
{
    runPromotion(false, true);
}

void
ExternalMediaCallTest::testPromotionCancellation()
{
    runPromotion(true);
}

void
ExternalMediaCallTest::testReturnExternalMediaAudio()
{
    runPromotion(false, false, true);
}

void
ExternalMediaCallTest::testReturnExternalMediaVideo()
{
    runPromotion(false, true, true);
}

void
ExternalMediaCallTest::runPromotion(bool cancel, bool video, bool demote)
{
    add_confirmed_contact(aliceId, bobId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    CPPUNIT_ASSERT(bobAccount);

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::string bobCallId;
    std::string aliceCallState;
    std::string accepted;
    std::string ready;
    std::string failure;
    std::string directAnswer;
    std::string directReady;
    std::string directFailure;
    bool bobOver = false;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    handlers.insert(
        libjami::exportable_callback<libjami::CallSignal::IncomingCall>([&](const std::string& account,
                                                                            const std::string& callId,
                                                                            const std::string&,
                                                                            const std::vector<libjami::MediaMap>&) {
            std::lock_guard guard(mtx);
            if (account == bobId)
                bobCallId = callId;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& account, const std::string&, const std::string& state, signed) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                aliceCallState = state;
            if (account == bobId && state == "OVER")
                bobOver = true;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::ExternalMediaPromotionAccepted>(
        [&](const std::string& account, const std::string&, const std::string& request) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                accepted = request;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::ExternalMediaPromotionReady>(
        [&](const std::string& account, const std::string&, const std::string& request) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                ready = request;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::ExternalMediaPromotionFailure>(
        [&](const std::string& account, const std::string&, const std::string& request, const std::string& reason) {
            std::lock_guard guard(mtx);
            if (account == aliceId && request == "promote")
                failure = reason;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::ExternalMediaDemotionAccepted>(
        [&](const std::string& account, const std::string&, const std::string& request, const std::string& answer) {
            std::lock_guard guard(mtx);
            if (account == aliceId && request == "return")
                directAnswer = answer;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::ExternalMediaDemotionReady>(
        [&](const std::string& account, const std::string&, const std::string& request) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                directReady = request;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::ExternalMediaDemotionFailure>(
        [&](const std::string& account, const std::string&, const std::string& request, const std::string& reason) {
            std::lock_guard guard(mtx);
            if (account == aliceId && request == "return")
                directFailure = reason;
            cv.notify_all();
        }));
    libjami::registerSignalHandlers(handlers);

    const auto aliceCallId = libjami::placeCallWithExternalMedia(aliceId,
                                                                 bobAccount->getUsername(),
                                                                 video ? kExternalVideoOffer : kExternalOffer);
    CPPUNIT_ASSERT(!aliceCallId.empty());
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !bobCallId.empty(); }));
    std::vector<libjami::MediaMap> media {
        {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
         {libjami::Media::MediaAttributeKey::ENABLED, "true"},
         {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}}};
    if (video)
        media.push_back({{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
                         {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                         {libjami::Media::MediaAttributeKey::MUTED, "false"},
                         {libjami::Media::MediaAttributeKey::SOURCE, "test"},
                         {libjami::Media::MediaAttributeKey::LABEL, "video_0"}});
    CPPUNIT_ASSERT(libjami::acceptWithMedia(bobId, bobCallId, media));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceCallState == "CURRENT"; }));
    std::this_thread::sleep_for(1s);
    auto aliceCall = std::dynamic_pointer_cast<SIPCall>(Manager::instance().getCallFromCallID(aliceCallId));
    CPPUNIT_ASSERT(aliceCall && aliceCall->hasExternalMedia());
    CPPUNIT_ASSERT_EQUAL(video ? std::string("AUDIO_VIDEO") : std::string("AUDIO_ONLY"),
                         libjami::getExternalMediaPromotionState(aliceId, aliceCallId));
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getNativeCallMediaState(aliceId, aliceCallId));
    if (video) {
        lk.unlock();
        CPPUNIT_ASSERT(!libjami::requestExternalMediaPromotion(aliceId, aliceCallId, "no-mixer"));
        CPPUNIT_ASSERT(aliceCall->hasExternalMedia());
        CPPUNIT_ASSERT(aliceCall->getRtpSessionList().empty());
        CPPUNIT_ASSERT(Manager::instance().getCallFromCallID(aliceCallId) == aliceCall);
        CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getNativeCallMediaState(aliceId, aliceCallId));
        CPPUNIT_ASSERT_EQUAL(std::string("AUDIO_VIDEO"), libjami::getExternalMediaPromotionState(aliceId, aliceCallId));
        auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
        auto conference = std::make_shared<Conference>(aliceAccount);
        auto host = Manager::instance().getRingBufferPool().createRingBuffer("video-promotion-test-host");
        CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));
        CPPUNIT_ASSERT(aliceCall->promoteExternalMedia("promote", conference));
        lk.lock();
    } else {
        lk.unlock();
        CPPUNIT_ASSERT(!libjami::requestExternalMediaPromotion(aliceId, aliceCallId, ""));
        CPPUNIT_ASSERT(libjami::requestExternalMediaPromotion(aliceId, aliceCallId, "promote"));
        lk.lock();
    }
    CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return accepted == "promote" || !failure.empty(); }));
    CPPUNIT_ASSERT_EQUAL(std::string("promote"), accepted);
    lk.unlock();
    CPPUNIT_ASSERT(!libjami::requestExternalMediaPromotion(aliceId, aliceCallId, "duplicate"));
    if (cancel)
        CPPUNIT_ASSERT(libjami::cancelExternalMediaPromotion(aliceId, aliceCallId, "promote"));
    lk.lock();
    CPPUNIT_ASSERT(cv.wait_for(lk, 45s, [&] { return ready == "promote" || !failure.empty(); }));
    if (cancel) {
        CPPUNIT_ASSERT(!failure.empty());
        CPPUNIT_ASSERT(ready.empty());
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return bobOver; }));
        lk.unlock();
        libjami::unregisterSignalHandlers();
        return;
    }
    CPPUNIT_ASSERT_MESSAGE(failure, failure.empty());
    CPPUNIT_ASSERT(!aliceCall->hasExternalMedia());
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getExternalMediaPromotionState(aliceId, aliceCallId));
    CPPUNIT_ASSERT_EQUAL(std::string("READY"), libjami::getNativeCallMediaState(aliceId, aliceCallId));
    CPPUNIT_ASSERT(!aliceCall->getRtpSessionList().empty());
    CPPUNIT_ASSERT_EQUAL(video ? size_t {1} : size_t {0}, aliceCall->getRtpSessionList(MediaType::MEDIA_VIDEO).size());
#ifdef ENABLE_VIDEO
    if (video) {
        auto videoSession = std::static_pointer_cast<video::VideoRtpSession>(
            aliceCall->getRtpSessionList(MediaType::MEDIA_VIDEO).front());
        CPPUNIT_ASSERT(videoSession->hasConference());
        CPPUNIT_ASSERT(videoSession->getVideoMixer());
        CPPUNIT_ASSERT(!videoSession->getVideoLocal());
        CPPUNIT_ASSERT_EQUAL(size_t {2}, aliceCall->getSDP().getMediaSlots().size());
        auto bobCall = std::dynamic_pointer_cast<SIPCall>(bobAccount->getCall(bobCallId));
        CPPUNIT_ASSERT(bobCall);
        const auto promotedSdp = Sdp::toString(bobCall->getSDP().getActiveRemoteSdpSession());
        CPPUNIT_ASSERT(promotedSdp.find("m=video ") != std::string::npos);
        CPPUNIT_ASSERT(promotedSdp.find("a=ice-ufrag:") != std::string::npos);
        CPPUNIT_ASSERT(promotedSdp.find("a=fingerprint:") != std::string::npos);
    }
#endif
    CPPUNIT_ASSERT(Manager::instance().getCallFromCallID(aliceCallId) == aliceCall);
    lk.unlock();
    if (demote) {
        auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
        auto* module = aliceAccount->convModule(true);
        CPPUNIT_ASSERT(module);
        const auto conversationId = libjami::startConversation(aliceId);
        CPPUNIT_ASSERT(!conversationId.empty());
        const auto conferenceId = Manager::instance().callFactory.getNewCallID();
        auto conference = module->hostBrowserConference(conversationId, conferenceId, {});
        CPPUNIT_ASSERT(conference);
        auto host = Manager::instance().getRingBufferPool().createRingBuffer("direct-return-host");
        CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));
        CPPUNIT_ASSERT(Manager::instance().addSubCall(aliceId, aliceCallId, aliceId, conferenceId));
        const auto offer = makeReturnOffer(video ? kExternalVideoOffer : kExternalOffer);
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(aliceId, aliceCallId, "wrong-host", "return", offer));
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(aliceId, aliceCallId, conferenceId, "return", "bad SDP"));
        auto wrongTransport = offer;
        const auto transport = wrongTransport.find("UDP/TLS/RTP/SAVPF");
        CPPUNIT_ASSERT(transport != std::string::npos);
        wrongTransport.replace(transport, std::string("UDP/TLS/RTP/SAVPF").size(), "RTP/AVP");
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
            aliceId, aliceCallId, conferenceId, "return", wrongTransport));
        auto noMux = offer;
        const auto mux = noMux.find("a=rtcp-mux\r\n");
        CPPUNIT_ASSERT(mux != std::string::npos);
        noMux.erase(mux, std::string("a=rtcp-mux\r\n").size());
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
            aliceId, aliceCallId, conferenceId, "return", noMux));
        auto noFingerprint = offer;
        const auto fingerprint = noFingerprint.find("a=fingerprint:sha-256 ");
        CPPUNIT_ASSERT(fingerprint != std::string::npos);
        noFingerprint.erase(fingerprint, noFingerprint.find("\r\n", fingerprint) + 2 - fingerprint);
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
            aliceId, aliceCallId, conferenceId, "return", noFingerprint));
        auto noSetup = offer;
        const auto setup = noSetup.find("a=setup:actpass\r\n");
        CPPUNIT_ASSERT(setup != std::string::npos);
        noSetup.erase(setup, std::string("a=setup:actpass\r\n").size());
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
            aliceId, aliceCallId, conferenceId, "return", noSetup));
        auto invalidSetup = offer;
        invalidSetup.replace(setup, std::string("a=setup:actpass").size(), "a=setup:invalid");
        CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
            aliceId, aliceCallId, conferenceId, "return", invalidSetup));
        if (video) {
            auto noVideoSetup = offer;
            const auto videoSetup = noVideoSetup.rfind("a=setup:actpass\r\n");
            CPPUNIT_ASSERT(videoSetup != std::string::npos && videoSetup != setup);
            noVideoSetup.erase(videoSetup, std::string("a=setup:actpass\r\n").size());
            CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
                aliceId, aliceCallId, conferenceId, "return", noVideoSetup));
            auto wrongOrder = offer;
            const auto audio = wrongOrder.find("m=audio ");
            CPPUNIT_ASSERT(audio != std::string::npos);
            wrongOrder.replace(audio, std::string("m=audio ").size(), "m=video ");
            CPPUNIT_ASSERT(!libjami::requestExternalMediaDemotion(
                aliceId, aliceCallId, conferenceId, "return", wrongOrder));
        }
        CPPUNIT_ASSERT(libjami::requestExternalMediaDemotion(aliceId, aliceCallId, conferenceId, "return", offer));
        lk.lock();
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !directAnswer.empty() || !directFailure.empty(); }));
        CPPUNIT_ASSERT_MESSAGE(directFailure, directFailure.empty());
        CPPUNIT_ASSERT(directAnswer.find("a=ice-ufrag:") != std::string::npos);
        CPPUNIT_ASSERT(directAnswer.find("a=fingerprint:") != std::string::npos);
        CPPUNIT_ASSERT(aliceCall->getConference() == conference);
        CPPUNIT_ASSERT(!aliceCall->getRtpSessionList().empty());
        lk.unlock();
        CPPUNIT_ASSERT(libjami::completeExternalMediaDemotion(aliceId, aliceCallId, "return"));
        lk.lock();
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return directReady == "return"; }));
        CPPUNIT_ASSERT(aliceCall->hasExternalMedia());
        CPPUNIT_ASSERT(aliceCall->getRtpSessionList().empty());
        CPPUNIT_ASSERT(!aliceCall->isConferenceParticipant());
        CPPUNIT_ASSERT(Manager::instance().getCallFromCallID(aliceCallId) == aliceCall);
        lk.unlock();
    }
    libjami::hangUp(aliceId, aliceCallId);
    libjami::unregisterSignalHandlers();
}

void
ExternalMediaCallTest::testNativeCallMediaReadiness()
{
    add_confirmed_contact(aliceId, bobId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    CPPUNIT_ASSERT(bobAccount);
    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::string bobCallId;
    std::string aliceState;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    handlers.insert(
        libjami::exportable_callback<libjami::CallSignal::IncomingCall>([&](const std::string& account,
                                                                            const std::string& callId,
                                                                            const std::string&,
                                                                            const std::vector<libjami::MediaMap>&) {
            std::lock_guard guard(mtx);
            if (account == bobId)
                bobCallId = callId;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& account, const std::string&, const std::string& state, signed) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                aliceState = state;
            cv.notify_all();
        }));
    libjami::registerSignalHandlers(handlers);
    std::vector<libjami::MediaMap> media {
        {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
         {libjami::Media::MediaAttributeKey::ENABLED, "true"},
         {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}}};
    CPPUNIT_ASSERT(libjami::placeCallWithMedia(aliceId, "", media).empty());
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getNativeCallMediaState(aliceId, "missing-call"));
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getExternalMediaPromotionState(aliceId, "missing-call"));
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"),
                         libjami::getExternalMediaPromotionState("missing-account", "missing-call"));
    const auto callId = libjami::placeCallWithMedia(aliceId, bobAccount->getUsername(), media);
    CPPUNIT_ASSERT(!callId.empty());
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getExternalMediaPromotionState(aliceId, callId));
    CPPUNIT_ASSERT_EQUAL(std::string("WAITING"), libjami::getNativeCallMediaState(aliceId, callId));
    CPPUNIT_ASSERT(!libjami::addNativeToBrowserConference(aliceId, callId, "missing-host"));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !bobCallId.empty(); }));
    CPPUNIT_ASSERT(libjami::acceptWithMedia(bobId, bobCallId, media));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceState == "CURRENT"; }));
    if (libjami::getNativeCallMediaState(aliceId, callId) == "WAITING")
        CPPUNIT_ASSERT(!libjami::addNativeToBrowserConference(aliceId, callId, "missing-host"));
    for (unsigned i = 0; i < 300 && libjami::getNativeCallMediaState(aliceId, callId) != "READY"; ++i)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT_EQUAL(std::string("READY"), libjami::getNativeCallMediaState(aliceId, callId));
    CPPUNIT_ASSERT(!libjami::addNativeToBrowserConference(aliceId, callId, "missing-host"));
    lk.unlock();
    CPPUNIT_ASSERT(libjami::hangUp(aliceId, callId));
    for (unsigned i = 0; i < 100 && libjami::getNativeCallMediaState(aliceId, callId) != "UNAVAILABLE"; ++i)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT_EQUAL(std::string("UNAVAILABLE"), libjami::getNativeCallMediaState(aliceId, callId));
    libjami::unregisterSignalHandlers();
}

void
ExternalMediaCallTest::testBrowserConferenceParticipantAudio()
{
    add_confirmed_contact(aliceId, bobId);
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    CPPUNIT_ASSERT(aliceAccount && bobAccount);
    const auto currentCallId = Manager::instance().getCurrentCallId();
    const auto callCount = aliceAccount->getCallList().size();
    CPPUNIT_ASSERT(
        libjami::placeBrowserParticipantCall(aliceId, bobAccount->getUsername(), "missing-host", false).empty());
    CPPUNIT_ASSERT(libjami::placeBrowserParticipantCall(aliceId, bobAccount->getUsername(), "missing-host").empty());
    CPPUNIT_ASSERT(
        libjami::placeBrowserParticipantCall(aliceId, bobAccount->getUsername(), "missing-host", true).empty());
    CPPUNIT_ASSERT_EQUAL(callCount, aliceAccount->getCallList().size());

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::string incomingId;
    std::string aliceState;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    handlers.insert(
        libjami::exportable_callback<libjami::CallSignal::IncomingCall>([&](const std::string& account,
                                                                            const std::string& callId,
                                                                            const std::string&,
                                                                            const std::vector<libjami::MediaMap>&) {
            std::lock_guard guard(mtx);
            if (account == bobId)
                incomingId = callId;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& account, const std::string&, const std::string& state, signed) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                aliceState = state;
            cv.notify_all();
        }));
    libjami::registerSignalHandlers(handlers);
    std::vector<libjami::MediaMap> audio {
        {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
         {libjami::Media::MediaAttributeKey::ENABLED, "true"},
         {libjami::Media::MediaAttributeKey::MUTED, "false"},
         {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}}};
    auto withVideo = audio;
    withVideo.push_back({{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
                         {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                         {libjami::Media::MediaAttributeKey::MUTED, "false"},
                         {libjami::Media::MediaAttributeKey::LABEL, "video_0"}});
    auto conference = std::make_shared<Conference>(aliceAccount);
    CPPUNIT_ASSERT(!aliceAccount->newBrowserConferenceOutgoingCall(bobAccount->getUsername(), withVideo, {}));
    auto outgoing = std::dynamic_pointer_cast<SIPCall>(
        aliceAccount->newBrowserConferenceOutgoingCall(bobAccount->getUsername(), audio, conference));
    CPPUNIT_ASSERT(outgoing);
    CPPUNIT_ASSERT_EQUAL(currentCallId, Manager::instance().getCurrentCallId());
    auto sessions = outgoing->getRtpSessionList(MediaType::MEDIA_AUDIO);
    CPPUNIT_ASSERT_EQUAL(size_t {1}, sessions.size());
    CPPUNIT_ASSERT(std::static_pointer_cast<AudioRtpSession>(sessions.front())->mode_
                   == AudioRtpSession::Mode::CONFERENCE_HOST);
    CPPUNIT_ASSERT(outgoing->getRtpSessionList(MediaType::MEDIA_VIDEO).empty());
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !incomingId.empty(); }));
    CPPUNIT_ASSERT(libjami::acceptWithMedia(bobId, incomingId, audio));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceState == "CURRENT"; }));
    for (unsigned i = 0; i < 300 && libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()) != "READY"; ++i)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT_EQUAL(std::string("READY"), libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()));
    CPPUNIT_ASSERT(!outgoing->requestMediaChange(withVideo));
    sessions = outgoing->getRtpSessionList(MediaType::MEDIA_AUDIO);
    CPPUNIT_ASSERT_EQUAL(size_t {1}, sessions.size());
    CPPUNIT_ASSERT(std::static_pointer_cast<AudioRtpSession>(sessions.front())->mode_
                   == AudioRtpSession::Mode::CONFERENCE_HOST);
    lk.unlock();
    CPPUNIT_ASSERT(libjami::hangUp(aliceId, outgoing->getCallId()));
    libjami::unregisterSignalHandlers();
}

#ifdef ENABLE_VIDEO
void
ExternalMediaCallTest::testBrowserConferenceParticipantVideo()
{
    add_confirmed_contact(aliceId, bobId);
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    CPPUNIT_ASSERT(aliceAccount && bobAccount);
    auto conference = std::make_shared<Conference>(aliceAccount);
    auto host = Manager::instance().getRingBufferPool().createRingBuffer("video-participant-test-host");
    CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));

    std::mutex mtx;
    std::unique_lock lk {mtx};
    std::condition_variable cv;
    std::string incomingId;
    std::string aliceState;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handlers;
    handlers.insert(
        libjami::exportable_callback<libjami::CallSignal::IncomingCall>([&](const std::string& account,
                                                                            const std::string& callId,
                                                                            const std::string&,
                                                                            const std::vector<libjami::MediaMap>&) {
            std::lock_guard guard(mtx);
            if (account == bobId)
                incomingId = callId;
            cv.notify_all();
        }));
    handlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& account, const std::string&, const std::string& state, signed) {
            std::lock_guard guard(mtx);
            if (account == aliceId)
                aliceState = state;
            cv.notify_all();
        }));
    libjami::registerSignalHandlers(handlers);
    std::vector<libjami::MediaMap> media {{{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                            libjami::Media::MediaAttributeValue::AUDIO},
                                           {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                                           {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}},
                                          {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                            libjami::Media::MediaAttributeValue::VIDEO},
                                           {libjami::Media::MediaAttributeKey::ENABLED, "true"},
                                           {libjami::Media::MediaAttributeKey::LABEL, "video_0"}}};
    auto outgoing = std::dynamic_pointer_cast<SIPCall>(
        aliceAccount->newBrowserConferenceOutgoingCall(bobAccount->getUsername(), media, conference));
    CPPUNIT_ASSERT(outgoing);
    auto audio = outgoing->getRtpSessionList(MediaType::MEDIA_AUDIO);
    auto video = outgoing->getRtpSessionList(MediaType::MEDIA_VIDEO);
    CPPUNIT_ASSERT_EQUAL(size_t {1}, audio.size());
    CPPUNIT_ASSERT_EQUAL(size_t {1}, video.size());
    CPPUNIT_ASSERT(std::static_pointer_cast<AudioRtpSession>(audio.front())->mode_
                   == AudioRtpSession::Mode::CONFERENCE_HOST);
    auto videoSession = std::static_pointer_cast<video::VideoRtpSession>(video.front());
    CPPUNIT_ASSERT(videoSession->hasConference());
    CPPUNIT_ASSERT(videoSession->getVideoMixer() == conference->getVideoMixer());
    CPPUNIT_ASSERT(!videoSession->getVideoLocal());

    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return !incomingId.empty(); }));
    auto answer = media;
    answer.back()[libjami::Media::MediaAttributeKey::SOURCE] = "test";
    CPPUNIT_ASSERT(libjami::acceptWithMedia(bobId, incomingId, answer));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return aliceState == "CURRENT"; }));
    for (unsigned i = 0; i < 300 && libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()) != "READY"; ++i)
        std::this_thread::sleep_for(100ms);
    CPPUNIT_ASSERT_EQUAL(std::string("READY"), libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()));
    CPPUNIT_ASSERT(!videoSession->getVideoLocal());
    lk.unlock();

    auto bobCall = std::dynamic_pointer_cast<SIPCall>(bobAccount->getCall(incomingId));
    CPPUNIT_ASSERT(bobCall);
    auto previousIce = outgoing->getIceMedia();
    answer.back()[libjami::Media::MediaAttributeKey::MUTED] = "true";
    CPPUNIT_ASSERT(libjami::requestMediaChange(bobId, incomingId, answer));
    for (unsigned i = 0; i < 300; ++i) {
        const auto* localSdp = bobCall->getSDP().getActiveLocalSdpSession();
        if (localSdp && Sdp::toString(localSdp).find("a=recvonly") != std::string::npos
            && outgoing->getIceMedia() != previousIce
            && libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()) == "READY")
            break;
        std::this_thread::sleep_for(100ms);
    }
    CPPUNIT_ASSERT(bobAccount->getCall(incomingId));
    CPPUNIT_ASSERT(aliceAccount->getCall(outgoing->getCallId()));
    CPPUNIT_ASSERT(outgoing->getIceMedia() != previousIce);
    CPPUNIT_ASSERT_EQUAL(std::string("READY"), libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()));
    CPPUNIT_ASSERT(videoSession->hasConference());
    CPPUNIT_ASSERT(videoSession->getVideoMixer() == conference->getVideoMixer());
    CPPUNIT_ASSERT(!videoSession->getVideoLocal());
    CPPUNIT_ASSERT(!videoSession->getVideoReceive());
    CPPUNIT_ASSERT(videoSession->isRtpReady(true, false));

    previousIce = outgoing->getIceMedia();
    answer.back()[libjami::Media::MediaAttributeKey::MUTED] = "false";
    CPPUNIT_ASSERT(libjami::requestMediaChange(bobId, incomingId, answer));
    for (unsigned i = 0; i < 300; ++i) {
        const auto* localSdp = bobCall->getSDP().getActiveLocalSdpSession();
        if (localSdp && Sdp::toString(localSdp).find("a=recvonly") == std::string::npos
            && outgoing->getIceMedia() != previousIce
            && libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()) == "READY")
            break;
        std::this_thread::sleep_for(100ms);
    }
    CPPUNIT_ASSERT(bobAccount->getCall(incomingId));
    CPPUNIT_ASSERT(aliceAccount->getCall(outgoing->getCallId()));
    CPPUNIT_ASSERT(outgoing->getIceMedia() != previousIce);
    CPPUNIT_ASSERT_EQUAL(std::string("READY"), libjami::getNativeCallMediaState(aliceId, outgoing->getCallId()));
    CPPUNIT_ASSERT(videoSession->hasConference());
    CPPUNIT_ASSERT(!videoSession->getVideoLocal());
    CPPUNIT_ASSERT(videoSession->isRtpReady(true, true));

    CPPUNIT_ASSERT(libjami::hangUp(aliceId, outgoing->getCallId()));
    libjami::unregisterSignalHandlers();
}
#endif

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ExternalMediaCallTest::name());
