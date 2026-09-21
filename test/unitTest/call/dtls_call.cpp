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
#include "media/dtls_srtp.h"
#include "media/media_attribute.h"
#include "sip/sipcall.h"
#include "../../test_runner.h"
#include "jami.h"
#include "media_const.h"
#include "common.h"

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <condition_variable>
#include <thread>
#include <string>

using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

namespace {

bool
hasSdpAttribute(const pjmedia_sdp_attr* const* attributes,
                unsigned attributeCount,
                std::string_view name)
{
    for (unsigned i = 0; i < attributeCount; ++i) {
        const auto* attribute = attributes[i];
        if (name == std::string_view(attribute->name.ptr, attribute->name.slen))
            return true;
    }
    return false;
}

} // namespace

class DtlsCallTest : public CppUnit::TestFixture
{
public:
    DtlsCallTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~DtlsCallTest() { libjami::fini(); }
    static std::string name() { return "DtlsCall"; }
    void setUp();
    void tearDown();

    std::string aliceId;
    std::string bobId;

private:
    void testAudioCallNegotiatesDtlsSrtp();
    void testAudioVideoCallNegotiatesDtlsSrtp();
    void testAudioVideoCallFallsBackWithoutRtcpMux();
    void testCallNegotiatesDtlsSrtp(bool withVideo, bool bobRtcpMuxEnabled = true);

    CPPUNIT_TEST_SUITE(DtlsCallTest);
    CPPUNIT_TEST(testAudioCallNegotiatesDtlsSrtp);
    CPPUNIT_TEST(testAudioVideoCallNegotiatesDtlsSrtp);
    CPPUNIT_TEST(testAudioVideoCallFallsBackWithoutRtcpMux);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(DtlsCallTest, DtlsCallTest::name());

void
DtlsCallTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
}

void
DtlsCallTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId});
}

void
DtlsCallTest::testAudioCallNegotiatesDtlsSrtp()
{
    testCallNegotiatesDtlsSrtp(false);
}

void
DtlsCallTest::testAudioVideoCallNegotiatesDtlsSrtp()
{
    testCallNegotiatesDtlsSrtp(true);
}

void
DtlsCallTest::testAudioVideoCallFallsBackWithoutRtcpMux()
{
    testCallNegotiatesDtlsSrtp(true, false);
}

