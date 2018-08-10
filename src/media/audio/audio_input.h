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

#ifndef __AUDIO_INPUT_H__
#define __AUDIO_INPUT_H__

namespace ring {
class MediaEncoder;
class MediaRecorder;
}

namespace ring { namespace audio {

class AudioInput
{
public:
    AudioInput();
    ~AudioInput();

    void process();
    bool isCapturing();
    bool captureFrame();

    void createEncoder();
    void deleteEncoder();

    std::shared_future<DeviceParams> switchInput(const std::string& resource);

    bool initFile(std::string path);
    void initRecorder(const std::shared_ptr<MediaRecorder>& rec);

private:
    std::unique_ptr<MediaEncoder> encoder_;
    std::weak_ptr<MediaRecorder> recorder_;
    bool recordingStarted_{false};
};

}} // namespace ring::audio

#endif // __AUDIO_INPUT_H__
