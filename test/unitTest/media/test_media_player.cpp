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
#include <cmath>
#include <cstring>
#include <filesystem>

extern "C" {
#include <libavformat/avformat.h>
}

#include "jami.h"
#include "manager.h"
#include "../../test_runner.h"
#include "client/videomanager.h"

namespace jami {
namespace test {

class MediaPlayerTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "media_player"; }

    void setUp();
    void tearDown();

private:
    void testCreate();
    void testJPG();
    void testAudioFile();
    void testPause();
    void testSeekWhilePaused();
    void testSeekWhilePlaying();
    void testPauseWithUndecodableVideo();

    bool isWithinUsec(int64_t currentTime, int64_t seekTime, int64_t margin);

    CPPUNIT_TEST_SUITE(MediaPlayerTest);
    CPPUNIT_TEST(testCreate);
    CPPUNIT_TEST(testJPG);
    CPPUNIT_TEST(testAudioFile);
    CPPUNIT_TEST(testPause);
    CPPUNIT_TEST(testSeekWhilePaused);
    CPPUNIT_TEST(testSeekWhilePlaying);
    CPPUNIT_TEST(testPauseWithUndecodableVideo);
    CPPUNIT_TEST_SUITE_END();

    std::string playerId1_ {};
    std::string playerId2_ {};
    int64_t duration_ {};
    int audio_stream_ {};
    int video_stream_ {};
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
        [=](const std::string& playerId, const std::map<std::string, std::string>& info) {
            duration_ = std::stol(info.at("duration"));
            audio_stream_ = std::stoi(info.at("audio_stream"));
            video_stream_ = std::stoi(info.at("video_stream"));
            playerId2_ = playerId;
            cv.notify_all();
        }));
    libjami::registerSignalHandlers(handler);
}

void
MediaPlayerTest::tearDown()
{
    jami::closeMediaPlayer(playerId1_);
    mediaPlayer.reset();
    playerId1_ = {};
    playerId2_ = {};
    libjami::fini();
}

void
MediaPlayerTest::testCreate()
{
    JAMI_LOG("Start testCreate");
    playerId1_ = jami::createMediaPlayer("./media/data/test_video_file.mp4");
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    cv.wait_for(lk, 5s);
    CPPUNIT_ASSERT(playerId1_ == playerId2_);
    CPPUNIT_ASSERT(mediaPlayer->getId() == playerId1_);
    CPPUNIT_ASSERT(mediaPlayer->isInputValid());
    CPPUNIT_ASSERT(audio_stream_ != -1);
    CPPUNIT_ASSERT(video_stream_ != -1);
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    CPPUNIT_ASSERT(mediaPlayer->getPlayerPosition() == 0);
    JAMI_LOG("End testCreate");
}

void
MediaPlayerTest::testJPG()
{
    JAMI_LOG("Start testJpg");
    playerId1_ = jami::createMediaPlayer("./media/data/jami.jpg");
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    cv.wait_for(lk, 5s);
    CPPUNIT_ASSERT(playerId1_ == playerId2_);
    CPPUNIT_ASSERT(mediaPlayer->getId() == playerId1_);
    CPPUNIT_ASSERT(mediaPlayer->isInputValid());
    CPPUNIT_ASSERT(video_stream_ != -1);
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    CPPUNIT_ASSERT(mediaPlayer->getPlayerPosition() == 0);
    JAMI_LOG("End testJpg");
}

void
MediaPlayerTest::testAudioFile()
{
    JAMI_LOG("Start testAudioFile");
    playerId1_ = jami::createMediaPlayer("./media/data/test.mp3");
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    cv.wait_for(lk, 5s);
    CPPUNIT_ASSERT(playerId1_ == playerId2_);
    CPPUNIT_ASSERT(mediaPlayer->getId() == playerId1_);
    CPPUNIT_ASSERT(mediaPlayer->isInputValid());
    CPPUNIT_ASSERT(audio_stream_ != -1);
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    CPPUNIT_ASSERT(mediaPlayer->getPlayerPosition() == 0);
    JAMI_LOG("End testAudioFile");
}

void
MediaPlayerTest::testPause()
{
    playerId1_ = jami::createMediaPlayer("./media/data/test_video_file.mp4");
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    cv.wait_for(lk, 5s);
    JAMI_LOG("Start testPause");
    // should start paused
    CPPUNIT_ASSERT(mediaPlayer->isPaused());
    mediaPlayer->pause(false);
    CPPUNIT_ASSERT(!mediaPlayer->isPaused());
    JAMI_LOG("End testPause");
}

