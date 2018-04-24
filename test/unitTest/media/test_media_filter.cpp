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

#include "libav_deps.h"
#include "media_buffer.h"
#include "media_filter.h"
#include "../../test_runner.h"

namespace ring { namespace test {

class MediaFilterTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_filter"; }

private:
    void testVideoFilter();

    CPPUNIT_TEST_SUITE(MediaFilterTest);
    CPPUNIT_TEST(testVideoFilter);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaFilterTest, MediaFilterTest::name());

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
    // constants
    const constexpr int width = 320, height = 240;
    const constexpr AVPixelFormat format = AV_PIX_FMT_YUV420P;

    // prepare video frame
    AVFrame* frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;
    CPPUNIT_ASSERT(av_frame_get_buffer(frame, 0) >= 0);
    fill_yuv_image(frame->data, frame->linesize, frame->width, frame->height, 0);

    // prepare AVCodecContext
    AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_RAWVIDEO);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    codecCtx->width = width;
    codecCtx->height = height;
    codecCtx->pix_fmt = format;
    codecCtx->time_base.num = 0;
    codecCtx->time_base.den = 1;
    codecCtx->sample_aspect_ratio.num = 0;
    codecCtx->sample_aspect_ratio.den = 0;

    // prepare filter
    MediaFilter* filter = new MediaFilter();
    CPPUNIT_ASSERT(filter->initializeFilters(codecCtx, "scale=200x100") >= 0);

    CPPUNIT_ASSERT(filter->applyFilters(frame) >= 0);

    CPPUNIT_ASSERT(frame->width == 200);
    CPPUNIT_ASSERT(frame->height == 100);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaFilterTest::name());
