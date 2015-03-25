/*
 *  Copyright (C) 2011-2015 Savoir-Faire Linux Inc.
 *
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Author: Guillaume Roguez <Guillaume.Roguez@savoirfairelinux.com>
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#ifndef __VIDEO_INPUT_H__
#define __VIDEO_INPUT_H__

#include "noncopyable.h"
#include "threadloop.h"
#include "media/media_device.h" // DeviceParams

#include <map>
#include <atomic>
#include <future>
#include <string>

namespace ring {
class MediaDecoder;
}

namespace ring { namespace video {

class SinkClient;

class VideoInput : public VideoGenerator
{
public:
    VideoInput();
    ~VideoInput();

    // as VideoGenerator
    int getWidth() const;
    int getHeight() const;
    int getPixelFormat() const;
    DeviceParams getParams() const;

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

private:
    NON_COPYABLE(VideoInput);

    std::string currentResource_;

    MediaDecoder *decoder_  = nullptr;
    std::shared_ptr<SinkClient> sink_;
    std::atomic<bool> switchPending_ = {false};

    DeviceParams decOpts_;
    std::promise<DeviceParams> foundDecOpts_;
    std::shared_future<DeviceParams> futureDecOpts_;

    std::atomic_bool decOptsFound_ {false};
    void foundDecOpts(const DeviceParams& params);

    bool emulateRate_       = false;
    ThreadLoop loop_;

    void clearOptions();

    void createDecoder();
    void deleteDecoder();

    bool initCamera(const std::string& device);
    bool initX11(std::string display);
    bool initFile(std::string path);

    // for ThreadLoop
    bool setup();
    void process();
    void cleanup();

    static int interruptCb(void *ctx);
    bool captureFrame();
};

}} // namespace ring::video

#endif // __VIDEO_INPUT_H__
