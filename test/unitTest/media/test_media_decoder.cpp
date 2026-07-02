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
#include "fileutils.h"
#include "media/libav_deps.h"
#include "media/media_buffer.h"
#include "media/media_decoder.h"
#include "media/media_device.h"
#include "media/media_encoder.h"
#include "media/media_io_handle.h"
#include "media/system_codec_container.h"
#ifdef ENABLE_HWACCEL
#include "media/video/accel.h"
#endif

#include "../../test_runner.h"

namespace jami {
namespace test {

class MediaDecoderTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "media_decoder"; }

    void setUp();
    void tearDown();

private:
    void testAudioFile();
    void testH264HighChromaDecoding();
    void testHardwareFormatFallback();

    CPPUNIT_TEST_SUITE(MediaDecoderTest);
    CPPUNIT_TEST(testAudioFile);
    CPPUNIT_TEST(testH264HighChromaDecoding);
    CPPUNIT_TEST(testHardwareFormatFallback);
    CPPUNIT_TEST_SUITE_END();

    void writeWav(); // writes a minimal wav file to test decoding
    void writeH264HighChromaFile(int width, int height, int frames);

    std::unique_ptr<MediaDecoder> decoder_;
    std::string filename_ = "test.wav";
    std::string h264Filename_ = "test444_dec.h264";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaDecoderTest, MediaDecoderTest::name());

void
MediaDecoderTest::setUp()
{
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    libav_utils::av_init();
}

void
MediaDecoderTest::tearDown()
{
    dhtnet::fileutils::remove(filename_);
    dhtnet::fileutils::remove(h264Filename_);
    libjami::fini();
}

void
MediaDecoderTest::testAudioFile()
{
    if (!avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE) || !avcodec_find_decoder(AV_CODEC_ID_PCM_S16BE))
        return; // no way to test the wav file, since it is in pcm signed 16

    writeWav();

    decoder_.reset(new MediaDecoder([this](const std::shared_ptr<MediaFrame>&& f) mutable {
        CPPUNIT_ASSERT(f->pointer()->sample_rate == decoder_->getStream().sampleRate);
        CPPUNIT_ASSERT(f->pointer()->ch_layout.nb_channels == decoder_->getStream().nbChannels);
    }));
    DeviceParams dev;
    dev.input = filename_;
    CPPUNIT_ASSERT(decoder_->openInput(dev) >= 0);
    CPPUNIT_ASSERT(decoder_->setupAudio() >= 0);

    bool done = false;
    while (!done) {
        switch (decoder_->decode()) {
        case MediaDemuxer::Status::ReadError:
            CPPUNIT_ASSERT_MESSAGE("Decode error", false);
            done = true;
            break;
        case MediaDemuxer::Status::EndOfFile:
            done = true;
            break;
        case MediaDemuxer::Status::Success:
        default:
            break;
        }
    }
    CPPUNIT_ASSERT(done);
}

