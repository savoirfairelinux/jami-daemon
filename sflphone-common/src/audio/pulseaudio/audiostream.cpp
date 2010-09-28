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

#include <audiostream.h>
#include "pulselayer.h"

static pa_channel_map channel_map;



AudioStream::AudioStream (PulseLayerType * driver, int smplrate)
        : _audiostream (NULL),
        _context (driver->context),
        _streamType (driver->type),
        _streamDescription (driver->description),
        _volume(),
        _flag (PA_STREAM_AUTO_TIMING_UPDATE),
        _sample_spec(),
        _mainloop (driver->mainloop)
{
    _sample_spec.format = PA_SAMPLE_S16LE; // PA_SAMPLE_FLOAT32LE;
    _sample_spec.rate = smplrate;
    _sample_spec.channels = 1;
    channel_map.channels = 1;
    pa_cvolume_set (&_volume , 1 , PA_VOLUME_NORM) ;  // * vol / 100 ;
}

AudioStream::~AudioStream()
{
    disconnectStream();
}

bool
AudioStream::connectStream (std::string* deviceName)
{
    ost::MutexLock guard (_mutex);

    if (!_audiostream)
        _audiostream = createStream (_context, deviceName);

    return true;
}

static void success_cb (pa_stream *s, int success UNUSED, void *userdata)
{

    assert (s);

    pa_threaded_mainloop * mainloop = (pa_threaded_mainloop *) userdata;

    pa_threaded_mainloop_signal (mainloop, 0);
}

bool
AudioStream::drainStream (void)
{
    if (_audiostream) {
        _info ("Audio: Draining stream");
        pa_operation * operation;

        pa_threaded_mainloop_lock (_mainloop);

        if ( (operation = pa_stream_drain (_audiostream, success_cb, _mainloop))) {
            while (pa_operation_get_state (operation) != PA_OPERATION_DONE) {
                if (!_context || pa_context_get_state (_context) != PA_CONTEXT_READY || !_audiostream || pa_stream_get_state (_audiostream) != PA_STREAM_READY) {
                    _warn ("Audio: Connection died: %s", _context ? pa_strerror (pa_context_errno (_context)) : "NULL");
                    pa_operation_unref (operation);
                    break;
                } else {
                    pa_threaded_mainloop_wait (_mainloop);
                }
            }
        }

        pa_threaded_mainloop_unlock (_mainloop);
    }

    return true;
}

bool
AudioStream::disconnectStream (void)
{
    _info ("Audio: Destroy audio streams");

    pa_threaded_mainloop_lock (_mainloop);

    if (_audiostream) {
        pa_stream_disconnect (_audiostream);

        // make sure we don't get any further callback
        pa_stream_set_state_callback (_audiostream, NULL, NULL);
        pa_stream_set_write_callback (_audiostream, NULL, NULL);
        pa_stream_set_underflow_callback (_audiostream, NULL, NULL);
        pa_stream_set_overflow_callback (_audiostream, NULL, NULL);

        pa_stream_unref (_audiostream);
        _audiostream = NULL;
    }

    pa_threaded_mainloop_unlock (_mainloop);

    return true;
}



void
AudioStream::stream_state_callback (pa_stream* s, void* user_data)
{
    pa_threaded_mainloop *m;

    _info ("Audio: The state of the stream changed");
    assert (s);

    char str[PA_SAMPLE_SPEC_SNPRINT_MAX];

    m = (pa_threaded_mainloop*) user_data;
    assert (m);

    switch (pa_stream_get_state (s)) {

        case PA_STREAM_CREATING:
            _info ("Audio: Stream is creating...");
            break;

        case PA_STREAM_TERMINATED:
            _info ("Audio: Stream is terminating...");
            break;

        case PA_STREAM_READY:
            _info ("Audio: Stream successfully created, connected to %s", pa_stream_get_device_name (s));
            // pa_buffer_attr *buffattr = (pa_buffer_attr *)pa_xmalloc (sizeof(pa_buffer_attr));
            _debug ("Audio: maxlength %u", pa_stream_get_buffer_attr (s)->maxlength);
            _debug ("Audio: tlength %u", pa_stream_get_buffer_attr (s)->tlength);
            _debug ("Audio: prebug %u", pa_stream_get_buffer_attr (s)->prebuf);
            _debug ("Audio: minreq %u", pa_stream_get_buffer_attr (s)->minreq);
            _debug ("Audio: fragsize %u", pa_stream_get_buffer_attr (s)->fragsize);
            _debug ("Audio: samplespec %s", pa_sample_spec_snprint (str, sizeof (str), pa_stream_get_sample_spec (s)));
            // pa_xfree (buffattr);
            break;

        case PA_STREAM_UNCONNECTED:
            _info ("Audio: Stream unconnected");
            break;

        case PA_STREAM_FAILED:

        default:
            _warn ("Audio: Error - Sink/Source doesn't exists: %s" , pa_strerror (pa_context_errno (pa_stream_get_context (s))));
            exit (0);
            break;
    }
}

