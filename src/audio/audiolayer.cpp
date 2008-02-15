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

#define PCM_NAME_DEFAULT  "plughw:0,0"

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
{
  _sampleRate = 8000;

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
  if(_playback_handle != NULL) {
    close_alsa();
    _playback_handle = NULL ;
  }
#ifdef SFL_TEST_SINE
  delete [] table_;
#endif
}

  void
AudioLayer::close_alsa (void) 
{
  _debug(" Alsa close stream \n");
  //ost::MutexLock guard(_mutex);
  snd_pcm_close(_playback_handle);
}

bool
AudioLayer::hasStream(void) {
  ost::MutexLock guard(_mutex);
  return (_playback_handle!=0 ? true : false); 
}

  void
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

  // Name of the PCM device
  // TODO: Must be dynamic

  // TODO: capture
  // Capture stream
  snd_pcm_stream_t capture_stream = SND_PCM_STREAM_CAPTURE;

  // Open the playback device
  open_playback_device(PCM_NAME_DEFAULT);

  // Set up the parameters required to open a device:
  init_hw_parameters();

  device_info();

  //ost::MutexLock guard(_mutex);
}

void 
AudioLayer::open_playback_device(std::string pcm_name)
{
  // Playback stream
  snd_pcm_stream_t playback_stream = SND_PCM_STREAM_PLAYBACK;

  if(snd_pcm_open(&_playback_handle, pcm_name.c_str(),  playback_stream, SND_PCM_NONBLOCK) < 0){
    _debug(" Error while opening PCM device %s\n", pcm_name.c_str());
  }
  else
    _debug(" Device %s successfully opened. \n", pcm_name.c_str());
}

void
AudioLayer::init_hw_parameters(  )
{
  int periods = 2;
  snd_pcm_uframes_t periodSize = 8192;
  unsigned int sr = 44100; //(unsigned int)sampleRate;

  // Information about the hardware
  snd_pcm_hw_params_t *hwparams;
  // Allocate the struct on the stack
  snd_pcm_hw_params_alloca( &hwparams );
  // Allocate memory space
  if(snd_pcm_hw_params_malloc( &hwparams) < 0)
    _debug(" can not allocate hardware paramater structure. \n");
  // Init hwparams with full configuration space
  if(snd_pcm_hw_params_any(_playback_handle, hwparams) < 0){
    _debug(" Can not configure this PCM device .\n");  
  }
  // Set access type
  if(snd_pcm_hw_params_set_access( _playback_handle , hwparams , SND_PCM_ACCESS_RW_INTERLEAVED ) <0){
    _debug(" Error setting access. \n");
  }
  // Set sample format 
  if(snd_pcm_hw_params_set_format( _playback_handle, hwparams , SND_PCM_FORMAT_S16_LE) < 0 ){
    _debug(" Error setting format. \n");
  }
  // Set sample rate
  if(snd_pcm_hw_params_set_rate_near( _playback_handle, hwparams, &sr, 0 ) <0 ) {
    _debug(" Error setting sample rate. \n");
  }
  if(_sampleRate != sr)
    _debug(" The rate %i Hz is not supported by your hardware.\n ==> Using %i Hz instead. \n", _sampleRate, sr);
  // Set number of channels
  if (snd_pcm_hw_params_set_channels( _playback_handle, hwparams, _inChannel) < 0) 
    _debug(" Error setting channels. \n");
  // Set number of periods
  if (snd_pcm_hw_params_set_periods( _playback_handle, hwparams, periods, 0) < 0)
    _debug(" Error setting periods. \n ");
  // Set buffer size
  if(snd_pcm_hw_params_set_buffer_size( _playback_handle, hwparams,  (periodSize * periods) >>2) < 0 )
    _debug(" Error setting buffer size. \n ");
  // Apply hardware parameters to the device
  if(snd_pcm_hw_params( _playback_handle, hwparams) < 0 )
    _debug(" Error setting HW params. \n ");

}

int
AudioLayer::device_info( void )
{
  /*snd_pcm_info_t *info;
  if(snd_pcm_info( _playback_handle , info) <0 ) {
    _debug(" Can not gather infos on opened device. \n");  
    return 0;
  }
  else{*/
      return 1;
 // }    
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

  snd_pcm_prepare( _playback_handle );
  _debug(" ALSA start stream. \n");
  if( snd_pcm_state( _playback_handle ) == SND_PCM_STATE_PREPARED )
    snd_pcm_start( _playback_handle );
  else
    _debug(" Device not ready to start. \n");
  //playSinusWave();
}


  void
AudioLayer::stopStream(void) 
{
  _debug(" Alsa stop stream. \n");
  ost::MutexLock guard(_mutex);
  if( isStreamActive() ){
    // tells the device to stop reading 
    snd_pcm_drop(_playback_handle);
    _mainSndRingBuffer.flush();
    _urgentRingBuffer.flush();
    _micRingBuffer.flush();
  }
  else
    _debug(" Device not running. \n" );
}

  void
AudioLayer::playSinusWave( void )
{
  unsigned char* data;
  int pcmreturn, l1, l2;
  short s1, s2;
  int frames;

  data = (unsigned char*)malloc(8192);
  frames = 8192 >> 2;
  for(l1 = 0; l1 < 100; l1++) {
    for(l2 = 0; l2 < frames; l2++) {
      s1 = (l2 % 128) * 100 - 5000;
      s2 = (l2 % 256) * 100 - 5000;
      data[4*l2] = (unsigned char)s1;
      data[4*l2+1] = s1 >> 8;
      data[4*l2+2] = (unsigned char)s2;
      data[4*l2+3] = s2 >> 8;
    }
    /* Write num_frames frames from buffer data to the PCM device pointed to by pcm_handle */
    /* writei for interleaved */
    /* writen for non-interleaved */
    while ((pcmreturn = snd_pcm_writei(_playback_handle, data, frames)) < 0) {
      snd_pcm_prepare(_playback_handle);
      _debug("<<<<<<<<<<<<<<< Buffer Underrun >>>>>>>>>>>>>>>\n");
    }
  }
}



