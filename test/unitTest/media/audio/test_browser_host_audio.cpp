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

#include "jami.h"
#include "manager.h"
#include "media/audio/audio_input.h"
#include "media/audio/audio_receive_thread.h"
#include "media/audio/audio_rtp_session.h"
#include "media/audio/audiolayer.h"
#include "media/audio/ringbuffer.h"
#include "media/audio/ringbufferpool.h"
#include "media/media_buffer.h"
#include "media/dtls_srtp.h"
#include "observer.h"
#ifdef ENABLE_VIDEO
#include "media/video/video_receive_thread.h"
#include "media/video/video_rtp_session.h"
#endif

#include "../../../test_runner.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>

namespace jami::test {

class BrowserHostAudioTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "browser_host_audio"; }

    BrowserHostAudioTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        if (not Manager::instance().initialized)
            CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    }

    ~BrowserHostAudioTest() override { libjami::fini(); }

    void testBrowserSenderReadsOnlyBoundSources();
    void testBrowserReceiverDoesNotUseLocalPlayback();
    void testDefaultReceiverStillUsesLocalPlayback();
    void testBrowserSenderStopsWithoutReceiver();
    void testAudioTransportFailureIsReported();
    void testDisabledAudioDoesNotReportFailure();
    void testAudioReceiverSetupFailureIsReported();
    void testQueuedStartupRemainsCancelled();
    void testStopInterruptsDtls();
    void testBundleWaitersCancelIndependently();
#ifdef ENABLE_VIDEO
    void testVideoTransportFailureIsReported();
    void testVideoReceiverSetupFailureCanStopFromCallback();
#endif

    CPPUNIT_TEST_SUITE(BrowserHostAudioTest);
    CPPUNIT_TEST(testBrowserSenderReadsOnlyBoundSources);
    CPPUNIT_TEST(testBrowserReceiverDoesNotUseLocalPlayback);
    CPPUNIT_TEST(testDefaultReceiverStillUsesLocalPlayback);
    CPPUNIT_TEST(testBrowserSenderStopsWithoutReceiver);
    CPPUNIT_TEST(testAudioTransportFailureIsReported);
    CPPUNIT_TEST(testDisabledAudioDoesNotReportFailure);
    CPPUNIT_TEST(testAudioReceiverSetupFailureIsReported);
    CPPUNIT_TEST(testQueuedStartupRemainsCancelled);
    CPPUNIT_TEST(testStopInterruptsDtls);
    CPPUNIT_TEST(testBundleWaitersCancelIndependently);
#ifdef ENABLE_VIDEO
    CPPUNIT_TEST(testVideoTransportFailureIsReported);
    CPPUNIT_TEST(testVideoReceiverSetupFailureCanStopFromCallback);
#endif
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(BrowserHostAudioTest, BrowserHostAudioTest::name());

static std::shared_ptr<AudioFrame>
audioFrame(const AudioFormat& format, int16_t sample)
{
    auto frame = std::make_shared<AudioFrame>(format, format.sample_rate / 50);
    auto* data = reinterpret_cast<int16_t*>(frame->pointer()->extended_data[0]);
    data[0] = sample;
    return frame;
}

void
BrowserHostAudioTest::testBrowserSenderReadsOnlyBoundSources()
{
    auto& pool = Manager::instance().getRingBufferPool();
    auto driver = Manager::instance().getAudioDriver();
    CPPUNIT_ASSERT(driver);
    const bool deviceWasStarted = driver->isStarted();
    const auto format = pool.getInternalAudioFormat();
    auto source = pool.createRingBuffer("browser-host-test-source");
    auto session = std::make_shared<AudioRtpSession>("browser-host-test",
                                                     "browser-host-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);

    pool.bindHalfDuplexOut(session->streamId(), source->getId());
    auto input = session->prepareAudioInput();
    CPPUNIT_ASSERT(input);
    CPPUNIT_ASSERT(input->isCapturing());
    CPPUNIT_ASSERT(input->getSourceRingBufferId().empty());
    CPPUNIT_ASSERT_EQUAL(deviceWasStarted, driver->isStarted());

    std::mutex mutex;
    std::condition_variable cv;
    bool received = false;
    int16_t receivedSample = 0;
    FuncObserver<std::shared_ptr<MediaFrame>> observer([&](const auto& media) {
        auto frame = std::static_pointer_cast<AudioFrame>(media);
        {
            std::lock_guard lk(mutex);
            receivedSample = reinterpret_cast<int16_t*>(frame->pointer()->extended_data[0])[0];
            received = true;
        }
        cv.notify_one();
    });
    input->attach(&observer);
    source->put(audioFrame(format, 1421));
    {
        std::unique_lock lk(mutex);
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&] { return received; }));
    }
    input->detach(&observer);
    CPPUNIT_ASSERT_EQUAL(int16_t(1421), receivedSample);
    CPPUNIT_ASSERT(!pool.getData(RingBufferPool::DEFAULT_ID));
    pool.unBindHalfDuplexOut(session->streamId(), source->getId());
}

