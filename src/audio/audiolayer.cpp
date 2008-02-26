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

#include "audiolayer.h"
#include "../global.h"
#include "../manager.h"
#include "../user_cfg.h"

#define PCM_PAUSE 1
#define PCM_RESUME  0

#ifdef SFL_TEST_SINE
#include <cmath>
#endif

  AudioLayer::AudioLayer(ManagerImpl* manager)
:   _defaultVolume(100)
  , _errorMessage("")
  , _manager(manager)
  , _playback_handle( NULL )
  , _capture_handle( NULL )
    , device_closed( true )
{

  _inChannel  = 1; // don't put in stereo
  _outChannel = 1; // don't put in stereo
  _echoTesting = false;

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
}


  bool 
AudioLayer::openDevice (int indexIn, int indexOut, int sampleRate, int frameSize, int flag) 
{

  if(device_closed == false)
  {
    if( flag == SFL_PCM_CAPTURE)
      if(_capture_handle)
      {
	_debugAlsa(" Close the current capture device\n");
	snd_pcm_drop( _capture_handle );
	snd_pcm_close( _capture_handle );
	_capture_handle = 0;
      }
      else if( flag == SFL_PCM_PLAYBACK)
	if(_playback_handle){
	  _debugAlsa(" Close the current playback device\n");
	  snd_pcm_drop( _playback_handle );
	  snd_pcm_close( _playback_handle );
	  _playback_handle = 0;
	}
  }

  _indexIn = indexIn;
  _indexOut = indexOut;
  _sampleRate = sampleRate;
  _frameSize = frameSize;	

  _debug(" Setting audiolayer: device     in=%2d, out=%2d\n", _indexIn, _indexOut);
  _debug("                   : nb channel in=%2d, out=%2d\n", _inChannel, _outChannel);
  _debug("                   : sample rate=%5d, format=%s\n", _sampleRate, SFLPortaudioFormatString);
  _debug("                   : frame per buffer=%d\n", FRAME_PER_BUFFER);

  ost::MutexLock guard( _mutex );

  std::string pcmp = buildDeviceTopo(PCM_DMIX, indexOut , 0);
  std::string pcmc = buildDeviceTopo(PCM_SURROUND40, indexIn , 0);
  return open_device( pcmp , pcmc , flag);
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
  _debug(" Start stream\n");
  ost::MutexLock guard( _mutex );
  snd_pcm_prepare( _capture_handle );
  snd_pcm_start( _capture_handle ) ;
  //snd_pcm_start( _playback_handle ) ;
}


  void
AudioLayer::stopStream(void) 
{
  ost::MutexLock guard( _mutex );
  snd_pcm_drop( _capture_handle );
  snd_pcm_prepare( _capture_handle );
}


  void
AudioLayer::sleep(int msec) 
{
  snd_pcm_wait(_playback_handle, msec);
}

  bool
AudioLayer::isStreamActive (void) 
{
  ost::MutexLock guard( _mutex );
  return (isPlaybackActive() && isCaptureActive());
}


  int 
AudioLayer::playSamples(void* buffer, int toCopy)
{
  ost::MutexLock guard( _mutex );
  if ( _playback_handle ){ 
    //_debug("Play samples\n");
    write(buffer, toCopy);
  }
  return 0;
}


  int
AudioLayer::playRingTone( void* buffer, int toCopy)
{
  _debug(" %d\n", toCopy);
  ost::MutexLock guard( _mutex );
  if( _playback_handle )
    snd_pcm_start( _playback_handle );
  write(buffer, toCopy);
  return 0;
}

  int
