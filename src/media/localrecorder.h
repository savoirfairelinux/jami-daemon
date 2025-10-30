/*
 *  Copyright (C) 2004-2025 Savoir-faire Linux Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "audio/audio_input.h"
#include "recordable.h"
#ifdef ENABLE_VIDEO
#include "video/video_input.h"
#endif

namespace jami {

/*
 * @file localrecorder.h
 * @brief Class for recording messages locally
 */

/*
 * The LocalRecorder class exposes the Recordable interface for
 * recording messages locally.
 */

class LocalRecorder : public Recordable
{
public:
    LocalRecorder(const std::string& inputUri);
    ~LocalRecorder();

    /**
     * Set recording path
     */
    void setPath(const std::string& path);

    /**
     * Start local recording. Return true if recording was successfully
     * started, false otherwise.
     */
    bool start();

    /**
     * Stops recording.
     */
    void stopRecording() override;

private:
    std::string path_;
    std::string inputUri_;

    // media inputs
#ifdef ENABLE_VIDEO
    std::shared_ptr<jami::video::VideoInput> videoInput_;
#endif
    std::shared_ptr<jami::AudioInput> audioInput_;
};

} // namespace jami
