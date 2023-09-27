/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "video_input.h"

#include "media_decoder.h"
#include "media_const.h"
#include "manager.h"
#include "client/videomanager.h"
#include "client/ring_signal.h"
#include "sinkclient.h"
#include "logger.h"
#include "media/media_buffer.h"

#include <libavformat/avio.h>

#include <string>
#include <sstream>
#include <cassert>
#ifdef _MSC_VER
#include <io.h> // for access
#else
#include <unistd.h>
#endif
extern "C" {
#include <libavutil/display.h>
}

namespace jami {
namespace video {

static constexpr unsigned default_grab_width = 640;
static constexpr unsigned default_grab_height = 480;

VideoInput::VideoInput(VideoInputMode inputMode, const std::string& id_, const std::string& sink)
    : VideoGenerator::VideoGenerator()
    , loop_(std::bind(&VideoInput::setup, this),
            std::bind(&VideoInput::process, this),
            std::bind(&VideoInput::cleanup, this))
{
    inputMode_ = inputMode;
    if (inputMode_ == VideoInputMode::Undefined) {
#if (defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS))
        inputMode_ = VideoInputMode::ManagedByClient;
#else
        inputMode_ = VideoInputMode::ManagedByDaemon;
#endif
    }
    sink_ = Manager::instance().createSinkClient(sink.empty() ? id_ : sink);
    switchInput(id_);
}

VideoInput::~VideoInput()
{
    isStopped_ = true;
    if (videoManagedByClient()) {
        emitSignal<libjami::VideoSignal::StopCapture>(decOpts_.input);
        capturing_ = false;
        return;
    }
    loop_.join();
}

void
VideoInput::startLoop()
{
    if (videoManagedByClient()) {
        switchDevice();
        return;
    }
    if (!loop_.isRunning())
        loop_.start();
}

void
VideoInput::switchDevice()
{
    if (switchPending_.exchange(false)) {
        JAMI_DBG("Switching input to '%s'", decOpts_.input.c_str());
        if (decOpts_.input.empty()) {
            capturing_ = false;
            return;
        }

        emitSignal<libjami::VideoSignal::StartCapture>(decOpts_.input);
        capturing_ = true;
    }
}

int
VideoInput::getWidth() const
{
    if (videoManagedByClient()) {
        return decOpts_.width;
    }
    return decoder_ ? decoder_->getWidth() : 0;
}

int
VideoInput::getHeight() const
{
    if (videoManagedByClient()) {
        return decOpts_.height;
    }
    return decoder_ ? decoder_->getHeight() : 0;
}

AVPixelFormat
VideoInput::getPixelFormat() const
{
    if (!videoManagedByClient()) {
        return decoder_->getPixelFormat();
    }
    return (AVPixelFormat) std::stoi(decOpts_.format);
}

void
VideoInput::setRotation(int angle)
{
    std::shared_ptr<AVBufferRef> displayMatrix {av_buffer_alloc(sizeof(int32_t) * 9),
                                                [](AVBufferRef* buf) {
                                                    av_buffer_unref(&buf);
                                                }};
    if (displayMatrix) {
        av_display_rotation_set(reinterpret_cast<int32_t*>(displayMatrix->data), angle);
        displayMatrix_ = std::move(displayMatrix);
    }
}

bool
VideoInput::setup()
{
    if (not attach(sink_.get())) {
        JAMI_ERR("attach sink failed");
        return false;
    }

    if (!sink_->start())
        JAMI_ERR("start sink failed");

    JAMI_DBG("VideoInput ready to capture");

    return true;
}

void
VideoInput::process()
{
    if (playingFile_) {
        if (paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return;
        }
        decoder_->emitFrame(false);
        return;
    }
    if (switchPending_)
        createDecoder();

    if (not captureFrame()) {
        loop_.stop();
        return;
    }
}

void
VideoInput::setSeekTime(int64_t time)
{
    if (decoder_) {
        decoder_->setSeekTime(time);
    }
}

void
VideoInput::cleanup()
{
    deleteDecoder(); // do it first to let a chance to last frame to be displayed
    stopSink();
    JAMI_DBG("VideoInput closed");
}

bool
VideoInput::captureFrame()
{
    // Return true if capture could continue, false if must be stop
    if (not decoder_)
        return false;

    switch (decoder_->decode()) {
    case MediaDemuxer::Status::EndOfFile:
        createDecoder();
        return static_cast<bool>(decoder_);
    case MediaDemuxer::Status::ReadError:
        JAMI_ERR() << "Failed to decode frame";
        return false;
    default:
        return true;
    }
}
void
VideoInput::flushBuffers()
{
    if (decoder_) {
        decoder_->flushBuffers();
    }
}

void
VideoInput::configureFilePlayback(const std::string&,
                                  std::shared_ptr<MediaDemuxer>& demuxer,
                                  int index)
{
        deleteDecoder();
    clearOptions();

    auto decoder = std::make_unique<MediaDecoder>(demuxer,
                                                  index,
                                                  [this](std::shared_ptr<MediaFrame>&& frame) {
                                                      publishFrame(
                                                          std::static_pointer_cast<VideoFrame>(
                                                              frame));
                                                  });
    decoder->setInterruptCallback(
        [](void* data) -> int { return not static_cast<VideoInput*>(data)->isCapturing(); }, this);
    decoder->emulateRate();

    decoder_ = std::move(decoder);
    playingFile_ = true;
    loop_.start();

    /* Signal the client about readable sink */
    sink_->setFrameSize(decoder_->getWidth(), decoder_->getHeight());
    decOpts_.width = ((decoder_->getWidth() >> 3) << 3);
    decOpts_.height = ((decoder_->getHeight() >> 3) << 3);
    decOpts_.framerate = decoder_->getFps();
    AVPixelFormat fmt = decoder_->getPixelFormat();
    if (fmt != AV_PIX_FMT_NONE) {
        decOpts_.pixel_format = av_get_pix_fmt_name(fmt);
    } else {
        JAMI_WARN("Could not determine pixel format, using default");
        decOpts_.pixel_format = av_get_pix_fmt_name(AV_PIX_FMT_YUV420P);
    }

    if (onSuccessfulSetup_)
        onSuccessfulSetup_(MEDIA_VIDEO, 0);
    foundDecOpts(decOpts_);
    futureDecOpts_ = foundDecOpts_.get_future().share();
}

void
VideoInput::setRecorderCallback(
    const std::function<void(const MediaStream& ms)>& cb)
{
    recorderCallback_ = cb;
    if (decoder_)
        decoder_->setContextCallback([this]() {
            if (recorderCallback_)
                recorderCallback_(getInfo());
        });
}

void
VideoInput::createDecoder()
{
    deleteDecoder();

    switchPending_ = false;

    if (decOpts_.input.empty()) {
        foundDecOpts(decOpts_);
        return;
    }

    auto decoder = std::make_unique<MediaDecoder>(
        [this](const std::shared_ptr<MediaFrame>& frame) mutable {
            publishFrame(std::static_pointer_cast<VideoFrame>(frame));
        });

    if (emulateRate_)
        decoder->emulateRate();

    decoder->setInterruptCallback(
        [](void* data) -> int { return not static_cast<VideoInput*>(data)->isCapturing(); }, this);

    bool ready = false, restartSink = false;
    if ((decOpts_.format == "x11grab" || decOpts_.format == "dxgigrab") && !decOpts_.is_area) {
        decOpts_.width = 0;
        decOpts_.height = 0;
    }
    while (!ready && !isStopped_) {
        // Retry to open the video till the input is opened
        auto ret = decoder->openInput(decOpts_);
        ready = ret >= 0;
        if (ret < 0 && -ret != EBUSY) {
            JAMI_ERR("Could not open input \"%s\" with status %i", decOpts_.input.c_str(), ret);
            foundDecOpts(decOpts_);
            return;
        } else if (-ret == EBUSY) {
            // If the device is busy, this means that it can be used by another call.
            // If this is the case, cleanup() can occurs and this will erase shmPath_
            // So, be sure to regenerate a correct shmPath for clients.
            restartSink = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (isStopped_)
        return;

    if (restartSink && !isStopped_) {
        sink_->start();
    }

    /* Data available, finish the decoding */
    if (decoder->setupVideo() < 0) {
        JAMI_ERR("decoder IO startup failed");
        foundDecOpts(decOpts_);
        return;
    }

    auto ret = decoder->decode(); // Populate AVCodecContext fields
    if (ret == MediaDemuxer::Status::ReadError) {
        JAMI_INFO() << "Decoder error";
        return;
    }

    decOpts_.width = ((decoder->getWidth() >> 3) << 3);
    decOpts_.height = ((decoder->getHeight() >> 3) << 3);
    decOpts_.framerate = decoder->getFps();
    AVPixelFormat fmt = decoder->getPixelFormat();
    if (fmt != AV_PIX_FMT_NONE) {
        decOpts_.pixel_format = av_get_pix_fmt_name(fmt);
    } else {
        JAMI_WARN("Could not determine pixel format, using default");
        decOpts_.pixel_format = av_get_pix_fmt_name(AV_PIX_FMT_YUV420P);
    }

    JAMI_DBG("created decoder with video params : size=%dX%d, fps=%lf pix=%s",
             decOpts_.width,
             decOpts_.height,
             decOpts_.framerate.real(),
             decOpts_.pixel_format.c_str());
    if (onSuccessfulSetup_)
        onSuccessfulSetup_(MEDIA_VIDEO, 0);

    decoder_ = std::move(decoder);

    foundDecOpts(decOpts_);

    /* Signal the client about readable sink */
    sink_->setFrameSize(decoder_->getWidth(), decoder_->getHeight());

    decoder_->setContextCallback([this]() {
        if (recorderCallback_)
            recorderCallback_(getInfo());
    });
}

void
VideoInput::deleteDecoder()
{
    if (not decoder_)
        return;
    flushFrames();
    decoder_.reset();
}

void
VideoInput::stopInput()
{
    clearOptions();
    loop_.stop();
}

void
VideoInput::clearOptions()
{
    decOpts_ = {};
    emulateRate_ = false;
}

bool
VideoInput::isCapturing() const noexcept
{
    if (videoManagedByClient()) {
        return capturing_;
    }
    return loop_.isRunning();
}

bool
VideoInput::initCamera(const std::string& device)
{
    decOpts_ = jami::getVideoDeviceMonitor().getDeviceParams(device);
    return true;
}

static constexpr unsigned
round2pow(unsigned i, unsigned n)
{
    return (i >> n) << n;
}

bool
VideoInput::initX11(const std::string& display)
{
    // Patterns
    // full screen sharing : :1+0,0 2560x1440 - SCREEN 1, POSITION 0X0, RESOLUTION 2560X1440
    // area sharing : :1+882,211 1532x779 - SCREEN 1, POSITION 882x211, RESOLUTION 1532x779
    // window sharing : :+1,0 0x0 window-id:0x0340021e - POSITION 0X0
    size_t space = display.find(' ');
    std::string windowIdStr = "window-id:";
    size_t winIdPos = display.find(windowIdStr);

    DeviceParams p = jami::getVideoDeviceMonitor().getDeviceParams(DEVICE_DESKTOP);
    if (winIdPos != std::string::npos) {
        p.window_id = display.substr(winIdPos + windowIdStr.size()); // "0x0340021e";
        p.is_area = 0;
    }
    if (space != std::string::npos) {
        p.input = display.substr(1, space);
        if (p.window_id.empty()) {
            p.input = display.substr(0, space);
            auto splits = jami::split_string_to_unsigned(display.substr(space + 1), 'x');
            // round to 8 pixel block
            p.width = round2pow(splits[0], 3);
            p.height = round2pow(splits[1], 3);
            p.is_area = 1;
        }
    } else {
        p.input = display;
        p.width = default_grab_width;
        p.height = default_grab_height;
        p.is_area = 1;
    }

    auto dec = std::make_unique<MediaDecoder>();
    if (dec->openInput(p) < 0 || dec->setupVideo() < 0)
        return initCamera(jami::getVideoDeviceMonitor().getDefaultDevice());

    clearOptions();
    decOpts_ = p;
    decOpts_.width = round2pow(dec->getStream().width, 3);
    decOpts_.height = round2pow(dec->getStream().height, 3);

    return true;
}

bool
VideoInput::initAVFoundation(const std::string& display)
{
    size_t space = display.find(' ');

    clearOptions();
    decOpts_.format = "avfoundation";
    decOpts_.pixel_format = "nv12";
    decOpts_.name = "Capture screen 0";
    decOpts_.input = "Capture screen 0";
    decOpts_.framerate = jami::getVideoDeviceMonitor().getDeviceParams(DEVICE_DESKTOP).framerate;

    if (space != std::string::npos) {
        std::istringstream iss(display.substr(space + 1));
        char sep;
        unsigned w, h;
        iss >> w >> sep >> h;
        decOpts_.width = round2pow(w, 3);
        decOpts_.height = round2pow(h, 3);
    } else {
        decOpts_.width = default_grab_width;
        decOpts_.height = default_grab_height;
    }
    return true;
}

#ifdef WIN32
bool
VideoInput::initWindowsGrab(const std::string& display)
{
    // Patterns
    // full screen sharing : :1+0,0 2560x1440 - SCREEN 1, POSITION 0X0, RESOLUTION 2560X1440
    // area sharing : :1+882,211 1532x779 - SCREEN 1, POSITION 882x211, RESOLUTION 1532x779
    // window sharing : :+1,0 0x0 window-id:HANDLE - POSITION 0X0
    size_t space = display.find(' ');
    std::string windowIdStr = "window-id:";
    size_t winHandlePos = display.find(windowIdStr);

    DeviceParams p = jami::getVideoDeviceMonitor().getDeviceParams(DEVICE_DESKTOP);
    if (winHandlePos != std::string::npos) {
        p.input = display.substr(winHandlePos + windowIdStr.size()); // "HANDLE";
        p.name  = display.substr(winHandlePos + windowIdStr.size()); // "HANDLE";
        p.is_area = 0;
    } else {
        p.input = display.substr(1);
        p.name = display.substr(1);
        p.is_area = 1;
        if (space != std::string::npos) {
            auto splits = jami::split_string_to_unsigned(display.substr(space + 1), 'x');
            if (splits.size() != 2)
                return false;

            // round to 8 pixel block
            p.width = splits[0];
            p.height = splits[1];

            size_t plus = display.find('+');
            auto position = display.substr(plus + 1, space - plus - 1);
            splits = jami::split_string_to_unsigned(position, ',');
            if (splits.size() != 2)
                return false;
            p.offset_x = splits[0];
            p.offset_y = splits[1];
        } else {
            p.width = default_grab_width;
            p.height = default_grab_height;
        }
    }

    auto dec = std::make_unique<MediaDecoder>();
    if (dec->openInput(p) < 0 || dec->setupVideo() < 0)
        return initCamera(jami::getVideoDeviceMonitor().getDefaultDevice());

    clearOptions();
    decOpts_ = p;
    decOpts_.width = dec->getStream().width;
    decOpts_.height = dec->getStream().height;

    return true;
}
#endif

bool
VideoInput::initFile(std::string path)
{
        size_t dot = path.find_last_of('.');
    std::string ext = dot == std::string::npos ? "" : path.substr(dot + 1);

    /* File exists? */
    if (access(path.c_str(), R_OK) != 0) {
        JAMI_ERR("file '%s' unavailable\n", path.c_str());
        return false;
    }

    // check if file has video, fall back to default device if none
    // FIXME the way this is done is hackish, but it can't be done in createDecoder because that
    // would break the promise returned in switchInput
    DeviceParams p;
    p.input = path;
    p.name = path;
    auto dec = std::make_unique<MediaDecoder>();
    if (dec->openInput(p) < 0 || dec->setupVideo() < 0) {
        return initCamera(jami::getVideoDeviceMonitor().getDefaultDevice());
    }

    clearOptions();
    emulateRate_ = true;
    decOpts_.input = path;
    decOpts_.name = path;
    decOpts_.loop = "1";

    // Force 1fps for static image
    if (ext == "jpeg" || ext == "jpg" || ext == "png") {
        decOpts_.format = "image2";
        decOpts_.framerate = 1;
    } else {
        JAMI_WARN("Guessing file type for %s", path.c_str());
    }

    return false;
}

void
VideoInput::restart()
{
    if (loop_.isStopping())
        switchInput(currentResource_);
}

std::shared_future<DeviceParams>
VideoInput::switchInput(const std::string& resource)
{
    JAMI_DBG("MRL: '%s'", resource.c_str());

    if (switchPending_.exchange(true)) {
        JAMI_ERR("Video switch already requested");
        return {};
    }

    currentResource_ = resource;
    decOptsFound_ = false;

    std::promise<DeviceParams> p;
    foundDecOpts_.swap(p);

    // Switch off video input?
    if (resource.empty()) {
        clearOptions();
        futureDecOpts_ = foundDecOpts_.get_future();
        startLoop();
        return futureDecOpts_;
    }

    // Supported MRL schemes
    static const std::string sep = libjami::Media::VideoProtocolPrefix::SEPARATOR;

    const auto pos = resource.find(sep);
    if (pos == std::string::npos)
        return {};

    const auto prefix = resource.substr(0, pos);
    if ((pos + sep.size()) >= resource.size())
        return {};

    const auto suffix = resource.substr(pos + sep.size());

    bool ready = false;

    if (prefix == libjami::Media::VideoProtocolPrefix::CAMERA) {
        /* Video4Linux2 */
        ready = initCamera(suffix);
    } else if (prefix == libjami::Media::VideoProtocolPrefix::DISPLAY) {
        /* X11 display name */
#ifdef __APPLE__
        ready = initAVFoundation(suffix);
#elif defined(WIN32)
        ready = initWindowsGrab(suffix);
#else
        ready = initX11(suffix);
#endif
    } else if (prefix == libjami::Media::VideoProtocolPrefix::FILE) {
        /* Pathname */
        ready = initFile(suffix);
    }

    if (ready) {
        foundDecOpts(decOpts_);
    }
    futureDecOpts_ = foundDecOpts_.get_future().share();
    startLoop();
    return futureDecOpts_;
}

MediaStream
VideoInput::getInfo() const
{
    if (!videoManagedByClient()) {
        if (decoder_)
            return decoder_->getStream("v:local");
    }
    auto opts = futureDecOpts_.get();
    rational<int> fr(opts.framerate.numerator(), opts.framerate.denominator());
    return MediaStream("v:local",
                       av_get_pix_fmt(opts.pixel_format.c_str()),
                       1 / fr,
                       opts.width,
                       opts.height,
                       0,
                       fr);
}

void
VideoInput::foundDecOpts(const DeviceParams& params)
{
    if (not decOptsFound_) {
        decOptsFound_ = true;
        foundDecOpts_.set_value(params);
    }
}

void
VideoInput::setSink(const std::string& sinkId)
{
    sink_ = Manager::instance().createSinkClient(sinkId);
}

void
VideoInput::setFrameSize(const int width, const int height)
{
    /* Signal the client about readable sink */
    sink_->setFrameSize(width, height);
}

void
VideoInput::setupSink()
{
    setup();
}

void
VideoInput::stopSink()
{
    detach(sink_.get());
    sink_->stop();
}

void
VideoInput::updateStartTime(int64_t startTime)
{
    if (decoder_) {
        decoder_->updateStartTime(startTime);
    }
}

} // namespace video
} // namespace jami
