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

#include "audiobuffer.h"
#include "audiorecorder.h"
#include "noncopyable.h"

#include <atomic>
#include <memory>
#include <string>
#include <cstdlib>

class SndfileHandle;

namespace ring {

class AudioRecord {
    public:
        AudioRecord();
        ~AudioRecord();

        void setSndFormat(AudioFormat format);
        void setRecordingOptions(AudioFormat format, const std::string &path);

        /**
         * Init recording file path
         */
        void initFilename(const std::string &peerNumber);

        /**
         * Return the filepath of the recording
         */
        std::string getFilename() const;

        /**
         * Check if no otehr file is opened, then create a new one
         * @param filename A string containing teh file (with/without extension)
         * @param type     The sound file format (FILE_RAW, FILE_WAVE)
         * @param format   Internal sound format (INT16 / INT32)
         * @return bool    True if file was opened
         */
        bool openFile();

        /**
         * Close the opend recording file. If wave: cout the number of byte
         */
        void closeFile();

        /**
         * Check if a file is already opened
         */
        bool isOpenFile() const noexcept;

        /**
         * Check if a file already exists
         */
        bool fileExists() const;

        /**
         * Check recording state
         */
        bool isRecording() const;

        /**
         * Toggle recording state
         */
        bool toggleRecording();

        /**
         * Stop recording flag
         */
        void stopRecording() const noexcept;

        /**
         * Record a chunk of data in an openend file
         * @param buffer  The data chunk to be recorded
         * @param nSamples Number of samples (number of bytes) to be recorded
         */
        void recData(AudioBuffer& buffer);

        std::string getRecorderID() const {
            return recorder_.getRecorderID();
        }

    private:
        NON_COPYABLE(AudioRecord);

        /**
         * Open an existing raw file, used when the call is set on hold
         */
        bool openExistingRawFile();

        /**
         * Open an existing wav file, used when the call is set on hold
         */
        bool openExistingWavFile();

        /**
         * Compute the number of byte recorded and close the file
         */
        void closeWavFile();

        /**
         * Pointer to the recorded file
         */
        std::shared_ptr<SndfileHandle> fileHandle_;

        /**
         * Number of channels
         */
        AudioFormat sndFormat_;

        /**
         * Recording flage
         */
        mutable std::atomic<bool> recordingEnabled_ {false};

        /**
         * Filename for this recording
         */
        std::string filename_;

        /**
         * Path for this recording
         */
        std::string savePath_;

        /**
         * Audio recording thread
         */
        AudioRecorder recorder_;
};

} // namespace ring
