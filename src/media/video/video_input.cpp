/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
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

namespace jami { namespace video {

static constexpr unsigned default_grab_width = 640;
static constexpr unsigned default_grab_height = 480;

VideoInput::VideoInput()
    : VideoGenerator::VideoGenerator()
    , sink_ {Manager::instance().createSinkClient("local")}
    , loop_(std::bind(&VideoInput::setup, this),
            std::bind(&VideoInput::process, this),
            std::bind(&VideoInput::cleanup, this))
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    , mutex_(), frame_cv_(), buffers_()
#endif
{}

VideoInput::~VideoInput()
{
#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
    /* we need to stop the loop and notify the condition variable
     * to unblock the process loop */
    loop_.stop();
    frame_cv_.notify_one();
#endif
    loop_.join();
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
bool VideoInput::waitForBufferFull()
{
    for(auto& buffer : buffers_) {
        if (buffer.status == BUFFER_FULL)
            return true;
    }

    /* If the loop is stopped, returned true so we can quit the process loop */
    return !isCapturing();
}

void VideoInput::process()
{
    foundDecOpts(decOpts_);

    if (switchPending_.exchange(false)) {
        JAMI_DBG("Switching input to '%s'", decOpts_.input.c_str());
        if (decOpts_.input.empty()) {
            loop_.stop();
            return;
        }

        emitSignal<DRing::VideoSignal::StopCapture>();
        emitSignal<DRing::VideoSignal::StartCapture>(decOpts_.input);
    }

    std::unique_lock<std::mutex> lck(mutex_);

    frame_cv_.wait(lck, [this] { return waitForBufferFull(); });
    std::weak_ptr<VideoInput> wthis;
    // shared_from_this throws in destructor
    // assumes C++17
    try {
        wthis = shared_from_this();
    } catch (...) {
        return;
    }

    if (decOpts_.orientation != rotation_) {
        setRotation(decOpts_.orientation);
        rotation_ = decOpts_.orientation;
    }

    for (auto& buffer : buffers_) {
        if (buffer.status == BUFFER_FULL && buffer.index == publish_index_) {
            auto& frame = getNewFrame();
            AVPixelFormat format = getPixelFormat();

            if (auto displayMatrix = displayMatrix_)
                av_frame_new_side_data_from_buf(frame.pointer(), AV_FRAME_DATA_DISPLAYMATRIX, av_buffer_ref(displayMatrix.get()));

            buffer.status = BUFFER_PUBLISHED;
            frame.setFromMemory((uint8_t*)buffer.data, format, decOpts_.width, decOpts_.height,
                                [wthis](uint8_t* ptr) {
                                    if (auto sthis = wthis.lock())
                                        sthis->releaseBufferCb(ptr);
                                    else
                                        std::free(ptr);
                                });
            publish_index_++;
            lck.unlock();
            publishFrame();
            break;
        }
    }
}

void
VideoInput::setRotation(int angle)
{
    std::shared_ptr<AVBufferRef> displayMatrix {
        av_buffer_alloc(sizeof(int32_t) * 9),
        [](AVBufferRef* buf){ av_buffer_unref(&buf); }
    };
    if (displayMatrix) {
        av_display_rotation_set(reinterpret_cast<int32_t*>(displayMatrix->data), angle);
        displayMatrix_ = std::move(displayMatrix);
    }
}

void VideoInput::cleanup()
{
    emitSignal<DRing::VideoSignal::StopCapture>();

    if (detach(sink_.get()))
        sink_->stop();

    std::lock_guard<std::mutex> lck(mutex_);
    for (auto& buffer : buffers_) {
        if (buffer.status == BUFFER_AVAILABLE ||
            buffer.status == BUFFER_FULL) {
            freeOneBuffer(buffer);
        } else if (buffer.status != BUFFER_NOT_ALLOCATED) {
            JAMI_ERR("Failed to free buffer [%p]", buffer.data);
        }
    }

    setRotation(0);
}
#else

void
VideoInput::process()
{
    if (switchPending_)
        createDecoder();

    if (not captureFrame()) {
        loop_.stop();
        return;
    }
}

void
VideoInput::cleanup()
{
    deleteDecoder(); // do it first to let a chance to last frame to be displayed
    detach(sink_.get());
    sink_->stop();
    JAMI_DBG("VideoInput closed");
}

#endif
bool VideoInput::setup()
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

void VideoInput::clearOptions()
{
    decOpts_ = {};
    emulateRate_ = false;
}

bool
VideoInput::isCapturing() const noexcept
{
    return loop_.isRunning();
}

bool VideoInput::captureFrame()
{
    // Return true if capture could continue, false if must be stop

    if (not decoder_)
        return false;

    auto& frame = getNewFrame();
    const auto ret = decoder_->decode(frame);
    switch (ret) {
        case MediaDecoder::Status::ReadError:
            return false;

        // try to keep decoding
        case MediaDecoder::Status::DecodeError:
            return true;

        case MediaDecoder::Status::RestartRequired:
            createDecoder();
#ifdef RING_ACCEL
            JAMI_WARN("Disabling hardware decoding due to previous failure");
            decoder_->enableAccel(false);
#endif
            return static_cast<bool>(decoder_);

        // End of streamed file
        case MediaDecoder::Status::EOFError:
            createDecoder();
            return static_cast<bool>(decoder_);

        case MediaDecoder::Status::FrameFinished:
            publishFrame();
            return true;
        // continue decoding
        case MediaDecoder::Status::Success:
        default:
            return true;
    }
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
int VideoInput::allocateOneBuffer(struct VideoFrameBuffer& b, int length)
{
    b.data = std::malloc(length);
    if (b.data) {
        b.status = BUFFER_AVAILABLE;
        b.length = length;
        JAMI_DBG("Allocated buffer [%p]", b.data);
        return 0;
    }

    JAMI_DBG("Failed to allocate memory for one buffer");
    return -ENOMEM;
}

void VideoInput::freeOneBuffer(struct VideoFrameBuffer& b)
{
    JAMI_DBG("Free buffer [%p]", b.data);
    std::free(b.data);
    b.data = nullptr;
    b.length = 0;
    b.status = BUFFER_NOT_ALLOCATED;
}

void VideoInput::releaseBufferCb(uint8_t* ptr)
{
    std::lock_guard<std::mutex> lck(mutex_);

    for(auto &buffer : buffers_) {
        if (buffer.data == ptr) {
            buffer.status = BUFFER_AVAILABLE;
            if (!isCapturing())
                freeOneBuffer(buffer);
            break;
        }
    }
}

void*
VideoInput::obtainFrame(int length)
{
    std::lock_guard<std::mutex> lck(mutex_);

    /* allocate buffers. This is done there because it's only when the Android
     * application requests a buffer that we know its size
     */
    for(auto& buffer : buffers_) {
        if (buffer.status == BUFFER_NOT_ALLOCATED) {
            allocateOneBuffer(buffer, length);
        }
    }

    /* search for an available frame */
    for(auto& buffer : buffers_) {
        if (buffer.length == static_cast<size_t>(length) && buffer.status == BUFFER_AVAILABLE) {
            buffer.status = BUFFER_CAPTURING;
            return buffer.data;
        }
    }

    JAMI_WARN("No buffer found");
    return nullptr;
}

void
VideoInput::releaseFrame(void *ptr)
{
    std::lock_guard<std::mutex> lck(mutex_);
    for(auto& buffer : buffers_) {
        if (buffer.data  == ptr) {
            if (buffer.status != BUFFER_CAPTURING)
                JAMI_ERR("Released a buffer with status %d, expected %d",
                         buffer.status, BUFFER_CAPTURING);
            if (isCapturing()) {
                buffer.status = BUFFER_FULL;
                buffer.index = capture_index_++;
                frame_cv_.notify_one();
            } else {
                freeOneBuffer(buffer);
            }
            break;
        }
    }
}
#endif

void
VideoInput::createDecoder()
{
    deleteDecoder();

    switchPending_ = false;

    if (decOpts_.input.empty()) {
        foundDecOpts(decOpts_);
        return;
    }

    auto decoder = std::unique_ptr<MediaDecoder>(new MediaDecoder());

    if (emulateRate_)
        decoder->emulateRate();

    decoder->setInterruptCallback(
        [](void* data) -> int { return not static_cast<VideoInput*>(data)->isCapturing(); },
        this);

    if (decoder->openInput(decOpts_) < 0) {
        JAMI_ERR("Could not open input \"%s\"", decOpts_.input.c_str());
        foundDecOpts(decOpts_);
        return;
    }

    /* Data available, finish the decoding */
    if (decoder->setupFromVideoData() < 0) {
        JAMI_ERR("decoder IO startup failed");
        foundDecOpts(decOpts_);
        return;
    }

    decOpts_.width = decoder->getWidth();
    decOpts_.height = decoder->getHeight();
    decOpts_.framerate = decoder->getFps();

    JAMI_DBG("created decoder with video params : size=%dX%d, fps=%lf",
             decOpts_.width, decOpts_.height, decOpts_.framerate.real());

    decoder_ = std::move(decoder);
    foundDecOpts(decOpts_);

    /* Signal the client about readable sink */
    sink_->setFrameSize(decoder_->getWidth(), decoder_->getHeight());
}

void
VideoInput::deleteDecoder()
{
    if (not decoder_)
        return;
    flushFrames();
    decoder_.reset();
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
VideoInput::initX11(std::string display)
{
    size_t space = display.find(' ');

    clearOptions();
    decOpts_.format = "x11grab";
    decOpts_.framerate = 25;

    if (space != std::string::npos) {
        std::istringstream iss(display.substr(space + 1));
        char sep;
        unsigned w, h;
        iss >> w >> sep >> h;
        // round to 8 pixel block
        decOpts_.width = round2pow(w, 3);
        decOpts_.height = round2pow(h, 3);
        decOpts_.input = display.erase(space);
    } else {
        decOpts_.input = display;
        //decOpts_.video_size = "vga";
        decOpts_.width = default_grab_width;
        decOpts_.height = default_grab_height;
    }

    return true;
}

bool
VideoInput::initAVFoundation(const std::string& display)
{
    size_t space = display.find(' ');

    clearOptions();
    decOpts_.format = "avfoundation";
    decOpts_.pixel_format = "nv12";
    decOpts_.input = "Capture screen 0";
    decOpts_.framerate = 30;

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


bool
VideoInput::initGdiGrab(const std::string& params)
{
    size_t space = params.find(' ');
    clearOptions();
    decOpts_.format = "gdigrab";
    decOpts_.input = "desktop";
    decOpts_.framerate = 30;

    if (space != std::string::npos) {
        std::istringstream iss(params.substr(space + 1));
        char sep;
        unsigned w, h;
        iss >> w >> sep >> h;
        decOpts_.width = round2pow(w, 3);
        decOpts_.height = round2pow(h, 3);

        size_t plus = params.find('+');
        std::istringstream dss(params.substr(plus + 1, space - plus));
        dss >> decOpts_.offset_x >> sep >> decOpts_.offset_y;
    } else {
        decOpts_.width = default_grab_width;
        decOpts_.height = default_grab_height;
    }

    return true;
}

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
    auto dec = std::make_unique<MediaDecoder>();
    if (dec->openInput(p) < 0 || dec->setupFromVideoData() < 0) {
        return initCamera(jami::getVideoDeviceMonitor().getDefaultDevice());
    }

    clearOptions();
    emulateRate_ = true;
    decOpts_.input = path;
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

std::shared_future<DeviceParams>
VideoInput::switchInput(const std::string& resource)
{
    if (resource == currentResource_)
        return futureDecOpts_;

    JAMI_DBG("MRL: '%s'", resource.c_str());

    if (switchPending_) {
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
        switchPending_ = true;
        if (!loop_.isRunning())
            loop_.start();
        futureDecOpts_   = foundDecOpts_.get_future();
        return futureDecOpts_;
    }

    // Supported MRL schemes
    static const std::string sep = DRing::Media::VideoProtocolPrefix::SEPARATOR;

    const auto pos = resource.find(sep);
    if (pos == std::string::npos)
        return {};

    const auto prefix = resource.substr(0, pos);
    if ((pos + sep.size()) >= resource.size())
        return {};

    const auto suffix = resource.substr(pos + sep.size());

    bool ready = false;

    if (prefix == DRing::Media::VideoProtocolPrefix::CAMERA) {
        /* Video4Linux2 */
        ready = initCamera(suffix);
    } else if (prefix == DRing::Media::VideoProtocolPrefix::DISPLAY) {
        /* X11 display name */
#ifdef __APPLE__
        ready = initAVFoundation(suffix);
#elif defined(_WIN32)
        ready = initGdiGrab(suffix);
#else
        ready = initX11(suffix);
#endif
    } else if (prefix == DRing::Media::VideoProtocolPrefix::FILE) {
        /* Pathname */
        ready = initFile(suffix);
    }

    if (ready) {
        foundDecOpts(decOpts_);
    }

    switchPending_ = true;
    if (!loop_.isRunning())
        loop_.start();
    futureDecOpts_ = foundDecOpts_.get_future().share();
    return futureDecOpts_;
}

#if defined(__ANDROID__) || defined(RING_UWP) || (defined(TARGET_OS_IOS) && TARGET_OS_IOS)
int VideoInput::getWidth() const
{ return decOpts_.width; }

int VideoInput::getHeight() const
{ return decOpts_.height; }

AVPixelFormat VideoInput::getPixelFormat() const
{
    int format;
    std::stringstream ss;
    ss << decOpts_.format;
    ss >> format;

    return (AVPixelFormat)format;
}
#else
int VideoInput::getWidth() const
{ return decoder_->getWidth(); }

int VideoInput::getHeight() const
{ return decoder_->getHeight(); }

AVPixelFormat VideoInput::getPixelFormat() const
{ return decoder_->getPixelFormat(); }
#endif

DeviceParams VideoInput::getParams() const
{ return decOpts_; }

MediaStream
VideoInput::getInfo() const
{
    return decoder_->getStream("v:local");
}

void
VideoInput::foundDecOpts(const DeviceParams& params)
{
    if (not decOptsFound_) {
        decOptsFound_ = true;
        foundDecOpts_.set_value(params);
    }
}

}} // namespace jami::video
