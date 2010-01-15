/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include "pulselayer.h"
#include "managerimpl.h"

int framesPerBuffer = 2048;

static  void playback_callback (pa_stream* s, size_t bytes, void* userdata)
{
    // _debug("playback_callback");

    assert (s && bytes);
    assert (bytes > 0);
    static_cast<PulseLayer*> (userdata)->processPlaybackData();
    // static_cast<PulseLayer*> (userdata)->processData();
}

static void capture_callback (pa_stream* s, size_t bytes, void* userdata)
{
    // _debug("capture_callback");

    assert (s && bytes);
    assert (bytes > 0);
    static_cast<PulseLayer*> (userdata)->processCaptureData();
    // static_cast<PulseLayer*> (userdata)->processData();
}

/*
static void stream_suspended_callback (pa_stream *s UNUSED, void *userdata UNUSED)
{
    _debug("PulseLayer::Stream Suspended");
}
*/

/*
static void stream_moved_callback(pa_stream *s UNUSED, void *userdata UNUSED)
{
    _debug("PulseLayer::Stream Moved");
}
*/

static void playback_underflow_callback (pa_stream* s,  void* userdata UNUSED)
{
    _debug ("PulseLayer::Buffer Underflow");
    // const pa_timing_info* info = pa_stream_get_timing_info(s);
    // _debug("         pa write_index: %l", (long)(info->write_index));
    // _debug("         pa write_index_corupt (if not 0): %i",  info->write_index_corrupt);
    // _debug("         pa read_index: %l", (long)(info->read_index));
    // _debug("         pa read_index_corrupt (if not 0): %i", info->read_index_corrupt);


    // fill in audio buffer twice the prebuffering value to restart playback
    pa_stream_writable_size (s);
    pa_stream_trigger (s, NULL, NULL);


}


static void playback_overflow_callback (pa_stream* s UNUSED, void* userdata UNUSED)
{
    _debug ("PulseLayer::Buffer OverFlow");
    //PulseLayer* pulse = (PulseLayer*) userdata;
    // pa_stream_drop (s);
    // pa_stream_trigger (s, NULL, NULL);
}


PulseLayer::PulseLayer (ManagerImpl* manager)
        : AudioLayer (manager , PULSEAUDIO)
        , context (NULL)
        , m (NULL)
        , playback()
        , record()
{
    _debug ("PulseLayer::Pulse audio constructor: Create context");

    _urgentRingBuffer.createReadPointer();
    dcblocker = new DcBlocker();
    is_started = false;
}

// Destructor
PulseLayer::~PulseLayer (void)
{
    closeLayer ();

    if (_converter) {
        delete _converter;
        _converter = NULL;
    }

    delete dcblocker;

    dcblocker = NULL;
}

bool
PulseLayer::closeLayer (void)
{
    _debug ("PulseLayer::closeLayer :: Destroy pulselayer");

    // Commenting the line below will make the
    // PulseLayer to close immediately, not
    // waiting for the playback buffer to be
    // emptied. It should not hurt.
    //playback->drainStream();

    if (m) {
        pa_threaded_mainloop_stop (m);
    }

    // playback->disconnectStream();
    // closePlaybackStream();

    // record->disconnectStream();
    // closeCaptureStream();

    // disconnectAudioStream();

    if (context) {
        pa_context_disconnect (context);
        pa_context_unref (context);
        context = NULL;
    }

    if (m) {
        pa_threaded_mainloop_free (m);
        m = NULL;
    }

    return true;
}

void
PulseLayer::connectPulseAudioServer (void)
{
    _debug ("PulseLayer::connectPulseAudioServer ");

    setenv ("PULSE_PROP_media.role", "phone", 1);

    pa_context_flags_t flag = PA_CONTEXT_NOAUTOSPAWN ;

    pa_threaded_mainloop_lock (m);

    _debug ("Connect the context to the server");
    pa_context_connect (context, NULL , flag , NULL);

    pa_context_set_state_callback (context, context_state_callback, this);
    pa_threaded_mainloop_wait (m);

    // Run the main loop

    if (pa_context_get_state (context) != PA_CONTEXT_READY) {
        _debug ("Error connecting to pulse audio server");
        // pa_threaded_mainloop_unlock (m);
    }

    pa_threaded_mainloop_unlock (m);

    //serverinfo();
    //muteAudioApps(99);
    _debug ("Context creation done");

}

