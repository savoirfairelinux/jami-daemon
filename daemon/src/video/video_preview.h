/*
 *  Copyright (C) 2011-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#ifndef __VIDEO_PREVIEW_H__
#define __VIDEO_PREVIEW_H__

#include "noncopyable.h"
#include "shm_sink.h"
#include "video_provider.h"
#include "video_scaler.h"
#include "video_decoder.h"
#include "video_mixer.h"
#include "sflthread.h"

#include <string>
#include <map>


namespace sfl_video {
using std::string;

class VideoPreview : public VideoProvider, public VideoSource, public SFLThread
{
public:
    VideoPreview(const std::map<string, string> &args);
    ~VideoPreview();
    int getWidth() const;
    int getHeight() const;
    VideoFrame *lockFrame();
    void unlockFrame();
    void waitFrame();
    void setMixer(VideoMixer* mixer);

    std::shared_ptr<VideoFrame> waitNewFrame();
    std::shared_ptr<VideoFrame> obtainLastFrame();

protected:
    // threading
    bool setup();
    void process();
    void cleanup();

private:
    NON_COPYABLE(VideoPreview);

    std::string id_;
    std::map<string, string> args_;
    VideoDecoder *decoder_;
    SHMSink sink_;
    size_t bufferSize_;
    int previewWidth_;
    int previewHeight_;
    VideoScaler scaler_;
    VideoFrame frame_;
    VideoMixer* mixer_;

    static int interruptCb(void *ctx);
    void fillBuffer(void *data);
    bool captureFrame();
    void renderFrame();
};

}

#endif // __VIDEO_PREVIEW_H__
