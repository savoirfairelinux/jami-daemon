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

#include <algorithm> // for std::find
#include "audiostream.h"
#include "pulselayer.h"
#include "audio/samplerateconverter.h"
#include "audio/dcblocker.h"
#include "managerimpl.h"

namespace
{
void playback_callback (pa_stream* s, size_t bytes, void* userdata)
{
    assert (s && bytes);
    assert (bytes > 0);
    static_cast<PulseLayer*> (userdata)->processPlaybackData();
}

void capture_callback (pa_stream* s, size_t bytes, void* userdata)
{
    assert (s && bytes);
    assert (bytes > 0);
    static_cast<PulseLayer*> (userdata)->processCaptureData();
}

void ringtone_callback (pa_stream* s, size_t bytes, void* userdata)
{

    assert (s && bytes);
    assert (bytes > 0);
    static_cast<PulseLayer*> (userdata)->processRingtoneData();

}

void stream_moved_callback (pa_stream *s, void *userdata UNUSED)
{
    _debug ("stream_moved_callback: stream %d to %d", pa_stream_get_index (s), pa_stream_get_device_index (s));
}

void latency_update_callback (pa_stream *p, void *userdata UNUSED)
{

    pa_usec_t r_usec;

    pa_stream_get_latency (p, &r_usec, NULL);

    /*
    _debug ("Audio: Stream letency update %0.0f ms for device %s", (float) r_usec/1000, pa_stream_get_device_name (p));
    _debug ("Audio: maxlength %u", pa_stream_get_buffer_attr (p)->maxlength);
    _debug ("Audio: tlength %u", pa_stream_get_buffer_attr (p)->tlength);
    _debug ("Audio: prebuf %u", pa_stream_get_buffer_attr (p)->prebuf);
    _debug ("Audio: minreq %u", pa_stream_get_buffer_attr (p)->minreq);
    _debug ("Audio: fragsize %u", pa_stream_get_buffer_attr (p)->fragsize);
    */
}

void sink_input_info_callback (pa_context *c UNUSED, const pa_sink_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];

    if (eol)
		return;

	_debug ("Sink %u\n"
			"    Name: %s\n"
			"    Driver: %s\n"
			"    Description: %s\n"
			"    Sample Specification: %s\n"
			"    Channel Map: %s\n"
			"    Owner Module: %u\n"
			"    Volume: %s\n"
			"    Monitor Source: %u\n"
			"    Latency: %0.0f usec\n"
			"    Flags: %s%s%s\n",
			i->index,
			i->name,
			i->driver,
			i->description,
			pa_sample_spec_snprint (s, sizeof (s), &i->sample_spec),
			pa_channel_map_snprint (cm, sizeof (cm), &i->channel_map),
			i->owner_module,
			i->mute ? "muted" : pa_cvolume_snprint (cv, sizeof (cv), &i->volume),
			i->monitor_source,
			(double) i->latency,
			i->flags & PA_SINK_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
			i->flags & PA_SINK_LATENCY ? "LATENCY " : "",
			i->flags & PA_SINK_HARDWARE ? "HARDWARE" : "");

	((PulseLayer *)userdata)->getSinkList()->push_back (i->name);
}

