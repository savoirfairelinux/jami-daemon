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

namespace ring {
class MediaRecorder;
}

namespace ring { namespace audio {

class AudioInput
{
public:
    AudioInput(bool muteState);
    ~AudioInput();

    bool isCapturing();
    bool captureFrame();

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

    void setMuted(bool isMuted);
    void initRecorder(const std::shared_ptr<MediaRecorder>& rec);

private:
    std::weak_ptr<MediaRecorder> recorder_;
    bool recordingStarted_{false};

    using seconds = std::chrono::duration<double, std::ratio<1>>;
    const seconds secondsPerPacket_ {0.02}; // 20 ms

    ThreadLoop loop_;
    void process();
    void cleanup();
};

}} // namespace ring::audio
