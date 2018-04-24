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

#include "media_filter.h"
#include "../../test_runner.h"

#include "dring.h"

namespace ring { namespace test {

class MediaFilterTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_filter"; }

private:
    void testAudioFilter();
    void testVideoFilter();

    CPPUNIT_TEST_SUITE(MediaFilterTest);
    CPPUNIT_TEST(testAudioFilter);
    CPPUNIT_TEST(testVideoFilter);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaFilterTest, MediaFilterTest::name());

//static void fill_yuv_image(uint8_t *data[4], int linesize[4],
//                           int width, int height, int frame_index)
//{
//    int x, y;
//
//    /* Y */
//    for (y = 0; y < height; y++)
//        for (x = 0; x < width; x++)
//            data[0][y * linesize[0] + x] = x + y + frame_index * 3;
//
//    /* Cb and Cr */
//    for (y = 0; y < height / 2; y++) {
//        for (x = 0; x < width / 2; x++) {
//            data[1][y * linesize[1] + x] = 128 + y + frame_index * 2;
//            data[2][y * linesize[2] + x] = 64 + x + frame_index * 5;
//        }
//    }
//}

//static void fill_samples(double *dst, int nb_samples, int nb_channels, int sample_rate, double *t)
//{
//    int i, j;
//    double tincr = 1.0 / sample_rate, *dstp = dst;
//    const double c = 2 * M_PI * 440.0;
//
//    /* generate sin tone with 440Hz frequency and duplicated channels */
//    for (i = 0; i < nb_samples; i++) {
//        *dstp = sin(c * *t);
//        for (j = 1; j < nb_channels; j++)
//            dstp[j] = dstp[0];
//        dstp += nb_channels;
//        *t += tincr;
//    }
//}

void
MediaFilterTest::testAudioFilter()
{
    // generate synthetic audio with fill_samples
    // filter chain: aresample=44100
    // compare AVFrame.sample_rate before and after
}

void
MediaFilterTest::testVideoFilter()
{
    // generate synthetic video with fill_yuv_image
    // filter chain: scale=w=2*iw:h=2*ih
    // compare AVFrame.width and AVFrame.height before and after
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::MediaFilterTest::name());
