/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Philippe Groarke <philippe.groarke@savoirfairelinux.com>
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

#ifndef CORE_LAYER_H_
#define CORE_LAYER_H_

#include "audio/audiolayer.h"
#include "noncopyable.h"
#include <CoreFoundation/CoreFoundation.h>


/**
 * @file  CoreLayer.h
 * @brief Main OSX sound class. Manages the data transfers between the application and the hardware.
 */

class CoreAudioThread;
class RingBuffer;

class CoreLayer : public AudioLayer {
    public:
        CoreLayer(const AudioPreference &pref);
        ~CoreLayer();

        /**
         * Scan the sound card available on the system
         * @return std::vector<std::string> The vector containing the string description of the card
         */
        virtual std::vector<std::string> getCaptureDeviceList() const;
        virtual std::vector<std::string> getPlaybackDeviceList() const;

        /**
         * Get the index of the audio card for capture
         * @return int The index of the card used for capture
         */
        virtual int getIndexCapture() const {
            return indexIn_;
        }

        /**
         * Get the index of the audio card for playback
         * @return int The index of the card used for playback
         */
        virtual int getIndexPlayback() const {
            return indexOut_;
        }

        /**
         * Get the index of the audio card for ringtone (could be differnet from playback)
         * @return int The index of the card used for ringtone
         */
        virtual int getIndexRingtone() const {
            return indexRing_;
        }

        /**
         * Start the capture stream and prepare the playback stream.
         * The playback starts accordingly to its threshold
         * CoreAudio Library API
         */

        virtual void startStream();

        /**
         * Stop the playback and capture streams.
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         * CoreAudio Library API
         */
        virtual void stopStream();

    private:
        friend class CoreAudioThread;
        virtual void updatePreference(AudioPreference &pref, int index, DeviceType type);

        /**
         * Number of audio cards on which capture stream has been opened
         */
        int indexIn_;

        /**
         * Number of audio cards on which playback stream has been opened
         */
        int indexOut_;

        /**
         * Number of audio cards on which ringtone stream has been opened
         */
        int indexRing_;

        /** Non-interleaved audio buffers */
        AudioBuffer playbackBuff_;
        AudioBuffer captureBuff_;

        /** Interleaved buffer */
        std::vector<SFLAudioSample> playbackIBuff_;
        std::vector<SFLAudioSample> captureIBuff_;

        bool is_playback_prepared_;
        bool is_capture_prepared_;
        bool is_playback_running_;
        bool is_capture_running_;
        bool is_playback_open_;
        bool is_capture_open_;

        CoreAudioThread* _audioThread;
        std::shared_ptr<RingBuffer> mainRingBuffer_;
};

#endif // CORE_LAYER_H_
