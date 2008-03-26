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
  , _PlaybackHandle( NULL )
  , _CaptureHandle( NULL )
  , deviceClosed( true )
    , _urgentBuffer( SIZEBUF )
    , _fstream("/tmp/audio.dat")
{

  _inChannel  = 1; // don't put in stereo
  _outChannel = 1; // don't put in stereo
  _echoTesting = false;

}

// Destructor
AudioLayer::~AudioLayer (void) 
{ 
  _debugAlsa("Close ALSA streams\n");
  closeCaptureStream();
  closePlaybackStream();
  deviceClosed = true;
  _fstream.flush();
  _fstream.close();
}


  bool 
AudioLayer::openDevice (int indexIn, int indexOut, int sampleRate, int frameSize, int stream , std::string plugin) 
{

  if(deviceClosed == false)
  {
    if( stream == SFL_PCM_CAPTURE )
    {
      closeCaptureStream();
    }
    else if( stream == SFL_PCM_PLAYBACK)
    {
      closePlaybackStream();
    }
    else
    {
      closeCaptureStream();
      closePlaybackStream();
    }
  }

  _indexIn = indexIn;
  _indexOut = indexOut;
  _sampleRate = sampleRate;
  _frameSize = frameSize;	
  _audioPlugin = plugin;

  _debugAlsa(" Setting audiolayer: device     in=%2d, out=%2d\n", _indexIn, _indexOut);
  _debugAlsa("                   : alsa plugin=%s\n", _audioPlugin.c_str());
  _debugAlsa("                   : nb channel in=%2d, out=%2d\n", _inChannel, _outChannel);
  _debugAlsa("                   : sample rate=%5d, format=%s\n", _sampleRate, SFLDataFormatString);

  ost::MutexLock lock( _mutex );

  std::string pcmp = buildDeviceTopo( plugin , indexOut , 0);
  std::string pcmc = buildDeviceTopo( PCM_SURROUND40 , indexIn , 0);
  return open_device( pcmp , pcmc , stream);
}

  void
AudioLayer::startStream(void) 
{
  _talk = true ;
  _debugAlsa(" Start stream\n");
  int err;
  //ost::MutexLock lock( _mutex );
  snd_pcm_prepare( _CaptureHandle );
  snd_pcm_start( _CaptureHandle ) ;

  snd_pcm_prepare( _PlaybackHandle );
  if( err = snd_pcm_start( _PlaybackHandle) < 0 )  _debugAlsa(" Cannot start (%s)\n", snd_strerror(err));
}

  void
AudioLayer::stopStream(void) 
{
  //ost::MutexLock lock( _mutex );
  _talk = false;
  snd_pcm_drop( _CaptureHandle );
  snd_pcm_prepare( _CaptureHandle );
  snd_pcm_drop( _PlaybackHandle );
  snd_pcm_prepare( _PlaybackHandle );
  _urgentBuffer.flush();
}

  int
AudioLayer::getDeviceCount()
{
  // TODO: everything
  return 1;
}

void AudioLayer::AlsaCallBack( snd_async_handler_t* pcm_callback )
{ 
  ( ( AudioLayer *)snd_async_handler_get_callback_private( pcm_callback )) -> playTones();
}

  void 
AudioLayer::fillHWBuffer( void)
{
  unsigned char* data;
  int pcmreturn, l1, l2;
  short s1, s2;
  int periodSize = 256 ;
  int frames = periodSize >> 2 ;
  _debug("frames  = %d\n");

  data = (unsigned char*)malloc(periodSize);
  for(l1 = 0; l1 < 100; l1++) {
    for(l2 = 0; l2 < frames; l2++) {
      s1 = 0;
      s2 = 0;
      data[4*l2] = (unsigned char)s1;
      data[4*l2+1] = s1 >> 8;
      data[4*l2+2] = (unsigned char)s2;
      data[4*l2+3] = s2 >> 8;
    }
    while ((pcmreturn = snd_pcm_mmap_writei(_PlaybackHandle, data, frames)) < 0) {
      snd_pcm_prepare(_PlaybackHandle);
      //_debugAlsa("< Buffer Underrun >\n");
    }
  }
}

  bool
AudioLayer::isStreamActive (void) 
{
  ost::MutexLock lock( _mutex );
  return (isPlaybackActive() && isCaptureActive());
}


  int 
