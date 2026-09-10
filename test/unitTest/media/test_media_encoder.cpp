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
#include "media/media_encoder.h"
#include "media/media_io_handle.h"
#include "media/system_codec_container.h"
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
    void testH264BitrateAdaptsLargeResolution();
    void testPassthroughPacketRtpTimestamps();
    void testPassthroughBitrateChangeKeepsStream();
    void testVP8DynamicBitrate();
    void testVP8BitrateAdaptsLargeResolution();
    void testVideoToolboxLiveBitrateAndResizeRequiresRestart();

    CPPUNIT_TEST_SUITE(MediaEncoderTest);
    CPPUNIT_TEST(testMultiStream);
    CPPUNIT_TEST(testH264DynamicBitrate);
    CPPUNIT_TEST(testH264BitrateAdaptsLargeResolution);
    CPPUNIT_TEST(testPassthroughPacketRtpTimestamps);
    CPPUNIT_TEST(testPassthroughBitrateChangeKeepsStream);
    CPPUNIT_TEST(testVP8DynamicBitrate);
    CPPUNIT_TEST(testVP8BitrateAdaptsLargeResolution);
    CPPUNIT_TEST(testVideoToolboxLiveBitrateAndResizeRequiresRestart);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaEncoder> encoder_;
    std::vector<std::string> files_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaEncoderTest, MediaEncoderTest::name());

void
MediaEncoderTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    libav_utils::av_init();
    encoder_.reset(new MediaEncoder);
    files_.push_back("test.mkv");
}

void
MediaEncoderTest::tearDown()
{
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
        size_t encodedBytes = 0;
        MediaIOHandle ioContext(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext.getContext());
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
MediaEncoderTest::testH264BitrateAdaptsLargeResolution()
{
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int initialBitrate = 2500;
    const constexpr int constrainedBitrate = 300;
    const constexpr int restoredBitrate = 2200;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        size_t encodedBytes = 0;
        MediaIOHandle ioContext(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext.getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());

        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(480, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(270, encoder_->getHeight());
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
        std::vector<uint32_t> timestamps;
        MediaIOHandle ioContext(4096, true, nullptr, collectRtpTimestamps, nullptr, &timestamps);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext.getContext());
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
        size_t encodedBytes = 0;
        MediaIOHandle ioContext(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext.getContext());
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
MediaEncoderTest::testVP8BitrateAdaptsLargeResolution()
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
        size_t encodedBytes = 0;
        MediaIOHandle ioContext(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext.getContext());
        encoder_->setOptions(v);
        int videoIdx = encoder_->addStream(*vp8Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());

        CPPUNIT_ASSERT_EQUAL(1, encoder_->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(480, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(270, encoder_->getHeight());
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

void
MediaEncoderTest::testVideoToolboxLiveBitrateAndResizeRequiresRestart()
{
#ifdef ENABLE_HWACCEL
    const constexpr int width = 1280;
    const constexpr int height = 720;
    const constexpr int initialBitrate = 2500;
    const constexpr int constrainedBitrate = 300;
    auto compatibleAccel = video::HardwareAccel::getCompatibleAccel(AV_CODEC_ID_H264, width, height, CODEC_ENCODER);
    auto videoToolbox = std::find_if(compatibleAccel.begin(), compatibleAccel.end(), [](const auto& accel) {
        return accel.getName() == "videotoolbox";
    });
    if (videoToolbox == compatibleAccel.end())
        return;

    CPPUNIT_ASSERT_EQUAL(std::string("h264_videotoolbox"), videoToolbox->getCodecName());
    CPPUNIT_ASSERT(!videoToolbox->dynBitrate());

    if (!std::getenv("JAMI_TEST_HW_ENCODERS") || !avcodec_find_encoder_by_name("h264_videotoolbox"))
        return;

    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    auto videoStream = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, initialBitrate, 30);

    try {
        size_t encodedBytes = 0;
        MediaIOHandle ioContext(4096, true, nullptr, countEncodedBytes, nullptr, &encodedBytes);
        encoder_->enableAccel(true);
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setIOContext(ioContext.getContext());
        encoder_->setOptions(videoStream);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encodeVideoFrame(*encoder_, videoIdx, width, height, 0);

        if (encoder_->getStream("v", videoIdx).format != AV_PIX_FMT_NV12)
            return;

        CPPUNIT_ASSERT_EQUAL(0, encoder_->setBitrate(constrainedBitrate));
        CPPUNIT_ASSERT_EQUAL(width, encoder_->getWidth());
        CPPUNIT_ASSERT_EQUAL(height, encoder_->getHeight());
        encodeVideoFrame(*encoder_, videoIdx, width, height, 1);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
#endif
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MediaEncoderTest::name());