void source_input_info_callback (pa_context *c UNUSED, const pa_source_info *i, int eol, void *userdata)
{
    char s[PA_SAMPLE_SPEC_SNPRINT_MAX], cv[PA_CVOLUME_SNPRINT_MAX], cm[PA_CHANNEL_MAP_SNPRINT_MAX];

    if (!eol)
		return;
	_debug ("Sink %u\n"
			"    Name: %s\n"
			"    Driver: %s\n"
			"    Description: %s\n"
			"    Sample Specification: %s\n"
			"    Channel Map: %s\n"
			"    Owner Module: %u\n"
			"    Volume: %s\n"
			"    Monitor if Sink: %u\n"
			"    Latency: %0.0f usec\n"
			"    Flags: %s%s%s\n",
			i->index,
			i->name,
			i->driver,
			i->description,
			pa_sample_spec_snprint (s, sizeof (s), &i->sample_spec),
			pa_channel_map_snprint (cm, sizeof (cm), &i->channel_map),
			i->owner_module,
			i->mute ? "muted" : pa_cvolume_snprint (cv, sizeof (cv), &i->volume),
			i->monitor_of_sink,
			(double) i->latency,
			i->flags & PA_SOURCE_HW_VOLUME_CTRL ? "HW_VOLUME_CTRL " : "",
			i->flags & PA_SOURCE_LATENCY ? "LATENCY " : "",
			i->flags & PA_SOURCE_HARDWARE ? "HARDWARE" : "");

	((PulseLayer *)userdata)->getSourceList()->push_back (i->name);
}

void context_changed_callback (pa_context* c, pa_subscription_event_type_t t, uint32_t idx UNUSED, void* userdata)
{

    switch (t) {
        case PA_SUBSCRIPTION_EVENT_SINK:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_SINK");
            ( (PulseLayer *) userdata)->getSinkList()->clear();
            pa_context_get_sink_info_list (c, sink_input_info_callback,  userdata);
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_SOURCE");
            ( (PulseLayer *) userdata)->getSourceList()->clear();
            pa_context_get_source_info_list (c, source_input_info_callback,  userdata);
            break;
        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_SINK_INPUT");
            break;
        case PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT");
            break;
        case PA_SUBSCRIPTION_EVENT_MODULE:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_MODULE");
            break;
        case PA_SUBSCRIPTION_EVENT_CLIENT:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_CLIENT");
            break;
        case PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_SAMPLE_CACHE");
            break;
        case PA_SUBSCRIPTION_EVENT_SERVER:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_SERVER");
            break;
        case PA_SUBSCRIPTION_EVENT_CARD:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_CARD");
            break;
        case PA_SUBSCRIPTION_EVENT_FACILITY_MASK:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_FACILITY_MASK");
            break;
        case PA_SUBSCRIPTION_EVENT_CHANGE:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_CHANGE");
            break;
        case PA_SUBSCRIPTION_EVENT_REMOVE:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_REMOVE");
            ( (PulseLayer *) userdata)->getSinkList()->clear();
            ( (PulseLayer *) userdata)->getSourceList()->clear();
            pa_context_get_sink_info_list (c, sink_input_info_callback,  userdata);
            pa_context_get_source_info_list (c, source_input_info_callback,  userdata);
            break;
        case PA_SUBSCRIPTION_EVENT_TYPE_MASK:
            _debug ("Audio: PA_SUBSCRIPTION_EVENT_TYPE_MASK");
            break;
        default:
            _debug ("Audio: Unknown event type");

    }
}

void playback_underflow_callback (pa_stream* s UNUSED,  void* userdata UNUSED)
{}

void playback_overflow_callback (pa_stream* s UNUSED, void* userdata UNUSED)
{}
} // end anonymous namespace


PulseLayer::PulseLayer ()
    : AudioLayer (PULSEAUDIO)
    , context_(0)
    , mainloop_(0)
    , playback_(0)
    , record_(0)
    , ringtone_(0)
    , converter_(0)
{
    urgentRingBuffer_.createReadPointer();

    openLayer();
}

// Destructor
PulseLayer::~PulseLayer (void)
{
    closeLayer ();
    delete converter_;
}

void
PulseLayer::openLayer (void)
{
	if (isStarted_)
		return;

	_info ("Audio: Open Pulseaudio layer");

	connectPulseAudioServer();

	isStarted_ = true;
}

