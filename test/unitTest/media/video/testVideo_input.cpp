/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Olivier Gregoire <olivier.gregoire@savoirfairelinux.com>
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

#include "test_runner.h"

#include "media/video/video_input.h"
#include "media_const.h"
#include "jami.h"
#include "manager.h"

#include <map>
#include <string>

namespace jami {
namespace test {

class VideoInputTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "video_input"; }

    VideoInputTest()
    {
        libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
        libjami::start("jami-sample.yml");
    }

    ~VideoInputTest() { libjami::fini(); }

private:
    void testInput();

    CPPUNIT_TEST_SUITE(VideoInputTest);
    CPPUNIT_TEST(testInput);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VideoInputTest, VideoInputTest::name());

void
VideoInputTest::testInput()
{
    static const std::string sep = libjami::Media::VideoProtocolPrefix::SEPARATOR;
    std::string resource = libjami::Media::VideoProtocolPrefix::DISPLAY + sep
                           + std::string(getenv("DISPLAY") ?: ":0.0");
    video::VideoInput video;
    libjami::switchInput("", "", resource);
}

} // namespace test
} // namespace jami

RING_TEST_RUNNER(jami::test::VideoInputTest::name());
