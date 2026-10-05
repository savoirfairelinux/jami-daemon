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

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <mutex>
#include <string>

#include "manager.h"
#include "client/videomanager.h"
#include "jamidht/jamiaccount.h"
#include "../../test_runner.h"
#include "jami.h"
#include "account_const.h"
#include "common.h"
#include "conference.h"
#include "media/browser_conference_media.h"
#include "media_const.h"
#include "media/media_encoder.h"
#include "media/media_recorder.h"
#include "media/audio/audiolayer.h"
#include "media/audio/audio_rtp_session.h"
#include "media/audio/ringbuffer.h"
#include "media/audio/ringbufferpool.h"
#include "media/video/sinkclient.h"
#include "media/video/video_mixer.h"
#include "media/video/video_rtp_session.h"
#include "sip/sipcall.h"
#include "sip/siptransport.h"

#include <dhtnet/connectionmanager.h>

using namespace libjami::Account;
using namespace std::literals::chrono_literals;

namespace jami {
namespace test {

struct CallData
{
    std::string callId {};
    std::string state {};
    std::string device {};
    std::string streamId {};
    std::string hostState {};
    bool moderatorMuted {false};
    bool raisedHand {false};
    bool active {false};
    bool recording {false};

    void reset()
    {
        callId = "";
        state = "";
        device = "";
        streamId = "";
        hostState = "";
        moderatorMuted = false;
        active = false;
        raisedHand = false;
        recording = false;
    }
};

class ConferenceTest : public CppUnit::TestFixture
{
public:
    ConferenceTest()
    {
        // Init daemon
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }
    ~ConferenceTest() { libjami::fini(); }
    static std::string name() { return "Conference"; }
    void setUp();
    void tearDown();

private:
    void testGetConference();
    void testConferenceMediaAnswerPreservesSlots();
    void testExternalHostAudioAttachDetach();
    void testExternalHostAudioLateParticipant();
    void testBrowserHostRejectsInvalidOffer();
    void testBrowserHostRejectsVideoFirstOffer();
    void testBrowserHostAnswersRecvonlyVideo();
    void testBrowserHostRejectsFailedRtpStartup();
    void testRingBufferPoolBindingDirections();
    void testOneSenderConferenceBudget();
    void testModeratorMuteUpdateParticipantsInfos();
    void testUnauthorizedMute();
    void testAudioVideoMutedStates();
    void testMuteStatusAfterAdd();
    void testCreateParticipantsSinks();
    void testAttachParticipantSinkAfterReceiverStarts();
    void testMuteStatusAfterRemove();
    void testActiveStatusAfterRemove();
    void testHandsUp();
    void testPeerLeaveConference();
    void testJoinCallFromOtherAccount();
    void testDevices();
    void testUnauthorizedSetActive();
    void testHangup();
    void testIsConferenceParticipant();
    void testAudioConferenceConfInfo();
    void testHostAddRmSecondVideo();
    void testParticipantAddRmSecondVideo();
    void testPropagateRecording();
    void testBrokenParticipantAudioAndVideo();
    void testBrokenParticipantAudioOnly();
    void testAudioOnlyLeaveLayout();
    void testRemoveConferenceInOneOne();

    CPPUNIT_TEST_SUITE(ConferenceTest);
    CPPUNIT_TEST(testGetConference);
    CPPUNIT_TEST(testConferenceMediaAnswerPreservesSlots);
    CPPUNIT_TEST(testExternalHostAudioAttachDetach);
    CPPUNIT_TEST(testExternalHostAudioLateParticipant);
    CPPUNIT_TEST(testBrowserHostRejectsInvalidOffer);
    CPPUNIT_TEST(testBrowserHostRejectsVideoFirstOffer);
    CPPUNIT_TEST(testBrowserHostAnswersRecvonlyVideo);
    CPPUNIT_TEST(testBrowserHostRejectsFailedRtpStartup);
    CPPUNIT_TEST(testRingBufferPoolBindingDirections);
    CPPUNIT_TEST(testOneSenderConferenceBudget);
    CPPUNIT_TEST(testModeratorMuteUpdateParticipantsInfos);
    CPPUNIT_TEST(testUnauthorizedMute);
    CPPUNIT_TEST(testAudioVideoMutedStates);
    CPPUNIT_TEST(testMuteStatusAfterAdd);
    CPPUNIT_TEST(testCreateParticipantsSinks);
    CPPUNIT_TEST(testAttachParticipantSinkAfterReceiverStarts);
    CPPUNIT_TEST(testMuteStatusAfterRemove);
    CPPUNIT_TEST(testActiveStatusAfterRemove);
    CPPUNIT_TEST(testHandsUp);
    CPPUNIT_TEST(testPeerLeaveConference);
    CPPUNIT_TEST(testJoinCallFromOtherAccount);
    CPPUNIT_TEST(testDevices);
    CPPUNIT_TEST(testUnauthorizedSetActive);
    CPPUNIT_TEST(testHangup);
    CPPUNIT_TEST(testIsConferenceParticipant);
    CPPUNIT_TEST(testAudioConferenceConfInfo);
    CPPUNIT_TEST(testHostAddRmSecondVideo);
    CPPUNIT_TEST(testParticipantAddRmSecondVideo);
    CPPUNIT_TEST(testPropagateRecording);
    CPPUNIT_TEST(testBrokenParticipantAudioAndVideo);
    CPPUNIT_TEST(testBrokenParticipantAudioOnly);
    CPPUNIT_TEST(testAudioOnlyLeaveLayout);
    CPPUNIT_TEST(testRemoveConferenceInOneOne);
    CPPUNIT_TEST_SUITE_END();

    // Common parts
    std::string aliceId;
    bool hostRecording {false};
    std::string bobId;
    std::string carlaId;
    std::string daviId;
    std::string confId {};
    std::vector<std::map<std::string, std::string>> pInfos_ {};
    bool confChanged {false};

    CallData bobCall {};
    CallData carlaCall {};
    CallData daviCall {};

    std::mutex mtx;
    std::condition_variable cv;

