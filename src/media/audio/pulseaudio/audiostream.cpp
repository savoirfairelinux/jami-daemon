/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Adrien Beraud <adrien.beraud@savoirfairelinux.com>
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

#include "audiostream.h"
#include "pulselayer.h"
#include "logger.h"
#include "compiler_intrinsics.h"

#include <stdexcept>

namespace ring {

AudioStream::AudioStream(pa_context *c,
                         pa_threaded_mainloop *m,
                         const char *desc,
                         int type,
                         unsigned samplrate,
                         const PaDeviceInfos* infos,
                         bool ec)
    : audiostream_(0), mainloop_(m)
{
    const pa_channel_map channel_map = infos->channel_map;

    pa_sample_spec sample_spec = {
        PA_SAMPLE_S16LE, // PA_SAMPLE_FLOAT32LE,
        samplrate,
        channel_map.channels
    };

    RING_DBG("%s: trying to create stream with device %s (%dHz, %d channels)", desc, infos->name.c_str(), samplrate, channel_map.channels);

    assert(pa_sample_spec_valid(&sample_spec));
    assert(pa_channel_map_valid(&channel_map));

    std::unique_ptr<pa_proplist, decltype(pa_proplist_free)&> pl (pa_proplist_new(), pa_proplist_free);
    pa_proplist_sets(pl.get(), PA_PROP_FILTER_WANT, "echo-cancel");

    audiostream_ = pa_stream_new_with_proplist(c, desc, &sample_spec, &channel_map, ec ? pl.get() : nullptr);
    if (!audiostream_) {
        RING_ERR("%s: pa_stream_new() failed : %s" , desc, pa_strerror(pa_context_errno(c)));
        throw std::runtime_error("Could not create stream\n");
    }

    pa_buffer_attr attributes;
    attributes.maxlength = pa_usec_to_bytes(160 * PA_USEC_PER_MSEC, &sample_spec);
    attributes.tlength = pa_usec_to_bytes(80 * PA_USEC_PER_MSEC, &sample_spec);
    attributes.prebuf = 0;
    attributes.fragsize = pa_usec_to_bytes(80 * PA_USEC_PER_MSEC, &sample_spec);
    attributes.minreq = (uint32_t) -1;

    {
        PulseMainLoopLock lock(mainloop_);
        const pa_stream_flags_t flags = static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);

        if (type == PLAYBACK_STREAM || type == RINGTONE_STREAM) {
            pa_stream_connect_playback(audiostream_,
                    infos->name.empty() ? nullptr : infos->name.c_str(),
                    &attributes,
                    flags,
                    nullptr, nullptr);
        } else if (type == CAPTURE_STREAM) {
            pa_stream_connect_record(audiostream_,
                    infos->name.empty() ? nullptr : infos->name.c_str(),
                    &attributes,
                    flags);
        }
    }

    pa_stream_set_state_callback(audiostream_, [](pa_stream* s, void* user_data){
        static_cast<AudioStream*>(user_data)->stateChanged(s);
    }, this);
    pa_stream_set_moved_callback(audiostream_, [](pa_stream* s, void* user_data){
        static_cast<AudioStream*>(user_data)->moved(s);
    }, this);
}

AudioStream::~AudioStream()
{
    PulseMainLoopLock lock(mainloop_);

    pa_stream_disconnect(audiostream_);

    // make sure we don't get any further callback
    pa_stream_set_state_callback(audiostream_, NULL, NULL);
    pa_stream_set_write_callback(audiostream_, NULL, NULL);
    pa_stream_set_read_callback(audiostream_, NULL, NULL);
    pa_stream_set_moved_callback(audiostream_, NULL, NULL);
    pa_stream_set_underflow_callback(audiostream_, NULL, NULL);
    pa_stream_set_overflow_callback(audiostream_, NULL, NULL);

    pa_stream_unref(audiostream_);
}

void AudioStream::moved(pa_stream* s)
{
    audiostream_ = s;
    RING_DBG("Stream %d to %s", pa_stream_get_index(s), pa_stream_get_device_name(s));
}

void
AudioStream::stateChanged(pa_stream* s)
{
    UNUSED char str[PA_SAMPLE_SPEC_SNPRINT_MAX];

    switch (pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
            RING_DBG("Stream is creating...");
            break;

        case PA_STREAM_TERMINATED:
            RING_DBG("Stream is terminating...");
            break;

        case PA_STREAM_READY:
            RING_DBG("Stream successfully created, connected to %s", pa_stream_get_device_name(s));
            //RING_DBG("maxlength %u", pa_stream_get_buffer_attr(s)->maxlength);
            //RING_DBG("tlength %u", pa_stream_get_buffer_attr(s)->tlength);
            //RING_DBG("prebuf %u", pa_stream_get_buffer_attr(s)->prebuf);
            //RING_DBG("minreq %u", pa_stream_get_buffer_attr(s)->minreq);
            //RING_DBG("fragsize %u", pa_stream_get_buffer_attr(s)->fragsize);
            //RING_DBG("samplespec %s", pa_sample_spec_snprint(str, sizeof(str), pa_stream_get_sample_spec(s)));
            break;

        case PA_STREAM_UNCONNECTED:
            RING_DBG("Stream unconnected");
            break;

        case PA_STREAM_FAILED:
        default:
            RING_ERR("Stream failure: %s" , pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            break;
    }
}

bool AudioStream::isReady()
{
    if (!audiostream_)
        return false;

    return pa_stream_get_state(audiostream_) == PA_STREAM_READY;
}

} // namespace ring
