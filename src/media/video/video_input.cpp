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
#if !VIDEO_CLIENT_INPUT
    , sink_ {Manager::instance().createSinkClient("local")}
    , loop_(std::bind(&VideoInput::setup, this),
            std::bind(&VideoInput::process, this),
            std::bind(&VideoInput::cleanup, this))
#endif
{}

VideoInput::~VideoInput()
{
#if VIDEO_CLIENT_INPUT
    emitSignal<DRing::VideoSignal::StopCapture>();
    capturing_ = false;
#else
    loop_.join();
#endif
}

void
VideoInput::startLoop()
{
#if VIDEO_CLIENT_INPUT
    switchDevice();
#else
    if (!loop_.isRunning())
        loop_.start();
#endif
}

#if VIDEO_CLIENT_INPUT
void
VideoInput::switchDevice()
{
    if (switchPending_.exchange(false)) {
        JAMI_DBG("Switching input to '%s'", decOpts_.input.c_str());
        if (decOpts_.input.empty()) {
            capturing_ = false;
            return;
        }

        emitSignal<DRing::VideoSignal::StopCapture>();
        emitSignal<DRing::VideoSignal::StartCapture>(decOpts_.input);
        capturing_ = true;
    }
}

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
VideoInput::createDecoder()
{
    deleteDecoder();

    switchPending_ = false;

    if (decOpts_.input.empty()) {
        foundDecOpts(decOpts_);
        return;
    }

    auto decoder = std::make_unique<MediaDecoder>([this](const std::shared_ptr<MediaFrame>& frame) mutable {
        #ifndef __ANDROID__
        //std::string path = "/home/ayounes/Projects/ring-plugins/build/x86_64-linux/simpleplugin/libsimpleplugin.so";
        //std::string path = "/home/ayounes/Projects/ring-plugins/build/x86_64-linux/mmotplugin/libmmotplugin.so";
        std::string path = "/home/ayounes/Projects/ring-plugins/build/x86_64-linux/facemarks/libfacemarks.so";
        //path = "";
        auto& psm = jami::Manager::instance().getPluginServicesManager();
        // If we have a plugin service
        if(psm) {
            // Is the frame a video frame push to the video input subject
            if (auto videoFrame = std::dynamic_pointer_cast<VideoFrame>(frame)){
                if(i == 0) {
                    // Load the plugin
                    //psm->loadPlugin(path);
                } else if(i == 200) {
                    //psm->unloadPlugin(path);
                } else if (i == 400) {
                    i = -1;
                }
                i++;
            }
        }
        #endif
        publishFrame(std::static_pointer_cast<VideoFrame>(frame));


    });

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
    if (decoder->setupVideo() < 0) {
        JAMI_ERR("decoder IO startup failed");
        foundDecOpts(decOpts_);
        return;
    }

    decoder->decode(); // Populate AVCodecContext fields

    decOpts_.width = decoder->getWidth();
    decOpts_.height = decoder->getHeight();
    decOpts_.framerate = decoder->getFps();
    AVPixelFormat fmt = decoder->getPixelFormat();
    if (fmt != AV_PIX_FMT_NONE) {
        decOpts_.pixel_format = av_get_pix_fmt_name(fmt);
    } else {
        JAMI_WARN("Could not determine pixel format, using default");
        decOpts_.pixel_format = av_get_pix_fmt_name(AV_PIX_FMT_YUV420P);
    }

    JAMI_DBG("created decoder with video params : size=%dX%d, fps=%lf pix=%s",
             decOpts_.width, decOpts_.height, decOpts_.framerate.real(),
             decOpts_.pixel_format.c_str());

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

int VideoInput::getWidth() const
{ return decoder_->getWidth(); }

int VideoInput::getHeight() const
{ return decoder_->getHeight(); }

AVPixelFormat VideoInput::getPixelFormat() const
{ return decoder_->getPixelFormat(); }

#endif

void VideoInput::clearOptions()
{
    decOpts_ = {};
    emulateRate_ = false;
}

bool
VideoInput::isCapturing() const noexcept
{
#if VIDEO_CLIENT_INPUT
    return capturing_;
#else
    return loop_.isRunning();
#endif
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
    if (dec->openInput(p) < 0 || dec->setupVideo() < 0) {
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
        futureDecOpts_  = foundDecOpts_.get_future();
        startLoop();
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
    futureDecOpts_ = foundDecOpts_.get_future().share();
    startLoop();
    return futureDecOpts_;
}

DeviceParams VideoInput::getParams() const
{ return decOpts_; }

MediaStream
VideoInput::getInfo() const
{
#if !VIDEO_CLIENT_INPUT
    if (decoder_)
        return decoder_->getStream("v:local");
#endif
    auto opts = futureDecOpts_.get();
    rational<int> fr(opts.framerate.numerator(), opts.framerate.denominator());
    return MediaStream("v:local", av_get_pix_fmt(opts.pixel_format.c_str()),
                       1 / fr, opts.width, opts.height, 0, fr);
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