    void registerSignalHandlers();
    void startConference(bool audioOnly = false, bool addDavi = false);
    void hangupConference();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(ConferenceTest, ConferenceTest::name());

void
ConferenceTest::testRingBufferPoolBindingDirections()
{
    auto& pool = Manager::instance().getRingBufferPool();
    auto reader = pool.createRingBuffer("conference-pool-reader");
    auto first = pool.createRingBuffer("conference-pool-source-first");
    auto second = pool.createRingBuffer("conference-pool-source-second");
    auto other = pool.createRingBuffer("conference-pool-other-reader");

    auto hasSubscriber = [](RingBuffer& buffer, const std::string& id) {
        const auto subscribers = buffer.getSubscribers();
        return std::find(subscribers.begin(), subscribers.end(), id) != subscribers.end();
    };
    pool.bindHalfDuplexOut(reader->getId(), first->getId());
    pool.bindHalfDuplexOut(reader->getId(), second->getId());
    pool.bindHalfDuplexOut(other->getId(), first->getId());
    CPPUNIT_ASSERT(hasSubscriber(*first, reader->getId()));
    CPPUNIT_ASSERT(hasSubscriber(*second, reader->getId()));

    pool.unBindAllHalfDuplexOut(reader->getId());
    CPPUNIT_ASSERT(!hasSubscriber(*first, reader->getId()));
    CPPUNIT_ASSERT(!hasSubscriber(*second, reader->getId()));
    CPPUNIT_ASSERT(hasSubscriber(*first, other->getId()));

    pool.bindHalfDuplexOut(reader->getId(), first->getId());
    pool.bindHalfDuplexOut(reader->getId(), second->getId());
    pool.unBindAllHalfDuplexIn(first->getId());
    CPPUNIT_ASSERT(first->getSubscribers().empty());
    CPPUNIT_ASSERT(hasSubscriber(*second, reader->getId()));
    pool.unBindAllHalfDuplexOut(reader->getId());
    CPPUNIT_ASSERT(second->getSubscribers().empty());
}

void
ConferenceTest::testBrowserHostAnswersRecvonlyVideo()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto conference = std::make_shared<Conference>(account);
    auto iceOptions = account->getIceOptions();
    iceOptions.stunServers.clear();
    iceOptions.turnServers.clear();
    iceOptions.upnpEnable = false;
    iceOptions.upnpContext.reset();

    Sdp browserOffer("browser-host-test-offer");
    browserOffer.setPublishedIP("127.0.0.1", pj_AF_INET());
    browserOffer.setSecureMediaKeyExchange(KeyExchangeProtocol::DTLS);
    std::string fingerprint("AA");
    for (int i = 1; i < 32; ++i)
        fingerprint += ":AA";
    browserOffer.setLocalDtlsFingerprint("SHA-256", fingerprint);
    browserOffer.enableRtcpMux(true);
    browserOffer.enableBundle(true);
    browserOffer.setLocalPublishedAudioPorts(50000, 0);
    browserOffer.setLocalPublishedVideoPorts(50000, 0);
    browserOffer.setLocalMediaCapabilities(MEDIA_AUDIO, account->getActiveAccountCodecInfoList(MEDIA_AUDIO));
    browserOffer.setLocalMediaCapabilities(MEDIA_VIDEO, account->getActiveAccountCodecInfoList(MEDIA_VIDEO));
    MediaAttribute audio {MEDIA_AUDIO, false, true, true, "", "audio_0"};
    MediaAttribute video {MEDIA_VIDEO, true, true, true, "", "video_0"};
    CPPUNIT_ASSERT(browserOffer.createOffer({audio, video}));
    browserOffer.addIceAttributes(dhtnet::IceTransport::Attribute {"browserUfrag", "browserPassword0123456789"});
    const std::vector<std::string> candidates {
        "1 1 UDP 2130706431 127.0.0.1 50000 typ host"
    };
    browserOffer.addIceCandidates(0, candidates);
    browserOffer.addIceCandidates(1, candidates);
    const auto offer = Sdp::toString(browserOffer.getLocalSdpSession());
    CPPUNIT_ASSERT(offer.find("a=recvonly\r\n") != std::string::npos);
    CPPUNIT_ASSERT(offer.find("a=ice-ufrag:browserUfrag\r\n") != std::string::npos);
    CPPUNIT_ASSERT(offer.find("a=candidate:1 1 UDP ") != std::string::npos);

    auto owner = std::make_shared<BrowserConferenceMedia>(account, conference, std::move(iceOptions));
    std::weak_ptr<BrowserConferenceMedia> weakOwner = owner;
    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::string answer;
    std::string error;
    bool ready = false;
    owner->start(offer,
                 [&, weakOwner](const std::string& sdp) {
                     if (auto active = weakOwner.lock())
                         active->stop();
                     std::lock_guard lock(callbackMutex);
                     answer = sdp;
                     callbackCv.notify_one();
                 },
                 [&](const std::string& reason) {
                     std::lock_guard lock(callbackMutex);
                     error = reason;
                     callbackCv.notify_one();
                 },
                 [&] {
                     std::lock_guard lock(callbackMutex);
                     ready = true;
                     callbackCv.notify_one();
                 });
    {
        std::unique_lock lock(callbackMutex);
        CPPUNIT_ASSERT(callbackCv.wait_for(lock, 20s, [&] { return !answer.empty() || !error.empty(); }));
        CPPUNIT_ASSERT_MESSAGE(error, error.empty());
        CPPUNIT_ASSERT(!ready);
        CPPUNIT_ASSERT(answer.find("a=ice-ufrag:") != std::string::npos);
        CPPUNIT_ASSERT(answer.find("a=ice-pwd:") != std::string::npos);
        CPPUNIT_ASSERT(answer.find("a=candidate:") != std::string::npos);
        CPPUNIT_ASSERT(answer.find("a=fingerprint:SHA-256 ") != std::string::npos);
        CPPUNIT_ASSERT(answer.find("a=group:BUNDLE ") != std::string::npos);
        auto videoLine = answer.find("m=video ");
        CPPUNIT_ASSERT(videoLine != std::string::npos);
        CPPUNIT_ASSERT(answer.substr(videoLine).find("a=sendonly\r\n") != std::string::npos);
        CPPUNIT_ASSERT(answer.substr(videoLine).find("a=recvonly\r\n") == std::string::npos);
    }
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_DETACHED);
    owner.reset();
}

void
ConferenceTest::testBrowserHostRejectsFailedRtpStartup()
{
    MediaDescription send;
    send.enabled = true;
    send.key_exchange = KeyExchangeProtocol::DTLS;
    MediaDescription receive = send;

    auto audio = std::make_shared<AudioRtpSession>("browser-transport-failure",
                                                   "browser-transport-failure-audio",
                                                   nullptr,
                                                   AudioRtpSession::Mode::CONFERENCE_HOST);
    audio->updateMedia(send, receive);
    audio->start(nullptr, nullptr);
    CPPUNIT_ASSERT(!audio->isRtpReady(true, true));

#ifdef ENABLE_VIDEO
    auto video = std::make_shared<video::VideoRtpSession>("browser-transport-failure",
                                                          "browser-transport-failure-video",
                                                          DeviceParams {},
                                                          nullptr);
    video->updateMedia(send, receive);
    video->start(nullptr, nullptr);
    CPPUNIT_ASSERT(!video->isRtpReady(true, true));
#endif
}

void
ConferenceTest::testBrowserHostRejectsVideoFirstOffer()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto conference = std::make_shared<Conference>(account);
    auto owner = std::make_shared<BrowserConferenceMedia>(account, conference);
    const std::string offer = "v=0\r\n"
                              "o=- 0 0 IN IP4 127.0.0.1\r\n"
                              "s=-\r\n"
                              "c=IN IP4 127.0.0.1\r\n"
                              "t=0 0\r\n"
                              "m=video 5004 UDP/TLS/RTP/SAVPF 96\r\n"
                              "a=rtpmap:96 VP8/90000\r\n"
                              "a=rtcp-mux\r\n"
                              "m=audio 5004 UDP/TLS/RTP/SAVPF 111\r\n"
                              "a=rtpmap:111 opus/48000/2\r\n"
                              "a=rtcp-mux\r\n";
    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::string error;
    bool answered = false;
    bool ready = false;
    owner->start(offer,
                 [&](const auto&) {
                     std::lock_guard lock(callbackMutex);
                     answered = true;
                     callbackCv.notify_one();
                 },
                 [&](const auto& reason) {
                     std::lock_guard lock(callbackMutex);
                     error = reason;
                     callbackCv.notify_one();
                 },
                 [&] {
                     std::lock_guard lock(callbackMutex);
                     ready = true;
                     callbackCv.notify_one();
                 });
    {
        std::unique_lock lock(callbackMutex);
        CPPUNIT_ASSERT(callbackCv.wait_for(lock, 5s, [&] { return !error.empty(); }));
        CPPUNIT_ASSERT(error.find("audio m-line first") != std::string::npos);
        CPPUNIT_ASSERT(!answered);
        CPPUNIT_ASSERT(!ready);
    }
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_DETACHED);
}