pa_stream_state_t
AudioStream::getStreamState (void)
{

    ost::MutexLock guard (_mutex);
    return pa_stream_get_state (_audiostream);
}



pa_stream*
AudioStream::createStream (pa_context* c, std::string *deviceName)
{
    ost::MutexLock guard (_mutex);

    pa_stream* s;

    assert (pa_sample_spec_valid (&_sample_spec));
    assert (pa_channel_map_valid (&channel_map));

    _info ("Audio: Create pulseaudio stream");

    pa_buffer_attr* attributes = (pa_buffer_attr*) malloc (sizeof (pa_buffer_attr));


    if (! (s = pa_stream_new (c, _streamDescription.c_str() , &_sample_spec, &channel_map)))
        _warn ("Audio: Error: %s: pa_stream_new() failed : %s" , _streamDescription.c_str(), pa_strerror (pa_context_errno (c)));

    assert (s);

    if (_streamType == PLAYBACK_STREAM) {

        attributes->maxlength = pa_usec_to_bytes (160 * PA_USEC_PER_MSEC, &_sample_spec); // -1;
        attributes->tlength = pa_usec_to_bytes (80 * PA_USEC_PER_MSEC, &_sample_spec);
        attributes->prebuf = 0;
        attributes->fragsize = pa_usec_to_bytes (80 * PA_USEC_PER_MSEC, &_sample_spec);
        attributes->minreq = (uint32_t) -1;

        pa_threaded_mainloop_lock (_mainloop);

        if (deviceName)
            pa_stream_connect_playback (s , deviceName->c_str(), attributes, (pa_stream_flags_t) (PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE), NULL, NULL);
        else
            pa_stream_connect_playback (s , NULL, attributes, (pa_stream_flags_t) (PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE), NULL, NULL);


        pa_threaded_mainloop_unlock (_mainloop);

    } else if (_streamType == CAPTURE_STREAM) {

        attributes->maxlength = pa_usec_to_bytes (160 * PA_USEC_PER_MSEC, &_sample_spec);// (uint32_t) -1;
        attributes->tlength = pa_usec_to_bytes (80 * PA_USEC_PER_MSEC, &_sample_spec);// pa_usec_to_bytes (20 * PA_USEC_PER_MSEC, &_sample_spec);
        attributes->prebuf = 0;
        attributes->fragsize = pa_usec_to_bytes (80 * PA_USEC_PER_MSEC, &_sample_spec); // pa_usec_to_bytes (20 * PA_USEC_PER_MSEC, &_sample_spec);
        attributes->minreq = (uint32_t) -1;

        pa_threaded_mainloop_lock (_mainloop);

        if (deviceName)
            pa_stream_connect_record (s, deviceName->c_str(), attributes, (pa_stream_flags_t) (PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE));
        else
            pa_stream_connect_record (s, NULL, attributes, (pa_stream_flags_t) (PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE));


        pa_threaded_mainloop_unlock (_mainloop);

    } else if (_streamType == RINGTONE_STREAM) {

        attributes->maxlength = pa_usec_to_bytes (160 * PA_USEC_PER_MSEC, &_sample_spec);;
        attributes->tlength = pa_usec_to_bytes (80 * PA_USEC_PER_MSEC, &_sample_spec);
        attributes->prebuf = 0;
        attributes->fragsize = pa_usec_to_bytes (80 * PA_USEC_PER_MSEC, &_sample_spec);
        attributes->minreq = (uint32_t) -1;

        pa_threaded_mainloop_lock (_mainloop);

        if (deviceName)
            pa_stream_connect_playback (s, deviceName->c_str(), attributes, (pa_stream_flags_t) (PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE), NULL, NULL);
        else
            pa_stream_connect_playback (s, NULL, attributes, (pa_stream_flags_t) (PA_STREAM_ADJUST_LATENCY), NULL, NULL);

        pa_threaded_mainloop_unlock (_mainloop);

    } else if (_streamType == UPLOAD_STREAM) {
        pa_stream_connect_upload (s , 1024);
    } else {
        _warn ("Audio: Error: Stream type unknown ");
    }

    pa_stream_set_state_callback (s , stream_state_callback, _mainloop);


    free (attributes);

    return s;
}

