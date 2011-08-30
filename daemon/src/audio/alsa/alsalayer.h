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
#include <alsa/asoundlib.h>

class RingBuffer;
class ManagerImpl;
class AlsaThread;

/**
 * @file  AlsaLayer.h
 * @brief Main sound class. Manages the data transfers between the application and the hardware.
 */

class AlsaLayer : public AudioLayer
{
    public:
        /**
         * Constructor
         */
        AlsaLayer ();

        /**
         * Destructor
         */
        ~AlsaLayer (void);

        /**
         * Start the capture stream and prepare the playback stream.
         * The playback starts accordingly to its threshold
         * ALSA Library API
         */
        void startStream (void);

        /**
         * Stop the playback and capture streams.
         * Drops the pending frames and put the capture and playback handles to PREPARED state
         * ALSA Library API
         */
        void stopStream (void);

        /**
         * Get data from the capture device
         * @param buffer The buffer for data
         * @param toCopy The number of bytes to get
         * @return int The number of bytes acquired ( 0 if an error occured)
         */
        int getMic (void * buffer, int toCopy);

        /**
         * Concatenate two strings. Used to build a valid pcm device name.
         * @param plugin the alsa PCM name
         * @param card the sound card number
         * @param subdevice the subdevice number
         * @return std::string the concatenated string
         */
        std::string buildDeviceTopo (const std::string &plugin, int card, int subdevice);

        /**
         * Scan the sound card available on the system
         * @param stream To indicate whether we are looking for capture devices or playback devices
         *		   SFL_PCM_CAPTURE
         *		   SFL_PCM_PLAYBACK
         *		   SFL_PCM_BOTH
         * @return std::vector<std::string> The vector containing the string description of the card
         */
        std::vector<std::string> getSoundCardsInfo (int stream);

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
        static bool soundCardIndexExist (int card , int stream);

        /**
         * An index is associated with its string description
         * @param description The string description
         * @return	int	  Its index
         */
        int soundCardGetIndex (const std::string &description);

        /**
         * Get the current audio plugin.
         * @return std::string  The name of the audio plugin
         */
        std::string getAudioPlugin (void) const {
            return audioPlugin_;
        }

        void audioCallback (void);

        /**
         * Get the index of the audio card for capture
         * @return int The index of the card used for capture
         *                     0 for the first available card on the system, 1 ...
         */
        int getIndexIn() const {
            return indexIn_;
        }
        void setIndexIn(int);

        /**
         * Get the index of the audio card for playback
         * @return int The index of the card used for playback
         *                     0 for the first available card on the system, 1 ...
         */
        int getIndexOut() const {
            return indexOut_;
        }
        void setIndexOut(int);

        /**
		 * Get the index of the audio card for ringtone (could be differnet from playback)
		 * @return int The index of the card used for ringtone
		 *                 0 for the first available card on the system, 1 ...
		 */
        int getIndexRing() const {
            return indexRing_;
        }
        void setIndexRing(int);

        void setPlugin(const std::string &plugin);

    private:


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

        /** Associate a sound card index to its string description */
        typedef std::pair<int , std::string> HwIDPair;


        // Copy Constructor
        AlsaLayer (const AlsaLayer& rh);

        // Assignment Operator
        AlsaLayer& operator= (const AlsaLayer& rh);

        /**
         * Drop the pending frames and close the capture device
         * ALSA Library API
         */
        void closeCaptureStream (void);
        void stopCaptureStream (void);
        void startCaptureStream (void);
        void prepareCaptureStream (void);

        void closePlaybackStream (void);
        void stopPlaybackStream (void);
        void startPlaybackStream (void);
        void preparePlaybackStream (void);

        bool alsa_set_params (snd_pcm_t *pcm_handle, int type);

        /**
         * Copy a data buffer in the internal ring buffer
         * ALSA Library API
         * @param buffer The data to be copied
         * @param length The size of the buffer
         * @return int The number of frames actually copied
         */
        int write (void* buffer, int length, snd_pcm_t *handle);

        /**
         * Read data from the internal ring buffer
         * ALSA Library API
         * @param buffer  The buffer to stock the read data
         * @param toCopy  The number of bytes to get
         * @return int The number of frames actually read
         */
        int read (void* buffer, int toCopy);

        /**
         * Recover from XRUN state for capture
         * ALSA Library API
         */
        void handle_xrun_capture (void);

        /**
         * Recover from XRUN state for playback
         * ALSA Library API
         */
        void handle_xrun_playback (snd_pcm_t *handle);

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
         * Alsa parameter - Size of a period in the hardware ring buffer
         */
        snd_pcm_uframes_t periodSize_;

        /**
         * name of the alsa audio plugin used
         */
        std::string audioPlugin_;

        /** Vector to manage all soundcard index - description association of the system */
        std::vector<HwIDPair> IDSoundCards_;

        bool is_playback_prepared_;
        bool is_capture_prepared_;
        bool is_playback_running_;
        bool is_capture_running_;
        bool is_playback_open_;
        bool is_capture_open_;
        bool trigger_request_;

        AlsaThread* audioThread_;
};

#endif // _ALSA_LAYER_H_