void
PulseLayer::closeLayer (void)
{
    _info ("Audio: Close Pulseaudio layer");

    isStarted_ = false;

    disconnectAudioStream();

    if (mainloop_)
        pa_threaded_mainloop_stop (mainloop_);

    if (context_) {
        pa_context_disconnect (context_);
        pa_context_unref (context_);
        context_ = NULL;
    }

    if (mainloop_) {
        pa_threaded_mainloop_free (mainloop_);
        mainloop_ = NULL;
    }
}

void
PulseLayer::connectPulseAudioServer (void)
{
    _info ("Audio: Connect to Pulseaudio server");

    setenv ("PULSE_PROP_media.role", "phone", 1);

    pa_context_flags_t flag = PA_CONTEXT_NOAUTOSPAWN ;

    if (!mainloop_) {

        // Instantiate a mainloop
        _info ("Audio: Creating PulseAudio mainloop");

        if (! (mainloop_ = pa_threaded_mainloop_new()))
            _warn ("Audio: Error: while creating pulseaudio mainloop");

        assert (mainloop_);
    }

    if (!context_) {

        // Instantiate a context
        if (! (context_ = pa_context_new (pa_threaded_mainloop_get_api (mainloop_) , "SFLphone")))
            _warn ("Audio: Error: while creating pulseaudio context");

        assert (context_);
    }

    // set context state callback before starting the mainloop
    pa_context_set_state_callback (context_, context_state_callback, this);

    _info ("Audio: Connect the context to the server");

    if (pa_context_connect (context_, NULL , flag , NULL) < 0) {
        _warn ("Audio: Error: Could not connect context to the server");
    }

    // Lock the loop before starting it
    pa_threaded_mainloop_lock (mainloop_);

    if (pa_threaded_mainloop_start (mainloop_) < 0)
        _warn ("Audio: Error: Failed to start pulseaudio mainloop");

    pa_threaded_mainloop_wait (mainloop_);

    // Run the main loop
    if (pa_context_get_state (context_) != PA_CONTEXT_READY) {
        _warn ("Audio: Error: connecting to pulse audio server");
    }

    pa_threaded_mainloop_unlock (mainloop_);

    _info ("Audio: Context creation done");
}

void PulseLayer::context_state_callback (pa_context* c, void* user_data)
{
    _info ("Audio: The state of the context changed");
    PulseLayer* pulse = (PulseLayer*) user_data;
    assert (c && pulse->mainloop_);

    switch (pa_context_get_state (c)) {

        case PA_CONTEXT_CONNECTING:

        case PA_CONTEXT_AUTHORIZING:

        case PA_CONTEXT_SETTING_NAME:
            _debug ("Audio: Waiting....");
            break;

        case PA_CONTEXT_READY:
            _debug ("Audio: Connection to PulseAudio server established");
            pa_threaded_mainloop_signal (pulse->mainloop_, 0);
            pa_context_subscribe (c, (pa_subscription_mask_t) (PA_SUBSCRIPTION_MASK_SINK|
                                  PA_SUBSCRIPTION_MASK_SOURCE), NULL, pulse);
            pa_context_set_subscribe_callback (c, context_changed_callback, pulse);
            pulse->updateSinkList();
            break;

        case PA_CONTEXT_TERMINATED:
            _debug ("Audio: Context terminated");
            break;

        case PA_CONTEXT_FAILED:

        default:
            _warn ("Audio: Error : %s" , pa_strerror (pa_context_errno (c)));
            pa_threaded_mainloop_signal (pulse->mainloop_, 0);
            pulse->disconnectAudioStream();
            break;
    }
}

void PulseLayer::openDevice (int indexIn UNUSED, int indexOut UNUSED, int indexRing UNUSED, int sampleRate, int frameSize , int stream UNUSED, const std::string &plugin UNUSED)
{
    _debug ("Audio: Open device sampling rate %d, frame size %d", audioSampleRate_, frameSize_);

    audioSampleRate_ = sampleRate;
    frameSize_ = frameSize;

    flushUrgent();

    // use 1 sec buffer for resampling
    converter_ = new SamplerateConverter (audioSampleRate_);
}


