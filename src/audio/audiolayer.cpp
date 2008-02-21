/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "audiolayer.h"
#include "../global.h"
#include "../manager.h"
#include "../user_cfg.h"

#define PCM_NAME_DEFAULT  "default"
#define PCM_PAUSE 1
#define PCM_RESUME  0

#ifdef SFL_TEST_SINE
#include <cmath>
#endif

  AudioLayer::AudioLayer(ManagerImpl* manager)
  : _urgentRingBuffer(SIZEBUF)
  , _mainSndRingBuffer(SIZEBUF)
  , _micRingBuffer(SIZEBUF)
    , _defaultVolume(100)
  , _errorMessage("")
  , _manager(manager)
  , _playback_handle( NULL )
  , _capture_handle( NULL )
    , device_closed( true )
{
  //_sampleRate = 8000;

  _inChannel  = 1; // don't put in stereo
  _outChannel = 1; // don't put in stereo
  _echoTesting = false;

#ifdef SFL_TEST_SINE
  leftPhase_ = 0;
  tableSize_ = 200;
  const double PI = 3.14159265;
  table_ = new float[tableSize_];
  for (int i = 0; i < tableSize_; ++i)
  {
    table_[i] = 0.125f * (float)sin(((double)i/(double)tableSize_)*PI*2.);
    _debug("%9.8f\n", table_[i]);
  }
#endif
}

// Destructor
AudioLayer::~AudioLayer (void) 
{ 
  device_closed = true;
  if(_capture_handle){
    snd_pcm_drop( _capture_handle );
    snd_pcm_close( _capture_handle );
    _capture_handle = 0;
  }
  if(_playback_handle){
    snd_pcm_drop( _playback_handle );
    snd_pcm_close( _playback_handle );
    _playback_handle = 0;
  }
#ifdef SFL_TEST_SINE
  delete [] table_;
#endif
}


  bool 
AudioLayer::openDevice (int indexIn, int indexOut, int sampleRate, int frameSize) 
{
  _indexIn = indexIn;
  _indexOut = indexOut;
  _sampleRate = sampleRate;
  _frameSize = frameSize;	

  _debug(" Setting audiolayer: device     in=%2d, out=%2d\n", _indexIn, _indexOut);
  _debug("                   : nb channel in=%2d, out=%2d\n", _inChannel, _outChannel);
  _debug("                   : sample rate=%5d, format=%s\n", _sampleRate, SFLPortaudioFormatString);
  _debug("                   : frame per buffer=%d\n", FRAME_PER_BUFFER);

  ost::MutexLock guard( _mutex );
  // TODO: Must be dynamic
  return open_device( PCM_NAME_DEFAULT );
}

  int
AudioLayer::getDeviceCount()
{
  // TODO: everything
  return 1;
}

  void
AudioLayer::startStream(void) 
{
  // STATE PREPARED
  ost::MutexLock guard( _mutex );
  _debug(" entry startStream() - c => %d - p => %d\n", snd_pcm_state(_capture_handle), snd_pcm_state( _playback_handle));
  snd_pcm_start( _capture_handle ) ;
  snd_pcm_start( _playback_handle ) ;
  // STATE RUNNING
  _debug(" exit startStream() - c => %d - p => %d\n", snd_pcm_state(_capture_handle), snd_pcm_state( _playback_handle));
}


  void
AudioLayer::stopStream(void) 
{
  ost::MutexLock guard( _mutex );
  _debug(" entry stopStream() - c => %d - p => %d\n", snd_pcm_state(_capture_handle), snd_pcm_state( _playback_handle));
  snd_pcm_drop( _capture_handle );
  snd_pcm_prepare( _capture_handle );
  snd_pcm_drop( _playback_handle );
  snd_pcm_prepare( _playback_handle );
  _debug(" exit stopStream() - c => %d - p => %d\n", snd_pcm_state(_capture_handle), snd_pcm_state( _playback_handle));
}


  void
AudioLayer::sleep(int msec) 
{
  //snd_pcm_wait(_playback_handle, msec);
}

  bool
AudioLayer::isStreamActive (void) 
{
  ost::MutexLock guard( _mutex );
  if(!device_closed)
    return true;
  else
    return false;
}


  int 
AudioLayer::putMain(void* buffer, int toCopy)
{
  ost::MutexLock guard( _mutex );
  if ( _playback_handle ) 
    write(buffer, toCopy);
  return 0;
}

  void
