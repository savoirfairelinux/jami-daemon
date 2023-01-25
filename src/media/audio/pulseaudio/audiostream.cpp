/*
 *  Copyright (C) 2004-2022 Savoir-faire Linux Inc.
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

namespace jami {

AudioStream::AudioStream(pa_context* c,
                         pa_threaded_mainloop* m,
                         const char* desc,
                         AudioDeviceType type,
                         unsigned samplrate,
                         const PaDeviceInfos& infos,
                         bool ec,
                         OnReady onReady,
                         OnData onData)
    : onReady_(std::move(onReady))
    , onData_(std::move(onData))
    , audiostream_(nullptr)
    , mainloop_(m)
    , audioType_(type)
{
    pa_sample_spec sample_spec = {PA_SAMPLE_S16LE, // PA_SAMPLE_FLOAT32LE,
                                  samplrate,
                                  infos.channel_map.channels};

    JAMI_DBG("%s: Creating stream with device %s (%dHz, %d channels)",
             desc,
             infos.name.c_str(),
             samplrate,
             infos.channel_map.channels);

    assert(pa_sample_spec_valid(&sample_spec));
    assert(pa_channel_map_valid(&infos.channel_map));

    std::unique_ptr<pa_proplist, decltype(pa_proplist_free)&> pl(pa_proplist_new(),
                                                                 pa_proplist_free);
    pa_proplist_sets(pl.get(), PA_PROP_FILTER_WANT, "echo-cancel");
    pa_proplist_sets(
        pl.get(), "filter.apply.echo-cancel.parameters", // needs pulseaudio >= 11.0
        "use_volume_sharing=0"  // share volume with master sink/source
        " use_master_format=1"  // use format/rate/channels from master sink/source
        " aec_args=\""
            "digital_gain_control=1"
            " analog_gain_control=0"
            " experimental_agc=1"
        "\"");

    audiostream_ = pa_stream_new_with_proplist(c,
                                               desc,
                                               &sample_spec,
                                               &infos.channel_map,
                                               ec ? pl.get() : nullptr);
    if (!audiostream_) {
        JAMI_ERR("%s: pa_stream_new() failed : %s", desc, pa_strerror(pa_context_errno(c)));
        throw std::runtime_error("Could not create stream\n");
    }

    pa_buffer_attr attributes;
    attributes.maxlength = pa_usec_to_bytes(160 * PA_USEC_PER_MSEC, &sample_spec);
    attributes.tlength = pa_usec_to_bytes(80 * PA_USEC_PER_MSEC, &sample_spec);
    attributes.prebuf = 0;
    attributes.fragsize = pa_usec_to_bytes(80 * PA_USEC_PER_MSEC, &sample_spec);
    attributes.minreq = (uint32_t) -1;

    pa_stream_set_state_callback(
        audiostream_,
        [](pa_stream* s, void* user_data) { static_cast<AudioStream*>(user_data)->stateChanged(s); },
        this);
    pa_stream_set_moved_callback(
        audiostream_,
        [](pa_stream* s, void* user_data) { static_cast<AudioStream*>(user_data)->moved(s); },
        this);

    constexpr pa_stream_flags_t flags = static_cast<pa_stream_flags_t>(
        PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_START_CORKED);

    if (type == AudioDeviceType::PLAYBACK || type == AudioDeviceType::RINGTONE) {
        pa_stream_set_write_callback(
            audiostream_,
            [](pa_stream* /*s*/, size_t bytes, void* userdata) {
                static_cast<AudioStream*>(userdata)->onData_(bytes);
            },
            this);

        pa_stream_connect_playback(audiostream_,
                                   infos.name.empty() ? nullptr : infos.name.c_str(),
                                   &attributes,
                                   flags,
                                   nullptr,
                                   nullptr);
    } else if (type == AudioDeviceType::CAPTURE) {
        pa_stream_set_read_callback(
            audiostream_,
            [](pa_stream* /*s*/, size_t bytes, void* userdata) {
                static_cast<AudioStream*>(userdata)->onData_(bytes);
            },
            this);

        pa_stream_connect_record(audiostream_,
                                 infos.name.empty() ? nullptr : infos.name.c_str(),
                                 &attributes,
                                 flags);
    }
}

