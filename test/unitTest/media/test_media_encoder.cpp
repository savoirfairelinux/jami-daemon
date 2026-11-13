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

#include <cstdlib>

#include "jami.h"
#include "fileutils.h"
#include "media/libav_deps.h"
#include "media/media_decoder.h"
#include "media/media_encoder.h"
#include "media/media_io_handle.h"
#include "media/socket_pair.h"
#include "media/system_codec_container.h"
#include "media/video/video_sender.h"
#ifdef ENABLE_HWACCEL
#include "media/video/accel.h"
#endif

#include "../../test_runner.h"

namespace jami {
namespace test {

class MediaEncoderTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "media_encoder"; }

    void setUp();
    void tearDown();

private:
    void testMultiStream();
    void testH264DynamicBitrate();
    void testH264BitrateKeepsResolution();
    void testPassthroughPacketRtpTimestamps();
    void testPassthroughBitrateChangeKeepsStream();
    void testVP8DynamicBitrate();
    void testVP8BitrateKeepsResolution();
    void testNonDynamicHwBitrateKeepsStreamAndResolution();
    void testNegotiatedDynamicResolutionKeepsRtpStreamContinuous();
    void testVideoSenderRestartPreservesSsrcAndDimensions();
    void testVideoSizeSelection();
    void testSourceSizeChangesWithinEightPixels();
    void testHardwareEncodingResolution();

    CPPUNIT_TEST_SUITE(MediaEncoderTest);
    CPPUNIT_TEST(testMultiStream);
    CPPUNIT_TEST(testH264DynamicBitrate);
    CPPUNIT_TEST(testH264BitrateKeepsResolution);
    CPPUNIT_TEST(testPassthroughPacketRtpTimestamps);
    CPPUNIT_TEST(testPassthroughBitrateChangeKeepsStream);
    CPPUNIT_TEST(testVP8DynamicBitrate);
    CPPUNIT_TEST(testVP8BitrateKeepsResolution);
    CPPUNIT_TEST(testNonDynamicHwBitrateKeepsStreamAndResolution);
    CPPUNIT_TEST(testNegotiatedDynamicResolutionKeepsRtpStreamContinuous);
    CPPUNIT_TEST(testVideoSenderRestartPreservesSsrcAndDimensions);
    CPPUNIT_TEST(testVideoSizeSelection);
    CPPUNIT_TEST(testSourceSizeChangesWithinEightPixels);
    CPPUNIT_TEST(testHardwareEncodingResolution);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaEncoder> encoder_;
    std::unique_ptr<MediaIOHandle> ioContext_;
    size_t encodedBytes_ {};
    std::vector<uint32_t> timestamps_;
    std::vector<std::string> files_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaEncoderTest, MediaEncoderTest::name());

void
MediaEncoderTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    libav_utils::av_init();
    encodedBytes_ = 0;
    timestamps_.clear();
    encoder_.reset(new MediaEncoder);
    files_.push_back("test.mkv");
}

void
MediaEncoderTest::tearDown()
{
    // Encoder teardown writes a trailer through the I/O callbacks.
    encoder_.reset();
    ioContext_.reset();
    // clean up behind ourselves
    for (const auto& file : files_)
        dhtnet::fileutils::remove(file);
    libjami::fini();
}

static AVFrame*
getVideoFrame(int width, int height, int frame_index)
{
    int x, y;
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return nullptr;

    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;

    if (av_frame_get_buffer(frame, 32) < 0) {
        av_frame_free(&frame);
        return nullptr;
    }

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            frame->data[0][y * frame->linesize[0] + x] = x + y + frame_index * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + frame_index * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }

    return frame;
}

static void
encodeVideoFrame(MediaEncoder& encoder, int streamIdx, int width, int height, int frameIndex)
{
    AVFrame* video = getVideoFrame(width, height, frameIndex);
    CPPUNIT_ASSERT(video);
    video->pts = frameIndex;
    CPPUNIT_ASSERT(encoder.encode(video, streamIdx) >= 0);
    av_frame_free(&video);
}

static int
countEncodedBytes(void* opaque, const uint8_t*, int bufSize)
{
    auto* byteCount = static_cast<size_t*>(opaque);
    *byteCount += static_cast<size_t>(bufSize);
    return bufSize;
}