void PulseLayer::context_state_callback (pa_context* c, void* user_data)
{
    _debug ("PulseLayer::context_state_callback ::The state of the context changed");
    PulseLayer* pulse = (PulseLayer*) user_data;
    assert (c && pulse->m);

    switch (pa_context_get_state (c)) {

        case PA_CONTEXT_CONNECTING:

        case PA_CONTEXT_AUTHORIZING:

        case PA_CONTEXT_SETTING_NAME:
            _debug ("Waiting....");
            break;

        case PA_CONTEXT_READY:
            pulse->createStreams (c);
            _debug ("Connection to PulseAudio server established");
            break;

        case PA_CONTEXT_TERMINATED:
            _debug ("Context terminated");
            break;

        case PA_CONTEXT_FAILED:

        default:
            _debug (" Error : %s" , pa_strerror (pa_context_errno (c)));
            pulse->disconnectAudioStream();
            exit (0);
            break;
    }
}

bool PulseLayer::disconnectAudioStream (void)
{
    _debug (" PulseLayer::disconnectAudioStream( void ) ");

    closePlaybackStream();

    closeCaptureStream();

    if (!playback && !record)
        return true;
    else
        return false;
}


bool PulseLayer::createStreams (pa_context* c)
{
    _debug ("PulseLayer::createStreams");

    PulseLayerType * playbackParam = new PulseLayerType();
    playbackParam->context = c;
    playbackParam->type = PLAYBACK_STREAM;
    playbackParam->description = PLAYBACK_STREAM_NAME;
    playbackParam->volume = _manager->getSpkrVolume();
    playbackParam->mainloop = m;

    playback = new AudioStream (playbackParam);
    playback->connectStream();
    pa_stream_set_write_callback (playback->pulseStream(), playback_callback, this);
    pa_stream_set_overflow_callback (playback->pulseStream(), playback_overflow_callback, this);
    pa_stream_set_underflow_callback (playback->pulseStream(), playback_underflow_callback, this);
    // pa_stream_set_suspended_callback(playback->pulseStream(), stream_suspended_callback, this);
    // pa_stream_set_moved_callback(playback->pulseStream(), stream_moved_callback, this);
    delete playbackParam;

    PulseLayerType * recordParam = new PulseLayerType();
    recordParam->context = c;
    recordParam->type = CAPTURE_STREAM;
    recordParam->description = CAPTURE_STREAM_NAME;
    recordParam->volume = _manager->getMicVolume();
    recordParam->mainloop = m;

    record = new AudioStream (recordParam);
    record->connectStream();
    pa_stream_set_read_callback (record->pulseStream() , capture_callback, this);
    // pa_stream_set_suspended_callback(record->pulseStream(), stream_suspended_callback, this);
    // pa_stream_set_moved_callback(record->pulseStream(), stream_moved_callback, this);
    delete recordParam;

    pa_threaded_mainloop_signal (m , 0);

    flushMain();
    flushUrgent();
    // _urgentRingBuffer.flushAll();


    return true;
}


bool PulseLayer::openDevice (int indexIn UNUSED, int indexOut UNUSED, int sampleRate, int frameSize , int stream UNUSED, std::string plugin UNUSED)
{
    _audioSampleRate = sampleRate;
    _frameSize = frameSize;

    // _urgentRingBuffer.flushAll();
    flushUrgent();

    _converter = new SamplerateConverter (_audioSampleRate, _frameSize*4);

    return true;
}

void PulseLayer::closeCaptureStream (void)
{
    if (record) {

        delete record;
        record=NULL;
    }
}

void PulseLayer::closePlaybackStream (void)
{
    if (playback) {

        delete playback;
        playback=NULL;
    }
}

