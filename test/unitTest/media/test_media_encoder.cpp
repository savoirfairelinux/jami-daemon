/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Philippe Gorley <philippe.gorley@savoirfairelinux.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

namespace jami { namespace test {

class MediaEncoderTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_encoder"; }

    void setUp();
    void tearDown();

private:
    void testMultiStream();

    CPPUNIT_TEST_SUITE(MediaEncoderTest);
    CPPUNIT_TEST(testMultiStream);
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
    auto vp8Codec = std::static_pointer_cast<jami::SystemVideoCodecInfo>(
        getSystemCodecContainer()->searchCodecByName("VP8", jami::MEDIA_VIDEO)
    );
    auto opusCodec = std::static_pointer_cast<SystemAudioCodecInfo>(
        getSystemCodecContainer()->searchCodecByName("opus", jami::MEDIA_AUDIO)
    );
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
            audio = getAudioFrame(sampleRate, 0.02*sampleRate, nbChannels);
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

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::MediaEncoderTest::name());
