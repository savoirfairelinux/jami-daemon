/*
 *  Copyright (C) 2018-2019 Savoir-faire Linux Inc.
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

#include "audio/audio_input.h"
#include "recordable.h"
#include "video/video_input.h"

namespace jami {

/*
 * @file localrecorder.h
 * @brief Class for recording messages locally
 */

/*
 * The LocalRecorder class exposes the Recordable interface for
 * recording messages locally.
 */

class LocalRecorder : public Recordable {
    public:

        LocalRecorder(const bool& audioOnly);
        ~LocalRecorder();

        /**
         * Start local recording. Return true if recording was successfully
         * started, false otherwise.
         */
        bool startRecording();

        /**
         * Stops recording.
         */
        void stopRecording();

        /**
         * Set recording path
         */
        void setPath(const std::string& path);

    private:
        std::string path_;

        // media inputs
        std::shared_ptr<jami::video::VideoInput> videoInput_;
        std::shared_ptr<jami::AudioInput> audioInput_;
};

} // namespace jami
