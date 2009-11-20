/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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
 */

#include <audiostream.h>
#include "pulselayer.h"

static pa_channel_map channel_map;



AudioStream::AudioStream (PulseLayerType * driver)
        : _audiostream (NULL),
        _context (driver->context),
        _streamType (driver->type),
        _streamDescription (driver->description),
        _volume(),
        flag (PA_STREAM_AUTO_TIMING_UPDATE),
        sample_spec(),
        _mainloop (driver->mainloop)
{
    sample_spec.format = PA_SAMPLE_S16LE;
    // sample_spec.format = PA_SAMPLE_FLOAT32LE;
    sample_spec.rate = 44100;
    sample_spec.channels = 1;
    channel_map.channels = 1;
    pa_cvolume_set (&_volume , 1 , PA_VOLUME_NORM) ;  // * vol / 100 ;
}

AudioStream::~AudioStream()
{
    disconnectStream();
}

bool
AudioStream::connectStream()
{
    ost::MutexLock guard (_mutex);

    if (!_audiostream)
        _audiostream = createStream (_context);

    return true;
}

static void success_cb (pa_stream *s, int success, void *userdata)
{

    assert (s);

    pa_threaded_mainloop * mainloop = (pa_threaded_mainloop *) userdata;

    pa_threaded_mainloop_signal (mainloop, 0);
}


bool
AudioStream::drainStream (void)
{
    if (_audiostream) {
        _debug ("Draining stream\n");
        pa_operation * operation;

        pa_threaded_mainloop_lock (_mainloop);

        if ( (operation = pa_stream_drain (_audiostream, success_cb, _mainloop))) {
            while (pa_operation_get_state (operation) != PA_OPERATION_DONE) {
                if (!_context || pa_context_get_state (_context) != PA_CONTEXT_READY || !_audiostream || pa_stream_get_state (_audiostream) != PA_STREAM_READY) {
                    _debug ("Connection died: %s\n", _context ? pa_strerror (pa_context_errno (_context)) : "NULL");
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
    _debug ("Destroy audio streams\n");

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

    _debug ("AudioStream::stream_state_callback :: The state of the stream changed\n");
    assert (s);

    m = (pa_threaded_mainloop*) user_data;
    assert (m);

    switch (pa_stream_get_state (s)) {

        case PA_STREAM_CREATING:
            _debug ("Stream is creating...\n");
            break;

        case PA_STREAM_TERMINATED:
            _debug ("Stream is terminating...\n");
            break;

        case PA_STREAM_READY:
            _debug ("Stream successfully created, connected to %s\n", pa_stream_get_device_name (s));
            // pa_stream_cork( s, 0, NULL, NULL);
            break;

        case PA_STREAM_UNCONNECTED:
            _debug ("Stream unconnected\n");
            break;

        case PA_STREAM_FAILED:

        default:
            _debug ("Stream error - Sink/Source doesn't exists: %s\n" , pa_strerror (pa_context_errno (pa_stream_get_context (s))));
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
AudioStream::createStream (pa_context* c)
{
    ost::MutexLock guard (_mutex);

    pa_stream* s;
    //pa_cvolume cv;

    // pa_sample_spec ss;
    // ss.format = PA_SAMPLE_S16LE;
    // ss.rate = 44100;
    // ss.channels = 1;


    assert (pa_sample_spec_valid (&sample_spec));
    assert (pa_channel_map_valid (&channel_map));

    pa_buffer_attr* attributes = (pa_buffer_attr*) malloc (sizeof (pa_buffer_attr));

    if (! (s = pa_stream_new (c, _streamDescription.c_str() , &sample_spec, &channel_map)))
        _debug ("%s: pa_stream_new() failed : %s\n" , _streamDescription.c_str(), pa_strerror (pa_context_errno (c)));

    assert (s);

    // parameters are defined as number of bytes
    // 2048 bytes (1024 int16) is 20 ms at 44100 Hz
    if (_streamType == PLAYBACK_STREAM) {

        // 20 ms framesize TODO: take framesize value from config
        attributes->maxlength = (uint32_t) -1;
        attributes->tlength = pa_usec_to_bytes (50 * PA_USEC_PER_MSEC, &sample_spec);
        attributes->prebuf = (uint32_t) -1;
        attributes->minreq = (uint32_t) -1;
        attributes->fragsize = (uint32_t) -1;

        pa_stream_connect_playback (s , NULL , attributes, PA_STREAM_ADJUST_LATENCY, &_volume, NULL);
    } else if (_streamType == CAPTURE_STREAM) {

        // 20 ms framesize TODO: take framesize value from config
        attributes->maxlength = (uint32_t) -1;
        attributes->tlength = (uint32_t) -1;
        attributes->prebuf = (uint32_t) -1;
        attributes->minreq = (uint32_t) -1;
        attributes->fragsize = pa_usec_to_bytes (50 * PA_USEC_PER_MSEC, &sample_spec);



        pa_stream_connect_record (s, NULL, attributes, (pa_stream_flags_t) (PA_STREAM_PEAK_DETECT|PA_STREAM_ADJUST_LATENCY));
    } else if (_streamType == UPLOAD_STREAM) {
        pa_stream_connect_upload (s , 1024);
    } else {
        _debug ("Stream type unknown \n");
    }

    pa_stream_set_state_callback (s , stream_state_callback, _mainloop);

    free (attributes);

    return s;
}