void
ConferenceTest::testBrowserHostRejectsInvalidOffer()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto conference = std::make_shared<Conference>(account);
    auto owner = std::make_shared<BrowserConferenceMedia>(account, conference);
    std::mutex callbackMutex;
    std::condition_variable callbackCv;
    std::string error;
    bool answered = false;

    owner->start("not an SDP offer",
                 [&](const auto&) {
                     std::lock_guard lock(callbackMutex);
                     answered = true;
                     callbackCv.notify_one();
                 },
                 [&](const auto& reason) {
                     std::lock_guard lock(callbackMutex);
                     error = reason;
                     callbackCv.notify_one();
                 });
    {
        std::unique_lock lock(callbackMutex);
        CPPUNIT_ASSERT(callbackCv.wait_for(lock, 5s, [&] { return !error.empty(); }));
        CPPUNIT_ASSERT(!answered);
    }
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_DETACHED);
    owner->stop();
}

void
ConferenceTest::testExternalHostAudioAttachDetach()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto& pool = Manager::instance().getRingBufferPool();
    auto audioDriver = Manager::instance().getAudioDriver();
    CPPUNIT_ASSERT(audioDriver);
    const bool audioWasStarted = audioDriver->isStarted();
    auto conference = std::make_shared<Conference>(account);

    CPPUNIT_ASSERT(!conference->attachExternalHostAudio(""));
    CPPUNIT_ASSERT(!conference->attachExternalHostAudio(RingBufferPool::DEFAULT_ID));
    CPPUNIT_ASSERT(!conference->attachExternalHostAudio("nonexistent-browser-stream"));
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_DETACHED);

    auto host = pool.createRingBuffer("external-host-test-audio");
    std::weak_ptr<RingBuffer> weakHost = host;
    CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_ATTACHED);
    CPPUNIT_ASSERT(!conference->attachExternalHostAudio(host->getId()));
    CPPUNIT_ASSERT(conference->hostAudioInputs_.empty());
    CPPUNIT_ASSERT_EQUAL(audioWasStarted, audioDriver->isStarted());
    auto sources = conference->currentMediaList();
    CPPUNIT_ASSERT_EQUAL(size_t {1}, sources.size());
    CPPUNIT_ASSERT_EQUAL(std::string(libjami::Media::MediaAttributeValue::AUDIO),
                         sources.front().at(libjami::Media::MediaAttributeKey::MEDIA_TYPE));
    CPPUNIT_ASSERT(!conference->requestMediaChange(sources));

    host.reset();
    CPPUNIT_ASSERT(!weakHost.expired());
    conference->detachHost();
    CPPUNIT_ASSERT(weakHost.expired());
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_DETACHED);
    conference->attachHost({});
    CPPUNIT_ASSERT(conference->getState() == Conference::State::ACTIVE_DETACHED);
    CPPUNIT_ASSERT(conference->hostAudioInputs_.empty());

    host = pool.createRingBuffer("external-host-test-audio");
#ifdef ENABLE_VIDEO
    CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId(), true));
    auto videoSources = conference->currentMediaList();
    CPPUNIT_ASSERT_EQUAL(size_t {2}, videoSources.size());
    CPPUNIT_ASSERT_EQUAL(std::string(libjami::Media::MediaAttributeValue::VIDEO),
                         videoSources.back().at(libjami::Media::MediaAttributeKey::MEDIA_TYPE));
    CPPUNIT_ASSERT(conference->hostAudioInputs_.empty());
#else
    CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));
#endif
    conference->detachHost();

    auto shutdownHost = pool.createRingBuffer("external-host-shutdown-audio");
    auto peer = pool.createRingBuffer("external-host-shutdown-peer");
    std::weak_ptr<RingBuffer> weakShutdownHost = shutdownHost;
    auto shuttingDown = std::make_shared<Conference>(account);
    CPPUNIT_ASSERT(shuttingDown->attachExternalHostAudio(shutdownHost->getId()));
    pool.bindRingBuffers(shutdownHost->getId(), peer->getId());
    shutdownHost.reset();
    shuttingDown.reset();
    // The synthetic link was not made by the conference; destroying it must
    // leave unrelated ringbuffer bindings alone.
    CPPUNIT_ASSERT(!weakShutdownHost.expired());
    const auto subscribers = peer->getSubscribers();
    CPPUNIT_ASSERT(std::find(subscribers.begin(), subscribers.end(), "external-host-shutdown-audio")
                   != subscribers.end());
    pool.unbindRingBuffers("external-host-shutdown-audio", peer->getId());
    CPPUNIT_ASSERT(weakShutdownHost.expired());
}

