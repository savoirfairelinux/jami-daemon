/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef _AUDIO_STREAM_H
#define _AUDIO_STREAM_H

#include <pulse/pulseaudio.h>
#include <string>
#include "noncopyable.h"
#include "pulselayer.h"

/**
 * This data structure contains the different king of audio streams available
 */
enum STREAM_TYPE {
    PLAYBACK_STREAM, CAPTURE_STREAM, RINGTONE_STREAM
};

class AudioStream {
    public:

        /**
         * Constructor
         *
         * @param context pulseaudio's application context.
         * @param mainloop pulseaudio's main loop
         * @param description
         * @param types
         * @param audio sampling rate
         * @param pointer to pa_source_info or pa_sink_info (depending on type).
         */
        AudioStream(pa_context *, pa_threaded_mainloop *, const char *, int, unsigned, const PaDeviceInfos*);

        ~AudioStream();

        /**
         * Accessor: Get the pulseaudio stream object
         * @return pa_stream* The stream
         */
        pa_stream* pulseStream() {
            return audiostream_;
        }

        const pa_sample_spec * sampleSpec() const {
            return pa_stream_get_sample_spec(audiostream_);
        }

        inline size_t sampleSize() const {
            return pa_sample_size(sampleSpec());
        }

        inline uint8_t channels() const {
            return sampleSpec()->channels;
        }

        inline AudioFormat getFormat() const {
            auto s = sampleSpec();
            return AudioFormat(s->rate, s->channels);
        }

        bool isReady();

    private:
        NON_COPYABLE(AudioStream);

        /**
         * Mandatory asynchronous callback on the audio stream state
         */
        static void stream_state_callback(pa_stream* s, void* user_data);

        /**
         * The pulse audio object
         */
        pa_stream* audiostream_;

        /**
         * A pointer to the opaque threaded main loop object
         */
        pa_threaded_mainloop * mainloop_;
};

#endif // _AUDIO_STREAM_H