void
MediaDecoderTest::writeH264HighChromaFile(int width, int height, int frames)
{
    auto codecs = std::make_shared<SystemCodecContainer>();
    codecs->init(false);
    auto h264Codec = std::static_pointer_cast<SystemVideoCodecInfo>(codecs->searchCodecByName("H264", MEDIA_VIDEO));
    CPPUNIT_ASSERT(h264Codec);

    MediaEncoder encoder;
    auto v = MediaStream("v", AV_PIX_FMT_YUV444P, rational<int>(1, 30), width, height, 1, 30);
    MediaDescription args;
    args.parameters = "profile-level-id=f40029"; // High 4:4:4 Predictive
    encoder.openOutput(h264Filename_);
    encoder.setOptions(v);
    encoder.setOptions(args);
    int videoIdx = encoder.addStream(*h264Codec.get());
    CPPUNIT_ASSERT(videoIdx >= 0);
    encoder.setIOContext(nullptr);

    for (int i = 0; i < frames; ++i) {
        AVFrame* frame = av_frame_alloc();
        CPPUNIT_ASSERT(frame);
        frame->format = AV_PIX_FMT_YUV444P;
        frame->width = width;
        frame->height = height;
        CPPUNIT_ASSERT(av_frame_get_buffer(frame, 32) >= 0);
        for (int y = 0; y < height; y++)
            for (int x = 0; x < width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        frame->pts = i;
        CPPUNIT_ASSERT(encoder.encode(frame, videoIdx) >= 0);
        av_frame_free(&frame);
    }
    CPPUNIT_ASSERT(encoder.flush() >= 0);
}

void
MediaDecoderTest::testH264HighChromaDecoding()
{
    if (!avcodec_find_decoder(AV_CODEC_ID_H264) || !avcodec_find_encoder(AV_CODEC_ID_H264))
        return;

    const constexpr int width = 320;
    const constexpr int height = 240;
    const constexpr int frames = 10;
    writeH264HighChromaFile(width, height, frames);

    int decodedFrames = 0;
    decoder_.reset(new MediaDecoder([&](const std::shared_ptr<MediaFrame>&& f) mutable {
        auto* frame = f->pointer();
        CPPUNIT_ASSERT_EQUAL(width, frame->width);
        CPPUNIT_ASSERT_EQUAL(height, frame->height);
        decodedFrames++;
    }));
    // Keep hardware acceleration enabled: decoding a high chroma stream
    // must fall back to software when the hardware does not support it.
    DeviceParams dev;
    dev.input = h264Filename_;
    dev.framerate = 30;
    CPPUNIT_ASSERT(decoder_->openInput(dev) >= 0);
    CPPUNIT_ASSERT(decoder_->setupVideo() >= 0);

    bool done = false;
    while (!done) {
        switch (decoder_->decode()) {
        case MediaDemuxer::Status::ReadError:
            CPPUNIT_ASSERT_MESSAGE("Decode error", false);
            done = true;
            break;
        case MediaDemuxer::Status::EndOfFile:
            done = true;
            break;
        case MediaDemuxer::Status::Success:
        default:
            break;
        }
    }
    CPPUNIT_ASSERT(done);
    CPPUNIT_ASSERT(decodedFrames > 0);
}

void
MediaDecoderTest::testHardwareFormatFallback()
{
#ifdef ENABLE_HWACCEL
    auto accels = video::HardwareAccel::getCompatibleAccel(AV_CODEC_ID_H264, 1280, 720, CODEC_DECODER);
    if (accels.empty())
        return; // no hardware decoder on this machine
    auto accel = std::make_unique<video::HardwareAccel>(accels.front());
    if (accel->initAPI(false, nullptr) < 0)
        return;

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    CPPUNIT_ASSERT(codec);
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    CPPUNIT_ASSERT(ctx);
    accel->setDetails(ctx);
    ctx->opaque = accel.get();
    CPPUNIT_ASSERT(ctx->get_format);

    // When the stream only offers software formats (e.g. a high chroma
    // profile the hardware cannot decode), the format callback must fall
    // back to the software format instead of failing.
    AVPixelFormat swOnly[] = {AV_PIX_FMT_YUV444P, AV_PIX_FMT_NONE};
    CPPUNIT_ASSERT_EQUAL(AV_PIX_FMT_YUV444P, ctx->get_format(ctx, swOnly));

    // The hardware format is still picked when present
    AVPixelFormat withHw[] = {accel->getFormat(), AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    CPPUNIT_ASSERT_EQUAL(accel->getFormat(), ctx->get_format(ctx, withHw));

    avcodec_free_context(&ctx);
#endif
}

// write bytes to file using native endianness
template<typename Word>
static std::ostream&
write(std::ostream& os, Word value, unsigned size)
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
    write(f, 16, 4);           // no extension data
    write(f, 1, 2);            // PCM integer samples
    write(f, 1, 2);            // channels
    write(f, 8000, 4);         // sample rate
    write(f, 8000 * 1 * 2, 4); // sample rate * channels * bytes per sample
    write(f, 4, 2);            // data block size
    write(f, 2 * 8, 2);        // bits per sample
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

} // namespace test
} // namespace jami

CORE_TEST_RUNNER(jami::test::MediaDecoderTest::name());
