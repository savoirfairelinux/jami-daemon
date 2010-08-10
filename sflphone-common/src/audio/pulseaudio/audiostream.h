/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <pulse/pulseaudio.h>
#include <string>

#include "global.h"
#include "audio/ringbuffer.h"
#include "audio/audioloop.h"

#include <cc++/thread.h>

/**
 * This data structure contains the different king of audio streams available
 */
enum STREAM_TYPE {
    PLAYBACK_STREAM,
    CAPTURE_STREAM,
    RINGTONE_STREAM,
    UPLOAD_STREAM
};

struct PulseLayerType {
    pa_context * context;
    pa_threaded_mainloop * mainloop;

    std::string description;

    int type;
    double volume;
};

class AudioStream
{
    public:
        /**
         * Constructor
         * @param context The PulseLayerType structure containing various information.
         */
        AudioStream (PulseLayerType * driver, int smplrate);

        /**
         * Destructor
         */
        ~AudioStream();

        /**
         * Write data to the main abstraction ring buffer.
         * @param buffer The buffer containing the data to be played
         * @param toCopy The number of samples, in bytes
         * @return int The number of bytes played
         */
        int putMain (void* buffer , int toCopy);

        /**
         * Write data to the urgent abstraction ring buffer. ( dtmf , double calls )
         * @param buffer The buffer containing the data to be played
         * @param toCopy The number of samples, in bytes
         * @return int The number of bytes played
         */
        int putUrgent (void* buffer , int toCopy);

        /**
         * Connect the pulse audio stream
         */
        bool connectStream (std::string* deviceName);

        /**
         * Drain the given stream.
         */
        bool drainStream (void);

        /**
         * Disconnect the pulseaudio stream
         */
        bool disconnectStream();

        /**
         * Accessor: Get the pulseaudio stream object
         * @return pa_stream* The stream
         */
        pa_stream* pulseStream() {
            return _audiostream;
        }

        /**
         * Accessor
         * @return std::string  The stream name
         */
        std::string getStreamName (void) {
            return _streamDescription;
        }

        /**
         * Accessor
         * @param name  The stream name
         */
        void setStreamName (std::string name) {
            _streamDescription = name;
        }

        void setVolume (double pc) {
            _volume.values[0] *= pc/100;
        }
        pa_cvolume getVolume (void) {
            return _volume;
        }

        /**
         * Accessor
         * @return stream state
         */
        pa_stream_state_t getStreamState (void);



    private:

        // Copy Constructor
        AudioStream (const AudioStream& rh);

        // Assignment Operator
        AudioStream& operator= (const AudioStream& rh);

        /**
         * Create the audio stream into the given context
         * @param c	The pulseaudio context
         * @return pa_stream* The newly created audio stream
         */
        pa_stream* createStream (pa_context* c, std::string* deviceName);

        /**
         * Mandatory asynchronous callback on the audio stream state
         */
        static void stream_state_callback (pa_stream* s, void* user_data);

        /**
         * Asynchronous callback on data processing ( write and read )
         */
        static void audioCallback (pa_stream* s, size_t bytes, void* userdata);

        /**
         * Write data to the sound device
         */
        void write (void);

        /**
         * The pulse audio object
         */
        pa_stream* _audiostream;

        /**
         * The pulse audio context
         */
        pa_context* _context;

        /**
         * The type of the stream
         */
        int _streamType;

        /**
         * The name of the stream
         */
        std::string _streamDescription;

        /**
         * Streams parameters
         */
        pa_cvolume _volume;
        pa_stream_flags_t _flag;
        pa_sample_spec _sample_spec ;

        pa_threaded_mainloop * _mainloop;

        ost::Mutex _mutex;

        bool _stream_is_ready;

};

#endif // _AUDIO_STREAM_H
