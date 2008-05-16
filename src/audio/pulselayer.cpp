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

static pa_context *context = NULL;
static pa_stream* playback = NULL;
static pa_stream* record = NULL;
static pa_mainloop_api *mainloop_api = NULL;
static pa_threaded_mainloop *m = NULL;

static pa_sample_spec sample_spec ;

static pa_channel_map channel_map;

static std::string pcm_p, pcm_r;

int framesPerBuffer = 882;

  PulseLayer::PulseLayer(ManagerImpl* manager)
  : AudioLayer( manager , PULSEAUDIO )    
    , _mainSndRingBuffer( SIZEBUF )
{
  _debug("Pulse audio constructor: Create context\n");
}

// Destructor
PulseLayer::~PulseLayer (void) 
{ 
  assert(mainloop_api);
  //mainloop_api->quit( mainloop_api, 0 );
   pa_stream_flush( playback , NULL, NULL);
   pa_stream_disconnect( playback );
   pa_context_disconnect(context);
}

  void
PulseLayer::connectPulseServer( void )
{
  pa_context_flags_t flag = PA_CONTEXT_NOAUTOSPAWN ;  
  int ret = 1;

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

  _debug("Context creation done\n");
}

  void 
PulseLayer::stream_state_callback( pa_stream* s, void* user_data )
{
  _debug("The state of the stream changed\n");
  assert(s);
  switch(pa_stream_get_state(s)){
    case PA_STREAM_CREATING:
    case PA_STREAM_TERMINATED:
      _debug("Stream is creating...\n");
      break;
    case PA_STREAM_READY:
      _debug("Stream successfully created\n");
      break;
    case PA_STREAM_FAILED:
    default:
      _debug("Stream error: %s\n" , pa_strerror(pa_context_errno(pa_stream_get_context(s))));
      break;
  }
}

  void 
PulseLayer::context_state_callback( pa_context* c, void* user_data )
{
  _debug("The state of the context changed\n");
  PulseLayer* pulse = (PulseLayer*)user_data;
  //pa_stream_flags_t flag = PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE ;  
  assert(c && m);
  switch(pa_context_get_state(c)){
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      _debug("Waiting....\n");
      break;
    case PA_CONTEXT_READY:
      pa_cvolume cv;
      assert(c && !playback && !record);
      pulse->createStreams( c );
      _debug("Connection to PulseAudio server established\n");	
      break;
    case PA_CONTEXT_TERMINATED:
      _debug("Context terminated\n");
      break;
    case PA_CONTEXT_FAILED:
    default:
      _debug(" Error : %s" , pa_strerror(pa_context_errno(context)));
      exit(1);
  }
}

  void
PulseLayer::createStreams( pa_context* c )
{
  _debug( " Create Audio Streams \n ");
  //pa_stream_flags_t flag = PA_STREAM_AUTO_TIMING_UPDATE;
  pa_stream_flags_t flag = PA_STREAM_FIX_RATE;
  sample_spec.format = PA_SAMPLE_S16LE; 
  sample_spec.rate = 44100; 
  sample_spec.channels = 1; 
  channel_map.channels = 1; 

  if( !pa_sample_spec_valid( &sample_spec ) ){
    _debug("Invalid sample specifications\n");
    exit(0);
  }

  if(!(playback = pa_stream_new( c, "SFLphone out" , &sample_spec, &channel_map))){
    _debug("Playback: pa_stream_new() failed : %s\n" , pa_strerror( pa_context_errno( c)));
    exit(0);
  }

  if(!(record = pa_stream_new( c, "SFLphone Mic" , &sample_spec, &channel_map))){
    _debug("Capture: pa_stream_new() failed : %s\n" , pa_strerror( pa_context_errno( c)));
    exit(0);
  }

  assert(playback);
  assert(record);
  assert(m);

  // Set up the parameters required to open a (Callback)Stream:

  pa_stream_connect_playback( playback, NULL , NULL , flag , NULL, NULL );
  pa_stream_set_state_callback(playback, stream_state_callback, NULL);
  // Transferring Data - Asynchronous Mode
  pa_stream_set_write_callback(playback, audioCallback, this);

  pa_threaded_mainloop_signal(m , 0);

  pa_stream_set_state_callback(record, stream_state_callback, NULL);
  // Transferring Data - Asynchronous Mode
  pa_stream_set_read_callback(record, audioCallback, this);
  pa_stream_connect_record( record, NULL , NULL , flag  );

}

  bool 
PulseLayer::openDevice(int indexIn, int indexOut, int sampleRate, int frameSize , int stream, std::string plugin) 
{
  _sampleRate = sampleRate;
  _frameSize = frameSize;	

  _debug(" Setting PulseLayer: device     in=%2d, out=%2d\n", indexIn, indexOut);
  _debug("                   : nb channel in=%2d, out=%2d\n", _inChannel, _outChannel);
  _debug("                   : sample rate=%5d\n", _sampleRate );

  m = pa_threaded_mainloop_new();
  assert(m);

  if( pa_threaded_mainloop_start( m ) < 0  ){
    _debug("Failed starting the mainloop\n");
  }

  // Instanciate a context
  if( !(context = pa_context_new( pa_threaded_mainloop_get_api( m ) , "SFLphone" )))
    _debug("Error while creating the context\n");

  assert(context);

  connectPulseServer();

  _debug("Connection Done!! \n");
}

  void 
