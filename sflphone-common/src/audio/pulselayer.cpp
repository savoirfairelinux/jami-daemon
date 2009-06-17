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

#include "pulselayer.h"

int framesPerBuffer = 2048;

int PulseLayer::streamState;

static  void audioCallback ( pa_stream* s, size_t bytes, void* userdata )
{ 
    assert( s && bytes );
    assert( bytes > 0 );
    static_cast<PulseLayer*>(userdata)->processData();
}



    PulseLayer::PulseLayer(ManagerImpl* manager)
    : AudioLayer( manager , PULSEAUDIO ) 
    , context(NULL)
    , m(NULL)
    , playback()
    , record()
{
    PulseLayer::streamState = 0;
    _debug("PulseLayer::Pulse audio constructor: Create context\n");
    
}

// Destructor
PulseLayer::~PulseLayer (void) 
{ 
    closeLayer ();

    // pa_context_disconnect( context );  
    // pa_context_unref( context );
}

bool
PulseLayer::closeLayer( void )
{ 
    _debug("PulseLayer::closeLayer :: Destroy pulselayer\n");

    playback->disconnectStream(); 
    record->disconnectStream();

    while(PulseLayer::streamState != 2);
    PulseLayer::streamState = 0; 

    //TODO  Remove this ugly hack
    sleep(2);

    pa_context_disconnect( context );  
    pa_context_unref( context );

    return true;
}

void
PulseLayer::connectPulseAudioServer( void )
{
    _debug("PulseLayer::connectPulseAudioServer \n");
    pa_context_flags_t flag = PA_CONTEXT_NOAUTOSPAWN ;  

    pa_threaded_mainloop_lock( m );

    _debug("Connect the context to the server\n");
    pa_context_connect( context, NULL , flag , NULL ); 

    pa_context_set_state_callback(context, context_state_callback, this);
    pa_threaded_mainloop_wait( m );

    // Run the main loop
    if( pa_context_get_state( context ) != PA_CONTEXT_READY ){
        _debug("Error connecting to pulse audio server\n");
        pa_threaded_mainloop_unlock( m );
    }

    pa_threaded_mainloop_unlock( m );
    //serverinfo();
    //muteAudioApps(99);
    _debug("Context creation done\n");

}

void PulseLayer::context_state_callback( pa_context* c, void* user_data )
{
    _debug("PulseLayer::context_state_callback ::The state of the context changed\n");
    PulseLayer* pulse = (PulseLayer*)user_data;
    assert(c && pulse->m);
    switch(pa_context_get_state(c)){
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            _debug("Waiting....\n");
            break;
        case PA_CONTEXT_READY:
            pulse->createStreams( c );
            _debug("Connection to PulseAudio server established\n");
            	
            break;
        case PA_CONTEXT_TERMINATED:
            _debug("Context terminated\n");
            break;
        case PA_CONTEXT_FAILED:
        default:
            _debug(" Error : %s\n" , pa_strerror(pa_context_errno(c)));
            pulse->disconnectPulseAudioServer();
            exit(0);
            break;
    }
}

bool PulseLayer::disconnectPulseAudioServer( void )
{
    _debug(" PulseLayer::disconnectPulseAudioServer( void ) \n");

    if( playback ){
      // playback->disconnectStream();
        delete playback; playback=NULL;
    }
    if( record ){
      // record->disconnectStream();
        delete record; record=NULL;
    }
    if (!playback && !record)
      return true;
    else
      return false;
}


bool PulseLayer::createStreams( pa_context* c )
{
    _debug("PulseLayer::createStreams \n");
    
    playback = new AudioStream(c, PLAYBACK_STREAM, PLAYBACK_STREAM_NAME, _manager->getSpkrVolume());
    playback->connectStream();
    pa_stream_set_write_callback( playback->pulseStream(), audioCallback, this);
    // pa_stream_set_overflow_callback( playback->pulseStream() , overflow , this);
    // pa_stream_set_suspended_callback( playback->pulseStream(), stream_suspended_callback, this);
    
    record = new AudioStream(c, CAPTURE_STREAM, CAPTURE_STREAM_NAME , _manager->getMicVolume());
    record->connectStream();
    pa_stream_set_read_callback( record->pulseStream() , audioCallback, this);
    // pa_stream_set_underflow_callback( record->pulseStream() , underflow , this);
    // pa_stream_set_suspended_callback(record->pulseStream(), stream_suspended_callback, this);


    pa_threaded_mainloop_signal(m , 0);

    isCorked = true;

    return true;
}


