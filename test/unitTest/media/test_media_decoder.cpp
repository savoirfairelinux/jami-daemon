/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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
#include "fileutils.h"
#include "libav_deps.h"
#include "media_buffer.h"
#include "media_decoder.h"
#include "media_device.h"
#include "media_io_handle.h"

#include "../../test_runner.h"

namespace jami { namespace test {

class MediaDecoderTest : public CppUnit::TestFixture {
public:
    static std::string name() { return "media_decoder"; }

    void setUp();
    void tearDown();

private:
    void testAudioFile();

    CPPUNIT_TEST_SUITE(MediaDecoderTest);
    CPPUNIT_TEST(testAudioFile);
    CPPUNIT_TEST_SUITE_END();

    void writeWav(); // writes a minimal wav file to test decoding

    std::unique_ptr<MediaDecoder> decoder_;
    std::string filename_ = "test.wav";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaDecoderTest, MediaDecoderTest::name());

void
MediaDecoderTest::setUp()
{
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));
    libav_utils::ring_avcodec_init();
    decoder_.reset(new MediaDecoder);
}

void
MediaDecoderTest::tearDown()
{
    fileutils::remove(filename_);
    DRing::fini();
}

void
MediaDecoderTest::testAudioFile()
{
    if (!avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE)
        || !avcodec_find_decoder(AV_CODEC_ID_PCM_S16BE))
        return; // no way to test the wav file, since it is in pcm signed 16

    writeWav();

    DeviceParams dev;
    dev.input = filename_;
    CPPUNIT_ASSERT(decoder_->openInput(dev) >= 0);
    CPPUNIT_ASSERT(decoder_->setupFromAudioData() >= 0);

    bool done = false;
    while (!done) {
        AudioFrame frame;
        switch (decoder_->decode(frame)) {
        case MediaDecoder::Status::FrameFinished:
            CPPUNIT_ASSERT(frame.pointer()->sample_rate == decoder_->getStream().sampleRate);
            CPPUNIT_ASSERT(frame.pointer()->channels == decoder_->getStream().nbChannels);
            break;
        case MediaDecoder::Status::DecodeError:
        case MediaDecoder::Status::ReadError:
            CPPUNIT_ASSERT_MESSAGE("Decode error", false);
            done = true;
            break;
        case MediaDecoder::Status::EOFError:
            done = true;
            break;
        case MediaDecoder::Status::Success:
        default:
            break;
        }
    }
    CPPUNIT_ASSERT(done);
}

// write bytes to file using native endianness
template<typename Word>
static std::ostream& write(std::ostream& os, Word value, unsigned size)
{
    for (; size; --size, value >>= 8)
        os.put(static_cast<char>(value & 0xFF));
    return os;
}

void
MediaDecoderTest::writeWav()
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

}} // namespace jami::test

RING_TEST_RUNNER(jami::test::MediaDecoderTest::name());
