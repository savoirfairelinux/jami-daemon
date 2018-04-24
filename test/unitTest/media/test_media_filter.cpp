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
    void testVideoFilter();

    CPPUNIT_TEST_SUITE(MediaFilterTest);
    CPPUNIT_TEST(testVideoFilter);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<MediaFilter> filter_;
    AVCodecContext* codecCtx_;
    AVFrame* frame_;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaFilterTest, MediaFilterTest::name());

void
MediaFilterTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    libav_utils::ring_avcodec_init();
}

void
MediaFilterTest::tearDown()
{
    av_frame_free(&frame_);
    avcodec_free_context(&codecCtx_);
}

static void fill_yuv_image(uint8_t *data[4], int linesize[4], int width, int height, int frame_index)
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

void
MediaFilterTest::testVideoFilter()
{
    int ret = 0;
    std::string err;

    // constants
    const constexpr int width = 320, height = 240;
    const constexpr AVPixelFormat format = AV_PIX_FMT_YUV420P;

    // prepare video frame
    frame_ = av_frame_alloc();
    frame_->format = AV_PIX_FMT_YUV420P;
    frame_->width = width;
    frame_->height = height;
    CPPUNIT_ASSERT(frame_->width == width && frame_->height == height);
    CPPUNIT_ASSERT(av_frame_get_buffer(frame_, 32) >= 0);
    fill_yuv_image(frame_->data, frame_->linesize, frame_->width, frame_->height, 0);

    // prepare AVCodecContext, avcodec_alloc_context3 fills it with default values
    AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
    codecCtx_ = avcodec_alloc_context3(codec);
    codecCtx_->width = width;
    codecCtx_->height = height;
    codecCtx_->pix_fmt = format;
    codecCtx_->time_base.num = 1;
    codecCtx_->time_base.den = 1;
    codecCtx_->sample_aspect_ratio.num = 1;
    codecCtx_->sample_aspect_ratio.den = 1;

    // prepare filter
    filter_ = std::unique_ptr<MediaFilter>(new MediaFilter());
    ret = filter_->initializeFilters(codecCtx_, "scale=200x100");
    err = libav_utils::getError(ret);
    CPPUNIT_ASSERT_MESSAGE(err, ret >= 0);

    // apply filter
    ret = filter_->applyFilters(frame_);
    err = libav_utils::getError(ret);
    CPPUNIT_ASSERT_MESSAGE(err, ret >= 0);

    CPPUNIT_ASSERT(frame_->width == 200 && frame_->height == 100);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaFilterTest::name());