bool PulseLayer::openDevice(int indexIn UNUSED, int indexOut UNUSED, int sampleRate, int frameSize , int stream UNUSED, std::string plugin UNUSED) 
{

    _debug("PulseLayer::openDevice \n");
    _sampleRate = sampleRate;
    _frameSize = frameSize;	

    m = pa_threaded_mainloop_new();
    assert(m);

    if( pa_threaded_mainloop_start( m ) < 0  ){
        _debug("Failed starting the mainloop\n");
    }

    // Instanciate a context
    if( !(context = pa_context_new( pa_threaded_mainloop_get_api( m ) , "SFLphone" )))
        _debug("Error while creating the context\n");

    assert(context);

    connectPulseAudioServer();
 
    // startStream();

    _debug("Connection Done!! \n");

}

void PulseLayer::closeCaptureStream( void )
{
}

void PulseLayer::closePlaybackStream( void )
{ 
}

int PulseLayer::canGetMic()
{
    if( record )
        return  _micRingBuffer.AvailForGet();
    else
        return 0;
}

int PulseLayer::getMic(void *buffer, int toCopy)
{
    if( record ){
        return _micRingBuffer.Get(buffer, toCopy, 100);
    }
    else
        return 0;
}

void PulseLayer::startStream (void) 
{
        _debug("PulseLayer::Start stream\n");
        _urgentRingBuffer.flush();
        _micRingBuffer.flush();
        _voiceRingBuffer.flush();

        pa_threaded_mainloop_lock(m);
   
        pa_stream_cork( playback->pulseStream(), 0, NULL, NULL);
        pa_stream_cork( record->pulseStream(), 0, NULL, NULL);
  
        pa_threaded_mainloop_unlock(m);

	isCorked = false;

}

    void 
PulseLayer::stopStream (void) 
{

        _debug("PulseLayer::Stop stream\n");
        pa_stream_flush( playback->pulseStream(), NULL, NULL );
	pa_stream_flush( record->pulseStream(), NULL, NULL );
    
	flushMic();
	flushMain();
	flushUrgent();

	pa_stream_cork( playback->pulseStream(), 1, NULL, NULL);
	pa_stream_cork( record->pulseStream(), 1, NULL, NULL);

	isCorked = true;
	
}



void PulseLayer::underflow ( pa_stream* s UNUSED,  void* userdata UNUSED )
{ 
    _debug("PulseLayer::Buffer Underflow\n");
}


void PulseLayer::overflow ( pa_stream* s, void* userdata UNUSED )
{ 
    //PulseLayer* pulse = (PulseLayer*) userdata;
    pa_stream_drop( s );
    pa_stream_trigger( s, NULL, NULL);
}

void PulseLayer::stream_suspended_callback(pa_stream *s, void *userdata UNUSED ) 
{
    
}

void PulseLayer::processData( void )
{

        // Handle the mic
        // We check if the stream is ready
        if( (record->pulseStream()) && (pa_stream_get_state( record->pulseStream()) == PA_STREAM_READY)) 
            readFromMic();

        // _debug("PulseLayer::processData() playback->pulseStream() \n");
    
        // Handle the data for the speakers
        if( (playback->pulseStream()) && (pa_stream_get_state( playback->pulseStream()) == PA_STREAM_READY)){

        // If the playback buffer is full, we don't overflow it; wait for it to have free space
        if( pa_stream_writable_size(playback->pulseStream()) == 0 )
            return;
        
        writeToSpeaker();
    }
}

