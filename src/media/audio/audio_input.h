/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#pragma once

#include <future>

#include "audio/audiobuffer.h"
#include "audio/resampler.h"
#include "media_decoder.h"
#include "media_recorder.h"

struct AVFrame;

namespace ring {

class AudioInput
{
public:
    AudioInput(const std::string& id);
    AudioInput(const std::string& id, AudioFormat target);
    ~AudioInput();

    void setTargetFormat(AudioFormat fmt) { targetFormat_ = fmt; }

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

    AVFrame* getNextFrame();

    void setMuted(bool isMuted);
    void initRecorder(const std::shared_ptr<MediaRecorder>& rec);

private:
    AVFrame* getNextFromInput();
    bool getNextFromFile(AudioFrame& frame);

    bool initFile(const std::string& path);
    bool initInput(const std::string& input);

    std::unique_ptr<MediaDecoder> decoder_;
    void createDecoder();

    std::weak_ptr<MediaRecorder> recorder_;
    std::unique_ptr<Resampler> resampler_;
    uint64_t sampleCount = 0;

    std::string id_;
    AudioBuffer micData_;
    bool muteState_ = false;
    AudioFormat targetFormat_;

    std::string currentResource_;
    std::atomic<bool> switchPending_ = {false};
    DeviceParams devOpts_;
    std::promise<DeviceParams> foundDevOpts_;
    std::shared_future<DeviceParams> futureDevOpts_;
    std::atomic_bool devOptsFound_ {false};
    void foundDevOpts(const DeviceParams& params);
};

} // namespace ring