static int
collectRtpTimestamps(void* opaque, const uint8_t* buf, int bufSize)
{
    auto* timestamps = static_cast<std::vector<uint32_t>*>(opaque);
    // RTP fixed header: V/P/X/CC, M/PT, sequence, then a 32-bit timestamp.
    // Skip RTCP packets (RFC 5761: second byte in [192, 223]).
    if (bufSize >= 12 && (buf[0] >> 6) == 2 && (buf[1] < 192 || buf[1] > 223)) {
        const uint32_t timestamp = (uint32_t(buf[4]) << 24) | (uint32_t(buf[5]) << 16) | (uint32_t(buf[6]) << 8)
                                   | uint32_t(buf[7]);
        if (timestamps->empty() || timestamps->back() != timestamp)
            timestamps->push_back(timestamp);
    }
    return bufSize;
}

static AVFrame*
getAudioFrame(int sampleRate, int nbSamples, int nbChannels)
{
    const constexpr float pi = 3.14159265358979323846264338327950288; // M_PI
    const float tincr = 2 * pi * 440.0 / sampleRate;
    float t = 0;
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return nullptr;

    frame->format = AV_SAMPLE_FMT_S16;
    av_channel_layout_default(&frame->ch_layout, nbChannels);
    frame->nb_samples = nbSamples;
    frame->sample_rate = sampleRate;

    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        return nullptr;
    }

    auto samples = reinterpret_cast<uint16_t*>(frame->data[0]);
    for (int i = 0; i < 200; ++i) {
        for (int j = 0; j < nbSamples; ++j) {
            samples[2 * j] = static_cast<int>(sin(t) * 10000);
            for (int k = 1; k < nbChannels; ++k) {
                samples[2 * j + k] = samples[2 * j];
            }
            t += tincr;
        }
    }

    return frame;
}

