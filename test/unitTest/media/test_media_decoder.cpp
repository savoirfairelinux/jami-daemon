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
#include "media/media_io_handle.h"

#include "../../test_runner.h"

#ifdef JAMI_TEST_WRAP_AVFORMAT_OPEN_INPUT
#include <cerrno>
#include <ctime>
#include <string_view>
#include <vector>

namespace {

constexpr const char* testInput = "jami-test-input";
constexpr const char* testCaptureInput = "jami-test-capture";

struct OpenInputCall
{
    bool hasContext;
    int flags;
    AVIOInterruptCB interrupt;
    const AVInputFormat* format;
};

std::vector<OpenInputCall> openInputCalls;

struct CaptureReadProbe;
thread_local CaptureReadProbe* captureReadProbe {};

struct CaptureReadProbe
{
    CaptureReadProbe()
    {
        format.name = testCaptureInput;
        format.flags = AVFMT_NOFILE;
        captureReadProbe = this;
    }

    ~CaptureReadProbe() { captureReadProbe = nullptr; }

    AVInputFormat format {};
    AVFormatContext* context {};
    int readResult = AVERROR(EAGAIN);
    unsigned reads {};
    int64_t now = AV_TIME_BASE;
    std::vector<int64_t> sleeps;
};

int
interruptCapture(void* opaque)
{
    return *static_cast<bool*>(opaque);
}

} // namespace

extern "C" int __real_avformat_open_input(AVFormatContext**, const char*, const AVInputFormat*, AVDictionary**);

extern "C" int
__wrap_avformat_open_input(AVFormatContext** ctx, const char* url, const AVInputFormat* format, AVDictionary** options)
{
    if (url && std::string_view(url) == testCaptureInput) {
        if (!captureReadProbe || !*ctx)
            return AVERROR(EINVAL);
        captureReadProbe->context = *ctx;
        // No device callbacks: reads are supplied below, teardown only frees the context.
        (*ctx)->iformat = &captureReadProbe->format;
        return 0;
    }
    if (!url || std::string_view(url) != testInput)
        return __real_avformat_open_input(ctx, url, format, options);

    openInputCalls.push_back(
        {*ctx != nullptr, *ctx ? (*ctx)->flags : 0, *ctx ? (*ctx)->interrupt_callback : AVIOInterruptCB {}, format});
    // Model FFmpeg's failed-open ownership contract without opening any device.
    avformat_free_context(*ctx);
    *ctx = nullptr;
    return AVERROR(EBUSY);
}

extern "C" int __real_av_read_frame(AVFormatContext*, AVPacket*);

extern "C" int
__wrap_av_read_frame(AVFormatContext* ctx, AVPacket* packet)
{
    if (!captureReadProbe || ctx != captureReadProbe->context)
        return __real_av_read_frame(ctx, packet);
    ++captureReadProbe->reads;
    packet->stream_index = 0;
    return captureReadProbe->readResult;
}

extern "C" int64_t __real_av_gettime_relative();

extern "C" int64_t
__wrap_av_gettime_relative()
{
    return captureReadProbe ? captureReadProbe->now : __real_av_gettime_relative();
}

extern "C" int __real_nanosleep(const timespec*, timespec*);

extern "C" int
__wrap_nanosleep(const timespec* requested, timespec* remaining)
{
    if (!captureReadProbe)
        return __real_nanosleep(requested, remaining);
    // Observe the requested wait without wall-clock timing or affecting other threads.
    auto delay = requested->tv_sec * AV_TIME_BASE + requested->tv_nsec / 1000;
    captureReadProbe->sleeps.push_back(delay);
    captureReadProbe->now += delay;
    return 0;
}
#endif

namespace jami {
namespace test {

class MediaDecoderTest : public CppUnit::TestFixture
{
public:
    static std::string name() { return "media_decoder"; }

    void setUp();
    void tearDown();

private:
    void testUnsetPixelFormat();
    void testAudioFile();
#ifdef JAMI_TEST_WRAP_AVFORMAT_OPEN_INPUT
    void testV4L2NonBlocking();
    void testV4L2AliasNonBlocking();
    void testV4L2RetryNonBlocking();
    void testOpenRetryInterruptCallback();
    void testNonV4L2FlagsUnchanged();
    void testV4L2NoFramePacing();
    void testV4L2StalledFramePacing();
    void testV4L2UnknownRatePacing();
    void testV4L2AliasPacing();
    void testV4L2PendingFramePacing();
    void testV4L2HighRatePacing();
    void testNonV4L2PacingUnchanged();
    void checkV4L2NonBlocking(const char* format, unsigned attempts);
    void checkV4L2Pacing(const char* format, unsigned rate, bool receivedFrame, bool pastDeadline);
#endif