void PulseLayer::updateSinkList (void)
{
    _debug ("Audio: Update sink list");

    getSinkList()->clear();

    pa_context_get_sink_info_list (context_, sink_input_info_callback,  this);
}

void PulseLayer::updateSourceList (void)
{
    _debug ("Audio: Update source list");

    getSourceList()->clear();

    pa_context_get_source_info_list (context_, source_input_info_callback, this);

}

bool PulseLayer::inSinkList (const std::string &deviceName) const
{
    return std::find(sinkList_.begin(), sinkList_.end(), deviceName) != sinkList_.end();
}


bool PulseLayer::inSourceList (const std::string &deviceName) const
{
    return std::find(sourceList_.begin(), sourceList_.end(), deviceName) != sourceList_.end();
}


void PulseLayer::createStreams (pa_context* c)
{
    _info ("Audio: Create streams");

    playback_ = new AudioStream (c, mainloop_, PLAYBACK_STREAM_NAME, PLAYBACK_STREAM, audioSampleRate_);

    std::string playbackDevice(audioPref.getDevicePlayback());
    std::string recordDevice(audioPref.getDeviceRecord());
    std::string ringtoneDevice(audioPref.getDeviceRingtone());

    _debug ("Audio: Device for playback: %s", playbackDevice.c_str());
    _debug ("Audio: Device for record: %s", recordDevice.c_str());
    _debug ("Audio: Device for ringtone: %s", ringtoneDevice.c_str());

    if (inSinkList (playbackDevice))
        playback_->connectStream (&playbackDevice);
    else
        playback_->connectStream (NULL);

    pa_stream_set_write_callback (playback_->pulseStream(), playback_callback, this);
    pa_stream_set_overflow_callback (playback_->pulseStream(), playback_overflow_callback, this);
    pa_stream_set_underflow_callback (playback_->pulseStream(), playback_underflow_callback, this);
    pa_stream_set_moved_callback (playback_->pulseStream(), stream_moved_callback, this);
    pa_stream_set_latency_update_callback (playback_->pulseStream(), latency_update_callback, this);

    record_ = new AudioStream (c, mainloop_, CAPTURE_STREAM_NAME, CAPTURE_STREAM, audioSampleRate_);

    if (inSourceList (recordDevice))
        record_->connectStream (&recordDevice);
    else
        record_->connectStream (NULL);

    pa_stream_set_read_callback (record_->pulseStream() , capture_callback, this);
    pa_stream_set_moved_callback (record_->pulseStream(), stream_moved_callback, this);
    pa_stream_set_latency_update_callback (record_->pulseStream(), latency_update_callback, this);

    ringtone_ = new AudioStream (c, mainloop_, RINGTONE_STREAM_NAME, RINGTONE_STREAM, audioSampleRate_);

    if (inSourceList (ringtoneDevice))
        ringtone_->connectStream (&ringtoneDevice);
    else
        ringtone_->connectStream (NULL);

    pa_stream_set_write_callback (ringtone_->pulseStream(), ringtone_callback, this);
    pa_stream_set_moved_callback (ringtone_->pulseStream(), stream_moved_callback, this);

    pa_threaded_mainloop_signal (mainloop_, 0);

    flushMain();
    flushUrgent();
}


void PulseLayer::disconnectAudioStream (void)
{
    _info ("Audio: Disconnect audio stream");

    closePlaybackStream();
    closeCaptureStream();
}


void PulseLayer::closeCaptureStream (void)
{
    if (record_) {

        if (record_->pulseStream()) {
            const char *name = pa_stream_get_device_name (record_->pulseStream());

            if (name && strlen (name)) {
                _debug ("Audio: record device to be stored in config: %s", name);
                audioPref.setDeviceRecord (name);
            }
        }

        delete record_;
        record_ = NULL;
    }
}


