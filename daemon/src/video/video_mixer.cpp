/*
 *  Copyright (C) 2013 Savoir-Faire Linux Inc.
 *
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
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

#include "video_mixer.h"
#include "check.h"

namespace sfl_video {

VideoMixer::VideoMixer() :
    VideoSource::VideoSource()
    , thread_()
    , threadRunning_(false)
    , updateMutex_()
    , updateCondition_()
    , updated_(false)
    , sourceList_()
    , width(0)
    , height(0)
{
    pthread_mutex_init(&updateMutex_, NULL);
    pthread_cond_init(&updateCondition_, NULL);
    pthread_create(&thread_, NULL, &runCallback, this);
}

VideoMixer::~VideoMixer()
{
    set_false_atomic(&threadRunning_);
    if (thread_)
        pthread_join(thread_, NULL);
    pthread_cond_destroy(&updateCondition_);
    pthread_mutex_destroy(&updateMutex_);

}

void VideoMixer::addVideoSource(VideoSource &source)
{

}

void VideoMixer::removeVideoSource(VideoSource &source)
{

}

int VideoMixer::interruptCb(void *ctx)
{
    VideoMixer *context = static_cast<VideoMixer*>(ctx);
    return not context->threadRunning_;
}

void *VideoMixer::runCallback(void *data)
{
    VideoMixer *context = static_cast<VideoMixer*>(data);
    context->run();
    return NULL;
}

void VideoMixer::run()
{
    set_true_atomic(&threadRunning_);
    setup();

    while (threadRunning_) {
        waitForUpdate();
        render();
    }
}

void VideoMixer::setup()
{

}

void VideoMixer::render()
{

}

void VideoMixer::updated()
{
    pthread_mutex_lock(&updateMutex_);
    updated_ = true;
    pthread_cond_signal(&updateCondition_);
    pthread_mutex_unlock(&updateMutex_);
}

void VideoMixer::waitForUpdate()
{
    pthread_mutex_lock(&updateMutex_);
    if (!updated_)
        pthread_cond_wait(&updateCondition_, &updateMutex_);
    pthread_mutex_unlock(&updateMutex_);
}

int VideoMixer::getWidth() const { return 0; }
int VideoMixer::getHeight() const { return 0; }

std::shared_ptr<VideoFrame> VideoMixer::waitNewFrame() { return nullptr; }
std::shared_ptr<VideoFrame> VideoMixer::obtainLastFrame() { return nullptr; }

} // end namspace sfl_video