void
MediaEncoderTest::testMultiStream()
{
    const constexpr int sampleRate = 48000;
    const constexpr int nbChannels = 2;
    const constexpr int width = 320;
    const constexpr int height = 240;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto vp8Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("VP8", jami::MEDIA_VIDEO));
    auto opusCodec = std::static_pointer_cast<SystemAudioCodecInfo>(
        codecs->searchCodecByName("opus", jami::MEDIA_AUDIO));
    auto v = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, 1, 30);
    auto a = MediaStream("a", AV_SAMPLE_FMT_S16, rational<int>(1, sampleRate), sampleRate, nbChannels, 960);

    try {
        encoder_->openOutput("test.mkv");
        encoder_->setOptions(a);
        CPPUNIT_ASSERT(encoder_->getStreamCount() == 1);
        int audioIdx = encoder_->addStream(*opusCodec.get());
        CPPUNIT_ASSERT(audioIdx >= 0);
        encoder_->setOptions(v);
        CPPUNIT_ASSERT(encoder_->getStreamCount() == 2);
        int videoIdx = encoder_->addStream(*vp8Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        CPPUNIT_ASSERT(videoIdx != audioIdx);
        encoder_->setIOContext(nullptr);
        int sentSamples = 0;
        AVFrame* audio = nullptr;
        AVFrame* video = nullptr;
        for (int i = 0; i < 25; ++i) {
            audio = getAudioFrame(sampleRate, 0.02 * sampleRate, nbChannels);
            CPPUNIT_ASSERT(audio);
            audio->pts = sentSamples;
            video = getVideoFrame(width, height, i);
            CPPUNIT_ASSERT(video);
            video->pts = i;

            CPPUNIT_ASSERT(encoder_->encode(audio, audioIdx) >= 0);
            sentSamples += audio->nb_samples;
            CPPUNIT_ASSERT(encoder_->encode(video, videoIdx) >= 0);

            av_frame_free(&audio);
            av_frame_free(&video);
        }
        CPPUNIT_ASSERT(encoder_->flush() >= 0);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testH264DynamicBitrate()
{
    const constexpr int width = 320;
    const constexpr int height = 240;
    const constexpr int initialBitrate = 800;
    const constexpr int updatedBitrate = 300;
    const constexpr int restoredBitrate = 1200;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes_);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(initialBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);

        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(updatedBitrate));
        CPPUNIT_ASSERT_EQUAL(updatedBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(restoredBitrate));
        CPPUNIT_ASSERT_EQUAL(restoredBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);
        CPPUNIT_ASSERT(encoder_->flush() >= 0);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testH264BitrateKeepsResolution()
{
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int initialBitrate = 2211;
    const constexpr int constrainedBitrate = 300;
    const constexpr int restoredBitrate = 2200;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes_);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());

        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        encodeVideoFrame(*encoder_, videoIdx, width, height, 1);
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(restoredBitrate));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        encodeVideoFrame(*encoder_, videoIdx, width, height, 2);
        CPPUNIT_ASSERT(encoder_->flush() >= 0);

        // Bitrate feedback arriving before the first frame is encoded must also
        // preserve the initial output dimensions while applying the new bitrate.
        auto preStartEncoder = std::make_unique<MediaEncoder>();
        size_t preStartBytes = 0;
        MediaIOHandle preStartIo(4096, true, nullptr, countEncodedBytes, nullptr, &preStartBytes);
        preStartEncoder->openOutput("rtp://127.0.0.1:5004", "rtp");
        preStartEncoder->setIOContext(preStartIo.getContext());
        preStartEncoder->setOptions(videoStream);
        int preStartIdx = preStartEncoder->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(preStartIdx >= 0);
        CPPUNIT_ASSERT_EQUAL(1, preStartEncoder->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(width, preStartEncoder->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, preStartEncoder->getHeight());
        encodeVideoFrame(*preStartEncoder, preStartIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(constrainedBitrate * 1000, preStartEncoder->getStream("v", preStartIdx).bitrate);
        CPPUNIT_ASSERT(preStartEncoder->flush() >= 0);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testPassthroughPacketRtpTimestamps()
{
    // Pre-encoded packets (hardware encoders on mobile) carry timestamps in
    // microseconds. The RTP muxer must receive them rescaled to the 90 kHz
    // RTP clock: 33.3 ms between frames = 3000 ticks.
    const constexpr int width = 320;
    const constexpr int height = 240;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, 800, 30);

    try {
        auto& timestamps = timestamps_;
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, collectRtpTimestamps, nullptr, &timestamps);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);

        // A minimal (syntactically irrelevant) NAL payload: the RTP payloader
        // treats it as opaque data.
        std::vector<uint8_t> nal {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x33, 0xff};

        for (int frame = 0; frame < 3; ++frame) {
            AVPacket* pkt = av_packet_alloc();
            CPPUNIT_ASSERT(av_new_packet(pkt, static_cast<int>(nal.size())) == 0);
            std::copy(nal.begin(), nal.end(), pkt->data);
            pkt->flags = AV_PKT_FLAG_KEY;
            pkt->pts = int64_t(frame) * 33'333; // microseconds, 30 fps
            CPPUNIT_ASSERT(encoder_->send(*pkt, -1, AVRational {1, 1'000'000}));
            av_packet_free(&pkt);
        }

        CPPUNIT_ASSERT(timestamps.size() >= 3);
        const auto delta = int64_t(timestamps[1]) - int64_t(timestamps[0]);
        std::string allTimestamps;
        for (auto timestamp : timestamps)
            allTimestamps += std::to_string(timestamp) + " ";
        // 33'333 us at 90 kHz is 2999.97 ticks: allow rounding.
        CPPUNIT_ASSERT_MESSAGE("RTP timestamp delta should be ~3000 ticks (90 kHz), got " + std::to_string(delta)
                                   + " (timestamps: " + allTimestamps + ")",
                               delta >= 2999 && delta <= 3001);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testPassthroughBitrateChangeKeepsStream()
{
    // In packet passthrough mode (hardware encoders on mobile) the daemon
    // does not control the source resolution: a bitrate change must not
    // re-target the resolution ladder and reset the RTP stream mid-flight,
    // which breaks remote decoders until the next keyframe.
    const constexpr int width = 720;
    const constexpr int height = 480;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, 800, 30);

    try {
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes_);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);

        std::vector<uint8_t> nal {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x33, 0xff};
        AVPacket* pkt = av_packet_alloc();
        CPPUNIT_ASSERT(av_new_packet(pkt, static_cast<int>(nal.size())) == 0);
        std::copy(nal.begin(), nal.end(), pkt->data);
        pkt->flags = AV_PKT_FLAG_KEY;
        pkt->pts = 0;
        encoder_->setPassthrough(true);
        CPPUNIT_ASSERT(encoder_->send(*pkt, -1, AVRational {1, 1'000'000}));
        av_packet_free(&pkt);

        // 100 kbit/s would select a much smaller ladder resolution.
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(100));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testVP8DynamicBitrate()
{
    const constexpr int width = 320;
    const constexpr int height = 240;
    const constexpr int initialBitrate = 800;
    const constexpr int updatedBitrate = 300;
    const constexpr int restoredBitrate = 1200;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto vp8Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("VP8", jami::MEDIA_VIDEO));
    auto v = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        encoder_->openOutput("test_vp8_dynamic_bitrate.mkv");
        files_.push_back("test_vp8_dynamic_bitrate.mkv");
        encoder_->setOptions(v);
        int videoIdx = encoder_->addStream(*vp8Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(initialBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);

        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(updatedBitrate));
        CPPUNIT_ASSERT_EQUAL(updatedBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(restoredBitrate));
        CPPUNIT_ASSERT_EQUAL(restoredBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);
        CPPUNIT_ASSERT(encoder_->flush() >= 0);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testVP8BitrateKeepsResolution()
{
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int initialBitrate = 2500;
    const constexpr int constrainedBitrate = 300;
    const constexpr int restoredBitrate = 2200;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto vp8Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("VP8", jami::MEDIA_VIDEO));
    auto v = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes_);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(v);
        int videoIdx = encoder_->addStream(*vp8Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());

        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        encodeVideoFrame(*encoder_, videoIdx, width, height, 1);
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(restoredBitrate));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        encodeVideoFrame(*encoder_, videoIdx, width, height, 2);
        CPPUNIT_ASSERT(encoder_->flush() >= 0);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

