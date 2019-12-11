/*
 *  Copyright (C) 2017-2019 Savoir-faire Linux Inc.
 *
 *  Author: Kateryna Kostiuk <kateryna.kostiuk@savoirfairelinux.com>
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

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "audio/audio_input.h"
#include "video/video_input.h"

namespace jami {

class MediaPlayer {
    public:
        MediaPlayer(const std::string& path);
        ~MediaPlayer();

        void pausePlayer(bool pause);
        void close();
        const std::string& getSinkId();
    void muteAudio(bool mute);

    private:
        std::string path_;

        // media inputs
        std::shared_ptr<jami::video::VideoInput> videoInput_;
        std::shared_ptr<jami::AudioInput> audioInput_;
        std::string playerId_;
        int64_t startTime_;
        int64_t lastPausedTime_;
        int64_t pauseInterval_;
    MediaStream audioStream_;
    MediaStream videoStream_;
    bool audioFinished_ = false;
    bool videoFinished_ = false;
    void playFileFromBegining();
    bool hasVideo = true;
    void initMediaInputs();
};
} // namespace jami