void PulseLayer::closePlaybackStream (void)
{
    if (playback_) {
        if (playback_->pulseStream()) {
            const char *name = pa_stream_get_device_name (playback_->pulseStream());

            if (name && strlen (name)) {
                _debug ("Audio: playback device to be stored in config: %s", name);
                audioPref.setDevicePlayback (name);
            }
        }

        delete playback_;
        playback_ = NULL;
    }

    if (ringtone_) {
        if (ringtone_->pulseStream()) {
            const char *name = pa_stream_get_device_name (ringtone_->pulseStream());

            if (name && strlen (name)) {
                _debug ("Audio: ringtone device to be stored in config: %s", name);
                audioPref.setDeviceRingtone (name);
            }
        }

        delete ringtone_;
        ringtone_ = NULL;
    }
}


void PulseLayer::startStream (void)
{
    // Create Streams
    if (!playback_ or !record_)
        createStreams (context_);

    // Flush outside the if statement: every time start stream is
    // called is to notify a new event
    flushUrgent();
    flushMain();
}


void
PulseLayer::stopStream (void)
{
    _info ("Audio: Stop audio stream");

    pa_threaded_mainloop_lock (mainloop_);

    if (playback_)
        pa_stream_flush (playback_->pulseStream(), NULL, NULL);

    if (record_)
        pa_stream_flush (record_->pulseStream(), NULL, NULL);

    pa_threaded_mainloop_unlock (mainloop_);

    disconnectAudioStream();
}



void PulseLayer::processPlaybackData (void)
{
    // Handle the data for the speakers
    if (playback_ and playback_->pulseStream() and (pa_stream_get_state (playback_->pulseStream()) == PA_STREAM_READY)) {

        // If the playback buffer is full, we don't overflow it; wait for it to have free space
        if (pa_stream_writable_size (playback_->pulseStream()) == 0)
            return;

        writeToSpeaker();
    }

}

void PulseLayer::processCaptureData (void)
{
    // Handle the mic
    // We check if the stream is ready
    if (record_ and record_->pulseStream() and pa_stream_get_state (record_->pulseStream()) == PA_STREAM_READY)
        readFromMic();
}

void PulseLayer::processRingtoneData (void)
{
    // handle ringtone playback
    if (ringtone_ and ringtone_->pulseStream() and (pa_stream_get_state (ringtone_->pulseStream()) == PA_STREAM_READY)) {

        // If the playback buffer is full, we don't overflow it; wait for it to have free space
        if (pa_stream_writable_size (ringtone_->pulseStream()) == 0)
            return;

        ringtoneToSpeaker();
    }
}


void PulseLayer::processData (void)
{
    // Handle the data for the speakers
    if (playback_ and playback_->pulseStream() and (pa_stream_get_state (playback_->pulseStream()) == PA_STREAM_READY)) {

        // If the playback buffer is full, we don't overflow it; wait for it to have free space
        if (pa_stream_writable_size (playback_->pulseStream()) == 0)
            return;

        writeToSpeaker();
    }

    // Handle the mic
    // We check if the stream is ready
    if (record_ and record_->pulseStream() and (pa_stream_get_state (record_->pulseStream()) == PA_STREAM_READY))
        readFromMic();
}