int PulseLayer::canGetMic()
{
    if (record)
        return 0; // _micRingBuffer.AvailForGet();
    else
        return 0;
}

int PulseLayer::getMic (void *buffer, int toCopy)
{
    if (record) {
        return 0; // _micRingBuffer.Get (buffer, toCopy, 100);
    } else
        return 0;
}

void PulseLayer::startStream (void)
{
    // connectPulseAudioServer();

    if (!is_started) {

        _debug ("PulseLayer::Start Stream");

        if (!m) {

            _debug ("Creating PulseAudio MainLoop");
            m = pa_threaded_mainloop_new();
            assert (m);

            if (pa_threaded_mainloop_start (m) < 0) {
                _debug ("Failed starting the mainloop");
            }
        }

        if (!context) {

            _debug ("Creating new PulseAudio Context");
            pa_threaded_mainloop_lock (m);
            // Instanciate a context

            if (! (context = pa_context_new (pa_threaded_mainloop_get_api (m) , "SFLphone")))
                _debug ("Error while creating the context");

            pa_threaded_mainloop_unlock (m);

            assert (context);
        }

        // Create Streams
        connectPulseAudioServer();

        is_started = true;
    }

    // Flush outside the if statement: every time start stream is
    // called is to notify a new event
    flushUrgent();

    flushMain();

}

void
PulseLayer::stopStream (void)
{

    if (is_started) {

        _debug ("PulseLayer::Stop Audio Stream");
        pa_stream_flush (playback->pulseStream(), NULL, NULL);
        pa_stream_flush (record->pulseStream(), NULL, NULL);

        if (m) {
            pa_threaded_mainloop_stop (m);
        }

        disconnectAudioStream();

        _debug ("Disconnecting PulseAudio context");

        if (context) {

            pa_threaded_mainloop_lock (m);
            pa_context_disconnect (context);
            pa_context_unref (context);
            pa_threaded_mainloop_unlock (m);
            context = NULL;
        }

        _debug ("Freeing Pulseaudio mainloop");

        if (m) {
            pa_threaded_mainloop_free (m);
            m = NULL;
        }


        is_started = false;

    }

}



// void PulseLayer::underflow (pa_stream* s UNUSED,  void* userdata UNUSED)
//{
//    _debug ("PulseLayer::Buffer Underflow");
//}

/*
void PulseLayer::overflow (pa_stream* s, void* userdata UNUSED)
{
    //PulseLayer* pulse = (PulseLayer*) userdata;
    pa_stream_drop (s);
    pa_stream_trigger (s, NULL, NULL);
}
*/


void PulseLayer::processPlaybackData (void)
{
    // Handle the data for the speakers
    if (playback && (playback->pulseStream()) && (pa_stream_get_state (playback->pulseStream()) == PA_STREAM_READY)) {

        // _debug("PulseLayer::processPlaybackData()");

        // If the playback buffer is full, we don't overflow it; wait for it to have free space
        if (pa_stream_writable_size (playback->pulseStream()) == 0)
            return;

        writeToSpeaker();
    }

}


void PulseLayer::processCaptureData (void)
{

    // Handle the mic
    // We check if the stream is ready
    if (record && (record->pulseStream()) && (pa_stream_get_state (record->pulseStream()) == PA_STREAM_READY))
        readFromMic();

}


void PulseLayer::processData (void)
{

    // Handle the data for the speakers
    if (playback && (playback->pulseStream()) && (pa_stream_get_state (playback->pulseStream()) == PA_STREAM_READY)) {

        // _debug("PulseLayer::processPlaybackData()");

        // If the playback buffer is full, we don't overflow it; wait for it to have free space
        if (pa_stream_writable_size (playback->pulseStream()) == 0)
            return;

        writeToSpeaker();
    }

    // Handle the mic
    // We check if the stream is ready
    if (record && (record->pulseStream()) && (pa_stream_get_state (record->pulseStream()) == PA_STREAM_READY))
        readFromMic();

}


