/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#pragma once

#include <future>
#include "audio/audiobuffer.h"
#include "media/media_recorder.h"
#include "audio/resampler.h"

namespace ring {
class MediaRecorder;
}

namespace ring {

class AudioInput
{
public:
    AudioInput(const std::string& id, const std::string& proc, AudioFormat target, bool loop);
    ~AudioInput();

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

    AVFrame* getNextFrame();

    void setMuted(bool isMuted);
    void initRecorder(const std::shared_ptr<MediaRecorder>& rec);

private:
    std::weak_ptr<MediaRecorder> recorder_;
    std::unique_ptr<Resampler> resampler_;
    uint64_t sent_samples = 0;

    std::string id_;
    std::string proc_;
    AudioBuffer micData_;
    bool muteState_ = false;
    AudioFormat targetFormat_;

    const std::chrono::milliseconds msPerPacket_ {20};

    ThreadLoop loop_;
    void process();
    void cleanup();
};

} // namespace ring
