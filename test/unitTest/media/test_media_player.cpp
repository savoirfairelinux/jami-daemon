/*
 *  Copyright (C) 2022 Savoir-faire Linux Inc.
 *
 *  Author: Ezra Pierce <ezra.pierce@savoirfairelinux.com>
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

#include <iostream>
#include <cstdlib>
#include <memory>
#include <unistd.h>

#include "jami.h"
#include "manager.h"
#include "fileutils.h"
#include "../../test_runner.h"
#include "videomanager_interface.h"
#include "client/videomanager.h"

namespace jami { namespace test {

class MediaPlayerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_player"; }

    void setUp();
    void tearDown();

private:
    std::string filePath = "./media/test_video_file.ogv";

    void testCreate();
    void testPause();
    void testSeek();

    CPPUNIT_TEST_SUITE(MediaPlayerTest);
    CPPUNIT_TEST(testCreate);
    CPPUNIT_TEST(testPause);
    CPPUNIT_TEST(testSeek);
    CPPUNIT_TEST_SUITE_END();

    std::string playerId {};
    std::shared_ptr<MediaPlayer> mediaPlayer {};
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaPlayerTest, MediaPlayerTest::name());

void
MediaPlayerTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));
    playerId = libjami::createMediaPlayer(filePath);
    mediaPlayer = Manager::instance().getVideoManager().mediaPlayers[playerId];
}

void
MediaPlayerTest::tearDown()
{

    libjami::fini();
}

void
MediaPlayerTest::testCreate()
{
    CPPUNIT_ASSERT(mediaPlayer->getId() == playerId);
    CPPUNIT_ASSERT(mediaPlayer->isInputValid());
}

void
MediaPlayerTest::testPause()
{
    mediaPlayer->pause(true);
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    mediaPlayer->pause(false);
    CPPUNIT_ASSERT(!mediaPlayer->isPaused());
}

void
MediaPlayerTest::testSeek()
{
    mediaPlayer->pause(true);
    int64_t startTime = mediaPlayer->getPlayerPosition();
    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime+1));
    CPPUNIT_ASSERT(mediaPlayer->getPlayerPosition() == startTime+1);
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::MediaPlayerTest::name());