void PulseLayer::writeToSpeaker (void)
{
    /** Bytes available in the urgent ringbuffer ( reserved for DTMF ) */
    int urgentAvailBytes;
    /** Bytes available in the regular ringbuffer ( reserved for voice ) */
    int normalAvailBytes;
    /** Bytes to get in the ring buffer **/
    int byteToGet;


    SFLDataFormat* out;// = (SFLDataFormat*)pa_xmalloc(framesPerBuffer);
    urgentAvailBytes = _urgentRingBuffer.AvailForGet();


    int writeableSize = pa_stream_writable_size (playback->pulseStream());
    // _debug("PulseLayer writablesize : %i", writeableSize);

    if (writeableSize < 0)
        _debug ("PulseLayer playback error : %s", pa_strerror (writeableSize));


    if (urgentAvailBytes > writeableSize) {

        // _debug("urgentAvailBytes: %i", urgentAvailBytes);

        // _debug("Play Urgent!");

        out = (SFLDataFormat*) pa_xmalloc (writeableSize);
        _urgentRingBuffer.Get (out, writeableSize, 100);
        // pa_threaded_mainloop_lock (m);
        pa_stream_write (playback->pulseStream(), out, writeableSize, NULL, 0, PA_SEEK_RELATIVE);
        // pa_threaded_mainloop_unlock (m);

        pa_xfree (out);

        // Consume the regular one as well (same amount of bytes)
        getMainBuffer()->discard (writeableSize);


    } else {

        AudioLoop* tone = _manager->getTelephoneTone();
        AudioLoop* file_tone = _manager->getTelephoneFile();

        // flush remaining samples in _urgentRingBuffer
        flushUrgent();

        if (tone != 0) {

            // _debug("PlayTone writeableSize: %i", writeableSize);

            if (playback->getStreamState() == PA_STREAM_READY) {

                // _debug("Play Sine Tone!");

                out = (SFLDataFormat*) pa_xmalloc (writeableSize);
                int copied = tone->getNext (out, writeableSize / sizeof (SFLDataFormat), 100);
                // pa_threaded_mainloop_lock (m);
                pa_stream_write (playback->pulseStream(), out, copied * sizeof (SFLDataFormat), NULL, 0, PA_SEEK_RELATIVE);
                // pa_threaded_mainloop_unlock (m);

                pa_xfree (out);

            }
        }

        else if (file_tone != 0) {

            if (playback->getStreamState() == PA_STREAM_READY) {
                // _debug("Play File Tone!");

                out = (SFLDataFormat*) pa_xmalloc (writeableSize);
                int copied = file_tone->getNext (out, writeableSize / sizeof (SFLDataFormat), 100);
                // pa_threaded_mainloop_lock (m);
                pa_stream_write (playback->pulseStream(), out, copied * sizeof (SFLDataFormat), NULL, 0, PA_SEEK_RELATIVE);

                pa_xfree (out);
                // pa_threaded_mainloop_unlock (m);

            }

        } else {

            int _mainBufferSampleRate = getMainBuffer()->getInternalSamplingRate();

            int maxNbBytesToGet = 0;


            // test if audio resampling is needed

            if (_mainBufferSampleRate && ( (int) _audioSampleRate != _mainBufferSampleRate)) {

                // upsamplefactor is used to compute the number of bytes to get in the ring buffer
                double upsampleFactor = (double) _mainBufferSampleRate / _audioSampleRate;

                maxNbBytesToGet = ( (double) writeableSize * upsampleFactor);

            } else {

                // maxNbSamplesToGet = framesPerBuffer;
                maxNbBytesToGet = writeableSize;

            }

            normalAvailBytes = getMainBuffer()->availForGet();

            byteToGet = (normalAvailBytes < (int) (maxNbBytesToGet)) ? normalAvailBytes : maxNbBytesToGet;
	    // _debug("byteToGet: %i", byteToGet);

            if (byteToGet) {

                // Sending an odd number of byte breaks the audio!
                // TODO, find out where the problem occurs to get rid of this hack
                if ( (byteToGet%2) != 0)
                    byteToGet = byteToGet-1;

                out = (SFLDataFormat*) pa_xmalloc (maxNbBytesToGet);

                getMainBuffer()->getData (out, byteToGet, 100);

                // test if resampling is required
                if (_mainBufferSampleRate && ( (int) _audioSampleRate != _mainBufferSampleRate)) {

                    SFLDataFormat* rsmpl_out = (SFLDataFormat*) pa_xmalloc (writeableSize);

                    // Do sample rate conversion
                    int nb_sample_down = byteToGet / sizeof (SFLDataFormat);

                    int nbSample = _converter->upsampleData ( (SFLDataFormat*) out, rsmpl_out, _mainBufferSampleRate, _audioSampleRate, nb_sample_down);

                    if ( (nbSample*sizeof (SFLDataFormat)) > (unsigned int) writeableSize)
                        _debug ("Error: nbsbyte exceed buffer length");

                    // pa_threaded_mainloop_lock (m);
                    pa_stream_write (playback->pulseStream(), rsmpl_out, nbSample*sizeof (SFLDataFormat), NULL, 0, PA_SEEK_RELATIVE);

                    // pa_threaded_mainloop_unlock (m);
                    pa_xfree (rsmpl_out);

                } else {

                    pa_stream_write (playback->pulseStream(), out, byteToGet, NULL, 0, PA_SEEK_RELATIVE);

                }

                pa_xfree (out);

            } else {

                if ( (tone == 0) && (file_tone == 0)) {

                    SFLDataFormat* zeros = (SFLDataFormat*) pa_xmalloc (writeableSize);

                    bzero (zeros, writeableSize);

                    // pa_threaded_mainloop_lock (m);
                    pa_stream_write (playback->pulseStream(), zeros, writeableSize, NULL, 0, PA_SEEK_RELATIVE);
                    // pa_threaded_mainloop_unlock (m);

                    pa_xfree (zeros);

                }
            }


            _urgentRingBuffer.Discard (byteToGet);

        }

    }

}