void PulseLayer::writeToSpeaker( void )
{   
    /** Bytes available in the urgent ringbuffer ( reserved for DTMF ) */
    int urgentAvail; 
    /** Bytes available in the regular ringbuffer ( reserved for voice ) */
    int normalAvail; 
    int toGet;
    int toPlay;

    SFLDataFormat* out;// = (SFLDataFormat*)pa_xmalloc(framesPerBuffer);
    urgentAvail = _urgentRingBuffer.AvailForGet();

    if (urgentAvail > 0) {
        // Urgent data (dtmf, incoming call signal) come first.		
        //_debug("Play urgent!: %i\e" , urgentAvail);
        toGet = (urgentAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? urgentAvail : framesPerBuffer * sizeof(SFLDataFormat);
        out =  (SFLDataFormat*)pa_xmalloc(toGet * sizeof(SFLDataFormat) );
        _urgentRingBuffer.Get(out, toGet, 100);
        pa_stream_write( playback->pulseStream() , out , toGet  , pa_xfree, 0 , PA_SEEK_RELATIVE);
        // Consume the regular one as well (same amount of bytes)
        _voiceRingBuffer.Discard(toGet);
    }
    else
    {
        AudioLoop* tone = _manager->getTelephoneTone();
        if ( tone != 0) {
            toGet = framesPerBuffer;
            out =  (SFLDataFormat*)pa_xmalloc(toGet * sizeof(SFLDataFormat) );
            tone->getNext(out, toGet , 100);
            pa_stream_write( playback->pulseStream() , out , toGet  * sizeof(SFLDataFormat)   , pa_xfree, 0 , PA_SEEK_RELATIVE);
        } 
        if ( (tone=_manager->getTelephoneFile()) != 0 ) {
            toGet = framesPerBuffer;
            toPlay = ( (int)(toGet * sizeof(SFLDataFormat)) > framesPerBuffer )? framesPerBuffer : toGet * sizeof(SFLDataFormat) ;
            out =  (SFLDataFormat*)pa_xmalloc(toPlay);
            tone->getNext(out, toPlay/2 , 100);
            pa_stream_write( playback->pulseStream() , out , toPlay   , pa_xfree, 0 , PA_SEEK_RELATIVE) ; 
        } 
        else {
            out =  (SFLDataFormat*)pa_xmalloc(framesPerBuffer * sizeof(SFLDataFormat));
            normalAvail = _voiceRingBuffer.AvailForGet();
            toGet = (normalAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? normalAvail : framesPerBuffer * sizeof(SFLDataFormat);
            if (toGet) {
                _voiceRingBuffer.Get(out, toGet, 100);
                _voiceRingBuffer.Discard(toGet);
            } 
            else {
                bzero(out, framesPerBuffer * sizeof(SFLDataFormat));
            }

            pa_stream_write( playback->pulseStream() , out , toGet  , NULL, 0 , PA_SEEK_RELATIVE);
            pa_xfree(out);
        }
    }

}

void PulseLayer::readFromMic( void )
{
    const char* data;
    size_t r;

    if( pa_stream_peek( record->pulseStream() , (const void**)&data , &r ) < 0 || !data ){
        //_debug("pa_stream_peek() failed: %s\n" , pa_strerror( pa_context_errno( context) ));
    }

    if( data != 0 ){
        _micRingBuffer.Put( (void*)data ,r, 100);
    }

    if( pa_stream_drop( record->pulseStream() ) < 0 ) {
        //_debug("pa_stream_drop() failed: %s\n" , pa_strerror( pa_context_errno( context) ));
    }
}

static void retrieve_server_info(pa_context *c UNUSED, const pa_server_info *i, void *userdata UNUSED)
{
    _debug("Server Info: Process owner : %s\n" , i->user_name);  
    _debug("\t\tServer name : %s - Server version = %s\n" , i->server_name, i->server_version);  
    _debug("\t\tDefault sink name : %s\n" , i->default_sink_name);  
    _debug("\t\tDefault source name : %s\n" , i->default_source_name);  
}

static void reduce_sink_list_cb(pa_context *c UNUSED, const pa_sink_input_info *i, int eol, void *userdata)
{
    PulseLayer* pulse = (PulseLayer*) userdata;
    if( !eol ){
        //_debug("Sink Info: index : %i\n" , i->index);  
        //_debug("\t\tClient : %i\n" , i->client); 
        //_debug("\t\tVolume : %i\n" , i->volume.values[0]); 
        //_debug("\t\tChannels : %i\n" , i->volume.channels); 
        if( strcmp( i->name , PLAYBACK_STREAM_NAME ) != 0)
            pulse->setSinkVolume( i->index , i->volume.channels, 10 );
    }  
}

static void restore_sink_list_cb(pa_context *c UNUSED, const pa_sink_input_info *i, int eol, void *userdata)
{
    PulseLayer* pulse = (PulseLayer*) userdata;
    if( !eol ){
        //_debug("Sink Info: index : %i\n" , i->index);  
        //_debug("\t\tSink name : -%s-\n" , i->name);  
        //_debug("\t\tClient : %i\n" , i->client); 
        //_debug("\t\tVolume : %i\n" , i->volume.values[0]); 
        //_debug("\t\tChannels : %i\n" , i->volume.channels); 
        if( strcmp( i->name , PLAYBACK_STREAM_NAME ) != 0)
            pulse->setSinkVolume( i->index , i->volume.channels, 100);
    }  
}

static void set_playback_volume_cb(pa_context *c UNUSED, const pa_sink_input_info *i, int eol, void *userdata)
{
    PulseLayer* pulse;
    int volume;

    pulse = (PulseLayer*) userdata;
    volume = pulse->getSpkrVolume();

    if( !eol ){
        if( strcmp( i->name , PLAYBACK_STREAM_NAME ) == 0)
            pulse->setSinkVolume( i->index , i->volume.channels, volume );
    }  
}

static void set_capture_volume_cb(pa_context *c UNUSED, const pa_source_output_info *i, int eol, void *userdata)
{
    PulseLayer* pulse;
    int volume;

    pulse = (PulseLayer*) userdata;
    volume = pulse->getMicVolume();

    if( !eol ){
        if( strcmp( i->name , CAPTURE_STREAM_NAME ) == 0)
            pulse->setSourceVolume( i->index , i->channel_map.channels, volume );
    }  
}

    void
PulseLayer::reducePulseAppsVolume( void )
{
    pa_context_get_sink_input_info_list( context , reduce_sink_list_cb , this );
}

    void
PulseLayer::restorePulseAppsVolume( void )
{
    pa_context_get_sink_input_info_list( context , restore_sink_list_cb , this );
}

    void
PulseLayer::serverinfo( void )
{
    pa_context_get_server_info( context , retrieve_server_info , NULL );
}


void PulseLayer::setSinkVolume( int index, int channels, int volume )
{

    pa_cvolume cvolume;
    pa_volume_t vol = PA_VOLUME_NORM * ((double)volume / 100) ;

    pa_cvolume_set( &cvolume , channels , vol);
    _debug("Set sink volume of index %i\n" , index);
    pa_context_set_sink_input_volume( context, index, &cvolume, NULL, NULL) ;

}

void PulseLayer::setSourceVolume( int index, int channels, int volume )
{

    pa_cvolume cvolume;
    pa_volume_t vol = PA_VOLUME_NORM * ((double)volume / 100) ;

    pa_cvolume_set( &cvolume , channels , vol);
    _debug("Set source volume of index %i\n" , index);
    pa_context_set_source_volume_by_index(context, index, &cvolume, NULL, NULL);

}


void PulseLayer::setPlaybackVolume( int volume )
{
    setSpkrVolume( volume );
    pa_context_get_sink_input_info_list( context , set_playback_volume_cb , this );
}

void PulseLayer::setCaptureVolume( int volume )
{
    setMicVolume( volume );
    pa_context_get_source_output_info_list( context , set_capture_volume_cb , this );
}