void
BrowserHostAudioTest::testBrowserReceiverDoesNotUseLocalPlayback()
{
    auto& pool = Manager::instance().getRingBufferPool();
    auto session = std::make_shared<AudioRtpSession>("browser-receiver-test",
                                                     "browser-receiver-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);
    auto participant = pool.createRingBuffer("browser-receiver-test-participant");
    session->bindReceivedAudio();
    pool.bindHalfDuplexOut(participant->getId(), session->streamId());
    session->ringbuffer_->put(audioFrame(pool.getInternalAudioFormat(), 514));
    CPPUNIT_ASSERT(!pool.getData(RingBufferPool::DEFAULT_ID));
    auto received = pool.getData(participant->getId());
    CPPUNIT_ASSERT(received);
    CPPUNIT_ASSERT_EQUAL(int16_t(514), reinterpret_cast<int16_t*>(received->pointer()->extended_data[0])[0]);
    pool.unBindHalfDuplexOut(participant->getId(), session->streamId());
    session->unbindReceivedAudio();
}

void
BrowserHostAudioTest::testDefaultReceiverStillUsesLocalPlayback()
{
    auto& pool = Manager::instance().getRingBufferPool();
    auto session = std::make_shared<AudioRtpSession>("default-receiver-test", "default-receiver-test-audio", nullptr);
    session->bindReceivedAudio();
    session->ringbuffer_->put(audioFrame(pool.getInternalAudioFormat(), 728));
    auto received = pool.getData(RingBufferPool::DEFAULT_ID);
    CPPUNIT_ASSERT(received);
    CPPUNIT_ASSERT_EQUAL(int16_t(728), reinterpret_cast<int16_t*>(received->pointer()->extended_data[0])[0]);
    session->unbindReceivedAudio();
}

void
BrowserHostAudioTest::testBrowserSenderStopsWithoutReceiver()
{
    auto session = std::make_shared<AudioRtpSession>("browser-stop-test",
                                                     "browser-stop-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);
    auto input = session->prepareAudioInput();
    CPPUNIT_ASSERT(input);
    std::weak_ptr<AudioInput> weakInput = input;
    input.reset();
    session->stop();
    CPPUNIT_ASSERT(!session->getAudioLocal());
    CPPUNIT_ASSERT(weakInput.expired());
}

void
BrowserHostAudioTest::testAudioTransportFailureIsReported()
{
    auto session = std::make_shared<AudioRtpSession>("failure-test",
                                                     "failure-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);
    MediaDescription media;
    media.enabled = true;
    media.key_exchange = KeyExchangeProtocol::DTLS;
    session->updateMedia(media, media);

    std::mutex mutex;
    std::condition_variable cv;
    std::string reason;
    MediaType failedType = MediaType::MEDIA_NONE;
    session->setStartupFailureCb([&](MediaType type, const std::string& failure) {
        {
            std::lock_guard lk(mutex);
            failedType = type;
            reason = failure;
        }
        cv.notify_one();
    });
    session->start(nullptr, nullptr);
    {
        std::unique_lock lk(mutex);
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&] { return !reason.empty(); }));
    }
    CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_AUDIO, failedType);
    CPPUNIT_ASSERT(reason.find("DTLS-SRTP") != std::string::npos);
    CPPUNIT_ASSERT(!session->isRtpReady(true, true));
    session->stop();
}