AudioLayer::flushMain()
{
}

  int
AudioLayer::putUrgent(void* buffer, int toCopy)
{
  ost::MutexLock guard( _mutex );
  if ( _playback_handle ) 
    //write(buffer, toCopy);
  return 0;
}

  int
AudioLayer::canGetMic()
{
  int avail;
  if ( _capture_handle ) {
    avail = snd_pcm_avail_update( _capture_handle );
    //printf("%d\n", avail ); 
    if(avail > 0)
      return avail;
    else 
      return 0;  
  }
  else
    return 0;
}

  int 
AudioLayer::getMic(void *buffer, int toCopy)
{

  if( _capture_handle ) 
    return read(buffer, toCopy);
  else
    return 0;
}

  void
AudioLayer::flushMic()
{
}

  bool
AudioLayer::isStreamStopped (void) 
{
  ost::MutexLock guard( _mutex );
  return !(is_playback_active() & is_capture_active());
}

void
AudioLayer::toggleEchoTesting() {
  ost::MutexLock guard( _mutex );
  _echoTesting = (_echoTesting == true) ? false : true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////   ALSA PRIVATE FUNCTIONS   ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

bool
AudioLayer::isPlaybackActive(void) {
  ost::MutexLock guard( _mutex );
  if( _playback_handle )
    return (snd_pcm_state(_playback_handle) == SND_PCM_STATE_RUNNING ? true : false); 
  else
    return false;
}

bool
AudioLayer::isCaptureActive(void) {
  ost::MutexLock guard( _mutex );
  if( _capture_handle )
    return (snd_pcm_state( _capture_handle) == SND_PCM_STATE_RUNNING ? true : false); 
  else
    return false;
}


  bool 
AudioLayer::open_device(std::string pcm_name)
{
  int err;
  snd_pcm_hw_params_t *hwparams = NULL;
  unsigned int rate_in = _sampleRate;
  unsigned int rate_out = _sampleRate;
  int direction = 0;
  snd_pcm_uframes_t period_size_in = 1024;
  snd_pcm_uframes_t buffer_size_in = 2048;
  snd_pcm_uframes_t period_size_out = 2048;
  snd_pcm_uframes_t buffer_size_out = 4096;
  snd_pcm_sw_params_t *swparams = NULL;

  _debug(" Opening capture device %s\n", pcm_name.c_str());
  if(err = snd_pcm_open(&_capture_handle, pcm_name.c_str(),  SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK) < 0){
    _debug(" Error while opening capture device %s (%s)\n", pcm_name.c_str(), snd_strerror(err));
    return false;
  }
  if( err = snd_pcm_hw_params_malloc( &hwparams ) < 0 ) {
    _debug(" Cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
    return false;
  }
  if( err = snd_pcm_hw_params_any(_capture_handle, hwparams) < 0) _debug(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_access( _capture_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) _debug(" Cannot set access type (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_format( _capture_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) _debug(" Cannot set sample format (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_rate_near( _capture_handle, hwparams, &rate_in, &direction) < 0) _debug(" Cannot set sample rate (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_channels( _capture_handle, hwparams, 1) < 0) _debug(" Cannot set channel count (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_period_size_near( _capture_handle, hwparams, &period_size_out , &direction) < 0) _debug(" Cannot set period size (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_buffer_size_near( _capture_handle, hwparams, &buffer_size_out ) < 0) _debug(" Cannot set buffer size (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params( _capture_handle, hwparams ) < 0) _debug(" Cannot set hw parameters (%s)\n", snd_strerror(err));
  snd_pcm_hw_params_free( hwparams );


  _debug(" Opening playback device %s\n", pcm_name.c_str());
  if(err = snd_pcm_open(&_playback_handle, pcm_name.c_str(),  SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0){
    _debug(" Error while opening playback device %s (%s)\n", pcm_name.c_str(), snd_strerror(err));
    return false;
  }
  if( err = snd_pcm_hw_params_malloc( &hwparams ) < 0 ) {
    _debug(" Cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
    return false;
  }
  if( err = snd_pcm_hw_params_any( _playback_handle, hwparams) < 0) _debug(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_access( _playback_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) _debug(" Cannot set access type (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_format( _playback_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) _debug(" Cannot set sample format (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_rate_near( _playback_handle, hwparams, &rate_out, &direction) < 0) _debug(" Cannot set sample rate (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_channels( _playback_handle, hwparams, 1) < 0) _debug(" Cannot set channel count (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_period_size_near( _playback_handle, hwparams, &period_size_out , &direction) < 0) _debug(" Cannot set period size (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params_set_buffer_size_near( _playback_handle, hwparams, &buffer_size_out ) < 0) _debug(" Cannot set buffer size (%s)\n", snd_strerror(err));
  if( err = snd_pcm_hw_params( _playback_handle, hwparams ) < 0) _debug(" Cannot set hw parameters (%s)\n", snd_strerror(err));
  snd_pcm_hw_params_free( hwparams );

  snd_pcm_uframes_t val;
  snd_pcm_sw_params_alloca( &swparams );
  snd_pcm_sw_params_current( _playback_handle, swparams );

  if( err = snd_pcm_sw_params_get_start_threshold( swparams, &val ) < 0 ) _debug(" Cannot get start threshold (%s)\n", snd_strerror(err)); 
  if( err = snd_pcm_sw_params_get_stop_threshold( swparams, &val ) < 0 ) _debug(" Cannot get stop threshold (%s)\n", snd_strerror(err)); 
  if( err = snd_pcm_sw_params_get_boundary( swparams, &val ) < 0 ) _debug(" Cannot get boundary (%s)\n", snd_strerror(err)); 
  if( err = snd_pcm_sw_params_set_silence_threshold( _playback_handle, swparams, 0 ) < 0 ) _debug(" Cannot set silence threshold (%s)\n", snd_strerror(err)); 
  if( err = snd_pcm_sw_params_set_silence_size( _playback_handle, swparams, 0 ) < 0 ) _debug(" Cannot set silence size (%s)\n", snd_strerror(err)); 
  if( err = snd_pcm_sw_params( _playback_handle, swparams ) < 0 ) _debug(" Cannot set sw parameters (%s)\n", snd_strerror(err)); 

  device_closed = false;

  return true;
}

  bool 
AudioLayer::is_playback_active( void )
{
  if(snd_pcm_state(_playback_handle) == SND_PCM_STATE_RUNNING)
    return true;
  else
    return false;
}

  bool
AudioLayer::is_capture_active( void )
{
  if(snd_pcm_state(_capture_handle) == SND_PCM_STATE_RUNNING)
    return true;
  else
    return false;
}


  int
AudioLayer::write(void* buffer, int length)
{
  if(device_closed || _playback_handle == NULL)
    return 0;

  int bytes;

  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _playback_handle, length);
  if( bytes = snd_pcm_writei( _playback_handle, buffer, frames) < 0 ) {
    snd_pcm_prepare( _playback_handle );
    _debug(" Playback error (%s)\n", snd_strerror(bytes));
    return 0;
  }
  return 1;
}

  int
AudioLayer::read( void* target_buffer, int toCopy)
{
  if(device_closed || _capture_handle == NULL)
    return 0;

  int bytes;
  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _capture_handle, toCopy);
  if( bytes = snd_pcm_readi( _capture_handle, target_buffer, frames) < 0 ) {
    int err = bytes;
    switch(err){
      case EPERM:
	_debug(" Capture EPERM (%s)\n", snd_strerror(bytes));
	handle_xrun_state();
	break;
      case -ESTRPIPE:
	_debug(" Capture ESTRPIPE (%s)\n", snd_strerror(bytes));
	snd_pcm_resume( _capture_handle);
	break;
      case -EAGAIN:
	_debug(" Capture EAGAIN (%s)\n", snd_strerror(bytes));
	break;
      case -EBADFD:
	_debug(" Capture EBADFD (%s)\n", snd_strerror(bytes));
	break;
      case -EPIPE:
	_debug(" Capture EPIPE (%s)\n", snd_strerror(bytes));
	handle_xrun_state();
	break;
    }
    return 0;
  }
  return toCopy;
}


  void
AudioLayer::handle_xrun_state( void )
{
  snd_pcm_status_t* status;
  snd_pcm_status_alloca( &status );

  int res = snd_pcm_status( _capture_handle, status );
  if( res <= 0){
    if(snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN ){
      snd_pcm_drop( _capture_handle );
      snd_pcm_prepare( _capture_handle );
      snd_pcm_start( _capture_handle ); 
    }
  }
  else
    _debug(" Get status failed\n");
}
