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
#include "libav_deps.h"
#include "media_buffer.h"
#include "media_filter.h"

#include "../../test_runner.h"

namespace jami { namespace test {

class MediaFilterTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_filter"; }

    void setUp();
    void tearDown();

private:
    void testAudioFilter();
    void testAudioMixing();
    void testVideoFilter();
    void testFilterParams();
    void testReinit();

    CPPUNIT_TEST_SUITE(MediaFilterTest);
    CPPUNIT_TEST(testAudioFilter);
    CPPUNIT_TEST(testAudioMixing);
    CPPUNIT_TEST(testVideoFilter);
    CPPUNIT_TEST(testFilterParams);
    CPPUNIT_TEST(testReinit);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaFilter> filter_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaFilterTest, MediaFilterTest::name());

void
MediaFilterTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    libav_utils::av_init();
    filter_.reset(new MediaFilter);
}

void
MediaFilterTest::tearDown()
{
    libjami::fini();
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
fill_samples(uint16_t* samples, int sampleRate, int nbSamples, int nbChannels, float tone, float& t)
{
    const constexpr double pi = 3.14159265358979323846264338327950288; // M_PI
    const float tincr = 2 * pi * tone / sampleRate;

    for (int j = 0; j < nbSamples; ++j) {
        samples[2 * j] = (int)(sin(t) * 10000);
        for (int k = 1; k < nbChannels; ++k)
            samples[2 * j + k] = samples[2 * j];
        t += tincr;
    }
}

static void
fill_samples(uint16_t* samples, int sampleRate, int nbSamples, int nbChannels, float tone)
{
    float t = 0;
    fill_samples(samples, sampleRate, nbSamples, nbChannels, tone, t);
}

static void
fillAudioFrameProps(AVFrame* frame, const MediaStream& ms)
{
    frame->format = ms.format;
    frame->channel_layout = av_get_default_channel_layout(ms.nbChannels);
    frame->nb_samples = ms.frameSize;
    frame->sample_rate = ms.sampleRate;
    frame->channels = ms.nbChannels;
    CPPUNIT_ASSERT(frame->format > AV_SAMPLE_FMT_NONE);
    CPPUNIT_ASSERT(frame->nb_samples > 0);
    CPPUNIT_ASSERT(frame->channel_layout != 0);
}

void
MediaFilterTest::testAudioFilter()
{
    std::string filterSpec = "[in1] aformat=sample_fmts=u8";

    // constants
    const constexpr int nbSamples = 100;
    const constexpr int64_t channelLayout = AV_CH_LAYOUT_STEREO;
    const constexpr int sampleRate = 44100;
    const constexpr enum AVSampleFormat format = AV_SAMPLE_FMT_S16;

    // prepare audio frame
    AudioFrame af;
    auto frame = af.pointer();
    frame->format = format;
    frame->channel_layout = channelLayout;
    frame->nb_samples = nbSamples;
    frame->sample_rate = sampleRate;
    frame->channels = av_get_channel_layout_nb_channels(channelLayout);

    // construct the filter parameters
    auto params = MediaStream("in1", format, rational<int>(1, sampleRate), sampleRate, frame->channels, nbSamples);

    // allocate and fill frame buffers
    CPPUNIT_ASSERT(av_frame_get_buffer(frame, 0) >= 0);
    fill_samples(reinterpret_cast<uint16_t*>(frame->data[0]), sampleRate, nbSamples, frame->channels, 440.0);

    // prepare filter
    std::vector<MediaStream> vec;
    vec.push_back(params);
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, vec) >= 0);

    // apply filter
    CPPUNIT_ASSERT(filter_->feedInput(frame, "in1") >= 0);
    auto out = filter_->readOutput();
    CPPUNIT_ASSERT(out);
    CPPUNIT_ASSERT(out->pointer());

    // check if the filter worked
    CPPUNIT_ASSERT(out->pointer()->format == AV_SAMPLE_FMT_U8);
}

void
MediaFilterTest::testAudioMixing()
{
    std::string filterSpec = "[a1] [a2] [a3] amix=inputs=3,aformat=sample_fmts=s16";

    AudioFrame af1;
    auto frame1 = af1.pointer();
    AudioFrame af2;
    auto frame2 = af2.pointer();
    AudioFrame af3;
    auto frame3 = af3.pointer();

    std::vector<MediaStream> vec;
    vec.emplace_back("a1", AV_SAMPLE_FMT_S16, rational<int>(1, 48000), 48000, 2, 960);
    vec.emplace_back("a2", AV_SAMPLE_FMT_S16, rational<int>(1, 48000), 48000, 2, 960);
    vec.emplace_back("a3", AV_SAMPLE_FMT_S16, rational<int>(1, 48000), 48000, 2, 960);
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, vec) >= 0);

    float t1 = 0, t2 = 0, t3 = 0;
    for (int i = 0; i < 100; ++i) {
        fillAudioFrameProps(frame1, vec[0]);
        frame1->pts = i * frame1->nb_samples;
        CPPUNIT_ASSERT(av_frame_get_buffer(frame1, 0) >= 0);
        fill_samples(reinterpret_cast<uint16_t*>(frame1->data[0]), frame1->sample_rate, frame1->nb_samples, frame1->channels, 440.0, t1);

        fillAudioFrameProps(frame2, vec[1]);
        frame2->pts = i * frame2->nb_samples;
        CPPUNIT_ASSERT(av_frame_get_buffer(frame2, 0) >= 0);
        fill_samples(reinterpret_cast<uint16_t*>(frame2->data[0]), frame2->sample_rate, frame2->nb_samples, frame2->channels, 329.6276, t2);

        fillAudioFrameProps(frame3, vec[2]);
        frame3->pts = i * frame3->nb_samples;
        CPPUNIT_ASSERT(av_frame_get_buffer(frame3, 0) >= 0);
        fill_samples(reinterpret_cast<uint16_t*>(frame3->data[0]), frame3->sample_rate, frame3->nb_samples, frame3->channels, 349.2282, t3);

        // apply filter
        CPPUNIT_ASSERT(filter_->feedInput(frame1, "a1") >= 0);
        CPPUNIT_ASSERT(filter_->feedInput(frame2, "a2") >= 0);
        CPPUNIT_ASSERT(filter_->feedInput(frame3, "a3") >= 0);

        // read output
        auto out = filter_->readOutput();
        CPPUNIT_ASSERT(out);
        CPPUNIT_ASSERT(out->pointer());

        av_frame_unref(frame1);
        av_frame_unref(frame2);
        av_frame_unref(frame3);
    }
}

