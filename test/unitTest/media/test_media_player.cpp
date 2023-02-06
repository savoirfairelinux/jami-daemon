/*
 *  Copyright (C) 2022-2023 Savoir-faire Linux Inc.
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
#include <cmath>

#include "jami.h"
#include "manager.h"
#include "../../test_runner.h"
#include "client/videomanager.h"

namespace jami { namespace test {

class MediaPlayerTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_player"; }

    void setUp();
    void tearDown();

private:
    std::string filePath = "./media/test_video_file.mp4";

    void testCreate();
    void testPause();
    void testSeekWhilePaused();
    void testSeekWhilePlaying();

    bool isWithinUsec(int64_t currentTime, int64_t seekTime, int64_t margin);

    CPPUNIT_TEST_SUITE(MediaPlayerTest);
    CPPUNIT_TEST(testCreate);
    CPPUNIT_TEST(testPause);
    CPPUNIT_TEST(testSeekWhilePaused);
    CPPUNIT_TEST(testSeekWhilePlaying);
    CPPUNIT_TEST_SUITE_END();

    std::string playerId1_ {};
    std::string playerId2_ {};
    std::string duration_ {};
    std::string audio_stream_ {};
    std::string video_stream_ {};
    std::shared_ptr<MediaPlayer> mediaPlayer {};

    std::mutex mtx;
    std::unique_lock<std::mutex> lk {mtx};
    std::condition_variable cv;
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaPlayerTest, MediaPlayerTest::name());

void
MediaPlayerTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    if (not Manager::instance().initialized)
        CPPUNIT_ASSERT(libjami::start("jami-sample.yml"));

    std::map<std::string, std::shared_ptr<libjami::CallbackWrapperBase>> handler;
    handler.insert(libjami::exportable_callback<libjami::MediaPlayerSignal::FileOpened>(
        [=](const std::string& playerId,
            const std::map<std::string, std::string>& info) {
            duration_ = info.at("duration");
            audio_stream_ = info.at("audio_stream");
            video_stream_ = info.at("video_stream");
            playerId2_ = playerId;
            cv.notify_all();
        }));

    libjami::registerSignalHandlers(handler);

    playerId1_ = libjami::createMediaPlayer(filePath);
    mediaPlayer = Manager::instance().getVideoManager().mediaPlayers[playerId1_];
    cv.wait_for(lk, 5s);
}

void
MediaPlayerTest::tearDown()
{
    libjami::fini();
}

void
MediaPlayerTest::testCreate()
{
    CPPUNIT_ASSERT(playerId1_ == playerId2_);
    CPPUNIT_ASSERT(mediaPlayer->getId() == playerId1_);
    CPPUNIT_ASSERT(mediaPlayer->isInputValid());
    CPPUNIT_ASSERT(audio_stream_ != "-1");
    CPPUNIT_ASSERT(video_stream_ != "-1");
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    CPPUNIT_ASSERT(mediaPlayer->getPlayerPosition() == 0);
}

void
MediaPlayerTest::testPause()
{
    mediaPlayer->pause(true);
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    mediaPlayer->pause(false);
    CPPUNIT_ASSERT(!mediaPlayer->isPaused());
}

bool
MediaPlayerTest::isWithinUsec(int64_t currentTime, int64_t seekTime, int64_t margin)
{
    return (std::abs(currentTime-seekTime) <= margin);
}

void
MediaPlayerTest::testSeekWhilePaused()
{
    mediaPlayer->pause(true);

    int64_t startTime = mediaPlayer->getPlayerPosition();
    int64_t duration = std::stoi(duration_);

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime+100));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime+100, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime+1000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime+1000, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime+500));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime+500, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(duration-1));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), duration-1, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(0));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), 0, 1));

    CPPUNIT_ASSERT(!(mediaPlayer->seekToTime(duration+1)));
}

void
MediaPlayerTest::testSeekWhilePlaying()
{
    mediaPlayer->pause(false);

    int64_t startTime = mediaPlayer->getPlayerPosition();

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime+10000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime+10000, 50));

    startTime = mediaPlayer->getPlayerPosition();
    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime-5000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime-5000, 50));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(10000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), 10000, 50));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(0));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), 0, 50));

    CPPUNIT_ASSERT(!(mediaPlayer->seekToTime(std::stoi(duration_)+1)));
}

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::MediaPlayerTest::name());