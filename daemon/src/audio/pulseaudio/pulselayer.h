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

class AudioStream;

class PulseLayer : public AudioLayer
{
    public:
        PulseLayer ();
        ~PulseLayer (void);

        std::list<std::string>& getSinkList (void) {
            return sinkList_;
        }

        std::list<std::string>& getSourceList (void) {
            return sourceList_;
        }

        /**
         * Write data from the ring buffer to the harware and read data from the hardware
         */
        void readFromMic (void);
        void writeToSpeaker (void);
        void ringtoneToSpeaker (void);


        void updateSinkList (void);

        void updateSourceList (void);

        bool inSinkList (const std::string &deviceName) const;

        bool inSourceList (const std::string &deviceName) const;

        void startStream (void);

        void stopStream (void);

        static void context_state_callback (pa_context* c, void* user_data);

    private:
        // Copy Constructor
        PulseLayer (const PulseLayer& rh);

        // Assignment Operator
        PulseLayer& operator= (const PulseLayer& rh);

        /**
         * Create the audio streams into the given context
         * @param c	The pulseaudio context
         */
        void createStreams (pa_context* c);

        /**
         * Close the connection with the local pulseaudio server
         */
        void disconnectAudioStream (void);

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

        std::list<std::string> sinkList_;
        std::list<std::string> sourceList_;

        /*
         * Buffers used to avoid doing malloc/free in the audio thread
         */
        SFLDataFormat *mic_buffer_;
        size_t mic_buf_size_;

    public:
        friend class AudioLayerTest;
};

#endif // _PULSE_LAYER_H_