struct RtpPacketHeaderInfo
{
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
};

static int
collectRtpHeaders(void* opaque, const uint8_t* buf, int bufSize)
{
    auto* headers = static_cast<std::vector<RtpPacketHeaderInfo>*>(opaque);
    if (bufSize >= 12 && (buf[0] >> 6) == 2 && (buf[1] < 192 || buf[1] > 223)) {
        const uint16_t sequence = (uint16_t(buf[2]) << 8) | uint16_t(buf[3]);
        const uint32_t timestamp = (uint32_t(buf[4]) << 24) | (uint32_t(buf[5]) << 16) | (uint32_t(buf[6]) << 8)
                                   | uint32_t(buf[7]);
        const uint32_t ssrc = (uint32_t(buf[8]) << 24) | (uint32_t(buf[9]) << 16) | (uint32_t(buf[10]) << 8)
                              | uint32_t(buf[11]);
        headers->push_back({sequence, timestamp, ssrc});
    }
    return bufSize;
}

void
MediaEncoderTest::testNonDynamicHwBitrateKeepsStreamAndResolution()
{
#ifdef ENABLE_HWACCEL
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int initialBitrate = 2500;
    const constexpr int constrainedBitrate = 300;
    const constexpr int jitterBitrate = 315;
    auto compatibleAccel = video::HardwareAccel::getCompatibleAccel(AV_CODEC_ID_H264, width, height, CODEC_ENCODER);
    auto nonDynAccel = std::find_if(compatibleAccel.begin(), compatibleAccel.end(), [](const auto& accel) {
        return !accel.dynBitrate() && (accel.getName() == "videotoolbox" || accel.getName() == "vaapi");
    });
    if (nonDynAccel == compatibleAccel.end())
        return;

    CPPUNIT_ASSERT(!nonDynAccel->dynBitrate());
    if (!avcodec_find_encoder_by_name(nonDynAccel->getCodecName().c_str()))
        return;

    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        std::vector<RtpPacketHeaderInfo> rtpHeaders;
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, collectRtpHeaders, nullptr, &rtpHeaders);
        encoder_->enableAccel(true);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);

        auto sendFrame = [&](int frameIdx) {
            auto input = std::make_shared<VideoFrame>();
            input->reserve(AV_PIX_FMT_YUV420P, width, height);
            libav_utils::fillWithBlack(input->pointer());
            CPPUNIT_ASSERT(encoder_->encode(input, frameIdx == 0, frameIdx) >= 0);
        };

        sendFrame(0);
        if (encoder_->getStream("v", videoIdx).format != AV_PIX_FMT_NV12)
            return;

        CPPUNIT_ASSERT_EQUAL(static_cast<int>(initialBitrate * 1000 * 0.8f), encoder_->getStream("v", videoIdx).bitrate);

        // Non-dynamic HW encoders must reopen only the codec context in-place:
        // RTP dimensions, SSRC, sequence numbers, and 90 kHz clock stay continuous.
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        sendFrame(1);
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        CPPUNIT_ASSERT_EQUAL(static_cast<int>(constrainedBitrate * 1000 * 0.8f),
                             encoder_->getStream("v", videoIdx).bitrate);

        // Small Transport-CC jitter (<10%) must not trigger another HW codec reopen.
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(jitterBitrate));
        sendFrame(2);
        CPPUNIT_ASSERT_EQUAL(static_cast<int>(constrainedBitrate * 1000 * 0.8f),
                             encoder_->getStream("v", videoIdx).bitrate);
        CPPUNIT_ASSERT(encoder_->flush() >= 0);

        CPPUNIT_ASSERT(!rtpHeaders.empty());
        const auto ssrc = rtpHeaders.front().ssrc;
        std::vector<uint32_t> frameTimestamps;
        for (size_t i = 0; i < rtpHeaders.size(); ++i) {
            CPPUNIT_ASSERT_EQUAL(ssrc, rtpHeaders[i].ssrc);
            if (i > 0) {
                CPPUNIT_ASSERT_EQUAL(uint16_t(rtpHeaders[i - 1].sequence + 1), rtpHeaders[i].sequence);
            }
            if (frameTimestamps.empty() || frameTimestamps.back() != rtpHeaders[i].timestamp)
                frameTimestamps.push_back(rtpHeaders[i].timestamp);
        }
        CPPUNIT_ASSERT(frameTimestamps.size() >= 3);
        CPPUNIT_ASSERT_EQUAL(uint32_t(3000), frameTimestamps[1] - frameTimestamps[0]);
        CPPUNIT_ASSERT_EQUAL(uint32_t(3000), frameTimestamps[2] - frameTimestamps[1]);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