    CPPUNIT_TEST_SUITE(MediaDecoderTest);
    CPPUNIT_TEST(testUnsetPixelFormat);
    CPPUNIT_TEST(testAudioFile);
#ifdef JAMI_TEST_WRAP_AVFORMAT_OPEN_INPUT
    CPPUNIT_TEST(testV4L2NonBlocking);
    CPPUNIT_TEST(testV4L2AliasNonBlocking);
    CPPUNIT_TEST(testV4L2RetryNonBlocking);
    CPPUNIT_TEST(testOpenRetryInterruptCallback);
    CPPUNIT_TEST(testNonV4L2FlagsUnchanged);
    CPPUNIT_TEST(testV4L2NoFramePacing);
    CPPUNIT_TEST(testV4L2StalledFramePacing);
    CPPUNIT_TEST(testV4L2UnknownRatePacing);
    CPPUNIT_TEST(testV4L2AliasPacing);
    CPPUNIT_TEST(testV4L2PendingFramePacing);
    CPPUNIT_TEST(testV4L2HighRatePacing);
    CPPUNIT_TEST(testNonV4L2PacingUnchanged);
#endif
    CPPUNIT_TEST_SUITE_END();

    void writeWav(); // writes a minimal wav file to test decoding

    std::unique_ptr<MediaDecoder> decoder_;
    std::string filename_ = "test.wav";
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MediaDecoderTest, MediaDecoderTest::name());

void
MediaDecoderTest::setUp()
{
#ifdef JAMI_TEST_WRAP_AVFORMAT_OPEN_INPUT
    openInputCalls.clear();
#endif
    libjami::init(libjami::InitFlag(libjami::LIBJAMI_FLAG_DEBUG | libjami::LIBJAMI_FLAG_CONSOLE_LOG));
    libav_utils::av_init();
}

void
MediaDecoderTest::tearDown()
{
    dhtnet::fileutils::remove(filename_);
    libjami::fini();
}

void
MediaDecoderTest::testUnsetPixelFormat()
{
    MediaDecoder decoder;
    CPPUNIT_ASSERT(!decoder.isReady());
    CPPUNIT_ASSERT_EQUAL(static_cast<int>(AV_PIX_FMT_NONE), static_cast<int>(decoder.getPixelFormat()));
}

#ifdef JAMI_TEST_WRAP_AVFORMAT_OPEN_INPUT
void
MediaDecoderTest::checkV4L2NonBlocking(const char* format, unsigned attempts)
{
    const auto* v4l2 = av_find_input_format(format);
    if (!v4l2)
        return; // FFmpeg can be built without V4L2.

    MediaDecoder decoder;
    DeviceParams params;
    params.input = testInput;
    params.format = format;
    for (unsigned attempt = 0; attempt < attempts; ++attempt)
        CPPUNIT_ASSERT_EQUAL(AVERROR(EBUSY), decoder.openInput(params));

    CPPUNIT_ASSERT_EQUAL(size_t(attempts), openInputCalls.size());
    for (const auto& call : openInputCalls) {
        CPPUNIT_ASSERT(call.format == v4l2);
        CPPUNIT_ASSERT_MESSAGE("V4L2 open and retries need an explicitly configured context", call.hasContext);
        CPPUNIT_ASSERT_MESSAGE("V4L2 must be nonblocking before avformat_open_input", call.flags & AVFMT_FLAG_NONBLOCK);
    }
}

void
MediaDecoderTest::testV4L2NonBlocking()
{
    checkV4L2NonBlocking("video4linux2", 1);
}

void
MediaDecoderTest::testV4L2AliasNonBlocking()
{
    checkV4L2NonBlocking("v4l2", 1);
}

void
MediaDecoderTest::testV4L2RetryNonBlocking()
{
    checkV4L2NonBlocking("video4linux2", 3);
}

void
MediaDecoderTest::testOpenRetryInterruptCallback()
{
    MediaDecoder decoder;
    bool stopped = false;
    decoder.setInterruptCallback(interruptCapture, &stopped);
    DeviceParams params;
    params.input = testInput;
    params.format = "video4linux2";
    for (unsigned attempt = 0; attempt < 3; ++attempt)
        CPPUNIT_ASSERT_EQUAL(AVERROR(EBUSY), decoder.openInput(params));

    CPPUNIT_ASSERT_EQUAL(size_t(3), openInputCalls.size());
    for (const auto& call : openInputCalls) {
        CPPUNIT_ASSERT_MESSAGE("Failed opens must not lose the capture interrupt callback",
                               call.interrupt.callback == interruptCapture);
        CPPUNIT_ASSERT(call.interrupt.opaque == &stopped);
        CPPUNIT_ASSERT_EQUAL(0, call.interrupt.callback(call.interrupt.opaque));
        stopped = true;
        CPPUNIT_ASSERT_EQUAL(1, call.interrupt.callback(call.interrupt.opaque));
        stopped = false;
    }

    decoder.setInterruptCallback(nullptr, nullptr);
    CPPUNIT_ASSERT_EQUAL(AVERROR(EBUSY), decoder.openInput(params));
    CPPUNIT_ASSERT_EQUAL(size_t(4), openInputCalls.size());
    CPPUNIT_ASSERT(openInputCalls.back().interrupt.callback == nullptr);
    CPPUNIT_ASSERT(openInputCalls.back().interrupt.opaque == nullptr);
}

void
MediaDecoderTest::testNonV4L2FlagsUnchanged()
{
    auto* defaults = avformat_alloc_context();
    CPPUNIT_ASSERT(defaults);
    const int defaultFlags = defaults->flags;
    avformat_free_context(defaults);

    // Files, network streams, filter sources, screen and audio capture (when available).
    for (const auto* format : {"", "wav", "sdp", "lavfi", "x11grab", "alsa", "pulse"}) {
        const auto* iformat = av_find_input_format(format);
        if (*format && !iformat)
            continue;
        MediaDecoder decoder;
        DeviceParams params;
        params.input = testInput;
        params.format = format;
        params.disable_dts_probe_delay = true;
        for (unsigned attempt = 0; attempt < 2; ++attempt) {
            CPPUNIT_ASSERT_EQUAL(AVERROR(EBUSY), decoder.openInput(params));
            const auto& call = openInputCalls.back();
            CPPUNIT_ASSERT(call.format == iformat);
            // A null context asks FFmpeg to allocate its defaults.
            CPPUNIT_ASSERT_EQUAL(defaultFlags, call.hasContext ? call.flags : defaultFlags);
        }
    }
}

void
MediaDecoderTest::checkV4L2Pacing(const char* format, unsigned rate, bool receivedFrame, bool pastDeadline)
{
    if (!av_find_input_format(format))
        return;

    CaptureReadProbe probe;
    MediaDemuxer demuxer;
    DeviceParams params;
    params.input = testCaptureInput;
    params.format = format;
    params.framerate = rate;
    CPPUNIT_ASSERT_EQUAL(0, demuxer.openInput(params));
    CPPUNIT_ASSERT(probe.context->flags & AVFMT_FLAG_NONBLOCK);
    demuxer.setStreamCallback(0);
    if (receivedFrame) {
        probe.readResult = 0;
        CPPUNIT_ASSERT(demuxer.decode() == MediaDemuxer::Status::Success);
    }
    if (pastDeadline)
        probe.now += 2 * AV_TIME_BASE;
    probe.readResult = AVERROR(EAGAIN);

    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        auto reads = probe.reads;
        auto waits = probe.sleeps.size();
        CPPUNIT_ASSERT(demuxer.decode() == MediaDemuxer::Status::Success);
        CPPUNIT_ASSERT_EQUAL(reads + 1, probe.reads);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Every empty V4L2 read must yield, even after the frame deadline",
                                     waits + 1,
                                     probe.sleeps.size());
        CPPUNIT_ASSERT_MESSAGE("Stalled capture must not busy-spin", probe.sleeps.back() >= 1000);
        CPPUNIT_ASSERT_MESSAGE("Capture shutdown must not wait more than 10 ms per empty read",
                               probe.sleeps.back() <= 10000);
        if (rate > 100)
            CPPUNIT_ASSERT(probe.sleeps.back() <= AV_TIME_BASE / rate);
    }
}