AudioLayer::playSamples(void* buffer, int toCopy, bool isTalking)
{
  //ost::MutexLock lock( _mutex );
  if( isTalking )
    _talk = true;
  if ( _PlaybackHandle ){ 
    write( adjustVolume( buffer , toCopy , SFL_PCM_PLAYBACK ) , toCopy );
  }
  return 0;
}

  int
AudioLayer::putUrgent(void* buffer, int toCopy)
{
  if ( _PlaybackHandle ){ 
    fillHWBuffer();
    int a = _urgentBuffer.AvailForPut();
    if( a >= toCopy ){
      return _urgentBuffer.Put( buffer , toCopy , _defaultVolume );
    } else {
      return _urgentBuffer.Put( buffer , a , _defaultVolume ) ;
    }
  }
  return 0;
}

  int
AudioLayer::canGetMic()
{
  int avail;
  if ( _CaptureHandle ) {
    avail = snd_pcm_avail_update( _CaptureHandle );
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
  int res = 0 ; 
  if( _CaptureHandle ) 
  {
    res = read( buffer, toCopy );
    adjustVolume( buffer , toCopy , SFL_PCM_CAPTURE );
  }
  return res ;
}


  bool
AudioLayer::isStreamStopped (void) 
{
  ost::MutexLock lock( _mutex );
  return !(isStreamActive());
}

void
AudioLayer::toggleEchoTesting() {
  ost::MutexLock lock( _mutex );
  _echoTesting = (_echoTesting == true) ? false : true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
/////////////////   ALSA PRIVATE FUNCTIONS   ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////



  void
AudioLayer::playTones( void )
{
  int frames = _periodSize ; 
  int maxBytes = frames * sizeof(SFLDataFormat) ;
  SFLDataFormat* out = (SFLDataFormat*)malloc(maxBytes * sizeof(SFLDataFormat));
  if( _talk ) {}
  else {
    AudioLoop *tone = _manager -> getTelephoneTone();
    int spkrVol = _manager -> getSpkrVolume();
    if( tone != 0 ){
      tone -> getNext( out , frames , spkrVol );
      //_fstream.write( (char*)out,  maxBytes );
      write( out , maxBytes );
    } 
    else if( ( tone=_manager->getTelephoneFile() ) != 0 ){
      tone ->getNext( out , frames , spkrVol );
      write( out , maxBytes );
    }
  }
  // free the temporary data buffer 
  free( out ); out = 0;
}

bool
AudioLayer::isPlaybackActive(void) {
  ost::MutexLock guard( _mutex );
  if( _PlaybackHandle )
    return (snd_pcm_state(_PlaybackHandle) == SND_PCM_STATE_RUNNING ? true : false); 
  else
    return false;
}

bool
AudioLayer::isCaptureActive(void) {
  ost::MutexLock guard( _mutex );
  if( _CaptureHandle )
    return (snd_pcm_state( _CaptureHandle) == SND_PCM_STATE_RUNNING ? true : false); 
  else
    return false;
}


  bool 
AudioLayer::open_device(std::string pcm_p, std::string pcm_c, int flag)
{
  std::stringstream errMsg;
  int err;
  snd_pcm_hw_params_t* hwParams = NULL;
  snd_pcm_sw_params_t *swparams = NULL;
  unsigned int rate_in = getSampleRate();
  unsigned int rate_out = getSampleRate();
  int dir = 0;
  snd_pcm_uframes_t period_size_in = getFrameSize() * getSampleRate() / 1000 *  2 ;
  snd_pcm_uframes_t buffer_size_in = period_size_in * 4 ; 
  snd_pcm_uframes_t threshold = 1024 ;
  snd_pcm_uframes_t period_size_out =  getFrameSize() * getSampleRate() / 1000 *  2;//1024 ;
  snd_pcm_uframes_t buffer_size_out = period_size_out * 4 ;

  int fragment_size = 6144;
  unsigned int buffer_time = 80000; //80ms
  unsigned int period_time = buffer_time / 4; //20ms

  if(flag == SFL_PCM_BOTH || flag == SFL_PCM_CAPTURE)
  {
    _debugAlsa(" Opening capture device %s\n", pcm_c.c_str());
    if(err = snd_pcm_open(&_CaptureHandle,  pcm_c.c_str(),  SND_PCM_STREAM_CAPTURE, 0) < 0){
      errMsg << " Error while opening capture device " << pcm_c.c_str() << " (" <<  snd_strerror(err) << ")";
      _debugAlsa(" %s\n", errMsg.str().c_str());
      setErrorMessage( errMsg.str() );
      return false;
    }

    if( err = snd_pcm_hw_params_malloc( &hwParams ) < 0 ) {
      _debugAlsa(" Cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
      return false;
    }
    if( err = snd_pcm_hw_params_any(_CaptureHandle, hwParams) < 0) _debugAlsa(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_access( _CaptureHandle, hwParams, SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0) _debugAlsa(" Cannot set access type (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_format( _CaptureHandle, hwParams, SND_PCM_FORMAT_S16_LE) < 0) _debugAlsa(" Cannot set sample format (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_rate_near( _CaptureHandle, hwParams, &rate_in, &dir) < 0) _debugAlsa(" Cannot set sample rate (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_channels( _CaptureHandle, hwParams, 1) < 0) _debugAlsa(" Cannot set channel count (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_period_size_near( _CaptureHandle, hwParams, &period_size_in , &dir) < 0) _debugAlsa(" Cannot set period size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_buffer_size_near( _CaptureHandle, hwParams, &buffer_size_in ) < 0) _debugAlsa(" Cannot set buffer size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params( _CaptureHandle, hwParams ) < 0) _debugAlsa(" Cannot set hw parameters (%s)\n", snd_strerror(err));
    snd_pcm_hw_params_free( hwParams );
    deviceClosed = false;
  }

  if(flag == SFL_PCM_BOTH || flag == SFL_PCM_PLAYBACK)
  {

    _debugAlsa(" Opening playback device %s\n", pcm_p.c_str());
    if(err = snd_pcm_open(&_PlaybackHandle, pcm_p.c_str(),  SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK ) < 0){
      errMsg << " Error while opening playback device " << pcm_p.c_str() << " (" <<  snd_strerror(err) << ")";
      _debugAlsa(" %s\n", errMsg.str().c_str());
      setErrorMessage( errMsg.str() );
      return false;
    }
    if( err = snd_pcm_hw_params_malloc( &hwParams ) < 0 ) {
      _debugAlsa(" Cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
      return false;
    }
    if( err = snd_pcm_hw_params_any( _PlaybackHandle,hwParams) < 0) _debugAlsa(" Cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
    //if( err = snd_pcm_hw_params_set_access( _PlaybackHandle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) _debugAlsa(" Cannot set access type (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_access( _PlaybackHandle, hwParams, SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0) _debugAlsa(" Cannot set access type (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_format( _PlaybackHandle, hwParams, SND_PCM_FORMAT_S16_LE) < 0) _debugAlsa(" Cannot set sample format (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_rate( _PlaybackHandle, hwParams, rate_out, dir) < 0) _debugAlsa(" Cannot set sample rate (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_channels( _PlaybackHandle, hwParams, 1) < 0) _debugAlsa(" Cannot set channel count (%s)\n", snd_strerror(err));
    //if( err = snd_pcm_hw_params_set_period_size_near( _PlaybackHandle, hwParams, &period_size_out , &dir) < 0) _debugAlsa(" Cannot set period size (%s)\n", snd_strerror(err));
    //if( err = snd_pcm_hw_params_set_buffer_size_near( _PlaybackHandle, hwParams, &buffer_size_out ) < 0) _debugAlsa(" Cannot set buffer size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_buffer_time_near( _PlaybackHandle, hwParams, &buffer_time , &dir) < 0) _debugAlsa(" Cannot set buffer time (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_set_period_time_near( _PlaybackHandle, hwParams, &period_time , &dir) < 0) _debugAlsa(" Cannot set period time (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_get_period_size(  hwParams, &_periodSize , &dir) < 0) _debugAlsa(" Cannot get period size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params_get_buffer_size(  hwParams, &buffer_size_out ) < 0) _debugAlsa(" Cannot get buffer size (%s)\n", snd_strerror(err));
    if( err = snd_pcm_hw_params( _PlaybackHandle, hwParams ) < 0) _debugAlsa(" Cannot set hw parameters (%s)\n", snd_strerror(err));


    snd_pcm_hw_params_free( hwParams );

    snd_pcm_uframes_t val ;
    snd_pcm_sw_params_malloc( &swparams );
    snd_pcm_sw_params_current( _PlaybackHandle, swparams );

    if( err = snd_pcm_sw_params_set_start_threshold( _PlaybackHandle, swparams, period_size_out) < 0 ) _debugAlsa(" Cannot set start threshold (%s)\n", snd_strerror(err)); 
    snd_pcm_sw_params_get_start_threshold( swparams , &val);
    _debug("Start threshold = %d\n" ,val);
    //if( err = snd_pcm_sw_params_set_stop_threshold( _PlaybackHandle, swparams, buffer_size_out ) < 0 ) _debugAlsa(" Cannot set stop threshold (%s)\n", snd_strerror(err)); 
    //snd_pcm_sw_params_get_stop_threshold( swparams , &val);
    //_debug("Stop threshold = %d\n" ,val);
    if( err = snd_pcm_sw_params_set_avail_min( _PlaybackHandle, swparams, period_size_out) < 0) _debugAlsa(" Cannot set min avail (%s)\n" , snd_strerror(err)); 
    if( err = snd_pcm_sw_params_set_xfer_align( _PlaybackHandle, swparams, 1) < 0) _debugAlsa(" Cannot set xfer align (%s)\n" , snd_strerror(err)); 
    snd_pcm_sw_params_get_avail_min( swparams , &val);
    _debug("Min available = %d\n" ,val);
    //if( err = snd_pcm_sw_params_set_silence_threshold( _PlaybackHandle, swparams, period_size_out) < 0) _debugAlsa(" Cannot set silence threshold (%s)\n" , snd_strerror(err)); 
    //snd_pcm_sw_params_get_silence_threshold( swparams , &val);
      //  _debug("Silence threshold = %d\n" ,val);
    if( err = snd_pcm_sw_params( _PlaybackHandle, swparams ) < 0 ) _debugAlsa(" Cannot set sw parameters (%s)\n", snd_strerror(err)); 
    snd_pcm_sw_params_free( swparams );

    if ( err = snd_async_add_pcm_handler( &_AsyncHandler, _PlaybackHandle , AlsaCallBack, this ) < 0)	_debugAlsa(" Unable to install the async callback handler (%s)\n", snd_strerror(err));
    deviceClosed = false;
  }
  //fillHWBuffer();

  _talk = false;
  return true;
}

//TODO	first frame causes broken pipe (underrun) because not enough data are send --> make the handle wait to be ready
  int
AudioLayer::write(void* buffer, int length)
{
  //if(snd_pcm_state( _PlaybackHandle ) == SND_PCM_STATE_XRUN)
    //handle_xrun_playback();  
  //_debugAlsa("avail = %d - toWrite = %d\n" , snd_pcm_avail_update( _PlaybackHandle ) , length / 2);

  
  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _PlaybackHandle, length);
  int err = snd_pcm_mmap_writei( _PlaybackHandle , buffer , frames );
  switch(err) {
    case -EAGAIN: 
      _debugAlsa(" (%s)\n", snd_strerror( err ));
      snd_pcm_resume( _PlaybackHandle );
      break;
    case -EPIPE: 
      _debugAlsa(" UNDERRUN (%s)\n", snd_strerror(err));
      handle_xrun_playback();
      snd_pcm_mmap_writei( _PlaybackHandle , buffer , frames );
      break;
    case -ESTRPIPE:
      _debugAlsa(" (%s)\n", snd_strerror(err));
      snd_pcm_resume( _PlaybackHandle );
      break;
    case -EBADFD:
      _debugAlsa(" (%s)\n", snd_strerror( err ));
      break;
  }
  
  if( err >=0 && err < frames )
    _debugAlsa("Short write : %d out of %d\n", err , frames);
  
  return ( err > 0 )? err : 0 ;
}

  int
AudioLayer::read( void* buffer, int toCopy)
{

  if(deviceClosed || _CaptureHandle == NULL)
    return 0;
  int err;
  if(snd_pcm_state( _CaptureHandle ) == SND_PCM_STATE_XRUN)
    snd_pcm_prepare( _CaptureHandle );
  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _CaptureHandle, toCopy );
  if( err = snd_pcm_mmap_readi( _CaptureHandle, buffer, frames) < 0 ) {
    switch(err){
      case EPERM:
	_debugAlsa(" Capture EPERM (%s)\n", snd_strerror(err));
	snd_pcm_prepare( _CaptureHandle);
	break;
      case -ESTRPIPE:
	_debugAlsa(" Capture ESTRPIPE (%s)\n", snd_strerror(err));
	snd_pcm_resume( _CaptureHandle);
	break;
      case -EAGAIN:
	_debugAlsa(" Capture EAGAIN (%s)\n", snd_strerror(err));
	break;
      case -EBADFD:
	_debugAlsa(" Capture EBADFD (%s)\n", snd_strerror(err));
	break;
      case -EPIPE:
	_debugAlsa(" Capture EPIPE (%s)\n", snd_strerror(err));
	handle_xrun_capture();
	break;
    }
    return 0;
  }

  return toCopy;

}

  void
AudioLayer::handle_xrun_capture( void )
{
  snd_pcm_status_t* status;
  snd_pcm_status_alloca( &status );

  int res = snd_pcm_status( _CaptureHandle, status );
  if( res <= 0){
    if(snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN ){
      snd_pcm_drop( _CaptureHandle );
      snd_pcm_prepare( _CaptureHandle );
      snd_pcm_start( _CaptureHandle ); 
    }
  }
  else
    _debugAlsa(" Get status failed\n");
}

  void
AudioLayer::handle_xrun_playback( void )
{
  int state; 
  snd_pcm_status_t* status;
  snd_pcm_status_alloca( &status );

  if( state = snd_pcm_status( _PlaybackHandle, status ) < 0 )   _debugAlsa(" Error: Cannot get playback handle status (%s)\n" , snd_strerror( state ) );
  else 
  { 
    state = snd_pcm_status_get_state( status );
    if( state  == SND_PCM_STATE_XRUN )
    {
      snd_pcm_drop( _PlaybackHandle );
      snd_pcm_prepare( _PlaybackHandle );
      //snd_pcm_start( _PlaybackHandle ); 
    }
  }
}

  std::string
AudioLayer::buildDeviceTopo( std::string plugin, int card, int subdevice )
{
  std::string pcm = plugin;
  std::stringstream ss,ss1;
  if( pcm == "default" || pcm == "pulse")
    return pcm;
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
AudioLayer::getSoundCardsInfo( int stream )
{
  std::vector<std::string> cards_id;
  HwIDPair p;

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
	snd_pcm_info_set_stream( pcminfo, ( stream == SFL_PCM_CAPTURE )? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK );
	if( snd_ctl_pcm_info ( handle ,pcminfo ) < 0) _debugAlsa(" Cannot get info\n");
	else{
	  _debugAlsa("card %i : %s [%s]\n", 
	      numCard, 
	      snd_ctl_card_info_get_id(info),
	      snd_ctl_card_info_get_name( info ));
	  description = snd_ctl_card_info_get_name( info );
	  description.append(" - ");
	  description.append(snd_pcm_info_get_name( pcminfo ));
	  cards_id.push_back( description );
	  // The number of the sound card is associated with a string description 
	  p = HwIDPair( numCard , description );
	  IDSoundCards.push_back( p );
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

  void
AudioLayer::closeCaptureStream( void)
{
  if(_CaptureHandle){
    snd_pcm_drop( _CaptureHandle );
    snd_pcm_close( _CaptureHandle );
    _CaptureHandle = 0;
  }
}

  void
AudioLayer::closePlaybackStream( void)
{
  if(_PlaybackHandle){
    snd_pcm_drop( _PlaybackHandle );
    snd_pcm_close( _PlaybackHandle );
    _PlaybackHandle = 0;
  }
}


  bool
AudioLayer::soundCardIndexExist( int card , int stream )
{
  snd_ctl_t* handle;
  snd_pcm_info_t *pcminfo;
  snd_pcm_info_alloca( &pcminfo );
  std::string name = "hw:";
  std::stringstream ss;
  ss << card ;
  name.append(ss.str());
  if(snd_ctl_open( &handle, name.c_str(), 0) == 0 ){
   snd_pcm_info_set_stream( pcminfo , ( stream == SFL_PCM_PLAYBACK )? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE );
    if( snd_ctl_pcm_info( handle , pcminfo ) < 0) return false;
    else
      return true;
  }
  else
    return false;
}  

  int
AudioLayer::soundCardGetIndex( std::string description )
{
  int i;
  for( i = 0 ; i < IDSoundCards.size() ; i++ )
  {
    HwIDPair p = IDSoundCards[i];
    if( p.second == description )
      return  p.first ;
  }
  // else return the default one
  return 0;
}

  void*
AudioLayer::adjustVolume( void* buffer , int len, int stream )
{
  int vol;
  if( stream == SFL_PCM_PLAYBACK )
    vol = _manager->getSpkrVolume();
  else
    vol = _manager->getMicVolume();

  SFLDataFormat* src = (SFLDataFormat*) buffer;
  if( vol != 100 )
  {
    int size = len / sizeof(SFLDataFormat);
    int i;
    for( i = 0 ; i < size ; i++ ){
      src[i] = src[i] * vol  / 100 ;
    }
  }
  return src ; 
}
