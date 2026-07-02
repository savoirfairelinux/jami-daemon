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
#include "fileutils.h"
#include "media/libav_deps.h"
#include "media/media_encoder.h"
#include "media/media_io_handle.h"
#include "media/system_codec_container.h"

#include "../../test_runner.h"

#include <array>

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
    void testPassthroughRtpTimestampsStayMonotonic();
    void testExtractProfileLevelID();
    void testH264HighChromaEncoding();

    CPPUNIT_TEST_SUITE(MediaEncoderTest);
    CPPUNIT_TEST(testMultiStream);
    CPPUNIT_TEST(testPassthroughRtpTimestampsStayMonotonic);
    CPPUNIT_TEST(testExtractProfileLevelID);
    CPPUNIT_TEST(testH264HighChromaEncoding);
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
getVideoFrame(int width, int height, int frame_index, AVPixelFormat format = AV_PIX_FMT_YUV420P)
{
    int x, y;
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return nullptr;

    frame->format = format;
    frame->width = width;
    frame->height = height;

    if (av_frame_get_buffer(frame, 32) < 0) {
        av_frame_free(&frame);
        return nullptr;
    }

    const auto* desc = av_pix_fmt_desc_get(format);
    const int chromaWidth = AV_CEIL_RSHIFT(width, desc->log2_chroma_w);
    const int chromaHeight = AV_CEIL_RSHIFT(height, desc->log2_chroma_h);

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            frame->data[0][y * frame->linesize[0] + x] = x + y + frame_index * 3;

    /* Cb and Cr */
    for (y = 0; y < chromaHeight; y++) {
        for (x = 0; x < chromaWidth; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + frame_index * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }

    return frame;
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

namespace {

struct RtpPacketCollector
{
    std::vector<uint32_t> timestamps;

    static int write(void* opaque, const uint8_t* buf, int buf_size)
    {
        auto* collector = static_cast<RtpPacketCollector*>(opaque);
        if (buf_size >= 12 && (buf[0] >> 6) == 2) {
            uint8_t payloadType = buf[1] & 0x7f;
            if (payloadType < 200 || payloadType > 204) {
                collector->timestamps.emplace_back((uint32_t(buf[4]) << 24) | (uint32_t(buf[5]) << 16)
                                                   | (uint32_t(buf[6]) << 8) | uint32_t(buf[7]));
            }
        }
        return buf_size;
    }
};

libjami::PacketBuffer
getPassthroughPacket(int64_t pts)
{
    auto* packet = av_packet_alloc();
    if (!packet)
        return {};
    if (av_new_packet(packet, 8) < 0) {
        av_packet_free(&packet);
        return {};
    }

    static constexpr std::array<uint8_t, 8> payload {0x10, 0x00, 0x00, 0x9d, 0x01, 0x2a, 0x00, 0x00};
    std::copy(payload.begin(), payload.end(), packet->data);
    packet->pts = pts;
    packet->dts = pts;
    packet->flags |= AV_PKT_FLAG_KEY;
    return libjami::PacketBuffer(packet);
}

} // namespace

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
MediaEncoderTest::testPassthroughRtpTimestampsStayMonotonic()
{
    const constexpr int width = 320;
    const constexpr int height = 240;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto vp8Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("VP8", jami::MEDIA_VIDEO));
    CPPUNIT_ASSERT(vp8Codec);

    auto v = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, 1, 30);
    RtpPacketCollector collector;
    auto ioHandle = std::make_unique<MediaIOHandle>(1500, true, nullptr, &RtpPacketCollector::write, nullptr, &collector);

    try {
        encoder_->openOutput("rtp://127.0.0.1:5004", "rtp");
        encoder_->setOptions(v);
        CPPUNIT_ASSERT(encoder_->addStream(*vp8Codec) >= 0);
        encoder_->setIOContext(ioHandle->getContext());
        encoder_->setVideoPassthrough(true);

        auto firstPacket = getPassthroughPacket(1000);
        CPPUNIT_ASSERT(firstPacket);
        CPPUNIT_ASSERT(encoder_->send(*firstPacket));
        CPPUNIT_ASSERT(!collector.timestamps.empty());
        auto firstTimestamp = collector.timestamps.back();

        auto secondPacket = getPassthroughPacket(1);
        CPPUNIT_ASSERT(secondPacket);
        CPPUNIT_ASSERT(encoder_->send(*secondPacket));
        CPPUNIT_ASSERT(collector.timestamps.size() >= 2);
        auto secondTimestamp = collector.timestamps.back();

        CPPUNIT_ASSERT(secondTimestamp > firstTimestamp);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

void
MediaEncoderTest::testExtractProfileLevelID()
{
    AVCodecContext* ctx = avcodec_alloc_context3(nullptr);
    CPPUNIT_ASSERT(ctx);

    // RFC 6184: no profile-level-id implies Constrained Baseline
    MediaEncoder::extractProfileLevelID("", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_CONSTRAINED_BASELINE, ctx->profile);
    CPPUNIT_ASSERT_EQUAL(0x0d, ctx->level);

    // Constrained Baseline (profile_idc 0x42, constraint_set1_flag) level 4.1
    MediaEncoder::extractProfileLevelID("profile-level-id=42e029", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_CONSTRAINED_BASELINE, ctx->profile);
    CPPUNIT_ASSERT_EQUAL(0x29, ctx->level);

    // High profile (profile_idc 0x64)
    MediaEncoder::extractProfileLevelID("profile-level-id=640029", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH, ctx->profile);
    CPPUNIT_ASSERT_EQUAL(0x29, ctx->level);

    // High 4:2:2 profile (profile_idc 0x7A)
    MediaEncoder::extractProfileLevelID("profile-level-id=7a0029", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_422, ctx->profile);
    CPPUNIT_ASSERT_EQUAL(0x29, ctx->level);

    // High 4:4:4 Predictive profile (profile_idc 0xF4)
    MediaEncoder::extractProfileLevelID("profile-level-id=f40029", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_444_PREDICTIVE, ctx->profile);
    CPPUNIT_ASSERT_EQUAL(0x29, ctx->level);

    // High 4:4:4 Intra (profile_idc 0xF4, constraint_set3_flag)
    MediaEncoder::extractProfileLevelID("profile-level-id=f41029", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_HIGH_444_PREDICTIVE | AV_PROFILE_H264_INTRA, ctx->profile);

    // Unknown profile falls back to Constrained Baseline
    MediaEncoder::extractProfileLevelID("profile-level-id=ff0029", ctx);
    CPPUNIT_ASSERT_EQUAL(AV_PROFILE_H264_CONSTRAINED_BASELINE, ctx->profile);

    avcodec_free_context(&ctx);
}

void
MediaEncoderTest::testH264HighChromaEncoding()
{
    const constexpr int width = 320;
    const constexpr int height = 240;
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        codecs->searchCodecByName("H264", jami::MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);

    auto v = MediaStream("v", AV_PIX_FMT_YUV420P, rational<int>(1, 30), width, height, 1, 30);
    MediaDescription args;
    args.parameters = "profile-level-id=f40029"; // High 4:4:4 Predictive, level 4.1

    files_.push_back("test444.h264");
    try {
        encoder_->openOutput("test444.h264");
        encoder_->setOptions(v);
        encoder_->setOptions(args);
        int videoIdx = encoder_->addStream(*h264Codec.get());
        CPPUNIT_ASSERT(videoIdx >= 0);
        encoder_->setIOContext(nullptr);

        for (int i = 0; i < 5; ++i) {
            AVFrame* video = getVideoFrame(width, height, i, AV_PIX_FMT_YUV444P);
            CPPUNIT_ASSERT(video);
            video->pts = i;
            CPPUNIT_ASSERT(encoder_->encode(video, videoIdx) >= 0);
            av_frame_free(&video);
        }
        CPPUNIT_ASSERT(encoder_->flush() >= 0);

        // The negotiated High 4:4:4 profile must select 4:4:4 chroma sampling
        auto ms = encoder_->getStream("v", videoIdx);
        CPPUNIT_ASSERT_EQUAL(static_cast<int>(AV_PIX_FMT_YUV444P), ms.format);
    } catch (const MediaEncoderException& e) {
        CPPUNIT_FAIL(e.what());
    }
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MediaEncoderTest::name());
