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


int framesPerBuffer = 4096;
const pa_buffer_attr *a;

  PulseLayer::PulseLayer(ManagerImpl* manager)
  : AudioLayer( manager , PULSEAUDIO )    
  , _urgentRingBuffer( SIZEBUF)
  ,_mainSndRingBuffer( SIZEBUF )
    ,_micRingBuffer( SIZEBUF )
{
  _debug("Pulse audio constructor: Create context\n");
}

// Destructor
PulseLayer::~PulseLayer (void) 
{ 
  delete playback;
  delete record;
  ////delete cache;
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
PulseLayer::context_state_callback( pa_context* c, void* user_data )
{
  _debug("The state of the context changed\n");
  PulseLayer* pulse = (PulseLayer*)user_data;
  assert(c && pulse->m);
  switch(pa_context_get_state(c)){
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      _debug("Waiting....\n");
      break;
    case PA_CONTEXT_READY:
      pa_cvolume cv;
      pulse->createStreams( c );
      _debug("Connection to PulseAudio server established\n");	
      break;
    case PA_CONTEXT_TERMINATED:
      _debug("Context terminated\n");
      break;
    case PA_CONTEXT_FAILED:
    default:
      _debug(" Error : %n" , pa_strerror(pa_context_errno(c)));
      exit(1);
  }
}

  void
PulseLayer::createStreams( pa_context* c )
{
  playback = new AudioStream(c, PLAYBACK_STREAM, "SFLphone out");
  pa_stream_set_write_callback( playback->pulseStream() , audioCallback, this);
  pa_stream_set_overflow_callback( playback->pulseStream() , overflow , this);
  record = new AudioStream(c, CAPTURE_STREAM, "SFLphone in");
  pa_stream_set_read_callback( record->pulseStream() , audioCallback, this);
  pa_stream_set_underflow_callback( playback->pulseStream() , underflow , this);
  cache = new AudioStream(c, UPLOAD_STREAM, "Cache samples");

  pa_threaded_mainloop_signal(m , 0);

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
  _mainSndRingBuffer.flush();
}

  int
PulseLayer::putUrgent(void* buffer, int toCopy)
{
  int a = _urgentRingBuffer.AvailForPut();
  if ( a >= toCopy ) {
    return _urgentRingBuffer.Put(buffer, toCopy, 100 );
  } else {
    return _urgentRingBuffer.Put(buffer, a, 100 );
  }
  return 0;
}

  int
PulseLayer::canGetMic()
{
  if( record )
  {
    int a = _micRingBuffer.AvailForGet();
    _debug("available for get = %i\n" , a);
    return a;
  }
  else
    return 0;
}

  int 
PulseLayer::getMic(void *buffer, int toCopy)
{
  if( record ){
    return _micRingBuffer.Get(buffer, toCopy, 100);
  }
  else
    return 0;
}

  void
PulseLayer::flushMic()
{
  _micRingBuffer.flush();
}

/*  bool
    PulseLayer::isStreamStopped (void) 
    {
    }
    */
  void 
PulseLayer::startStream (void) 
{
  _debug("Start stream\n");
}

  void 
PulseLayer::stopStream (void) 
{
  _debug("Stop stream\n");
  //pa_stream_drop( playback );
}

  bool 
PulseLayer::isStreamActive (void) 
{
}

  void 
PulseLayer::audioCallback ( pa_stream* s, size_t bytes, void* userdata )
{ 
  PulseLayer* pulse = (PulseLayer*) userdata;
  assert( s && bytes );
  assert( bytes > 0 );
  pulse->write();
  pulse->read();
}

  void 
PulseLayer::underflow ( pa_stream* s,  void* userdata )
{ 
  _debug("Buffer Underflow\n");
}


  void 
PulseLayer::overflow ( pa_stream* s, void* userdata )
{ 
  _debug("Buffer Overflow\n");
}

  void
PulseLayer::write( void )
{
 // a =  pa_stream_get_buffer_attr(record->pulseStream());
  //if (a)
    //_debug("Buffer attributes: maxlength=%u , fragsize=%u\n" , a->maxlength, a->fragsize );
  //_debug("Device name = %s ; device index = %i\n" , pa_stream_get_device_name( playback->pulseStream()) , pa_stream_get_device_index( playback->pulseStream()));
  int toGet; 
  int urgentAvail; // number of data right and data left  
  int normalAvail; // number of data right and data left
  SFLDataFormat* out = (SFLDataFormat*)pa_xmalloc(framesPerBuffer * sizeof(SFLDataFormat));
  urgentAvail = _urgentRingBuffer.AvailForGet();
  if (urgentAvail > 0) {
    // Urgent data (dtmf, incoming call signal) come first.		
    _debug("Play urgent!\n");
    toGet = (urgentAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? urgentAvail : framesPerBuffer * sizeof(SFLDataFormat);
    _urgentRingBuffer.Get(out, toGet, 100);
    // Consume the regular one as well (same amount of bytes)
    _mainSndRingBuffer.Discard(toGet);
  }
  else
  {
    AudioLoop* tone = 0;//_manager->getTelephoneTone();
    if ( tone != 0) {
      //tone->getNext(out, framesPerBuffer, 100);
      toGet = framesPerBuffer;
    } /*else if ( (tone=_manager->getTelephoneFile()) != 0 ) {
      tone->getNext(out, framesPerBuffer, 100);
      toGet = framesPerBuffer;
    } */else {
      normalAvail = _mainSndRingBuffer.AvailForGet();
      toGet = (normalAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? normalAvail : framesPerBuffer * sizeof(SFLDataFormat);
      if (toGet) {
	_mainSndRingBuffer.Get(out, toGet, 100);
	_debug("Write %i bytes\n" , toGet);
	_mainSndRingBuffer.Discard(toGet);
      } else {
	bzero(out, framesPerBuffer * sizeof(SFLDataFormat));
      }
    }
  }
  pa_stream_write( playback->pulseStream() , out , toGet  , pa_xfree, 0 , PA_SEEK_RELATIVE);
}

  void 
PulseLayer::read( void )
{
  //assert( record->pulseStream() );
  int micAvailPut;
  size_t toPut;
  const void* data; 
  // Handle the mic's audio stream 
  micAvailPut = _micRingBuffer.AvailForPut();
  toPut = 2048;
  //toPut = (micAvailPut <= (size_t)(framesPerBuffer * sizeof(SFLDataFormat))) ? micAvailPut : framesPerBuffer * sizeof(SFLDataFormat);
  //_debug("micAvailPut: %i - - toPut : %i\n", micAvailPut, toPut);

  if( pa_stream_peek( record->pulseStream() , &data , &toPut ) < 0 ){
    _debug("pa_stream_peek() failed: %s\n" , pa_strerror( pa_context_errno( context) ));
    return;
  }

  _debug("- toPut : %i\n", toPut);
  if( data != NULL )
    _micRingBuffer.Put( (void*)data , toPut, 100);

  if( pa_stream_drop( record->pulseStream()) < 0 )
    _debug("pa_stream_drop() failed: %s\n" , pa_strerror( pa_context_errno( context) ));
  
}

  int
PulseLayer::putInCache( char code, void *buffer, int toCopy )
{
  _debug("Put the DTMF in cache\n");
  //pa_stream_write( cache->pulseStream() , buffer , toCopy  , pa_xfree, 0 , PA_SEEK_RELATIVE);
  //pa_stream_finish_upload( cache->pulseStream() );
}

