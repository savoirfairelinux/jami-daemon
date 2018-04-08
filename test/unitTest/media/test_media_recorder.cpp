/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
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

#include "dring.h"
#include "libav_deps.h"
#include "media_recorder.h"

#include "../../test_runner.h"

namespace ring { namespace test {

class MediaRecorderTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_recorder"; }

    void setUp();
    void tearDown();

private:
    void testMultiStream();

    CPPUNIT_TEST_SUITE(MediaRecorderTest);
    CPPUNIT_TEST(testMultiStream);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaRecorder> recorder_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaRecorderTest, MediaRecorderTest::name());

void
MediaRecorderTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    libav_utils::ring_avcodec_init();
}

void
MediaRecorderTest::tearDown()
{
    DRing::fini();
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
    frame->channels = nbChannels;
    frame->channel_layout = av_get_default_channel_layout(nbChannels);
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
MediaRecorderTest::testMultiStream()
{
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaRecorderTest::name());