void PulseLayer::readFromMic (void)
{
    const char* data = NULL;
    size_t r;

    // if (record->getStreamState()

    int readableSize = pa_stream_readable_size (record->pulseStream());

    // _debug("readableSize: %i", readableSize);

    if (pa_stream_peek (record->pulseStream() , (const void**) &data , &r) < 0 || !data) {
        _debug ("pa_stream_peek() failed: %s" , pa_strerror (pa_context_errno (context)));
    }


    if (data != 0) {

        int _mainBufferSampleRate = getMainBuffer()->getInternalSamplingRate();

        // test if resampling is required

        if (_mainBufferSampleRate && ( (int) _audioSampleRate != _mainBufferSampleRate)) {



            SFLDataFormat* rsmpl_out = (SFLDataFormat*) pa_xmalloc (readableSize);
            // _debug("Byte read: %i", r);
            int nbSample = r / sizeof (SFLDataFormat);

            int nb_sample_up = nbSample;


            nbSample = _converter->downsampleData ( (SFLDataFormat*) data, rsmpl_out, _mainBufferSampleRate, _audioSampleRate, nb_sample_up);

            // remove dc offset
            dcblocker->filter_signal (rsmpl_out, nbSample);

            getMainBuffer()->putData ( (void*) rsmpl_out, nbSample*sizeof (SFLDataFormat), 100);

            pa_xfree (rsmpl_out);

        } else {

            // no resampling required
            getMainBuffer()->putData ( (void*) data, r, 100);
        }



    }

    if (pa_stream_drop (record->pulseStream()) < 0) {
        //_debug("pa_stream_drop() failed: %s" , pa_strerror( pa_context_errno( context) ));
    }

    // pa_threaded_mainloop_unlock (m);
}

static void retrieve_server_info (pa_context *c UNUSED, const pa_server_info *i, void *userdata UNUSED)
{
    _debug ("Server Info: Process owner : %s" , i->user_name);
    _debug ("\t\tServer name : %s - Server version = %s" , i->server_name, i->server_version);
    _debug ("\t\tDefault sink name : %s" , i->default_sink_name);
    _debug ("\t\tDefault source name : %s" , i->default_source_name);
}