//TODO
  void
AudioLayer::sleep(int msec) 
{
}

//TODO
  bool
AudioLayer::isStreamActive (void) 
{
  ost::MutexLock guard(_mutex);
  if(snd_pcm_state(_playback_handle) == SND_PCM_STATE_RUNNING)
    return true;
  else
    return false;
}

  int 
AudioLayer::putMain(void* buffer, int toCopy)
{
  play_alsa(buffer, toCopy);
  //ost::MutexLock guard(_mutex);
  /*int a = _mainSndRingBuffer.AvailForPut();
  if ( a >= toCopy ) {
    return _mainSndRingBuffer.Put(buffer, toCopy, _defaultVolume);
  } else {
    _debug("Chopping sound, Ouch! RingBuffer full ?\n");
    return _mainSndRingBuffer.Put(buffer, a, _defaultVolume);
  }*/
  return 0;
}

  void
AudioLayer::flushMain()
{
  ost::MutexLock guard(_mutex);
  _mainSndRingBuffer.flush();
}

  int
AudioLayer::putUrgent(void* buffer, int toCopy)
{
  ost::MutexLock guard(_mutex);
  if ( hasStream() ) {
    int a = _urgentRingBuffer.AvailForPut();
    if ( a >= toCopy ) {
      return _urgentRingBuffer.Put(buffer, toCopy, _defaultVolume);
    } else {
      return _urgentRingBuffer.Put(buffer, a, _defaultVolume);
    }
  }
  return 0;
}

  int
AudioLayer::canGetMic()
{
  if ( hasStream() ) {
    return _micRingBuffer.AvailForGet();
  } else {
    return 0;
  }
}

  int 
AudioLayer::getMic(void *buffer, int toCopy)
{
  if( hasStream() ) {
    return _micRingBuffer.Get(buffer, toCopy, 100);
  } else {
    return 0;
  }
}

  void
AudioLayer::flushMic()
{
  _micRingBuffer.flush();
}

  bool
AudioLayer::isStreamStopped (void) 
{
  ost::MutexLock guard(_mutex);
  if(snd_pcm_state( _playback_handle ) == SND_PCM_STATE_XRUN)
    return true;
  else
    return false;
}

void
AudioLayer::toggleEchoTesting() {
  ost::MutexLock guard(_mutex);
  _echoTesting = (_echoTesting == true) ? false : true;
}

int 
AudioLayer::audioCallback (const void *inputBuffer, void *outputBuffer, 
    unsigned long framesPerBuffer){ 

  SFLDataFormat *in  = (SFLDataFormat *) inputBuffer;
  SFLDataFormat *out = (SFLDataFormat *) outputBuffer;

  if (_echoTesting) {
    memcpy(out, in, framesPerBuffer*sizeof(SFLDataFormat));
    return framesPerBuffer*sizeof(SFLDataFormat);
  }

  int toGet; 
  int toPut;
  int urgentAvail; // number of data right and data left
  int normalAvail; // number of data right and data left
  int micAvailPut;
  unsigned short spkrVolume = _manager->getSpkrVolume();
  unsigned short micVolume  = _manager->getMicVolume();

  // AvailForGet tell the number of chars inside the buffer
  // framePerBuffer are the number of data for one channel (left)
  urgentAvail = _urgentRingBuffer.AvailForGet();
  if (urgentAvail > 0) {
    // Urgent data (dtmf, incoming call signal) come first.		
    toGet = (urgentAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? urgentAvail : framesPerBuffer * sizeof(SFLDataFormat);
    _urgentRingBuffer.Get(out, toGet, spkrVolume);
    // Consume the regular one as well (same amount of bytes)
    _mainSndRingBuffer.Discard(toGet);
  } else {
    AudioLoop* tone = _manager->getTelephoneTone();
    if ( tone != 0) {
      tone->getNext(out, framesPerBuffer, spkrVolume);
    } else if ( (tone=_manager->getTelephoneFile()) != 0 ) {
      tone->getNext(out, framesPerBuffer, spkrVolume);
    } else {
      // If nothing urgent, play the regular sound samples
      normalAvail = _mainSndRingBuffer.AvailForGet();
      toGet = (normalAvail < (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? normalAvail : framesPerBuffer * sizeof(SFLDataFormat);

      if (toGet) {
	_mainSndRingBuffer.Get(out, toGet, spkrVolume);
      } else {
	bzero(out, framesPerBuffer * sizeof(SFLDataFormat));
      }
    }
  }

  // Additionally handle the mic's audio stream 
  micAvailPut = _micRingBuffer.AvailForPut();
  toPut = (micAvailPut <= (int)(framesPerBuffer * sizeof(SFLDataFormat))) ? micAvailPut : framesPerBuffer * sizeof(SFLDataFormat);
  //_debug("AL: Nb sample: %d char, [0]=%f [1]=%f [2]=%f\n", toPut, in[0], in[1], in[2]);
  _micRingBuffer.Put(in, toPut, micVolume);

  return toPut;
}



  int
AudioLayer::play_alsa(void* buffer, int length)
{
  if(_playback_handle == NULL)
    return 0;

  snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames( _playback_handle, length);
  snd_pcm_prepare( _playback_handle );
  if( snd_pcm_writei( _playback_handle, buffer, frames) < 0){
    snd_pcm_prepare( _playback_handle );
    _debug(" Buffer underrun!!!\n");
    return 0;
  }
  return 1;
}