void
BrowserHostAudioTest::testDisabledAudioDoesNotReportFailure()
{
    auto session = std::make_shared<AudioRtpSession>("disabled-test",
                                                     "disabled-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);
    std::mutex mutex;
    std::condition_variable cv;
    bool failed = false;
    session->setStartupFailureCb([&](MediaType, const std::string&) {
        {
            std::lock_guard lk(mutex);
            failed = true;
        }
        cv.notify_one();
    });
    session->start(nullptr, nullptr);
    CPPUNIT_ASSERT(!session->isRtpReady(false, false));
    std::unique_lock lk(mutex);
    CPPUNIT_ASSERT(!cv.wait_for(lk, std::chrono::milliseconds(100), [&] { return failed; }));
    lk.unlock();
    session->stop();
}

void
BrowserHostAudioTest::testAudioReceiverSetupFailureIsReported()
{
    auto& pool = Manager::instance().getRingBufferPool();
    const auto format = pool.getInternalAudioFormat();
    auto receiver = std::make_shared<AudioReceiveThread>("failed-audio-receiver", format, "not-an-sdp", 1200);
    std::weak_ptr<AudioReceiveThread> weak = receiver;
    std::mutex mutex;
    std::condition_variable cv;
    std::string reason;
    receiver->setSetupFailureCb([&, weak](const std::string& failure) {
        if (auto shared = weak.lock())
            shared->stopReceiver();
        {
            std::lock_guard lk(mutex);
            reason = failure;
        }
        cv.notify_one();
    });
    receiver->startReceiver();
    {
        std::unique_lock lk(mutex);
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&] { return !reason.empty(); }));
    }
    CPPUNIT_ASSERT_EQUAL(std::string("Audio RTP input is not configured"), reason);
}

