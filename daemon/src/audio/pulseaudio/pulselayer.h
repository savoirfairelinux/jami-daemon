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

#ifndef _PULSE_LAYER_H
#define _PULSE_LAYER_H

#include "audio/audiolayer.h"
#include <pulse/pulseaudio.h>
#include <pulse/stream.h>

#include <list>
#include <string>

#define PLAYBACK_STREAM_NAME	    "SFLphone playback"
#define CAPTURE_STREAM_NAME	    "SFLphone capture"
#define RINGTONE_STREAM_NAME        "SFLphone ringtone"

class RingBuffer;
class ManagerImpl;
class AudioStream;
class DcBlocker;
class SamplerateConverter;

typedef std::list<std::string> DeviceList;

class PulseLayer : public AudioLayer
{
    public:
        PulseLayer (ManagerImpl* manager);
        ~PulseLayer (void);
        /**
         * Check if no devices are opened, otherwise close them.
         * Then open the specified devices by calling the private functions open_device
         * @param indexIn	The number of the card chosen for capture
         * @param indexOut	The number of the card chosen for playback
         * @param sampleRate  The sample rate
         * @param frameSize	  The frame size
         * @param stream	  To indicate which kind of stream you want to open
         *			  SFL_PCM_CAPTURE
         *			  SFL_PCM_PLAYBACK
         *			  SFL_PCM_BOTH
         * @param plugin	  The alsa plugin ( dmix , default , front , surround , ...)
         */
        void openDevice (int indexIn, int indexOut, int indexRing, int sampleRate, int frameSize , int stream, const std::string &plugin) ;

        DeviceList* getSinkList (void) {
            return &sinkList_;
        }

        DeviceList* getSourceList (void) {
            return &sourceList_;
        }

        void updateSinkList (void);

        void updateSourceList (void);

        bool inSinkList (const std::string &deviceName) const;

        bool inSourceList (const std::string &deviceName) const;

        void startStream (void);

        void stopStream (void);

        static void context_state_callback (pa_context* c, void* user_data);

        /**
         * Reduce volume of every audio applications connected to the same sink
         */
        void reducePulseAppsVolume (void);

        /**
         * Restore the volume of every audio applications connected to the same sink to PA_VOLUME_NORM
         */
        void restorePulseAppsVolume (void);

        /**
         * Set the volume of a sink.
         * @param index The index of the stream
         * @param channels	The stream's number of channels
         * @param volume The new volume (between 0 and 100)
         */
        void setSinkVolume (int index, int channels, int volume);
        void setSourceVolume (int index, int channels, int volume);

        void setPlaybackVolume (int volume);
        void setCaptureVolume (int volume);

        /**
         * Accessor
         * @return AudioStream* The pointer on the playback AudioStream object
         */
        AudioStream* getPlaybackStream() const {
            return playback_;
        }

        /**
         * Accessor
         * @return AudioStream* The pointer on the record AudioStream object
         */
        AudioStream* getRecordStream() const {
            return record_;
        }

        /**
         * Accessor
         * @return AudioStream* The pointer on the ringtone AudioStream object
         */
        AudioStream* getRingtoneStream() const {
            return ringtone_;
        }

        int getSpkrVolume (void) const {
            return spkrVolume_;
        }
        void setSpkrVolume (int value) {
            spkrVolume_ = value;
        }

        int getMicVolume (void) const {
            return micVolume_;
        }
        void setMicVolume (int value) {
            micVolume_ = value;
        }

        /**
         * Handle used to write voice data to speaker
         */
        void processPlaybackData (void);

        /**
         * Handle used to write voice data to microphone
         */
        void processCaptureData (void);

        /**
         * Handle used to write audio data to speaker
         */
        void processRingtoneData (void);

        /**
         * Process speaker and microphone audio data
         */
        void processData (void);

    private:
        // Copy Constructor
        PulseLayer (const PulseLayer& rh);

        // Assignment Operator
        PulseLayer& operator= (const PulseLayer& rh);


        /**
         * Drop the pending frames and close the capture device
         */
        void closeCaptureStream (void);

        /**
         * Write data from the ring buffer to the harware and read data from the hardware
         */
        void readFromMic (void);
        void writeToSpeaker (void);
        void ringtoneToSpeaker (void);

        /**
         * Create the audio streams into the given context
         * @param c	The pulseaudio context
         */
        void createStreams (pa_context* c);

        /**
         * Drop the pending frames and close the playback device
         */
        void closePlaybackStream (void);

        /**
         * Establishes the connection with the local pulseaudio server
         */
        void connectPulseAudioServer (void);

        /**
         * Close the connection with the local pulseaudio server
         */
        void disconnectAudioStream (void);

        /**
         * Get some information about the pulseaudio server
         */
        void serverinfo (void);

        void openLayer (void);

        void closeLayer (void);


        /** PulseAudio context and asynchronous loop */
        pa_context* context_;
        pa_threaded_mainloop* mainloop_;

        /**
         * A stream object to handle the pulseaudio playback stream
         */
        AudioStream* playback_;

        /**
         * A stream object to handle the pulseaudio capture stream
         */
        AudioStream* record_;

        /**
         * A special stream object to handle specific playback stream for ringtone
         */
        AudioStream* ringtone_;

        /** Sample rate converter object */
        SamplerateConverter * converter_;

        int spkrVolume_;
        int micVolume_;

        DeviceList sinkList_;
        DeviceList sourceList_;

    public:
        friend class AudioLayerTest;
};

#endif // _PULSE_LAYER_H_

