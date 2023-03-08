/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

inline AVSampleFormat
sampleFormatFromPulse(pa_sample_format_t format) {
switch (format)
{
case PA_SAMPLE_S16LE:
case PA_SAMPLE_S16BE:
    return AV_SAMPLE_FMT_S16;
case PA_SAMPLE_FLOAT32LE:
case PA_SAMPLE_FLOAT32BE:
    return AV_SAMPLE_FMT_FLT;
case PA_SAMPLE_S32LE:
case PA_SAMPLE_S32BE:
    return AV_SAMPLE_FMT_S32;
default:
    return AV_SAMPLE_FMT_S16;
}
}

inline pa_sample_format_t
pulseSampleFormatFromAv(AVSampleFormat format) {
switch (format)
{
case AV_SAMPLE_FMT_S16:
    return PA_SAMPLE_S16LE;
case AV_SAMPLE_FMT_FLT:
    return PA_SAMPLE_FLOAT32LE;
case AV_SAMPLE_FMT_S32:
    return PA_SAMPLE_S32LE;
default:
    return PA_SAMPLE_S16LE;
}
}

class AudioStream
{
public:
    using OnReady = std::function<void()>;
    using OnData = std::function<void(size_t)>;

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
    AudioStream(pa_context*,
                pa_threaded_mainloop*,
                const char*,
                AudioDeviceType,
                unsigned,
                pa_sample_format_t,
                const PaDeviceInfos&,
                bool,
                OnReady onReady,
                OnData onData);

    ~AudioStream();

    void start();
    void stop();

    /**
     * Accessor: Get the pulseaudio stream object
     * @return pa_stream* The stream
     */
    pa_stream* stream() { return audiostream_; }

    const pa_sample_spec* sampleSpec() const { return pa_stream_get_sample_spec(audiostream_); }

    inline size_t sampleSize() const { return pa_sample_size(sampleSpec()); }
    inline size_t frameSize() const { return pa_frame_size(sampleSpec()); }

    inline uint8_t channels() const { return sampleSpec()->channels; }

    inline AudioFormat format() const
    {
        auto s = sampleSpec();
        return AudioFormat(s->rate, s->channels, sampleFormatFromPulse(s->format));
    }

    inline std::string getDeviceName() const
    {
        auto res = pa_stream_get_device_name(audiostream_);
        if (res == reinterpret_cast<decltype(res)>(-PA_ERR_NOTSUPPORTED) or !res)
            return {};
        return res;
    }

    bool isReady();

    void setEchoCancelCb(std::function<void(bool)>&& cb) { echoCancelCb = cb; }

private:
    NON_COPYABLE(AudioStream);

    OnReady onReady_;
    OnData onData_;

    /**
     * Mandatory asynchronous callback on the audio stream state
     */
    void stateChanged(pa_stream* s);
    void moved(pa_stream* s);
    void opEnded(pa_operation* s);

    /**
     * The pulse audio object
     */
    pa_stream* audiostream_;

    /**
     * A pointer to the opaque threaded main loop object
     */
    pa_threaded_mainloop* mainloop_;

    /**
     * The type of this audio stream
     */
    AudioDeviceType audioType_;

    /**
     * Function called whenever the stream is moved and we check for an echo canceller
     */
    std::function<void(bool)> echoCancelCb;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::set<pa_operation*> ongoing_ops;
};

} // namespace jami