void
MediaFilterTest::testVideoFilter()
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
    VideoFrame vf1;
    auto frame = vf1.pointer();
    frame->format = format;
    frame->width = width1;
    frame->height = height1;
    VideoFrame vf2;
    auto extra = vf2.pointer();
    extra->format = format;
    extra->width = width2;
    extra->height = height2;

    // construct the filter parameters
    rational<int> one = rational<int>(1);
    auto params1 = MediaStream("main", format, one, width1, height1, one.real<int>(), one);
    auto params2 = MediaStream("top", format, one, width2, height2, one.real<int>(), one);

    // allocate and fill frame buffers
    CPPUNIT_ASSERT(av_frame_get_buffer(frame, 32) >= 0);
    fill_yuv_image(frame->data, frame->linesize, frame->width, frame->height, 0);
    CPPUNIT_ASSERT(av_frame_get_buffer(extra, 32) >= 0);
    fill_yuv_image(extra->data, extra->linesize, extra->width, extra->height, 0);

    // prepare filter
    auto vec = std::vector<MediaStream>();
    vec.push_back(params2); // order does not matter, as long as names match
    vec.push_back(params1);
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, vec) >= 0);

    // apply filter
    CPPUNIT_ASSERT(filter_->feedInput(frame, main) >= 0);
    CPPUNIT_ASSERT(filter_->feedInput(extra, top) >= 0);
    auto out = filter_->readOutput();
    CPPUNIT_ASSERT(out);
    CPPUNIT_ASSERT(out->pointer());

    // check if the filter worked
    CPPUNIT_ASSERT(out->pointer()->width == width1 && out->pointer()->height == height1);
}

void
MediaFilterTest::testFilterParams()
{
    std::string filterSpec = "[main] [top] overlay=main_w-overlay_w-10:main_h-overlay_h-10";

    // constants
    const constexpr int width1 = 320;
    const constexpr int height1 = 240;
    const constexpr int width2 = 30;
    const constexpr int height2 = 30;
    const constexpr AVPixelFormat format = AV_PIX_FMT_YUV420P;

    // construct the filter parameters
    rational<int> one = rational<int>(1);
    auto params1 = MediaStream("main", format, one, width1, height1, one.real<int>(), one);
    auto params2 = MediaStream("top", format, one, width2, height2, one.real<int>(), one);

    // returned params should be invalid
    CPPUNIT_ASSERT(filter_->getOutputParams().format < 0);

    // prepare filter
    auto vec = std::vector<MediaStream>();
    vec.push_back(params2); // order does not matter, as long as names match
    vec.push_back(params1);
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, vec) >= 0);

    // check input params
    auto main = filter_->getInputParams("main");
    CPPUNIT_ASSERT(main.format == format && main.width == width1 && main.height == height1);
    auto top = filter_->getInputParams("top");
    CPPUNIT_ASSERT(top.format == format && top.width == width2 && top.height == height2);

    // output params should now be valid
    auto ms = filter_->getOutputParams();
    CPPUNIT_ASSERT(ms.format >= 0 && ms.width == width1 && ms.height == height1);
}

void
MediaFilterTest::testReinit()
{
    std::string filterSpec = "[in1] aresample=48000";

    // prepare audio frame
    AudioFrame af;
    auto frame = af.pointer();
    frame->format = AV_SAMPLE_FMT_S16;
    frame->channel_layout = AV_CH_LAYOUT_STEREO;
    frame->nb_samples = 100;
    frame->sample_rate = 44100;
    frame->channels = 2;

    // construct the filter parameters with different sample rate
    auto params = MediaStream("in1", frame->format, rational<int>(1, 16000), 16000, frame->channels, frame->nb_samples);

    // allocate and fill frame buffers
    CPPUNIT_ASSERT(av_frame_get_buffer(frame, 0) >= 0);
    fill_samples(reinterpret_cast<uint16_t*>(frame->data[0]), frame->sample_rate, frame->nb_samples, frame->channels, 440.0);

    // prepare filter
    std::vector<MediaStream> vec;
    vec.push_back(params);
    CPPUNIT_ASSERT(filter_->initialize(filterSpec, vec) >= 0);

    // filter should reinitialize on feedInput
    CPPUNIT_ASSERT(filter_->feedInput(frame, "in1") >= 0);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::MediaFilterTest::name());
