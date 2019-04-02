/*
 *  Copyright (C) 2011-2019 Savoir-faire Linux Inc.
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
#include "dring.h"

#include <map>
#include <string>

namespace jami { namespace test {

class VideoInputTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "video_input"; }

private:
    void testInput();
    void init_daemon();

    CPPUNIT_TEST_SUITE(VideoInputTest);
    CPPUNIT_TEST(init_daemon);
    CPPUNIT_TEST(testInput);
    CPPUNIT_TEST_SUITE_END();
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VideoInputTest, VideoInputTest::name());

void
VideoInputTest::init_daemon()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    DRing::start("dring-sample.yml");
}

void
VideoInputTest::testInput()
{
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;
    std::string resource = DRing::Media::VideoProtocolPrefix::DISPLAY + sep + std::string(getenv("DISPLAY") ? : ":0.0");
    video::VideoInput video;
    video.switchInput(resource);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::VideoInputTest::name());