void PulseLayer::writeToSpeaker (void)
{
    notifyincomingCall();

    // available bytes to be written in pulseaudio internal buffer
    int writeableSizeBytes = pa_stream_writable_size (playback_->pulseStream());
    if (writeableSizeBytes < 0) {
        _error ("Audio: playback error : %s", pa_strerror (writeableSizeBytes));
        return;
    }

    int urgentBytes = urgentRingBuffer_.AvailForGet();
    if (urgentBytes > writeableSizeBytes)
    	urgentBytes = writeableSizeBytes;
    if (urgentBytes) {
    	SFLDataFormat *out = (SFLDataFormat*) pa_xmalloc (urgentBytes);
        urgentRingBuffer_.Get (out, urgentBytes);
        pa_stream_write (playback_->pulseStream(), out, urgentBytes, NULL, 0, PA_SEEK_RELATIVE);
        pa_xfree (out);
        // Consume the regular one as well (same amount of bytes)
        getMainBuffer()->discard (urgentBytes);
        return;
    }

    AudioLoop *toneToPlay = Manager::instance().getTelephoneTone();
    if (toneToPlay) {
		if (playback_->getStreamState() == PA_STREAM_READY) {
			SFLDataFormat *out = (SFLDataFormat*) pa_xmalloc (writeableSizeBytes);
			toneToPlay->getNext (out, writeableSizeBytes / sizeof (SFLDataFormat), 100);
			pa_stream_write (playback_->pulseStream(), out, writeableSizeBytes, NULL, 0, PA_SEEK_RELATIVE);
			pa_xfree (out);
		}
		return;
    }

	flushUrgent(); // flush remaining samples in _urgentRingBuffer

	int availSamples = getMainBuffer()->availForGet() / sizeof(SFLDataFormat);
	if (availSamples == 0) {
		// play silence
		SFLDataFormat* zeros = (SFLDataFormat*) pa_xmalloc0(writeableSizeBytes);
		pa_stream_write (playback_->pulseStream(), zeros, writeableSizeBytes, NULL, 0, PA_SEEK_RELATIVE);
		pa_xfree (zeros);
		return;
	}

	unsigned int mainBufferSampleRate = getMainBuffer()->getInternalSamplingRate();
	bool resample = audioSampleRate_ != mainBufferSampleRate;

	// how much samples we can write in the output
	int outSamples = writeableSizeBytes / sizeof(SFLDataFormat);

	// how much samples we want to read from the buffer
	int inSamples = outSamples;

	double resampleFactor = 1.;
	if (resample) {
		resampleFactor = (double) audioSampleRate_ / mainBufferSampleRate;
		inSamples = (double) inSamples / resampleFactor;
	}

	if (inSamples > availSamples)
		inSamples = availSamples;
	int outBytes = (double)inSamples * resampleFactor * sizeof(SFLDataFormat);

	int inBytes = inSamples * sizeof (SFLDataFormat);
	SFLDataFormat *out = (SFLDataFormat*) pa_xmalloc (inBytes);
	getMainBuffer()->getData (out, inBytes);

	// test if resampling is required
	if (resample) {
		SFLDataFormat* rsmpl_out = (SFLDataFormat*) pa_xmalloc (outBytes);
		converter_->resample (out, rsmpl_out, mainBufferSampleRate, audioSampleRate_, inSamples);
		pa_stream_write (playback_->pulseStream(), rsmpl_out, outBytes, NULL, 0, PA_SEEK_RELATIVE);
		pa_xfree (rsmpl_out);
	} else
		pa_stream_write (playback_->pulseStream(), out, inBytes, NULL, 0, PA_SEEK_RELATIVE);

	pa_xfree (out);
}

