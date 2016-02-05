/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
 *
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

#include "audio/audiobuffer.h"

#include <string>
#include <memory>

namespace ring {

class AudioRecord;
class AudioRecorder;

class Recordable {
public:
    Recordable();
    virtual ~Recordable();

    /**
     * Return recording state (true/false)
     */
    bool isRecording() const;

    /**
     * This method must be implemented for this interface as calls and conferences
     * have different behavior.
     * Implementations must call the super method.
     */
    virtual bool toggleRecording();

    /**
     * Stop recording
     */
    void stopRecording();

    /**
     * Init the recording file name according to path specified in configuration
     */
    void initRecFilename(const std::string& filename);

    /**
     * Return the file path for this recording
     */
    virtual std::string getFilename() const;

    /**
     * Set recording sampling rate.
     */
    void setRecordingFormat(AudioFormat format);

protected:
    std::unique_ptr<AudioRecord> recAudio_;
    std::unique_ptr<AudioRecorder> recorder_;
};

} // namespace ring
