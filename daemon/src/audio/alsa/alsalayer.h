/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#ifndef _ALSA_LAYER_H
#define _ALSA_LAYER_H

#include "audio/audiolayer.h"
#include "noncopyable.h"
#include <alsa/asoundlib.h>

class RingBuffer;
class ManagerImpl;
class AlsaThread;

/**
 * @file  AlsaLayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware.
 */

/** Associate a sound card index to its string description */
typedef std::pair<int , std::string> HwIDPair;

class AlsaLayer : public AudioLayer {
    public:
        /**
         * Constructor
         */
        AlsaLayer();

        /**
         * Destructor
         */
        ~AlsaLayer();

        /**
         * Start the capture stream and prepare the playback stream.
         * The playback starts accordingly to its threshold
         * ALSA Library API
         */
        virtual void startStream();

        /**
         * Stop the playback and capture streams.
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         * ALSA Library API
         */
        virtual void stopStream();

        /**
         * Concatenate two strings. Used to build a valid pcm device name.
         * @param plugin the alsa PCM name
         * @param card the sound card number
         * @return std::string the concatenated string
         */
        std::string buildDeviceTopo(const std::string &plugin, int card);

        /**
         * Scan the sound card available on the system
         * @param stream To indicate whether we are looking for capture devices or playback devices
         *		   SFL_PCM_CAPTURE
         *		   SFL_PCM_PLAYBACK
         *		   SFL_PCM_BOTH
         * @return std::vector<std::string> The vector containing the string description of the card
         */
        virtual std::vector<std::string> getAudioDeviceList(AudioStreamDirection dir) const;

        /**
         * Returns a map of audio device hardware description and index
         */
        std::vector<HwIDPair> getAudioDeviceIndexMap(AudioStreamDirection dir) const;

        /**
         * Check if the given index corresponds to an existing sound card and supports the specified streaming mode
         * @param card   An index
         * @param stream  The stream mode
         *		  SFL_PCM_CAPTURE
         *		  SFL_PCM_PLAYBACK
         *		  SFL_PCM_BOTH
         * @return bool True if it exists and supports the mode
         *		    false otherwise
         */
        static bool soundCardIndexExists(int card, int stream);

        /**
         * An index is associated with its string description
         * @param description The string description
         * @return	int	  Its index
         */
        int getAudioDeviceIndex(const std::string &description) const;

        void playback(int maxSamples);
        void capture();

        void audioCallback();

        /**
         * Get the index of the audio card for capture
         * @return int The index of the card used for capture
         *                     0 for the first available card on the system, 1 ...
         */
        int getIndexCapture() const {
            return indexIn_;
        }

        /**
         * Get the index of the audio card for playback
         * @return int The index of the card used for playback
         *                     0 for the first available card on the system, 1 ...
         */
        int getIndexPlayback() const {
            return indexOut_;
        }

        /**
         * Get the index of the audio card for ringtone (could be differnet from playback)
         * @return int The index of the card used for ringtone
         *                 0 for the first available card on the system, 1 ...
         */
        int getIndexRingtone() const {
            return indexRing_;
        }

    private:
        friend class AlsaThread;

        /**
         * Calls snd_pcm_open and retries if device is busy, since dmix plugin
         * will often hold on to a device temporarily after it has been opened
         * and closed.
         */
        bool openDevice(snd_pcm_t **pcm, const std::string &dev, snd_pcm_stream_t stream);

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

        NON_COPYABLE(AlsaLayer);

        /**
         * Drop the pending frames and close the capture device
         * ALSA Library API
         */
        void closeCaptureStream();
        void stopCaptureStream();
        void startCaptureStream();
        void prepareCaptureStream();

        void closePlaybackStream();
        void stopPlaybackStream();
        void startPlaybackStream();
        void preparePlaybackStream();

        bool alsa_set_params(snd_pcm_t *pcm_handle);

        /**
         * Copy a data buffer in the internal ring buffer
         * ALSA Library API
         * @param buffer The data to be copied
         * @param length The size of the buffer
         */
        void write(void* buffer, int length, snd_pcm_t *handle);

        /**
         * Read data from the internal ring buffer
         * ALSA Library API
         * @param buffer  The buffer to stock the read data
         * @param toCopy  The number of bytes to get
         * @return int The number of frames actually read
         */
        int read(void* buffer, int toCopy);

        /**
         * Handles to manipulate playback stream
         * ALSA Library API
         */
        snd_pcm_t* playbackHandle_;

        /**
         * Handles to manipulate ringtone stream
         *
         */
        snd_pcm_t *ringtoneHandle_;

        /**
         * Handles to manipulate capture stream
         * ALSA Library API
         */
        snd_pcm_t* captureHandle_;

        /**
         * name of the alsa audio plugin used
         */
        std::string audioPlugin_;

        /** Vector to manage all soundcard index - description association of the system */
        // std::vector<HwIDPair> IDSoundCards_;

        bool is_playback_prepared_;
        bool is_capture_prepared_;
        bool is_playback_running_;
        bool is_capture_running_;
        bool is_playback_open_;
        bool is_capture_open_;

        AlsaThread *audioThread_;
};

#endif // _ALSA_LAYER_H_
