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
#include "media_filter.h"

#include "../../test_runner.h"

namespace ring { namespace test {

class MediaFilterTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_filter"; }

    void setUp();
    void tearDown();

private:
    void testSimpleVideoFilter();
    void testSimpleAudioFilter();
    void testComplexVideoFilter();

    CPPUNIT_TEST_SUITE(MediaFilterTest);
    CPPUNIT_TEST(testSimpleVideoFilter);
    CPPUNIT_TEST(testSimpleAudioFilter);
    CPPUNIT_TEST(testComplexVideoFilter);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaFilter> filter_;
    AVFrame* frame_ = nullptr;
    AVFrame* extra_ = nullptr; // used for filters with multiple inputs
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaFilterTest, MediaFilterTest::name());

void
MediaFilterTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    libav_utils::ring_avcodec_init();
    filter_.reset(new MediaFilter);
}

void
MediaFilterTest::tearDown()
{
    av_frame_free(&frame_);
    av_frame_free(&extra_);
    DRing::fini();
}

static void
fill_yuv_image(uint8_t *data[4], int linesize[4], int width, int height, int frame_index)
{
    int x, y;

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            data[0][y * linesize[0] + x] = x + y + frame_index * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            data[1][y * linesize[1] + x] = 128 + y + frame_index * 2;
            data[2][y * linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }
}

static void
fill_samples(uint16_t* samples, int sampleRate, int nbSamples, int nbChannels, float tone)
{
    const constexpr float pi = 3.14159265358979323846264338327950288; // M_PI
    const float tincr = 2 * pi * tone / sampleRate;
    float t = 0;

    for (int i = 0; i < 200; ++i) {
        for (int j = 0; j < nbSamples; ++j) {
            samples[2 * j] = static_cast<int>(sin(t) * 10000);
            for (int k = 1; k < nbChannels; ++k) {
                samples[2 * j + k] = samples[2 * j];
            }
            t += tincr;
        }
    }
}

void
MediaFilterTest::testSimpleVideoFilter()
{
    std::string filterSpec = "scale=200x100";

    // constants
    const constexpr int width = 320;
    const constexpr int height = 240;
    const constexpr AVPixelFormat format = AV_PIX_FMT_YUV420P;

    // prepare video frame
    frame_ = av_frame_alloc();
    frame_->format = format;
    frame_->width = width;
    frame_->height = height;

    // construct the filter parameters
    rational<int> one = rational<int>(1);
    auto params = MediaStream("vf", format, one, width, height, one, one);

    // allocate and fill frame buffers
    CPPUNIT_ASSERT(av_frame_get_buffer(frame_, 32) >= 0);
    fill_yuv_image(frame_->data, frame_->linesize, frame_->width, frame_->height, 0);

    // prepare filter
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, params) >= 0);

    // apply filter
    CPPUNIT_ASSERT(filter_->feedInput(frame_) >= 0);
    frame_ = filter_->readOutput();
    CPPUNIT_ASSERT(frame_);

    // check if the filter worked
    CPPUNIT_ASSERT(frame_->width == 200 && frame_->height == 100);
}

void
MediaFilterTest::testSimpleAudioFilter()
{
    std::string filterSpec = "aformat=sample_fmts=u8";

    // constants
    const constexpr int nbSamples = 100;
    const constexpr int64_t channelLayout = AV_CH_LAYOUT_STEREO;
    const constexpr int sampleRate = 44100;
    const constexpr enum AVSampleFormat format = AV_SAMPLE_FMT_S16;

    // prepare audio frame
    frame_ = av_frame_alloc();
    frame_->format = format;
    frame_->channel_layout = channelLayout;
    frame_->nb_samples = nbSamples;
    frame_->sample_rate = sampleRate;
    frame_->channels = av_get_channel_layout_nb_channels(channelLayout);

    // construct the filter parameters
    auto params = MediaStream("af", format, rational<int>(1, 1), sampleRate, frame_->channels);

    // allocate and fill frame buffers
    CPPUNIT_ASSERT(av_frame_get_buffer(frame_, 0) >= 0);
    fill_samples(reinterpret_cast<uint16_t*>(frame_->data[0]), sampleRate, nbSamples, frame_->channels, 440.0);

    // prepare filter
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, params) >= 0);

    // apply filter
    CPPUNIT_ASSERT(filter_->feedInput(frame_) >= 0);
    frame_ = filter_->readOutput();
    CPPUNIT_ASSERT(frame_);

    // check if the filter worked
    CPPUNIT_ASSERT(frame_->format == AV_SAMPLE_FMT_U8);
}

void
MediaFilterTest::testComplexVideoFilter()
{
    std::string filterSpec = "[main] [top] overlay=main_w-overlay_w-10:main_h-overlay_h-10";
    std::string main = "main";
    std::string top = "top";

    // constants
    const constexpr int width1 = 320;
    const constexpr int height1 = 240;
    const constexpr int width2 = 30;
    const constexpr int height2 = 30;
    const constexpr AVPixelFormat format = AV_PIX_FMT_YUV420P;

    // prepare video frame
    frame_ = av_frame_alloc();
    frame_->format = format;
    frame_->width = width1;
    frame_->height = height1;
    extra_ = av_frame_alloc();
    extra_->format = format;
    extra_->width = width2;
    extra_->height = height2;

    // construct the filter parameters
    rational<int> one = rational<int>(1);
    auto params1 = MediaStream("main", format, one, width1, height1, one, one);
    auto params2 = MediaStream("top", format, one, width2, height2, one, one);

    // allocate and fill frame buffers
    CPPUNIT_ASSERT(av_frame_get_buffer(frame_, 32) >= 0);
    fill_yuv_image(frame_->data, frame_->linesize, frame_->width, frame_->height, 0);
    CPPUNIT_ASSERT(av_frame_get_buffer(extra_, 32) >= 0);
    fill_yuv_image(extra_->data, extra_->linesize, extra_->width, extra_->height, 0);

    // prepare filter
    auto vec = std::vector<MediaStream>();
    vec.push_back(params1);
    vec.push_back(params2);
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, vec) >= 0);

    // apply filter
    CPPUNIT_ASSERT(filter_->feedInput(frame_, main) >= 0);
    CPPUNIT_ASSERT(filter_->feedInput(extra_, top) >= 0);
    av_frame_free(&frame_);
    av_frame_free(&extra_);
    frame_ = filter_->readOutput();
    CPPUNIT_ASSERT(frame_);

    // check if the filter worked
    CPPUNIT_ASSERT(frame_->width == width1 && frame_->height == height1);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaFilterTest::name());