#endif
}

void
MediaEncoderTest::testNegotiatedDynamicResolutionKeepsRtpStreamContinuous()
{
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int initialBitrate = 2211;
    const constexpr int constrainedBitrate = 300;
    const constexpr int borderlineBitrate = 1700;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<SystemVideoCodecInfo>(codecs->searchCodecByName("H264", MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);

    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        std::vector<RtpPacketHeaderInfo> rtpHeaders;
        ioContext_ = std::make_unique<MediaIOHandle>(4096, true, nullptr, collectRtpHeaders, nullptr, &rtpHeaders);
#ifdef ENABLE_HWACCEL
        encoder_->enableAccel(false);
#endif
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext_->getContext());
        encoder_->setOptions(videoStream);
        encoder_->setDynamicResolution(true);
        int videoIdx = encoder_->addStream(*h264Codec);
        CPPUNIT_ASSERT(videoIdx >= 0);

        auto sendFrame = [&](int frameIdx) {
            auto input = std::make_shared<VideoFrame>();
            input->reserve(AV_PIX_FMT_YUV420P, width, height);
            libav_utils::fillWithBlack(input->pointer());
            CPPUNIT_ASSERT(encoder_->encode(input, frameIdx == 0, frameIdx) >= 0);
        };

        sendFrame(0);
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());

        // With dynamic resolution negotiated, congestion feedback downscales
        // in-place on the next frame without resetting the RTP muxer.
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(constrainedBitrate));
        sendFrame(1);
        CPPUNIT_ASSERT_EQUAL(480, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(270, encoder_->getHeight());
        CPPUNIT_ASSERT_EQUAL(constrainedBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);

        // Cooldown prevents immediate resolution flapping while still updating
        // the active encoder's rate control immediately.
        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(borderlineBitrate));
        sendFrame(2);
        CPPUNIT_ASSERT_EQUAL(480, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(270, encoder_->getHeight());
        CPPUNIT_ASSERT_EQUAL(borderlineBitrate * 1000, encoder_->getStream("v", videoIdx).bitrate);
        CPPUNIT_ASSERT(encoder_->flush() >= 0);

        CPPUNIT_ASSERT(!rtpHeaders.empty());
        const auto ssrc = rtpHeaders.front().ssrc;
        std::vector<uint32_t> frameTimestamps;
        for (size_t i = 0; i < rtpHeaders.size(); ++i) {
            CPPUNIT_ASSERT_EQUAL(ssrc, rtpHeaders[i].ssrc);
            if (i > 0)
                CPPUNIT_ASSERT_EQUAL(uint16_t(rtpHeaders[i - 1].sequence + 1), rtpHeaders[i].sequence);
            if (frameTimestamps.empty() || frameTimestamps.back() != rtpHeaders[i].timestamp)
                frameTimestamps.push_back(rtpHeaders[i].timestamp);
        }
        CPPUNIT_ASSERT_EQUAL(size_t(3), frameTimestamps.size());
        CPPUNIT_ASSERT_EQUAL(uint32_t(3000), frameTimestamps[1] - frameTimestamps[0]);
        CPPUNIT_ASSERT_EQUAL(uint32_t(3000), frameTimestamps[2] - frameTimestamps[1]);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testVideoSenderRestartPreservesSsrcAndDimensions()
{
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int seededBitrate = 2211;
    const constexpr int adaptedBitrate = 300;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<SystemVideoCodecInfo>(codecs->searchCodecByName("H264", MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);

    SocketPair socketPair(dhtnet::IpAddr("127.0.0.1:50130"), dhtnet::IpAddr("127.0.0.1:50131"), 0, 0, true);
    MediaDescription args;
    args.enabled = true;
    args.codec = h264Codec;
    args.payload_type = h264Codec->payloadType;

    auto initialOpts
        = MediaStream("video sender", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, seededBitrate, 30);
    auto frame = std::make_shared<VideoFrame>();
    frame->reserve(AV_PIX_FMT_YUV420P, width, height);
    libav_utils::fillWithBlack(frame->pointer());

    auto sender
        = std::make_unique<video::VideoSender>("rtp://127.0.0.1:50130", initialOpts, args, socketPair, 1, 1200, false);
    CPPUNIT_ASSERT_EQUAL(width, sender->getWidth());
    CPPUNIT_ASSERT_EQUAL(height, sender->getHeight());
    sender->update(nullptr, frame);
    CPPUNIT_ASSERT(socketPair.getLocalSsrc().has_value());
    const auto initialSsrc = *socketPair.getLocalSsrc();
    const auto nextSeq = sender->getLastSeqValue();

    // Simulate a sender restart after congestion drops the target bitrate:
    // initial sizing budget preserves 1280x720 and SocketPair preserves SSRC,
    // while setBitrate applies the adapted 300 kbps rate before frame 0.
    sender.reset();
    sender = std::make_unique<video::VideoSender>("rtp://127.0.0.1:50130",
                                                  initialOpts,
                                                  args,
                                                  socketPair,
                                                  nextSeq,
                                                  1200,
                                                  false);
    CPPUNIT_ASSERT_EQUAL(1, sender->setBitrate(adaptedBitrate));
    CPPUNIT_ASSERT_EQUAL(width, sender->getWidth());
    CPPUNIT_ASSERT_EQUAL(height, sender->getHeight());
    sender->update(nullptr, frame);
    CPPUNIT_ASSERT_EQUAL(initialSsrc, *socketPair.getLocalSsrc());
    CPPUNIT_ASSERT_EQUAL(adaptedBitrate * 1000, sender->getStream("v").bitrate);
}

void
MediaEncoderTest::testVideoSizeSelection()
{
    struct SizeCase
    {
        int width;
        int height;
        uint64_t bitrate;
        int outputWidth;
        int outputHeight;
    };
    const SizeCase cases[] {{960, 540, 1200, 960, 540},
                            {854, 480, 1200, 854, 480},
                            {961, 541, 1200, 960, 540},
                            {1280, 720, 1200, 960, 540},
                            {800, 600, 800, 640, 480},
                            {160, 90, 1, 160, 90}};

    for (const auto& size : cases) {
        auto stream
            = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), size.width, size.height, size.bitrate, 30);
        encoder_->setOptions(stream);
        CPPUNIT_ASSERT_EQUAL(size.outputWidth, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(size.outputHeight, encoder_->getHeight());
    }
}

void
MediaEncoderTest::testSourceSizeChangesWithinEightPixels()
{
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<SystemVideoCodecInfo>(codecs->searchCodecByName("H264", MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);
    const std::string fileName = "test_source_size_change.h264";
    files_.push_back(fileName);
    encoder_->openOutput(fileName);
    encoder_->setOptions(MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), 641, 481, 1200, 30));
#ifdef ENABLE_HWACCEL
    encoder_->enableAccel(false);
#endif
    CPPUNIT_ASSERT(encoder_->addStream(*h264Codec) >= 0);
    encoder_->setIOContext(nullptr);

    for (int i = 0; i < 2; ++i) {
        auto input = std::make_shared<VideoFrame>();
        input->reserve(AV_PIX_FMT_YUV444P, 641 + 2 * i, 481 + 2 * i);
        libav_utils::fillWithBlack(input->pointer());
        CPPUNIT_ASSERT(encoder_->encode(input, true, i) >= 0);
        CPPUNIT_ASSERT_EQUAL(640 + 2 * i, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(480 + 2 * i, encoder_->getHeight());
    }
    CPPUNIT_ASSERT(encoder_->flush() >= 0);
}

void
MediaEncoderTest::testHardwareEncodingResolution()
{
    struct SizeCase
    {
        int width;
        int height;
        uint64_t bitrate;
        int outputWidth;
        int outputHeight;
        bool accelerated;
    };
    const SizeCase cases[] {{960, 540, 1200, 960, 540, false},
                            {800, 600, 800, 640, 480, false},
                            {960, 541, 1200, 960, 540, false},
                            {960, 540, 1200, 960, 540, true},
                            {800, 600, 800, 640, 480, true},
                            {960, 541, 1200, 960, 540, true}};
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(true);
    auto h264Codec = std::static_pointer_cast<SystemVideoCodecInfo>(codecs->searchCodecByName("H264", MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);

    for (const auto& size : cases) {
        const auto fileName = "test_" + std::to_string(size.width) + "x" + std::to_string(size.height)
                              + (size.accelerated ? "_hw.h264" : "_sw.h264");
        files_.push_back(fileName);
        encoder_ = std::make_unique<MediaEncoder>();
        encoder_->openOutput(fileName);
        encoder_->setOptions(
            MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), size.width, size.height, size.bitrate, 30));
#ifdef ENABLE_HWACCEL
        encoder_->enableAccel(size.accelerated);
#endif
        CPPUNIT_ASSERT(encoder_->addStream(*h264Codec) >= 0);
        encoder_->setIOContext(nullptr);

        for (int i = 0; i < 10; ++i) {
            auto input = std::make_shared<VideoFrame>();
            input->reserve(AV_PIX_FMT_YUV422P, size.width, size.height);
            auto* avf = input->pointer();
            for (int y = 0; y < size.height; ++y) {
                std::memset(avf->data[0] + y * avf->linesize[0], 128, size.width);
                std::memset(avf->data[1] + y * avf->linesize[1], 128, size.width / 2);
                std::memset(avf->data[2] + y * avf->linesize[2], 128, size.width / 2);
            }
            CPPUNIT_ASSERT(encoder_->encode(input, i == 0, i) >= 0);
        }
        CPPUNIT_ASSERT(encoder_->flush() >= 0);
        encoder_.reset();

        std::shared_ptr<VideoFrame> decodedFrame;
        MediaDecoder decoder([&](const std::shared_ptr<MediaFrame>& frame) {
            decodedFrame = std::dynamic_pointer_cast<VideoFrame>(frame);
        });
#ifdef ENABLE_HWACCEL
        decoder.enableAccel(false);
#endif
        DeviceParams params;
        params.input = fileName;
        CPPUNIT_ASSERT(decoder.openInput(params) >= 0);
        CPPUNIT_ASSERT(decoder.setupVideo() >= 0);
        while (!decodedFrame) {
            auto status = decoder.decode();
            if (status == MediaDemuxer::Status::EndOfFile || status == MediaDemuxer::Status::ReadError)
                break;
        }
        CPPUNIT_ASSERT(decodedFrame);
        CPPUNIT_ASSERT_EQUAL(size.outputWidth, decodedFrame->width());
        CPPUNIT_ASSERT_EQUAL(size.outputHeight, decodedFrame->height());

        const auto* out = decodedFrame->pointer();
        const int cx = out->width / 4;
        const int cy = out->height / 4;
        const int u = out->data[1][cy * out->linesize[1] + cx];
        const int vChroma = out->data[2][cy * out->linesize[2] + cx];
        // Failed GPU chroma uploads turn this neutral gray picture green.
        CPPUNIT_ASSERT(std::abs(u - 128) < 15);
        CPPUNIT_ASSERT(std::abs(vChroma - 128) < 15);
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MediaEncoderTest::name());
