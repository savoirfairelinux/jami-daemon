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
#include "videomanager_interface.h"
#include "video_scaler.h"

#include "../../test_runner.h"

namespace ring { namespace test {

class VideoScalerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "video_scaler"; }

    void setUp();
    void tearDown();

private:
    void testSemantics();

    CPPUNIT_TEST_SUITE(VideoScalerTest);
    CPPUNIT_TEST(testConvertFrame);
    CPPUNIT_TEST_SUITE_END();
};


CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(VideoScalerTest, VideoScalerTest::name());

void
VideoScalerTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
}

void
VideoScalerTest::tearDown()
{
    DRing::fini();
}

void
VideoScalerTest::testConvertFrame()
{
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::VideoScalerTest::name());
