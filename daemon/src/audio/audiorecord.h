/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef _AUDIO_RECORD_H
#define _AUDIO_RECORD_H

#include "audiobuffer.h"
#include "sfl_types.h"
#include "noncopyable.h"

#include <memory>
#include <string>
#include <cstdlib>

class SndfileHandle;

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
        bool isOpenFile() const;

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
        void stopRecording();

        /**
         * Record a chunk of data in an openend file
         * @param buffer  The data chunk to be recorded
         * @param nSamples Number of samples (number of bytes) to be recorded
         */
        void recData(AudioBuffer& buffer);

    protected:

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
        SndfileHandle *fileHandle_;

        /**
         * Number of channels
         */
        AudioFormat sndFormat_;

        /**
         * Recording flage
         */
        bool recordingEnabled_;

        /**
         * Filename for this recording
         */
        std::string filename_;

        /**
         * Path for this recording
         */
        std::string savePath_;

    private:
        NON_COPYABLE(AudioRecord);
};

#endif // _AUDIO_RECORD_H