void PulseLayer::readFromMic (void)
{
    const char* data = NULL;
    size_t r;

	unsigned int mainBufferSampleRate = getMainBuffer()->getInternalSamplingRate();
	bool resample = audioSampleRate_ != mainBufferSampleRate;

    if (pa_stream_peek (record_->pulseStream() , (const void**) &data , &r) < 0 or !data) {
        _error("Audio: Error capture stream peek failed: %s" , pa_strerror (pa_context_errno (context_)));
        goto end;
    }

	if (resample) {
		int inSamples = r / sizeof(SFLDataFormat);
		double resampleFactor = (double) audioSampleRate_ / mainBufferSampleRate;
		int outSamples = (double) inSamples * resampleFactor;
		int outBytes = outSamples * sizeof(SFLDataFormat);

		SFLDataFormat* rsmpl_out = (SFLDataFormat*) pa_xmalloc (outBytes);
		converter_->resample ( (SFLDataFormat *) data, rsmpl_out, mainBufferSampleRate, audioSampleRate_, inSamples);
		// remove dc offset
		dcblocker_.process(rsmpl_out, outBytes);
		getMainBuffer()->putData (rsmpl_out, outBytes);
		pa_xfree (rsmpl_out);
	} else {
		SFLDataFormat* filter_out = (SFLDataFormat*) pa_xmalloc (r);
		// remove dc offset
		dcblocker_.process( (SFLDataFormat *) data, filter_out, r);
		getMainBuffer()->putData (filter_out, r);
		pa_xfree (filter_out);
	}

end:
    if (pa_stream_drop (record_->pulseStream()) < 0)
        _error ("Audio: Error: capture stream drop failed: %s" , pa_strerror (pa_context_errno (context_)));
}


void PulseLayer::ringtoneToSpeaker (void)
{
    AudioLoop* fileToPlay = Manager::instance().getTelephoneFile();
    int writableSize = pa_stream_writable_size (ringtone_->pulseStream());

    if (fileToPlay) {

        if (ringtone_->getStreamState() != PA_STREAM_READY) {
            _error("PulseAudio: Error: Ringtone stream not in state ready");
            return;
        }

        SFLDataFormat *out = (SFLDataFormat *) pa_xmalloc (writableSize);
        if (out == NULL) {
            _error("PulseAudio: Error: Could not allocate memory for buffer");
            return;
        }

        memset (out, 0, writableSize);

        fileToPlay->getNext (out, writableSize/sizeof (SFLDataFormat), 100);
        pa_stream_write (ringtone_->pulseStream(), out, writableSize, NULL, 0, PA_SEEK_RELATIVE);

        pa_xfree (out);
    } 
    else {

        if (ringtone_->getStreamState() != PA_STREAM_READY) {
            _error("PulseAudio: Error: Ringtone stream not in state ready");
       	    return;
        }

        SFLDataFormat *out = (SFLDataFormat*) pa_xmalloc (writableSize);
        memset (out, 0, writableSize);

        pa_stream_write (ringtone_->pulseStream(), out, writableSize, NULL, 0, PA_SEEK_RELATIVE);

        pa_xfree (out);
    }
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
    pa_context_get_sink_input_info_list (context_, reduce_sink_list_cb , this);
}

void
PulseLayer::restorePulseAppsVolume (void)
{
    pa_context_get_sink_input_info_list (context_, restore_sink_list_cb , this);
}

void
PulseLayer::serverinfo (void)
{
    pa_context_get_server_info (context_, retrieve_server_info , NULL);
}


void PulseLayer::setSinkVolume (int index, int channels, int volume)
{
    pa_cvolume cvolume;
    pa_volume_t vol = PA_VOLUME_NORM * ( (double) volume / 100) ;

    pa_cvolume_set (&cvolume , channels , vol);
    _debug ("Set sink volume of index %i" , index);
    pa_context_set_sink_input_volume (context_, index, &cvolume, NULL, NULL) ;

}

void PulseLayer::setSourceVolume (int index, int channels, int volume)
{

    pa_cvolume cvolume;
    pa_volume_t vol = PA_VOLUME_NORM * ( (double) volume / 100) ;

    pa_cvolume_set (&cvolume , channels , vol);
    _debug ("Set source volume of index %i" , index);
    pa_context_set_source_volume_by_index (context_, index, &cvolume, NULL, NULL);
}


void PulseLayer::setPlaybackVolume (int volume)
{
    setSpkrVolume (volume);
    pa_context_get_sink_input_info_list (context_, set_playback_volume_cb , this);
}

void PulseLayer::setCaptureVolume (int volume)
{
    setMicVolume (volume);
    pa_context_get_source_output_info_list (context_, set_capture_volume_cb , this);
}