bool
MediaPlayerTest::isWithinUsec(int64_t currentTime, int64_t seekTime, int64_t margin)
{
    return std::abs(currentTime - seekTime) <= margin;
}

void
MediaPlayerTest::testSeekWhilePaused()
{
    JAMI_LOG("Start testSeekWhilePaused");
    playerId1_ = jami::createMediaPlayer("./media/data/test_video_file.mp4");
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    cv.wait_for(lk, 5s);

    int64_t startTime = mediaPlayer->getPlayerPosition();

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime + 100));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime + 100, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime + 1000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime + 1000, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime + 500));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime + 500, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(duration_ - 1));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), duration_ - 1, 1));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(0));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), 0, 1));

    CPPUNIT_ASSERT(!(mediaPlayer->seekToTime(duration_ + 1)));
    JAMI_LOG("End testSeekWhilePaused");
}

void
MediaPlayerTest::testSeekWhilePlaying()
{
    JAMI_LOG("Start testSeekWhilePlaying");
    playerId1_ = jami::createMediaPlayer("./media/data/test_video_file.mp4");
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    cv.wait_for(lk, 5s);
    mediaPlayer->pause(false);

    int64_t startTime = mediaPlayer->getPlayerPosition();

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime + 10000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime + 10000, 50));

    startTime = mediaPlayer->getPlayerPosition();
    CPPUNIT_ASSERT(mediaPlayer->seekToTime(startTime - 5000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), startTime - 5000, 50));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(10000));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), 10000, 50));

    CPPUNIT_ASSERT(mediaPlayer->seekToTime(0));
    CPPUNIT_ASSERT(isWithinUsec(mediaPlayer->getPlayerPosition(), 0, 50));

    CPPUNIT_ASSERT(!(mediaPlayer->seekToTime(duration_ + 1)));
    JAMI_LOG("End testSeekWhilePlaying");
}

// Write a Matroska file with a single MPEG-2 video stream; the daemon's
// FFmpeg build has no MPEG-2 decoder, so video decoder setup fails.
static bool
writeUndecodableVideoFile(const std::string& path)
{
    AVFormatContext* ctx = nullptr;
    if (avformat_alloc_output_context2(&ctx, nullptr, "matroska", path.c_str()) < 0 || !ctx)
        return false;
    bool ok = false;
    if (auto* st = avformat_new_stream(ctx, nullptr)) {
        st->time_base = {1, 1000};
        st->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
        st->codecpar->codec_id = AV_CODEC_ID_MPEG2VIDEO;
        st->codecpar->width = 64;
        st->codecpar->height = 64;
        if (avio_open(&ctx->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0) {
            if (avformat_write_header(ctx, nullptr) >= 0) {
                ok = true;
                for (int i = 0; i < 10 && ok; ++i) {
                    AVPacket* pkt = av_packet_alloc();
                    ok = pkt && av_new_packet(pkt, 16) >= 0;
                    if (ok) {
                        std::memset(pkt->data, 0, pkt->size);
                        pkt->stream_index = st->index;
                        pkt->pts = pkt->dts = av_rescale_q(i * 100, {1, 1000}, st->time_base);
                        pkt->duration = av_rescale_q(100, {1, 1000}, st->time_base);
                        pkt->flags |= AV_PKT_FLAG_KEY;
                        ok = av_interleaved_write_frame(ctx, pkt) >= 0;
                    }
                    av_packet_free(&pkt);
                }
                ok = av_write_trailer(ctx) >= 0 && ok;
            }
            avio_closep(&ctx->pb);
        }
    }
    avformat_free_context(ctx);
    return ok;
}

void
MediaPlayerTest::testPauseWithUndecodableVideo()
{
    JAMI_LOG("Start testPauseWithUndecodableVideo");
    const std::string path = "./media/data/undecodable_video.mkv";
    CPPUNIT_ASSERT(writeUndecodableVideoFile(path));

    playerId1_ = jami::createMediaPlayer(path);
    mediaPlayer = jami::getMediaPlayer(playerId1_);
    CPPUNIT_ASSERT(mediaPlayer);
    CPPUNIT_ASSERT(cv.wait_for(lk, 5s, [&] { return playerId2_ == playerId1_; }));

    // Must not dereference a reset video input when its stream failed to open.
    mediaPlayer->pause(false);
    mediaPlayer->pause(true);
    mediaPlayer->pause(false);

    jami::closeMediaPlayer(playerId1_);
    mediaPlayer.reset();
    playerId1_ = {};
    std::error_code ec;
    std::filesystem::remove(path, ec);
    JAMI_LOG("End testPauseWithUndecodableVideo");
}

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MediaPlayerTest::name());