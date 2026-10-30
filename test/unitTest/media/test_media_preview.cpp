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
#include "jami/videomanager_interface.h"
#include "manager.h"
#include "../../test_runner.h"

extern "C" {
#include <libavutil/frame.h>
}

namespace jami {
namespace test {

class MediaPreviewTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "media_preview"; }

    void setUp();
    void tearDown();

private:
    void testVideoFile();
    void testCoverArtIsNotReturnedAsVideo();
    void testMissingFile();

    CPPUNIT_TEST_SUITE(MediaPreviewTest);
    CPPUNIT_TEST(testVideoFile);
    CPPUNIT_TEST(testCoverArtIsNotReturnedAsVideo);
    CPPUNIT_TEST(testMissingFile);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaPreviewTest, MediaPreviewTest::name());

void
MediaPreviewTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
}

void
MediaPreviewTest::tearDown()
{
    libjami::fini();
}

void
MediaPreviewTest::testVideoFile()
{
    auto preview = libjami::getMediaPreview("./media/data/test_video_file.mp4");

    CPPUNIT_ASSERT(std::stoll(preview.info.at("duration")) > 0);
    CPPUNIT_ASSERT(std::stoi(preview.info.at("audio_stream")) >= 0);
    CPPUNIT_ASSERT(std::stoi(preview.info.at("video_stream")) >= 0);
    CPPUNIT_ASSERT(preview.frame);
    CPPUNIT_ASSERT_EQUAL(320, preview.frame->width);
    CPPUNIT_ASSERT_EQUAL(240, preview.frame->height);
}

void
MediaPreviewTest::testCoverArtIsNotReturnedAsVideo()
{
    auto preview = libjami::getMediaPreview("./media/data/test.mp3");

    CPPUNIT_ASSERT(std::stoll(preview.info.at("duration")) > 0);
    CPPUNIT_ASSERT(std::stoi(preview.info.at("audio_stream")) >= 0);
    CPPUNIT_ASSERT_EQUAL(std::string("-1"), preview.info.at("video_stream"));
    CPPUNIT_ASSERT(!preview.frame);
}

void
MediaPreviewTest::testMissingFile()
{
    auto preview = libjami::getMediaPreview("./media/data/does_not_exist.mp4");

    CPPUNIT_ASSERT_EQUAL(std::string("0"), preview.info.at("duration"));
    CPPUNIT_ASSERT_EQUAL(std::string("-1"), preview.info.at("audio_stream"));
    CPPUNIT_ASSERT_EQUAL(std::string("-1"), preview.info.at("video_stream"));
    CPPUNIT_ASSERT(!preview.frame);
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MediaPreviewTest::name());