PulseLayer::closeCaptureStream( void )
{
}

  void 
PulseLayer::closePlaybackStream( void )
{
}

  int 
PulseLayer::playSamples(void* buffer, int toCopy, bool isTalking)
{
}

  int 
PulseLayer::putMain(void* buffer, int toCopy)
{
  int a = _mainSndRingBuffer.AvailForPut();
  if ( a >= toCopy ) {
    return _mainSndRingBuffer.Put(buffer, toCopy, 100);
  } else {
    _debug("Chopping sound, Ouch! RingBuffer full ?\n");
    return _mainSndRingBuffer.Put(buffer, a, 100);
  }

  return 0;
}

  void
PulseLayer::flushMain()
{
}

  int
PulseLayer::putUrgent(void* buffer, int toCopy)
{
}

  int
PulseLayer::canGetMic()
{
  return 882;
}

  int 
PulseLayer::getMic(void *buffer, int toCopy)
{
  return 160;
}

  void
PulseLayer::flushMic()
{
}

  bool
PulseLayer::isStreamStopped (void) 
{
}

  void 
PulseLayer::startStream (void) 
{
  _debug("Start stream\n");
  //pa_stream_cork( playback , 0, NULL, NULL);
}

  void 
PulseLayer::stopStream (void) 
{
  _debug("Stop stream\n");
  pa_stream_drop( playback );
}

  bool 
PulseLayer::isStreamActive (void) 
{
}

  void 
PulseLayer::audioCallback ( pa_stream* s, size_t bytes, void* userdata )
{ 
  pa_threaded_mainloop_signal( m , 0);

  assert( s && bytes );

  PulseLayer* pulse = (PulseLayer*) userdata;
  pulse->write();
  //if(pa_stream_get_state(s) == PA_STREAM_READY )
  //{
  //if( bytes > 0 ){
  //  pa_stream_write( s, user_data, bytes, pa_xfree, 0, PA_SEEK_RELATIVE );
  //}
  //}
  // pa_stream_write
  //  // pa_stream_peek ( to read the next fragment from the buffer ) / pa_stream_drop( to remove the data from the buffer )
  //
  //  int toPut;
  //  int urgentAvail; // number of data right and data left  
  //  int micAvailPut;
  //
  //  // AvailForGet tell the number of chars inside the buffer
  //  // framePerBuffer are the number of data for one channel (left)
  //  urgentAvail = _urgentRingBuffer.AvailForGet();
  //  if (urgentAvail > 0) {
  //    // Urgent data (dtmf, incoming call signal) come first.		
  //    toGet = (urgentAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? urgentAvail : framesPerBuffer * sizeof(SFLDataFormat);
  //    _urgentRingBuffer.Get(out, toGet, spkrVolume);
  //    // Consume the regular one as well (same amount of bytes)
  //    _mainSndRingBuffer.Discard(toGet);
  //  } else {
  //    AudioLoop* tone = _manager->getTelephoneTone();
  //    if ( tone != 0) {
  //      tone->getNext(out, framesPerBuffer, spkrVolume);
  //    } else if ( (tone=_manager->getTelephoneFile()) != 0 ) {
  //      tone->getNext(out, framesPerBuffer, spkrVolume);
  //    } else {

  //    }
  //  }
  //
  //  // Additionally handle the mic's audio stream 
  //  micAvailPut = _micRingBuffer.AvailForPut();
  //  toPut = (micAvailPut <= (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? micAvailPut : framesPerBuffer * sizeof(SFLDataFormat);
  //  //_debug("AL: Nb sample: %d char, [0]=%f [1]=%f [2]=%f\n", toPut, in[0], in[1], in[2]);
  //  _micRingBuffer.Put(in, toPut, micVolume);

}

  void
PulseLayer::write( void )
{
  int toGet; 
  int normalAvail; // number of data right and data left
  SFLDataFormat* out = (SFLDataFormat*)malloc(framesPerBuffer * sizeof(SFLDataFormat));
  normalAvail = _mainSndRingBuffer.AvailForGet();
  toGet = (normalAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? normalAvail : framesPerBuffer * sizeof(SFLDataFormat);
  if (toGet) {
    _mainSndRingBuffer.Get(out, toGet, 100);
    _debug("Write %i bytes\n" , toGet);
    pa_stream_write( playback , out , toGet  , pa_xfree, 0 , PA_SEEK_RELATIVE);
    _mainSndRingBuffer.Discard(toGet);
  } else {
    bzero(out, framesPerBuffer * sizeof(SFLDataFormat));
  }
} 