void
ConferenceTest::testExternalHostAudioLateParticipant()
{
    registerSignalHandlers();

    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto& pool = Manager::instance().getRingBufferPool();
    auto host = pool.createRingBuffer("external-late-host-audio");
    auto conference = std::make_shared<Conference>(account);
    CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));
    CPPUNIT_ASSERT(conference->hostAudioInputs_.empty());
    confId = conference->getConfId();
    account->attach(conference);

    libjami::MediaMap audio {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
                             {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
                             {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    auto callId = libjami::placeCallWithMedia(aliceId, bobAccount->getUsername(), {audio});
    CPPUNIT_ASSERT(!callId.empty());
    {
        std::unique_lock lk(mtx);
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    }
    CPPUNIT_ASSERT(Manager::instance().acceptCall(bobId, bobCall.callId));
    {
        std::unique_lock lk(mtx);
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.hostState == "CURRENT"; }));
    }
    auto call = std::dynamic_pointer_cast<SIPCall>(account->getCall(callId));
    CPPUNIT_ASSERT(call);
    auto streams = call->getRemoteAudioStreams();
    CPPUNIT_ASSERT_EQUAL(size_t {1}, streams.size());
    const auto participantId = streams.begin()->first;
    auto participant = pool.getRingBuffer(participantId);
    CPPUNIT_ASSERT(participant);

    conference->addSubCall(callId);
    auto hasSubscriber = [](RingBuffer& buffer, const std::string& id) {
        auto subscribers = buffer.getSubscribers();
        return std::find(subscribers.begin(), subscribers.end(), id) != subscribers.end();
    };
    CPPUNIT_ASSERT(hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(hasSubscriber(*participant, host->getId()));
    CPPUNIT_ASSERT(conference->hostAudioInputs_.empty());

    conference->muteLocalHost(true, libjami::Media::Details::MEDIA_TYPE_AUDIO);
    CPPUNIT_ASSERT(!hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(hasSubscriber(*participant, host->getId()));
    conference->muteLocalHost(false, libjami::Media::Details::MEDIA_TYPE_AUDIO);
    CPPUNIT_ASSERT(hasSubscriber(*host, participantId));

    conference->muteCall(callId, true);
    CPPUNIT_ASSERT(!hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(!hasSubscriber(*participant, host->getId()));
    conference->muteCall(callId, false);
    CPPUNIT_ASSERT(hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(hasSubscriber(*participant, host->getId()));

    conference->detachHost();
    CPPUNIT_ASSERT(!hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(!hasSubscriber(*participant, host->getId()));
    CPPUNIT_ASSERT(conference->attachExternalHostAudio(host->getId()));
    CPPUNIT_ASSERT(hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(hasSubscriber(*participant, host->getId()));

    conference->removeSubCall(callId);
    CPPUNIT_ASSERT(!hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(!hasSubscriber(*participant, host->getId()));
    conference->detachHost();
    CPPUNIT_ASSERT(!hasSubscriber(*host, participantId));
    CPPUNIT_ASSERT(Manager::instance().hangupCall(aliceId, callId));
    account->removeConference(confId);
    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testConferenceMediaAnswerPreservesSlots()
{
    MediaAttribute audio {MediaType::MEDIA_AUDIO, false, false, true, "host-mic", "audio_0"};
    MediaAttribute video {MediaType::MEDIA_VIDEO, false, false, true, "host-camera", "video_0"};
    const auto hostAudio = MediaAttribute::toMediaMap(audio);
    const auto hostVideo = MediaAttribute::toMediaMap(video);
    auto remoteAudio = hostAudio;
    remoteAudio[libjami::Media::MediaAttributeKey::SOURCE] = "guest-mic";
    auto remoteVideo = hostVideo;
    remoteVideo[libjami::Media::MediaAttributeKey::SOURCE] = "guest-camera";

    auto answer = [&](const std::vector<libjami::MediaMap>& offer) {
        return conference_detail::mediaAnswerForOffer({audio, video}, offer);
    };

    CPPUNIT_ASSERT(answer({remoteAudio, remoteVideo}) == (std::vector<libjami::MediaMap> {hostAudio, hostVideo}));

    audio.muted_ = true;
    CPPUNIT_ASSERT(answer({remoteAudio, remoteVideo}) == (std::vector<libjami::MediaMap> {remoteAudio, hostVideo}));

    video.muted_ = true;
    CPPUNIT_ASSERT(answer({remoteAudio, remoteVideo}) == (std::vector<libjami::MediaMap> {remoteAudio, remoteVideo}));

    audio.muted_ = false;
    video.muted_ = false;
    auto extraVideo = remoteVideo;
    extraVideo[libjami::Media::MediaAttributeKey::LABEL] = "video_1";
    CPPUNIT_ASSERT(answer({remoteAudio, remoteVideo, extraVideo})
                   == (std::vector<libjami::MediaMap> {hostAudio, hostVideo, extraVideo}));

    auto disabledVideo = remoteVideo;
    disabledVideo[libjami::Media::MediaAttributeKey::ENABLED] = FALSE_STR;
    CPPUNIT_ASSERT(answer({remoteAudio, disabledVideo}) == (std::vector<libjami::MediaMap> {hostAudio, disabledVideo}));
    audio.enabled_ = false;
    CPPUNIT_ASSERT(answer({remoteAudio, remoteVideo}) == (std::vector<libjami::MediaMap> {remoteAudio, hostVideo}));
    audio.enabled_ = true;
    CPPUNIT_ASSERT(answer({remoteVideo, remoteAudio}) == (std::vector<libjami::MediaMap> {remoteVideo, remoteAudio}));
    CPPUNIT_ASSERT(answer({remoteAudio}) == (std::vector<libjami::MediaMap> {hostAudio}));
}

void
ConferenceTest::testOneSenderConferenceBudget()
{
    auto account = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto conference = std::make_shared<Conference>(account);
    auto mixer = conference->getVideoMixer();
    CPPUNIT_ASSERT(mixer);

    auto session = std::make_shared<video::VideoRtpSession>("bitrate-test",
                                                            "video_0",
                                                            DeviceParams {},
                                                            std::make_shared<MediaRecorder>());
    session->enterConference(*conference);
    MediaDescription media;
    media.codec = std::make_shared<SystemVideoCodecInfo>(96, AV_CODEC_ID_H264, "H264", "H264", "libx264");

    struct Expected
    {
        int width, height;
        unsigned bitrate;
        int encodedWidth, encodedHeight;
    };
    const std::array<Expected, 3> cases {{
        {1280, 720, 2211, 1280, 720},
        {1920, 1080, 4976, 1920, 1080},
        {2560, 1440, 6000, 1920, 1080},
    }};
    for (const auto& expected : cases) {
        mixer->setParameters(expected.width, expected.height, AV_PIX_FMT_YUV420P);
        session->updateMedia(media, media);
        const auto& bitrate = session->getVideoBitrateInfo();
        CPPUNIT_ASSERT_EQUAL(expected.bitrate, bitrate.videoBitrateCurrent);
        CPPUNIT_ASSERT_EQUAL(SystemCodecInfo::DEFAULT_MAX_BITRATE, bitrate.videoBitrateMax);

        auto stream = mixer->getStream("Video Sender");
        stream.bitrate = static_cast<int>(bitrate.videoBitrateCurrent);
        MediaEncoder encoder;
        encoder.setOptions(stream);
        CPPUNIT_ASSERT_EQUAL(expected.encodedWidth, encoder.getWidth());
        CPPUNIT_ASSERT_EQUAL(expected.encodedHeight, encoder.getHeight());
    }
}

void
ConferenceTest::setUp()
{
    auto actors = load_actors_and_wait_for_announcement("actors/alice-bob-carla-davi-public-incoming.yml");
    aliceId = actors["alice"];
    bobId = actors["bob"];
    carlaId = actors["carla"];
    daviId = actors["davi"];

    bobCall.reset();
    carlaCall.reset();
    daviCall.reset();
    confId = {};
    confChanged = false;
    hostRecording = false;
}

void
ConferenceTest::tearDown()
{
    wait_for_removal_of({aliceId, bobId, carlaId, daviId});
}

void
ConferenceTest::registerSignalHandlers()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto aliceUri = aliceAccount->getUsername();
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto daviUri = daviAccount->getUsername();

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> confHandlers;
    // Watch signals
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::IncomingCall>(
        [=](const std::string& accountId,
            const std::string& callId,
            const std::string&,
            const std::vector<std::map<std::string, std::string>>&) {
            std::lock_guard lk(mtx);
            if (accountId == bobId) {
                bobCall.callId = callId;
            } else if (accountId == carlaId) {
                carlaCall.callId = callId;
            } else if (accountId == daviId) {
                daviCall.callId = callId;
            }
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::StateChange>(
        [=](const std::string& accountId, const std::string& callId, const std::string& state, signed) {
            std::lock_guard lk(mtx);
            if (accountId == aliceId) {
                auto details = libjami::getCallDetails(aliceId, callId);
                if (details["PEER_NUMBER"].find(bobUri) != std::string::npos)
                    bobCall.hostState = state;
                else if (details["PEER_NUMBER"].find(carlaUri) != std::string::npos)
                    carlaCall.hostState = state;
                else if (details["PEER_NUMBER"].find(daviUri) != std::string::npos)
                    daviCall.hostState = state;
            } else if (bobCall.callId == callId)
                bobCall.state = state;
            else if (carlaCall.callId == callId)
                carlaCall.state = state;
            else if (daviCall.callId == callId)
                daviCall.state = state;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceCreated>(
        [=](const std::string&, const std::string&, const std::string& conferenceId) {
            std::lock_guard lk(mtx);
            confId = conferenceId;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceRemoved>(
        [=](const std::string&, const std::string& conferenceId) {
            std::lock_guard lk(mtx);
            if (confId == conferenceId)
                confId = "";
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::ConferenceChanged>(
        [=](const std::string&, const std::string& conferenceId, const std::string&) {
            std::lock_guard lk(mtx);
            if (confId == conferenceId)
                confChanged = true;
            cv.notify_one();
        }));
    confHandlers.insert(libjami::exportable_callback<libjami::CallSignal::OnConferenceInfosUpdated>(
        [=](const std::string&, const std::vector<std::map<std::string, std::string>> participantsInfos) {
            std::lock_guard lock(mtx);
            pInfos_ = participantsInfos;
            for (const auto& infos : participantsInfos) {
                if (infos.at("uri").find(bobUri) != std::string::npos) {
                    bobCall.active = infos.at("active") == "true";
                    bobCall.recording = infos.at("recording") == "true";
                    bobCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                    bobCall.raisedHand = infos.at("handRaised") == "true";
                    bobCall.device = infos.at("device");
                    bobCall.streamId = infos.at("sinkId");
                } else if (infos.at("uri").find(carlaUri) != std::string::npos) {
                    carlaCall.active = infos.at("active") == "true";
                    carlaCall.recording = infos.at("recording") == "true";
                    carlaCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                    carlaCall.raisedHand = infos.at("handRaised") == "true";
                    carlaCall.device = infos.at("device");
                    carlaCall.streamId = infos.at("sinkId");
                } else if (infos.at("uri").find(daviUri) != std::string::npos) {
                    daviCall.active = infos.at("active") == "true";
                    daviCall.recording = infos.at("recording") == "true";
                    daviCall.moderatorMuted = infos.at("audioModeratorMuted") == "true";
                    daviCall.raisedHand = infos.at("handRaised") == "true";
                    daviCall.device = infos.at("device");
                    daviCall.streamId = infos.at("sinkId");
                } else if (infos.at("uri").find(aliceUri) != std::string::npos) {
                    hostRecording = infos.at("recording") == "true";
                }
            }
            cv.notify_one();
        }));

    libjami::registerSignalHandlers(confHandlers);
}

void
ConferenceTest::startConference(bool audioOnly, bool addDavi)
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto daviUri = daviAccount->getUsername();

    std::vector<std::map<std::string, std::string>> mediaList;
    if (audioOnly) {
        std::map<std::string, std::string> mediaAttribute = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                                              libjami::Media::MediaAttributeValue::AUDIO},
                                                             {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
                                                             {libjami::Media::MediaAttributeKey::MUTED, FALSE_STR},
                                                             {libjami::Media::MediaAttributeKey::SOURCE, ""},
                                                             {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
        mediaList.emplace_back(mediaAttribute);
    }

    JAMI_LOG("Start call between Alice and Bob");
    auto call1 = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.hostState == "CURRENT"; }));
    }

    JAMI_LOG("Start call between Alice and Carla");
    auto call2 = libjami::placeCallWithMedia(aliceId, carlaUri, mediaList);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !carlaCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(carlaId, carlaCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return carlaCall.hostState == "CURRENT"; }));
    }

    JAMI_LOG("Start conference");
    confChanged = false;
    Manager::instance().joinParticipant(aliceId, call1, aliceId, call2);
    // ConfChanged is the signal emitted when the 2 calls will be added to the conference
    // Also, wait for participants to appear in conf info to get all good information
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] {
            return !confId.empty() && confChanged && !carlaCall.device.empty() && !bobCall.device.empty();
        }));
    }

    if (addDavi) {
        JAMI_LOG("Start call between Alice and Davi");
        auto call1 = libjami::placeCallWithMedia(aliceId, daviUri, mediaList);
        {
            std::unique_lock lk {mtx};
            CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
        }
        Manager::instance().acceptCall(daviId, daviCall.callId);
        {
            std::unique_lock lk {mtx};
            CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
        }
        Manager::instance().addSubCall(aliceId, call1, aliceId, confId);
        {
            std::unique_lock lk {mtx};
            CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
        }
    }
}

void
ConferenceTest::hangupConference()
{
    JAMI_LOG("Stop conference");
    Manager::instance().hangupConference(aliceId, confId);
    std::unique_lock lk {mtx};
    CPPUNIT_ASSERT(
        cv.wait_for(lk, 30s, [&] { return bobCall.state == "OVER" && carlaCall.state == "OVER" && confId.empty(); }));
    CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return daviCall.callId.empty() ? true : daviCall.state == "OVER"; }));
}