void
DtlsCallTest::testCallNegotiatesDtlsSrtp(bool withVideo, bool bobRtcpMuxEnabled)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    bobAccount->enableRtcpMux(bobRtcpMuxEnabled);
    auto bobUri = bobAccount->getUsername();

    std::mutex mtx;
    std::condition_variable cv;
    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    std::string bobCallId;
    std::string aliceCallState;
    std::atomic<int> negotiationSuccess {0};
    std::atomic<int> callStopped {0};

    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
        [&](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            std::lock_guard lock {mtx};
            if (accountId == bobId)
                bobCallId = callId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [&](const std::string& accountId, const std::string&, const std::string& state, signed) {
            std::lock_guard lock {mtx};
            if (accountId == aliceId)
                aliceCallState = state;
            if (state == "OVER") {
                callStopped += 1;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::MediaNegotiationStatus>(
        [&](const std::string&, const std::string& event, const std::vector<std::map<std::string, std::string>>&) {
            if (event == libjami::Media::MediaNegotiationStatusEvents::NEGOTIATION_SUCCESS)
                negotiationSuccess += 1;
            cv.notify_one();
        }));
    libjami::registerSignalHandlers(confHandlers);

    MediaAttribute audio(MediaType::MEDIA_AUDIO);
    audio.enabled_ = true;
    audio.label_ = "audio_0";
    std::vector<MediaAttribute> media {audio};
    if (withVideo) {
        MediaAttribute video(MediaType::MEDIA_VIDEO);
        video.enabled_ = true;
        video.label_ = "video_0";
        media.emplace_back(std::move(video));
    }
    const auto mediaList = MediaAttribute::mediaAttributesToMediaMaps(media);

    JAMI_LOG("Start audio call between alice and bob");
    auto aliceCallId = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    {
        std::unique_lock lock {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lock, 30s, [&] { return !bobCallId.empty(); }));
    }

    libjami::acceptWithMedia(bobId, bobCallId, mediaList);
    {
        std::unique_lock lock {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lock, 60s, [&] { return aliceCallState == "CURRENT" && negotiationSuccess >= 2; }));
    }

    auto aliceCall = std::dynamic_pointer_cast<SIPCall>(aliceAccount->getCall(aliceCallId));
    auto bobCall = std::dynamic_pointer_cast<SIPCall>(bobAccount->getCall(bobCallId));
    CPPUNIT_ASSERT(aliceCall);
    CPPUNIT_ASSERT(bobCall);

    const auto* activeOffer = bobCall->getSDP().getRemoteSdpSession();
    CPPUNIT_ASSERT(activeOffer);
    if (!bobRtcpMuxEnabled)
        CPPUNIT_ASSERT(!hasSdpAttribute(activeOffer->attr, activeOffer->attr_count, "group"));
    for (unsigned i = 0; i < activeOffer->media_count; ++i) {
        const auto* media = activeOffer->media[i];
        CPPUNIT_ASSERT_EQUAL(
            bobRtcpMuxEnabled,
            hasSdpAttribute(media->attr, media->attr_count, "rtcp-mux"));
    }

    const auto aliceSlots = aliceCall->getSDP().getMediaSlots();
    const auto bobSlots = bobCall->getSDP().getMediaSlots();
    CPPUNIT_ASSERT_EQUAL(media.size(), aliceSlots.size());
    CPPUNIT_ASSERT_EQUAL(media.size(), bobSlots.size());
    const auto aliceFingerprint = aliceSlots.front().first.dtls_fingerprint;
    const auto bobFingerprint = bobSlots.front().first.dtls_fingerprint;
    CPPUNIT_ASSERT(!aliceFingerprint.empty());
    CPPUNIT_ASSERT(!bobFingerprint.empty());
    CPPUNIT_ASSERT(aliceFingerprint != bobFingerprint);
    CPPUNIT_ASSERT(aliceFingerprint != getDtlsFingerprint(*aliceAccount->identity().second));
    CPPUNIT_ASSERT(bobFingerprint != getDtlsFingerprint(*bobAccount->identity().second));
    for (const auto& [local, remote] : aliceSlots) {
        CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::DTLS, local.key_exchange);
        CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::DTLS, remote.key_exchange);
        CPPUNIT_ASSERT_EQUAL(aliceFingerprint, local.dtls_fingerprint);
        CPPUNIT_ASSERT_EQUAL(bobFingerprint, remote.dtls_fingerprint);
        CPPUNIT_ASSERT(!static_cast<bool>(local.crypto));
        CPPUNIT_ASSERT(!static_cast<bool>(remote.crypto));
    }

    for (const auto& [local, remote] : bobSlots) {
        CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::DTLS, local.key_exchange);
        CPPUNIT_ASSERT_EQUAL(KeyExchangeProtocol::DTLS, remote.key_exchange);
        CPPUNIT_ASSERT_EQUAL(bobFingerprint, local.dtls_fingerprint);
        CPPUNIT_ASSERT_EQUAL(aliceFingerprint, remote.dtls_fingerprint);
    }

    // Recording readiness proves the negotiated media path is up.
    libjami::toggleRecording(aliceId, aliceCallId);
    bool recording = false;
    for (int i = 0; i < 120 && !(recording = libjami::getIsRecording(aliceId, aliceCallId)); i++)
        std::this_thread::sleep_for(500ms);
    CPPUNIT_ASSERT(recording);

    JAMI_LOG("Stop call between alice and bob");
    Manager::instance().hangupCall(aliceId, aliceCallId);
    {
        std::unique_lock lock {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lock, 30s, [&] { return callStopped >= 2; }));
    }

    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::DtlsCallTest::name());