void
disconnectStream(pa_stream* s)
{
    // make sure we don't get any further callback
    pa_stream_set_write_callback(s, nullptr, nullptr);
    pa_stream_set_read_callback(s, nullptr, nullptr);
    pa_stream_set_moved_callback(s, nullptr, nullptr);
    pa_stream_set_underflow_callback(s, nullptr, nullptr);
    pa_stream_set_overflow_callback(s, nullptr, nullptr);
    pa_stream_set_suspended_callback(s, nullptr, nullptr);
    pa_stream_set_started_callback(s, nullptr, nullptr);
}

void
destroyStream(pa_stream* s)
{
    pa_stream_disconnect(s);
    pa_stream_set_state_callback(s, nullptr, nullptr);
    disconnectStream(s);
    pa_stream_unref(s);
}

AudioStream::~AudioStream()
{
    stop();
}

void
AudioStream::start()
{
    pa_stream_cork(audiostream_, 0, nullptr, nullptr);

    // trigger echo cancel check
    moved(audiostream_);
}

void
AudioStream::stop()
{
    if (not audiostream_)
        return;
    JAMI_DBG("Destroying stream with device %s", pa_stream_get_device_name(audiostream_));
    if (pa_stream_get_state(audiostream_) == PA_STREAM_CREATING) {
        disconnectStream(audiostream_);
        pa_stream_set_state_callback(
            audiostream_, [](pa_stream* s, void*) { destroyStream(s); }, nullptr);
    } else {
        destroyStream(audiostream_);
    }
    audiostream_ = nullptr;
}

void
AudioStream::moved(pa_stream* s)
{
    audiostream_ = s;
    JAMI_DBG("[audiostream] Stream moved: %d, %s",
             pa_stream_get_index(s),
             pa_stream_get_device_name(s));

    if (audioType_ == AudioDeviceType::CAPTURE) {
        // check for echo cancel
        const char* name = pa_stream_get_device_name(s);
        if (!name) {
            JAMI_ERR("[audiostream] moved() unable to get audio stream device");
            return;
        }

        pa_context* context = pa_stream_get_context(s);
        auto* op = pa_context_get_source_info_by_name(
            context,
            name,
            [](pa_context* /*c*/, const pa_source_info* i, int /*eol*/, void* userdata) {
                AudioStream* thisPtr = (AudioStream*) userdata;
                // this whole closure gets called twice by pulse for some reason
                // the 2nd time, i is invalid
                if (!i) {
                    // JAMI_ERR("[audiostream] source info not found for %s", realName);
                    return;
                }

                // string compare
                bool usingEchoCancel = std::string_view(i->driver) == "module-echo-cancel.c";
                JAMI_WARN("[audiostream] capture stream using pulse echo cancel module? %s (%s)",
                          usingEchoCancel ? "yes" : "no",
                          i->name);
                if (!thisPtr) {
                    JAMI_ERR("[audiostream] AudioStream pointer became invalid during "
                             "pa_source_info_cb_t callback!");
                    return;
                }
                thisPtr->echoCancelCb(usingEchoCancel);
            },
            this);

        pa_operation_unref(op);
    }
}

void
AudioStream::stateChanged(pa_stream* s)
{
    // UNUSED char str[PA_SAMPLE_SPEC_SNPRINT_MAX];

    switch (pa_stream_get_state(s)) {
    case PA_STREAM_CREATING:
        JAMI_DBG("Stream is creating...");
        break;

    case PA_STREAM_TERMINATED:
        JAMI_DBG("Stream is terminating...");
        break;

    case PA_STREAM_READY:
        JAMI_DBG("Stream successfully created, connected to %s", pa_stream_get_device_name(s));
        // JAMI_DBG("maxlength %u", pa_stream_get_buffer_attr(s)->maxlength);
        // JAMI_DBG("tlength %u", pa_stream_get_buffer_attr(s)->tlength);
        // JAMI_DBG("prebuf %u", pa_stream_get_buffer_attr(s)->prebuf);
        // JAMI_DBG("minreq %u", pa_stream_get_buffer_attr(s)->minreq);
        // JAMI_DBG("fragsize %u", pa_stream_get_buffer_attr(s)->fragsize);
        // JAMI_DBG("samplespec %s", pa_sample_spec_snprint(str, sizeof(str), pa_stream_get_sample_spec(s)));
        onReady_();
        break;

    case PA_STREAM_UNCONNECTED:
        JAMI_DBG("Stream unconnected");
        break;

    case PA_STREAM_FAILED:
    default:
        JAMI_ERR("Stream failure: %s", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
        break;
    }
}

bool
AudioStream::isReady()
{
    if (!audiostream_)
        return false;

    return pa_stream_get_state(audiostream_) == PA_STREAM_READY;
}

} // namespace jami