static void reduce_sink_list_cb (pa_context *c UNUSED, const pa_sink_input_info *i, int eol, void *userdata)
{
    PulseLayer* pulse = (PulseLayer*) userdata;

    if (!eol) {
        //_debug("Sink Info: index : %i" , i->index);
        //_debug("\t\tClient : %i" , i->client);
        //_debug("\t\tVolume : %i" , i->volume.values[0]);
        //_debug("\t\tChannels : %i" , i->volume.channels);
        if (strcmp (i->name , PLAYBACK_STREAM_NAME) != 0)
            pulse->setSinkVolume (i->index , i->volume.channels, 10);
    }
}

static void restore_sink_list_cb (pa_context *c UNUSED, const pa_sink_input_info *i, int eol, void *userdata)
{
    PulseLayer* pulse = (PulseLayer*) userdata;

    if (!eol) {
        //_debug("Sink Info: index : %i" , i->index);
        //_debug("\t\tSink name : -%s-" , i->name);
        //_debug("\t\tClient : %i" , i->client);
        //_debug("\t\tVolume : %i" , i->volume.values[0]);
        //_debug("\t\tChannels : %i" , i->volume.channels);
        if (strcmp (i->name , PLAYBACK_STREAM_NAME) != 0)
            pulse->setSinkVolume (i->index , i->volume.channels, 100);
    }
}

static void set_playback_volume_cb (pa_context *c UNUSED, const pa_sink_input_info *i, int eol, void *userdata)
{
    PulseLayer* pulse;
    int volume;

    pulse = (PulseLayer*) userdata;
    volume = pulse->getSpkrVolume();

    if (!eol) {
        if (strcmp (i->name , PLAYBACK_STREAM_NAME) == 0)
            pulse->setSinkVolume (i->index , i->volume.channels, volume);
    }
}

static void set_capture_volume_cb (pa_context *c UNUSED, const pa_source_output_info *i, int eol, void *userdata)
{
    PulseLayer* pulse;
    int volume;

    pulse = (PulseLayer*) userdata;
    volume = pulse->getMicVolume();

    if (!eol) {
        if (strcmp (i->name , CAPTURE_STREAM_NAME) == 0)
            pulse->setSourceVolume (i->index , i->channel_map.channels, volume);
    }
}

void
PulseLayer::reducePulseAppsVolume (void)
{
    pa_context_get_sink_input_info_list (context , reduce_sink_list_cb , this);
}

void
PulseLayer::restorePulseAppsVolume (void)
{
    pa_context_get_sink_input_info_list (context , restore_sink_list_cb , this);
}

void
PulseLayer::serverinfo (void)
{
    pa_context_get_server_info (context , retrieve_server_info , NULL);
}


void PulseLayer::setSinkVolume (int index, int channels, int volume)
{

    pa_cvolume cvolume;
    pa_volume_t vol = PA_VOLUME_NORM * ( (double) volume / 100) ;

    pa_cvolume_set (&cvolume , channels , vol);
    _debug ("Set sink volume of index %i" , index);
    pa_context_set_sink_input_volume (context, index, &cvolume, NULL, NULL) ;

}

void PulseLayer::setSourceVolume (int index, int channels, int volume)
{

    pa_cvolume cvolume;
    pa_volume_t vol = PA_VOLUME_NORM * ( (double) volume / 100) ;

    pa_cvolume_set (&cvolume , channels , vol);
    _debug ("Set source volume of index %i" , index);
    pa_context_set_source_volume_by_index (context, index, &cvolume, NULL, NULL);

}


void PulseLayer::setPlaybackVolume (int volume)
{
    setSpkrVolume (volume);
    pa_context_get_sink_input_info_list (context , set_playback_volume_cb , this);
}

void PulseLayer::setCaptureVolume (int volume)
{
    setMicVolume (volume);
    pa_context_get_source_output_info_list (context , set_capture_volume_cb , this);
}

