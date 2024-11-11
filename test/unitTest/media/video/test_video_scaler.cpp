/*
 *  Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
#include "videomanager_interface.h"
#include "media/video/video_scaler.h"

#include "../../../test_runner.h"

namespace jami { namespace video { namespace test {

class VideoScalerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "video_scaler"; }

    void setUp();
    void tearDown();

private:
    void testConvertFrame();
    void testScale();
    void testScaleWithAspect();

    CPPUNIT_TEST_SUITE(VideoScalerTest);
    CPPUNIT_TEST(testConvertFrame);
    CPPUNIT_TEST(testScale);
    CPPUNIT_TEST(testScaleWithAspect);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<VideoScaler> scaler_;
};


CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VideoScalerTest, VideoScalerTest::name());

void
VideoScalerTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
}

void
VideoScalerTest::tearDown()
{
    libjami::fini();
}

void
VideoScalerTest::testConvertFrame()
{
    scaler_.reset(new VideoScaler);

    libjami::VideoFrame input;
    input.reserve(AV_PIX_FMT_YUV420P, 100, 100);
    auto output = scaler_->convertFormat(input, AV_PIX_FMT_RGB24);
    CPPUNIT_ASSERT(static_cast<AVPixelFormat>(output->format()) == AV_PIX_FMT_RGB24);
}

void
VideoScalerTest::testScale()
{
    scaler_.reset(new VideoScaler);

    libjami::VideoFrame input, output;
    input.reserve(AV_PIX_FMT_YUV420P, 100, 100);
    output.reserve(AV_PIX_FMT_YUV420P, 200, 200);
    scaler_->scale(input, output);
    CPPUNIT_ASSERT(static_cast<AVPixelFormat>(output.format()) == AV_PIX_FMT_YUV420P);
    CPPUNIT_ASSERT(output.width() == 200);
    CPPUNIT_ASSERT(output.height() == 200);
}

void
VideoScalerTest::testScaleWithAspect()
{
    scaler_.reset(new VideoScaler);

    libjami::VideoFrame input, output;
    input.reserve(AV_PIX_FMT_YUV420P, 320, 240); // 4:3
    output.reserve(AV_PIX_FMT_YUV420P, 640, 360); // 16:9
    scaler_->scale_with_aspect(input, output);
    CPPUNIT_ASSERT(static_cast<AVPixelFormat>(output.format()) == AV_PIX_FMT_YUV420P);
    CPPUNIT_ASSERT(output.width() == 640);
    CPPUNIT_ASSERT(output.height() == 360);
}

}}} // namespace jami::test

RING_TEST_RUNNER(jami::video::test::VideoScalerTest::name());