void
BrowserHostAudioTest::testQueuedStartupRemainsCancelled()
{
    auto session = std::make_shared<AudioRtpSession>("cancel-test",
                                                     "cancel-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);
    MediaDescription media;
    media.enabled = true;
    media.key_exchange = KeyExchangeProtocol::DTLS;
    session->updateMedia(media, media);

    const auto queued = session->prepareStart();
    std::unique_lock lock(session->mutex_);
    std::promise<void> cancelled;
    std::stop_callback onCancel(queued, [&] { cancelled.set_value(); });
    auto stopping = std::async(std::launch::async, [&] { session->stop(); });
    cancelled.get_future().wait();
    const auto duringStop = session->prepareStart();
    lock.unlock();
    stopping.get();
    const auto next = session->prepareStart();
    session->start(nullptr, nullptr, queued);
    session->start(nullptr, nullptr, duringStop);
    CPPUNIT_ASSERT(!session->startupFailed_.load());
    CPPUNIT_ASSERT(!session->socketPair_);
    CPPUNIT_ASSERT(!next.stop_requested());
    session->stop();
}

void
BrowserHostAudioTest::testStopInterruptsDtls()
{
    using namespace std::chrono_literals;
    auto session = std::make_shared<AudioRtpSession>("dtls-stop-test",
                                                     "dtls-stop-test-audio",
                                                     nullptr,
                                                     AudioRtpSession::Mode::CONFERENCE_HOST);
    const auto identity = generateDtlsSrtpIdentity();
    session->setDtlsSrtpIdentity(identity.second, identity.first);
    MediaDescription media;
    media.enabled = true;
    media.key_exchange = KeyExchangeProtocol::DTLS;
    media.dtls_setup = DtlsSetup::PASSIVE;
    media.dtls_fingerprint_type = "SHA-256";
    media.dtls_fingerprint = getDtlsFingerprint(*identity.second);
    session->updateMedia(media, media);
    const auto token = session->prepareStart();
    std::promise<void> entered;
    auto starting = std::async(std::launch::async, [&] {
        std::lock_guard lock(session->mutex_);
        entered.set_value();
        session->start(std::make_unique<dhtnet::IceSocket>(nullptr, 1), nullptr, token);
    });
    entered.get_future().wait();
    const auto wasWaiting = starting.wait_for(50ms);
    const auto begin = std::chrono::steady_clock::now();
    session->stop();
    starting.get();
    CPPUNIT_ASSERT(wasWaiting == std::future_status::timeout);
    CPPUNIT_ASSERT(std::chrono::steady_clock::now() - begin < 100ms);
    CPPUNIT_ASSERT(!session->startupFailed_.load());
    CPPUNIT_ASSERT(!session->socketPair_);
}

void
BrowserHostAudioTest::testBundleWaitersCancelIndependently()
{
    using namespace std::chrono_literals;
    const auto identity = generateDtlsSrtpIdentity();
    const auto fingerprint = getDtlsFingerprint(*identity.second);
    std::stop_source transport;
    auto bundle = SocketPair::createBundleContext(std::make_unique<dhtnet::IceSocket>(nullptr, 1),
                                                  nullptr,
                                                  true,
                                                  false,
                                                  transport.get_token());
    std::stop_source first;
    std::stop_source second;
    const auto wait = [&](std::stop_token token) {
        try {
            SocketPair::ensureBundleDtlsContext(bundle,
                                                DtlsSetup::PASSIVE,
                                                "SHA-256",
                                                fingerprint,
                                                identity.second,
                                                identity.first,
                                                token);
            return false;
        } catch (const DtlsCancelled&) {
            return true;
        }
    };
    auto firstWait = std::async(std::launch::async, wait, first.get_token());
    auto secondWait = std::async(std::launch::async, wait, second.get_token());
    const auto firstPending = firstWait.wait_for(50ms);
    first.request_stop();
    const auto firstStopped = firstWait.wait_for(100ms);
    const auto secondPending = secondWait.wait_for(0ms);
    const auto cancelledAt = std::chrono::steady_clock::now();
    transport.request_stop();
    const auto secondStopped = secondWait.wait_for(100ms);
    SocketPair::joinBundleDtls(bundle);
    const auto stopDuration = std::chrono::steady_clock::now() - cancelledAt;
    CPPUNIT_ASSERT(firstWait.get());
    CPPUNIT_ASSERT(secondWait.get());
    CPPUNIT_ASSERT(firstPending == std::future_status::timeout);
    CPPUNIT_ASSERT(firstStopped == std::future_status::ready);
    CPPUNIT_ASSERT(secondPending == std::future_status::timeout);
    CPPUNIT_ASSERT(secondStopped == std::future_status::ready);
    CPPUNIT_ASSERT(stopDuration < 100ms);
}

#ifdef ENABLE_VIDEO
void
BrowserHostAudioTest::testVideoTransportFailureIsReported()
{
    auto session = std::make_shared<video::VideoRtpSession>("failure-test",
                                                            "failure-test-video",
                                                            DeviceParams {},
                                                            nullptr);
    MediaDescription media;
    media.enabled = true;
    media.key_exchange = KeyExchangeProtocol::DTLS;
    session->updateMedia(media, media);

    std::mutex mutex;
    std::condition_variable cv;
    std::string reason;
    MediaType failedType = MediaType::MEDIA_NONE;
    session->setStartupFailureCb([&](MediaType type, const std::string& failure) {
        {
            std::lock_guard lk(mutex);
            failedType = type;
            reason = failure;
        }
        cv.notify_one();
    });
    session->start(nullptr, nullptr);
    {
        std::unique_lock lk(mutex);
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&] { return !reason.empty(); }));
    }
    CPPUNIT_ASSERT_EQUAL(MediaType::MEDIA_VIDEO, failedType);
    CPPUNIT_ASSERT(reason.find("DTLS-SRTP") != std::string::npos);
    CPPUNIT_ASSERT(!session->isRtpReady(true, true));
}

void
BrowserHostAudioTest::testVideoReceiverSetupFailureCanStopFromCallback()
{
    auto receiver = std::make_shared<video::VideoReceiveThread>("failed-video-receiver", false, "not-an-sdp", 1200);
    std::weak_ptr<video::VideoReceiveThread> weak = receiver;
    std::mutex mutex;
    std::condition_variable cv;
    std::string reason;
    receiver->setSetupFailureCb([&, weak](const std::string& failure) {
        if (auto shared = weak.lock())
            shared->stopLoop();
        {
            std::lock_guard lk(mutex);
            reason = failure;
        }
        cv.notify_one();
    });
    receiver->startLoop();
    {
        std::unique_lock lk(mutex);
        CPPUNIT_ASSERT(cv.wait_for(lk, std::chrono::seconds(3), [&] { return !reason.empty(); }));
    }
    CPPUNIT_ASSERT_EQUAL(std::string("Video RTP input is not configured"), reason);
}
#endif

} // namespace jami::test

CORE_TEST_RUNNER(jami::test::BrowserHostAudioTest::name());