void
MediaDecoderTest::testV4L2NoFramePacing()
{
    checkV4L2Pacing("video4linux2", 30, false, false);
}

void
MediaDecoderTest::testV4L2StalledFramePacing()
{
    checkV4L2Pacing("video4linux2", 30, true, true);
}

void
MediaDecoderTest::testV4L2UnknownRatePacing()
{
    checkV4L2Pacing("video4linux2", 0, false, false);
}

void
MediaDecoderTest::testV4L2AliasPacing()
{
    checkV4L2Pacing("v4l2", 30, true, true);
}

void
MediaDecoderTest::testV4L2PendingFramePacing()
{
    checkV4L2Pacing("video4linux2", 1, true, false);
}

void
MediaDecoderTest::testV4L2HighRatePacing()
{
    checkV4L2Pacing("video4linux2", 240, false, false);
}

void
MediaDecoderTest::testNonV4L2PacingUnchanged()
{
    for (unsigned rate : {0, 30}) {
        CaptureReadProbe probe;
        MediaDemuxer demuxer;
        DeviceParams params;
        params.input = testCaptureInput;
        params.format = "wav";
        params.framerate = rate;
        CPPUNIT_ASSERT_EQUAL(0, demuxer.openInput(params));
        demuxer.setStreamCallback(0);
        probe.readResult = 0;
        CPPUNIT_ASSERT(demuxer.decode() == MediaDemuxer::Status::Success);
        probe.readResult = AVERROR(EAGAIN);
        CPPUNIT_ASSERT(demuxer.decode() == MediaDemuxer::Status::Success);
        if (rate) {
            CPPUNIT_ASSERT_EQUAL(size_t(1), probe.sleeps.size());
            CPPUNIT_ASSERT_EQUAL(int64_t(AV_TIME_BASE / rate), probe.sleeps.back());
        } else {
            CPPUNIT_ASSERT(probe.sleeps.empty());
        }

        probe.sleeps.clear();
        probe.now += 2 * AV_TIME_BASE;
        for (unsigned attempt = 0; attempt < 4; ++attempt)
            CPPUNIT_ASSERT(demuxer.decode() == MediaDemuxer::Status::Success);
        CPPUNIT_ASSERT(probe.sleeps.empty());
    }
}
#endif

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