AudioLayer::putUrgent(void* buffer, int toCopy)
{
  ost::MutexLock guard( _mutex );
  if ( _playback_handle ) 
    write(buffer, toCopy);
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


  bool
AudioLayer::isStreamStopped (void) 
{
  ost::MutexLock guard( _mutex );
  return !(isStreamActive());
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
AudioLayer::open_device(std::string pcm_p, std::string pcm_c, int flag)
{
  int err;
  snd_pcm_hw_params_t *hwparams = NULL;
  unsigned int rate_in = getSampleRate();
  unsigned int rate_out = getSampleRate();
  int dir = 0;
  snd_pcm_uframes_t period_size_in =  getFrameSize() * getSampleRate() / 1000 ;
  snd_pcm_uframes_t buffer_size_in = 4096;
  snd_pcm_uframes_t threshold = getFrameSize() * getSampleRate() / 1000 ;
  snd_pcm_uframes_t period_size_out = 2048 ;
  snd_pcm_uframes_t buffer_size_out = 4096 ;
  snd_pcm_sw_params_t *swparams = NULL;

  if(flag == SFL_PCM_BOTH || flag == SFL_PCM_CAPTURE)
  {
    _debug(" Opening capture device %s\n", pcm_c.c_str());
    if(err = snd_pcm_open(&_capture_handle,  pcm_c.c_str(),  SND_PCM_STREAM_CAPTURE, 0) < 0){
      _debug(" Error while opening capture device %s (%s)\n", pcm_c.c_str(), snd_strerror(err));
      return false;
    }

    if( err = snd_pcm_hw_params_malloc( &hwparams ) < 0 ) {
      _debug(" Cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
      return false;
    }
    if( err = snd_pcm_hw_params_any(_capture_handle, hwparams) < 0) _debug(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_access( _capture_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) _debug(" Cannot set access type (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_format( _capture_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) _debug(" Cannot set sample format (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_rate_near( _capture_handle, hwparams, &rate_in, &dir) < 0) _debug(" Cannot set sample rate (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_channels( _capture_handle, hwparams, 1) < 0) _debug(" Cannot set channel count (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_period_size_near( _capture_handle, hwparams, &period_size_in , &dir) < 0) _debug(" Cannot set period size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_buffer_size_near( _capture_handle, hwparams, &buffer_size_in ) < 0) _debug(" Cannot set buffer size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params( _capture_handle, hwparams ) < 0) _debug(" Cannot set hw parameters (%s)\n", snd_strerror(err));
    snd_pcm_hw_params_free( hwparams );

    snd_pcm_sw_params_alloca( &swparams );
    snd_pcm_sw_params_current( _capture_handle, swparams );

    if( err = snd_pcm_sw_params_set_start_threshold( _capture_handle, swparams, 1 ) < 0 ) _debug(" Cannot set start threshold (%s)\n", snd_strerror(err)); 
    if( err = snd_pcm_sw_params_set_stop_threshold( _capture_handle, swparams, buffer_size_in ) < 0 ) _debug(" Cannot get stop threshold (%s)\n", snd_strerror(err)); 
    if( err = snd_pcm_sw_params( _capture_handle, swparams ) < 0 ) _debug(" Cannot set sw parameters (%s)\n", snd_strerror(err)); 
    device_closed = false;
  }

  if(flag == SFL_PCM_BOTH || flag == SFL_PCM_PLAYBACK)
  {

    _debug(" Opening playback device %s\n", pcm_p.c_str());
    if(err = snd_pcm_open(&_playback_handle, pcm_p.c_str(),  SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0){
      _debug(" Error while opening playback device %s (%s)\n", pcm_p.c_str(), snd_strerror(err));
      return false;
    }
    if( err = snd_pcm_hw_params_malloc( &hwparams ) < 0 ) {
      _debug(" Cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
      return false;
    }
    if( err = snd_pcm_hw_params_any( _playback_handle, hwparams) < 0) _debug(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_access( _playback_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) _debug(" Cannot set access type (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_format( _playback_handle, hwparams, SND_PCM_FORMAT_S16_LE) < 0) _debug(" Cannot set sample format (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_rate_near( _playback_handle, hwparams, &rate_out, &dir) < 0) _debug(" Cannot set sample rate (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_channels( _playback_handle, hwparams, 1) < 0) _debug(" Cannot set channel count (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_period_size_near( _playback_handle, hwparams, &period_size_out , &dir) < 0) _debug(" Cannot set period size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_buffer_size_near( _playback_handle, hwparams, &buffer_size_out ) < 0) _debug(" Cannot set buffer size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params( _playback_handle, hwparams ) < 0) _debug(" Cannot set hw parameters (%s)\n", snd_strerror(err));
    snd_pcm_hw_params_free( hwparams );

    snd_pcm_uframes_t val = 1024 ;
    snd_pcm_sw_params_alloca( &swparams );
    snd_pcm_sw_params_current( _playback_handle, swparams );

    if( err = snd_pcm_sw_params_set_start_threshold( _playback_handle, swparams, val ) < 0 ) _debug(" Cannot set start threshold (%s)\n", snd_strerror(err)); 
    if( err = snd_pcm_sw_params_set_stop_threshold( _playback_handle, swparams, buffer_size_out ) < 0 ) _debug(" Cannot set stop threshold (%s)\n", snd_strerror(err)); 
    if( err = snd_pcm_sw_params( _playback_handle, swparams ) < 0 ) _debug(" Cannot set sw parameters (%s)\n", snd_strerror(err)); 
    device_closed = false;
  }
  return true;
}

  int
AudioLayer::write(void* buffer, int length)
{
  
  if(device_closed || _playback_handle == NULL)
    return 0;
  
  int bytes;
  short* buff = (short*) buffer;
  int result = 0 ;


  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _playback_handle, length);
  
  while( frames > 0 )
  {
  bytes = snd_pcm_writei( _playback_handle, buff, frames);

  if( bytes == -EAGAIN || (bytes >=0 && bytes < frames))
  {
    //snd_pcm_wait( _playback_handle, 20 );
    break;
  } 
  else if( bytes == -EPIPE )
  {  
      _debugAlsa(" %d Alsa error from writei (%s)\n", bytes, snd_strerror(bytes));
      snd_pcm_prepare( _playback_handle );
      snd_pcm_writei( _playback_handle , buff , frames );
      break;
  }
  else if( bytes == -ESTRPIPE )
  {
    _debug(" Playback suspend \n");
    snd_pcm_resume( _playback_handle );
    break;
  }
  if( bytes != frames)
  {	
    _debug(" Short write\n");
    frames -= bytes;
    buff += bytes;
  }
  
  }

  return 0;

}

  int
AudioLayer::read( void* buffer, int toCopy)
{

  if(device_closed || _capture_handle == NULL)
    return 0;
  int err;
  if(snd_pcm_state( _capture_handle ) == SND_PCM_STATE_XRUN)
    snd_pcm_prepare( _capture_handle );
  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _capture_handle, toCopy );
  if( err = snd_pcm_readi( _capture_handle, buffer, frames) < 0 ) {
    switch(err){
      case EPERM:
	_debug(" Capture EPERM (%s)\n", snd_strerror(err));
	//handle_xrun_state();
	snd_pcm_prepare( _capture_handle);
	break;
      case -ESTRPIPE:
	_debug(" Capture ESTRPIPE (%s)\n", snd_strerror(err));
	snd_pcm_resume( _capture_handle);
	break;
      case -EAGAIN:
	_debug(" Capture EAGAIN (%s)\n", snd_strerror(err));
	break;
      case -EBADFD:
	_debug(" Capture EBADFD (%s)\n", snd_strerror(err));
	break;
      case -EPIPE:
	_debug(" Capture EPIPE (%s)\n", snd_strerror(err));
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

  std::string
AudioLayer::buildDeviceTopo( std::string plugin, int card, int subdevice )
{
  std::string pcm = plugin;
  std::stringstream ss,ss1;
  ss << card;
  pcm.append(":");
  pcm.append(ss.str());
  if( subdevice != 0 ){
    pcm.append(",");
    ss1 << subdevice;
    pcm.append(ss1.str());
  }
  return pcm;
}

  std::vector<std::string>
AudioLayer::getSoundCardsInfo( int flag )
{
  std::vector<std::string> cards_id;

  snd_ctl_t* handle;
  snd_ctl_card_info_t *info;
  snd_pcm_info_t* pcminfo;
  snd_ctl_card_info_alloca( &info );
  snd_pcm_info_alloca( &pcminfo );

  int numCard = -1 ;
  int err;
  std::string description;

  if(snd_card_next( &numCard ) < 0 || numCard < 0)
    return cards_id;

  while(numCard >= 0){
    std::stringstream ss;
    ss << numCard;
    std::string name= "hw:";
    name.append(ss.str());

    if( snd_ctl_open( &handle, name.c_str(), 0) == 0 ){
      if( snd_ctl_card_info( handle, info) == 0){
	snd_pcm_info_set_device( pcminfo , 0);
	if(flag == SFL_PCM_CAPTURE)
	  snd_pcm_info_set_stream( pcminfo, SND_PCM_STREAM_CAPTURE );
	else
	  snd_pcm_info_set_stream( pcminfo, SND_PCM_STREAM_PLAYBACK );

	if( snd_ctl_pcm_info ( handle ,pcminfo ) < 0) _debug(" Cannot get info\n");
	else{
	  _debug("card %i : %s [%s]- device %i : %s [%s] \n - driver %s - dir %i\n", 
	      numCard, 
	      snd_ctl_card_info_get_id(info),
	      snd_ctl_card_info_get_name( info ),
	      snd_pcm_info_get_device( pcminfo ), 
	      snd_pcm_info_get_id(pcminfo),
	      snd_pcm_info_get_name( pcminfo),
	      snd_ctl_card_info_get_driver( info ),
	      snd_pcm_info_get_stream( pcminfo ));
	  description = snd_ctl_card_info_get_name( info );
	  description.append(" - ");
	  description.append(snd_pcm_info_get_name( pcminfo ));
	  cards_id.push_back( description );
	}
      }
      snd_ctl_close( handle );
    }
    if ( snd_card_next( &numCard ) < 0 ) {
      break;
    }

  }
  return cards_id;
}