void
ConferenceTest::testGetConference()
{
    registerSignalHandlers();

    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(libjami::getConferenceList(aliceId).size() == 0);
    }

    startConference();

    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(libjami::getConferenceList(aliceId).size() == 1);
        CPPUNIT_ASSERT(libjami::getConferenceList(aliceId)[0] == confId);
    }

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto conference = aliceAccount->getConference(confId);
    CPPUNIT_ASSERT(conference);
    auto mixer = conference->getVideoMixer();
    CPPUNIT_ASSERT(mixer);
    mixer->setParameters(1280, 720, AV_PIX_FMT_YUV420P);
    const auto stream = mixer->getStream("Video Sender");
    const auto pixels = static_cast<unsigned>(stream.width) * static_cast<unsigned>(stream.height);
    const auto fullSizeBitrate = static_cast<unsigned>(pixels * stream.frameRate.real() * 0.06 / 1000) + 1;
    const auto uncappedMax = std::max(static_cast<unsigned>(pixels * 0.0015), SystemCodecInfo::DEFAULT_MAX_BITRATE);
    const auto subCalls = conference->getSubCalls();
    CPPUNIT_ASSERT_EQUAL(size_t {2}, subCalls.size());
    for (const auto& callId : subCalls) {
        auto call = std::dynamic_pointer_cast<SIPCall>(aliceAccount->getCall(callId));
        CPPUNIT_ASSERT(call);
        auto sessions = call->getRtpSessionList(MediaType::MEDIA_VIDEO);
        CPPUNIT_ASSERT_EQUAL(size_t {1}, sessions.size());
        auto video = std::dynamic_pointer_cast<video::VideoRtpSession>(sessions.front());
        CPPUNIT_ASSERT(video);
        const auto expectedMax = uncappedMax / subCalls.size();
        video->restartSender();
        CPPUNIT_ASSERT(video->getVideoBitrateInfo().videoBitrateCurrent >= fullSizeBitrate);
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(expectedMax), video->getVideoBitrateInfo().videoBitrateMax);
        video->restartSender();
        CPPUNIT_ASSERT_EQUAL(static_cast<unsigned>(expectedMax), video->getVideoBitrateInfo().videoBitrateMax);
    }

    hangupConference();

    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(libjami::getConferenceList(aliceId).size() == 0);
    }
    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testModeratorMuteUpdateParticipantsInfos()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    JAMI_LOG("Play with mute from the moderator");
    libjami::muteStream(aliceId, confId, bobUri, bobCall.device, bobCall.streamId, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.moderatorMuted; }));
    }

    libjami::muteStream(aliceId, confId, bobUri, bobCall.device, bobCall.streamId, false);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.moderatorMuted; }));
    }
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testUnauthorizedMute()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    JAMI_LOG("Play with mute from unauthorized");
    libjami::muteStream(carlaId, confId, bobUri, bobCall.device, bobCall.streamId, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(!cv.wait_for(lk, 15s, [&] { return bobCall.moderatorMuted; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testAudioVideoMutedStates()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    JAMI_LOG("Start call between Alice and Bob");
    auto call1Id = libjami::placeCallWithMedia(aliceId, bobUri, {});
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.hostState == "CURRENT"; }));
    }
    auto call1 = aliceAccount->getCall(call1Id);
    call1->muteMedia(libjami::Media::MediaAttributeValue::AUDIO, true);
    call1->muteMedia(libjami::Media::MediaAttributeValue::VIDEO, true);

    JAMI_LOG("Start call between Alice and Carla");
    auto call2Id = libjami::placeCallWithMedia(aliceId, carlaUri, {});
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !carlaCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(carlaId, carlaCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return carlaCall.hostState == "CURRENT"; }));
    }

    auto call2 = aliceAccount->getCall(call2Id);
    call2->muteMedia(libjami::Media::MediaAttributeValue::AUDIO, true);
    call2->muteMedia(libjami::Media::MediaAttributeValue::VIDEO, true);

    JAMI_LOG("Start conference");
    Manager::instance().joinParticipant(aliceId, call1Id, aliceId, call2Id);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !confId.empty(); }));
    }

    auto conf = aliceAccount->getConference(confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return conf->isMediaSourceMuted(jami::MediaType::MEDIA_AUDIO); }));
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return conf->isMediaSourceMuted(jami::MediaType::MEDIA_VIDEO); }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testMuteStatusAfterAdd()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();
    auto daviUri = daviAccount->getUsername();

    std::vector<std::map<std::string, std::string>> mediaList;
    std::map<std::string, std::string> mediaAttribute = {{libjami::Media::MediaAttributeKey::MEDIA_TYPE,
                                                          libjami::Media::MediaAttributeValue::AUDIO},
                                                         {libjami::Media::MediaAttributeKey::ENABLED, TRUE_STR},
                                                         {libjami::Media::MediaAttributeKey::MUTED, TRUE_STR},
                                                         {libjami::Media::MediaAttributeKey::SOURCE, ""},
                                                         {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}};
    mediaList.emplace_back(mediaAttribute);

    JAMI_LOG("Start call between Alice and Bob");
    auto call1 = libjami::placeCallWithMedia(aliceId, bobUri, mediaList);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !bobCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.hostState == "CURRENT"; }));
    }

    JAMI_LOG("Start call between Alice and Carla");
    auto call2 = libjami::placeCallWithMedia(aliceId, carlaUri, mediaList);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !carlaCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(carlaId, carlaCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return carlaCall.hostState == "CURRENT"; }));
    }

    JAMI_LOG("Start conference");
    Manager::instance().joinParticipant(aliceId, call1, aliceId, call2);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !confId.empty(); }));
    }
    JAMI_LOG("Add Davi");
    auto call3 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    }
    Manager::instance().addSubCall(aliceId, call3, aliceId, confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
    }
    auto aliceConf = aliceAccount->getConference(confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return aliceConf->isMediaSourceMuted(jami::MediaType::MEDIA_AUDIO); }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testCreateParticipantsSinks()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobUri = bobAccount->getUsername();
    auto carlaUri = carlaAccount->getUsername();

    startConference();

    auto expectedNumberOfParticipants = 3u;

    // Check participants number
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));
    }

    auto dm = jami::getVideoDeviceMonitor();
    if (dm && !dm->getDeviceList().empty()) {
        JAMI_LOG("Check sinks if video device available.");
        std::unique_lock lk {mtx};
        for (auto& info : pInfos_) {
            auto uri = string_remove_suffix(info["uri"], '@');
            if (uri == bobUri) {
                CPPUNIT_ASSERT(
                    cv.wait_for(lk, 5s, [&] { return Manager::instance().getSinkClient(info["sinkId"]) != nullptr; }));
            } else if (uri == carlaUri) {
                CPPUNIT_ASSERT(
                    cv.wait_for(lk, 5s, [&] { return Manager::instance().getSinkClient(info["sinkId"]) != nullptr; }));
            }
        }
    } else {
        JAMI_LOG("Check sinks if no video device available.");
        std::unique_lock lk {mtx};
        for (auto& info : pInfos_) {
            auto uri = string_remove_suffix(info["uri"], '@');
            if (uri == bobUri) {
                CPPUNIT_ASSERT(
                    cv.wait_for(lk, 5s, [&] { return Manager::instance().getSinkClient(info["sinkId"]) == nullptr; }));
            } else if (uri == carlaUri) {
                CPPUNIT_ASSERT(
                    cv.wait_for(lk, 5s, [&] { return Manager::instance().getSinkClient(info["sinkId"]) == nullptr; }));
            }
        }
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testAttachParticipantSinkAfterReceiverStarts()
{
    ConfInfo infos;
    infos.w = 2560;
    infos.h = 1440;
    ParticipantInfo participant;
    participant.sinkId = "late-video-receiver";
    participant.w = 960;
    participant.h = 720;
    infos.emplace_back(participant);

    std::map<std::string, std::shared_ptr<video::SinkClient>> sinks;
    auto receiver = std::make_shared<video::VideoFrameActiveWriter>();
    auto& manager = Manager::instance();

    manager.createSinkClients("late-receiver-call", infos, {}, sinks, "");
    CPPUNIT_ASSERT_EQUAL(size_t(1), sinks.size());
    CPPUNIT_ASSERT_EQUAL(size_t(0), receiver->getObserversCount());

    manager.createSinkClients("late-receiver-call", infos, {receiver}, sinks, "");
    CPPUNIT_ASSERT_EQUAL(size_t(1), receiver->getObserversCount());

    manager.createSinkClients("late-receiver-call", infos, {receiver}, sinks, "");
    CPPUNIT_ASSERT_EQUAL(size_t(1), receiver->getObserversCount());

    auto restartedReceiver = std::make_shared<video::VideoFrameActiveWriter>();
    manager.createSinkClients("late-receiver-call", infos, {restartedReceiver}, sinks, "");
    CPPUNIT_ASSERT_EQUAL(size_t(1), restartedReceiver->getObserversCount());

    manager.createSinkClients("late-receiver-call", ConfInfo {}, {receiver, restartedReceiver}, sinks, "");
    CPPUNIT_ASSERT_EQUAL(size_t(0), receiver->getObserversCount());
    CPPUNIT_ASSERT_EQUAL(size_t(0), restartedReceiver->getObserversCount());
}

void
ConferenceTest::testMuteStatusAfterRemove()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    libjami::muteStream(aliceId, confId, daviUri, daviCall.device, daviCall.streamId, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return daviCall.moderatorMuted; }));
    }

    Manager::instance().hangupCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    }
    daviCall.reset();

    auto call2 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    }
    Manager::instance().addSubCall(aliceId, call2, aliceId, confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
    }

    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !daviCall.moderatorMuted; }));
    }

    Manager::instance().hangupCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    }
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testActiveStatusAfterRemove()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    MediaAttribute defaultAudio(MediaType::MEDIA_AUDIO);
    defaultAudio.label_ = "audio_0";
    defaultAudio.enabled_ = true;

    libjami::setActiveStream(aliceId, confId, daviUri, daviCall.device, daviCall.streamId, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return daviCall.active; }));
    }

    Manager::instance().hangupCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    }
    daviCall.reset();

    auto call2 = libjami::placeCallWithMedia(aliceId,
                                             daviUri,
                                             MediaAttribute::mediaAttributesToMediaMaps({defaultAudio}));
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    }
    Manager::instance().addSubCall(aliceId, call2, aliceId, confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
    }

    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !daviCall.active; }));
    }

    Manager::instance().hangupCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    }
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testHandsUp()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    JAMI_LOG("Play with raise hand");
    libjami::raiseHand(bobId, bobCall.callId, bobUri, bobCall.device, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.raisedHand; }));
    }

    libjami::raiseHand(bobId, bobCall.callId, bobUri, bobCall.device, false);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.raisedHand; }));
    }

    // Remove davi from moderators
    libjami::setModerator(aliceId, confId, daviUri, false);

    // Test to raise hand
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
    }
    libjami::raiseHand(daviId, daviCall.callId, daviUri, daviCall.device, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return daviCall.raisedHand; }));
    }

    // Test to raise hand for another one (should fail)
    libjami::raiseHand(bobId, bobCall.callId, carlaUri, carlaCall.device, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&] { return carlaCall.raisedHand; }));
    }

    // However, a moderator should be able to lower the hand (but not a non moderator)
    libjami::raiseHand(carlaId, carlaCall.callId, carlaUri, carlaCall.device, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return carlaCall.raisedHand; }));
    }

    libjami::raiseHand(daviId, carlaCall.callId, carlaUri, carlaCall.device, false);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(!cv.wait_for(lk, 5s, [&] { return !carlaCall.raisedHand; }));
    }

    libjami::raiseHand(bobId, bobCall.callId, carlaUri, carlaCall.device, false);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !carlaCall.raisedHand; }));
    }

    Manager::instance().hangupCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    }
    daviCall.reset();

    auto call2 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    }
    Manager::instance().addSubCall(aliceId, call2, aliceId, confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.device.empty(); }));
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !daviCall.raisedHand; }));
    }

    Manager::instance().hangupCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.state == "OVER"; }));
    }
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testPeerLeaveConference()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();
    Manager::instance().hangupCall(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return bobCall.state == "OVER" && confId.empty(); }));
    }

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testJoinCallFromOtherAccount()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference();

    JAMI_LOG("Play with raise hand");
    libjami::raiseHand(aliceId, confId, bobUri, bobCall.device, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.raisedHand; }));
    }

    libjami::raiseHand(aliceId, confId, bobUri, bobCall.device, false);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.raisedHand; }));
    }
    JAMI_LOG("Start call between Alice and Davi");
    auto call1 = libjami::placeCallWithMedia(aliceId, daviUri, {});
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return !daviCall.callId.empty(); }));
    }
    Manager::instance().acceptCall(daviId, daviCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return daviCall.hostState == "CURRENT"; }));
    }
    CPPUNIT_ASSERT(Manager::instance().addSubCall(daviId, daviCall.callId, aliceId, confId));
    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testDevices()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto bobDevice = std::string(bobAccount->currentDeviceId());
    auto carlaDevice = std::string(carlaAccount->currentDeviceId());

    startConference();

    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(
            cv.wait_for(lk, 5s, [&] { return bobCall.device == bobDevice && carlaDevice == carlaCall.device; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testUnauthorizedSetActive()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto carlaUri = carlaAccount->getUsername();

    startConference();

    libjami::setActiveStream(carlaId, confId, bobUri, bobCall.device, bobCall.streamId, true);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(!cv.wait_for(lk, 15s, [&] { return bobCall.active; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testHangup()
{
    registerSignalHandlers();

    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    auto carlaAccount = Manager::instance().getAccount<JamiAccount>(carlaId);
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto daviUri = daviAccount->getUsername();

    startConference(false, true);

    libjami::hangupParticipant(carlaId, confId, daviUri, daviCall.device); // Unauthorized
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(!cv.wait_for(lk, 10s, [&] { return daviCall.state == "OVER"; }));
    }
    libjami::hangupParticipant(aliceId, confId, daviUri, daviCall.device);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return daviCall.state == "OVER"; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testIsConferenceParticipant()
{
    registerSignalHandlers();

    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();

    startConference();

    // is Conference participant should be true for Carla
    auto subCalls = aliceAccount->getConference(confId)->getSubCalls();
    CPPUNIT_ASSERT(subCalls.size() == 2);
    auto call1 = *subCalls.begin();
    auto call2 = *subCalls.rbegin();
    CPPUNIT_ASSERT(aliceAccount->getCall(call1)->isConferenceParticipant());
    CPPUNIT_ASSERT(aliceAccount->getCall(call2)->isConferenceParticipant());

    // hangup bob will stop the conference
    Manager::instance().hangupCall(aliceId, call1);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return confId.empty(); }));
    }
    CPPUNIT_ASSERT(!aliceAccount->getCall(call2)->isConferenceParticipant());
    Manager::instance().hangupCall(aliceId, call2);

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testHostAddRmSecondVideo()
{
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();
    registerSignalHandlers();

    startConference();

    // Alice adds new media
    pInfos_.clear();
    std::vector<std::map<std::string, std::string>> mediaList
        = {{{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "bar"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "foo"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_1"}}};
    libjami::requestMediaChange(aliceId, confId, mediaList);

    // Check that alice has two videos attached to the conference
    auto aliceVideos = [&]() {
        int result = 0;
        for (auto i = 0u; i < pInfos_.size(); ++i)
            if (pInfos_[i]["uri"].find(aliceUri) != std::string::npos)
                result += 1;
        return result;
    };
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceVideos() == 2; }));
    }

    // Alice removes her second video
    {
        std::lock_guard lock(mtx);
        pInfos_.clear();
    }
    mediaList.pop_back();
    libjami::requestMediaChange(aliceId, confId, mediaList);

    // Check that alice has ont video attached to the conference
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceVideos() == 1; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testAudioConferenceConfInfo()
{
    registerSignalHandlers();
    auto aliceAccount = Manager::instance().getAccount<JamiAccount>(aliceId);
    auto aliceUri = aliceAccount->getUsername();

    startConference(true);

    // Check that alice's video is muted
    auto aliceVideoMuted = [&]() {
        int result = 0;
        for (auto i = 0u; i < pInfos_.size(); ++i) {
            if (pInfos_[i]["uri"].find(aliceUri) != std::string::npos && pInfos_[i]["videoMuted"] == "true"
                && pInfos_[i]["sinkId"] == "host_video_0")
                result += 1;
        }
        return result;
    };
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return aliceVideoMuted() == 1; }));
    }

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testParticipantAddRmSecondVideo()
{
    auto bobAccount = Manager::instance().getAccount<JamiAccount>(bobId);
    auto bobUri = bobAccount->getUsername();
    registerSignalHandlers();

    startConference();

    // Bob adds new media
    pInfos_.clear();
    std::vector<std::map<std::string, std::string>> mediaList
        = {{{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::AUDIO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, ""},
            {libjami::Media::MediaAttributeKey::LABEL, "audio_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "bar"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_0"}},
           {{libjami::Media::MediaAttributeKey::MEDIA_TYPE, libjami::Media::MediaAttributeValue::VIDEO},
            {libjami::Media::MediaAttributeKey::ENABLED, "true"},
            {libjami::Media::MediaAttributeKey::MUTED, "false"},
            {libjami::Media::MediaAttributeKey::SOURCE, "foo"},
            {libjami::Media::MediaAttributeKey::LABEL, "video_1"}}};
    libjami::requestMediaChange(bobId, bobCall.callId, mediaList);

    // Check that bob has two videos attached to the conference
    auto bobVideos = [&]() {
        int result = 0;
        for (auto i = 0u; i < pInfos_.size(); ++i)
            if (pInfos_[i]["uri"].find(bobUri) != std::string::npos)
                result += 1;
        return result;
    };
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return bobVideos() == 2; }));
    }

    // Bob removes his second video
    {
        std::lock_guard lock(mtx);
        pInfos_.clear();
    }
    mediaList.pop_back();
    libjami::requestMediaChange(bobId, bobCall.callId, mediaList);

    // Check that bob has ont video attached to the conference
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 10s, [&] { return bobVideos() == 1; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testPropagateRecording()
{
    registerSignalHandlers();

    startConference();

    JAMI_LOG("Play with recording state");
    libjami::toggleRecording(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return bobCall.recording; }));
    }

    libjami::toggleRecording(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !bobCall.recording; }));
    }
    libjami::toggleRecording(aliceId, confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return hostRecording; }));
    }

    libjami::toggleRecording(aliceId, confId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return !hostRecording; }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testBrokenParticipantAudioAndVideo()
{
    registerSignalHandlers();

    // Start conference with four participants
    startConference(false, true);
    auto expectedNumberOfParticipants = 4u;

    // Check participants number
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));
    }

    // Crash participant
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto call2Crash = std::dynamic_pointer_cast<SIPCall>(daviAccount->getCall(daviCall.callId));
    pjsip_transport_shutdown(call2Crash->getTransport()->get());

    // Check participants number
    // It should have one less participant than in the conference start
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return expectedNumberOfParticipants - 1 == pInfos_.size(); }));
    }

    hangupConference();

    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testBrokenParticipantAudioOnly()
{
    registerSignalHandlers();

    // Start conference with four participants
    startConference(true, true);
    auto expectedNumberOfParticipants = 4u;

    // Check participants number
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));
    }

    // Crash participant
    auto daviAccount = Manager::instance().getAccount<JamiAccount>(daviId);
    auto call2Crash = std::dynamic_pointer_cast<SIPCall>(daviAccount->getCall(daviCall.callId));
    pjsip_transport_shutdown(call2Crash->getTransport()->get());

    // Check participants number
    // It should have one less participant than in the conference start
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return expectedNumberOfParticipants - 1 == pInfos_.size(); }));
    }

    hangupConference();
    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testAudioOnlyLeaveLayout()
{
    registerSignalHandlers();

    // Start conference with four participants
    startConference(true, true);
    auto expectedNumberOfParticipants = 4u;

    // Check participants number
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return pInfos_.size() == expectedNumberOfParticipants; }));
    }

    // Carla Leave
    Manager::instance().hangupCall(carlaId, carlaCall.callId);

    // Check participants number
    // It should have one less participant than in the conference start
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return expectedNumberOfParticipants - 1 == pInfos_.size(); }));
    }

    hangupConference();
    libjami::unregisterSignalHandlers();
}

void
ConferenceTest::testRemoveConferenceInOneOne()
{
    registerSignalHandlers();
    startConference();
    // Here it's 1:1 calls we merged, so we can close the conference
    JAMI_LOG("Hangup Bob");
    Manager::instance().hangupCall(bobId, bobCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 20s, [&] { return confId.empty() && bobCall.state == "OVER"; }));
    }
    Manager::instance().hangupCall(carlaId, carlaCall.callId);
    {
        std::unique_lock lk {mtx};
        CPPUNIT_ASSERT(cv.wait_for(lk, 30s, [&] { return carlaCall.state == "OVER"; }));
    }
    libjami::unregisterSignalHandlers();
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::ConferenceTest::name())
