/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "video_preview.h"
#include "video_decoder.h"
#include "check.h"

#include "manager.h"
#include "client/video_controls.h"

#include <map>
#include <string>

namespace sfl_video {

using std::string;

VideoPreview::VideoPreview(const std::map<std::string, std::string> &args) :
    id_("local"),
    args_(args),
    decoder_(0),
    threadRunning_(false),
    thread_(0),
    accessMutex_(),
    sink_(),
    bufferSize_(0),
    previewWidth_(0),
    previewHeight_(0),
    scaler_(),
    frame_(),
    frameReady_(false),
    frameMutex_(),
    frameCondition_()
{
    pthread_mutex_init(&accessMutex_, NULL);
    pthread_mutex_init(&frameMutex_, NULL);
    pthread_cond_init(&frameCondition_, NULL);
    pthread_create(&thread_, NULL, &runCallback, this);
}

VideoPreview::~VideoPreview()
{
    set_false_atomic(&threadRunning_);
    string name = sink_.openedName();
    Manager::instance().getVideoControls()->stoppedDecoding(id_, name);
    if (thread_)
        pthread_join(thread_, NULL);
    pthread_mutex_destroy(&accessMutex_);
}

int VideoPreview::interruptCb(void *ctx)
{
    VideoPreview *context = static_cast<VideoPreview*>(ctx);
    return not context->threadRunning_;
}

void *VideoPreview::runCallback(void *data)
{
    VideoPreview *context = static_cast<VideoPreview*>(data);
    context->run();
    return NULL;
}

void VideoPreview::run()
{
    set_true_atomic(&threadRunning_);
    decoder_ = new VideoDecoder();
    setup();

    while (threadRunning_) {
        if (captureFrame())
            renderFrame();
    }

    delete decoder_;
}

void VideoPreview::setup()
{
    // it's a v4l device if starting with /dev/video
    static const char * const V4L_PATH = "/dev/video";

    string format_str;
    string input = args_["input"];

    if (args_["input"].find(V4L_PATH) != std::string::npos) {
        DEBUG("Using v4l2 format");
        format_str = "video4linux2";
    }
    if (!args_["framerate"].empty())
        decoder_->setOption("framerate", args_["framerate"].c_str());
    if (!args_["video_size"].empty())
        decoder_->setOption("video_size", args_["video_size"].c_str());
    if (!args_["channel"].empty())
        decoder_->setOption("channel", args_["channel"].c_str());

    decoder_->setInterruptCallback(interruptCb, this);

    EXIT_IF_FAIL(decoder_->openInput(input, format_str) >= 0,
                 "Could not open input \"%s\"", input.c_str());

    /* Data available, finish the decoding */
    EXIT_IF_FAIL(!decoder_->setupFromVideoData(),
                 "decoder IO startup failed");

    /* Preview frame size? (defaults from decoder) */
    if (!args_["width"].empty())
        previewWidth_ = atoi(args_["width"].c_str());
    else
        previewWidth_ = decoder_->getWidth();
    if (!args_["height"].empty())
        previewHeight_ = atoi(args_["height"].c_str());
    else
        previewHeight_ = decoder_->getHeight();

    /* Previewing setup */
    EXIT_IF_FAIL(sink_.start(), "Cannot start shared memory sink");

    frame_.setGeometry(previewWidth_, previewHeight_, VIDEO_PIXFMT_BGRA);
    bufferSize_ = frame_.getSize();
    EXIT_IF_FAIL(bufferSize_ > 0, "Incorrect buffer size for decoding");

    string name = sink_.openedName();
    Manager::instance().getVideoControls()->startedDecoding(id_, name,
                                                            previewWidth_,
                                                            previewHeight_);
    DEBUG("TX: shm sink started with size %d, width %d and height %d",
          bufferSize_, previewWidth_, previewHeight_);
}

bool VideoPreview::captureFrame()
{
    pthread_mutex_lock(&frameMutex_);
    frameReady_ = false;
    pthread_mutex_unlock(&frameMutex_);

    int ret = decoder_->decode();

    if (ret <= 0) {
        if (ret < 0)
            threadRunning_ = false;
        return false;
    }

    // Signal threads waiting in waitFrame()
    pthread_mutex_lock(&frameMutex_);
    frameReady_ = true;
    pthread_cond_signal(&frameCondition_);
    pthread_mutex_unlock(&frameMutex_);

    return true;
}

void VideoPreview::renderFrame()
{
    // we want our rendering code to be called by the shm_sink,
    // because it manages the shared memory synchronization
    sink_.render_callback(*this, bufferSize_);
}

// This function is called by sink
void VideoPreview::fillBuffer(void *data)
{
    frame_.setDestination(data);
    decoder_->scale(scaler_, frame_);
}

int VideoPreview::getWidth() const { return decoder_->getWidth(); }
int VideoPreview::getHeight() const { return decoder_->getHeight(); }
VideoFrame *VideoPreview::lockFrame() {return decoder_->lockFrame();}
void VideoPreview::unlockFrame() {decoder_->unlockFrame();}

void VideoPreview::waitFrame()
{
    pthread_mutex_lock(&frameMutex_);
    if (!frameReady_)
        pthread_cond_wait(&frameCondition_, &frameMutex_);
    pthread_mutex_unlock(&frameMutex_);
}

} // end namspace sfl_video
