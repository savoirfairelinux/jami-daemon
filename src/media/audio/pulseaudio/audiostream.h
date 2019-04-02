/*
 *  Copyright (C) 2004-2019 Savoir-faire Linux Inc.
 *
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
 */

#pragma once

#include "noncopyable.h"
#include "pulselayer.h"

#include <pulse/pulseaudio.h>
#include <string>

namespace jami {

/**
 * This data structure contains the different king of audio streams available
 */
enum class StreamType { Playback, Capture, Ringtone };

class AudioStream {
public:
    using OnReady = std::function<void()>;

    /**
     * Constructor
     *
     * @param context pulseaudio's application context.
     * @param mainloop pulseaudio's main loop
     * @param description
     * @param types
     * @param audio sampling rate
     * @param pointer to pa_source_info or pa_sink_info (depending on type).
     * @param true if echo cancelling should be used with this stream
     */
    AudioStream(pa_context *, pa_threaded_mainloop *, const char *, StreamType, unsigned, const PaDeviceInfos*, bool, OnReady onReady);

    ~AudioStream();

    void start();

    /**
     * Accessor: Get the pulseaudio stream object
     * @return pa_stream* The stream
     */
    pa_stream* stream() {
        return audiostream_;
    }

    const pa_sample_spec * sampleSpec() const {
        return pa_stream_get_sample_spec(audiostream_);
    }

    inline size_t sampleSize() const {
        return pa_sample_size(sampleSpec());
    }
    inline size_t frameSize() const {
        return pa_frame_size(sampleSpec());
    }

    inline uint8_t channels() const {
        return sampleSpec()->channels;
    }

    inline AudioFormat format() const {
        auto s = sampleSpec();
        return AudioFormat(s->rate, s->channels);
    }

    inline std::string getDeviceName() const {
        auto res = pa_stream_get_device_name(audiostream_);
        if (res == reinterpret_cast<decltype(res)>(-PA_ERR_NOTSUPPORTED) or !res)
            return {};
        return res;
    }

    bool isReady();

private:
    NON_COPYABLE(AudioStream);

    OnReady onReady_;

    /**
     * Mandatory asynchronous callback on the audio stream state
     */
    void stateChanged(pa_stream* s);
    void moved(pa_stream* s);

    /**
     * The pulse audio object
     */
    pa_stream* audiostream_;

    /**
     * A pointer to the opaque threaded main loop object
     */
    pa_threaded_mainloop * mainloop_;
};

}
