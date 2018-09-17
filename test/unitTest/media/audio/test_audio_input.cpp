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

#include "audio/audio_input.h"
#include "audio/ringbufferpool.h"
#include "dring.h"
#include "fileutils.h"
#include "libav_deps.h"
#include "manager.h"

#include "../../../test_runner.h"

#include <chrono>
#include <thread>

namespace ring { namespace test {

class AudioInputTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "audio_input"; }

    void setUp();
    void tearDown();

private:
    void testSwitchInput();
    void writeWav();

    CPPUNIT_TEST_SUITE(AudioInputTest);
    CPPUNIT_TEST(testSwitchInput);
    CPPUNIT_TEST_SUITE_END();

    std::unique_ptr<AudioInput> audioInput_;
    std::string id_ = "audio_input_test";
    std::string filename_ = "audio_input.wav";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(AudioInputTest, AudioInputTest::name());

void
AudioInputTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    DRing::start("dring-sample.yml");
    libav_utils::ring_avcodec_init();
}

void
AudioInputTest::tearDown()
{
    fileutils::remove(filename_);
    DRing::fini();
}

void
AudioInputTest::testSwitchInput()
{
    AVFrame* frame = nullptr;

    // create read offset for default ring buffer
    Manager::instance().getRingBufferPool().bindHalfDuplexOut(id_, RingBufferPool::DEFAULT_ID);
    Manager::instance().startAudioDriverStream();

    // test wav file
    writeWav();
    CPPUNIT_ASSERT(fileutils::isFile(filename_)); // make sure file was created

    // initializing the audio layer takes time
    std::this_thread::sleep_for(std::chrono::seconds(1));

    audioInput_.reset(new AudioInput(id_, AudioFormat::STEREO(), false));
    CPPUNIT_ASSERT(audioInput_.get());

    // the audio layer takes a non deterministic amount of time to initalize, so don't test mic input
    frame = audioInput_->getNextFrame();
    av_frame_unref(frame);

    audioInput_->switchInput("file://" + filename_);

    frame = audioInput_->getNextFrame();
    std::cout << frame->pts << "\n";
    CPPUNIT_ASSERT(frame);
    CPPUNIT_ASSERT(frame->sample_rate == 8000 && frame->channels == 1);
    av_frame_free(&frame);

    Manager::instance().getRingBufferPool().unBindHalfDuplexOut(id_, RingBufferPool::DEFAULT_ID);
    audioInput_.reset();
}

template<typename Word>
static std::ostream&
write(std::ostream& os, Word value, unsigned size)
{
    for (; size; --size, value >>= 8)
        os.put(static_cast<char>(value & 0xFF));
    return os;
}

void
AudioInputTest::writeWav()
{
    auto f = std::ofstream(filename_, std::ios::binary);
    f << "RIFF----WAVEfmt ";
    write(f, 16, 4); // no extension data
    write(f, 1, 2); // PCM integer samples
    write(f, 1, 2); // channels
    write(f, 8000, 4); // sample rate
    write(f, 8000 * 1 * 2, 4); // sample rate * channels * bytes per sample
    write(f, 4, 2); // data block size
    write(f, 2 * 8, 2); // bits per sample
    size_t dataChunk = f.tellp();
    f << "data----";

    // fill file with silence
    // make sure there is more than 1 AVFrame in the file
    for (int i = 0; i < 8192; ++i)
        write(f, 0, 2);

    size_t length = f.tellp();
    f.seekp(dataChunk + 4);
    write(f, length - dataChunk + 8, 4);
    f.seekp(4);
    write(f, length - 8, 4);
}

}} // namespace ring::test

RING_TEST_RUNNER(ring::test::AudioInputTest::name());
