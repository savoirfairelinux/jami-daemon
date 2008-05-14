/*
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com> 
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

#include <stdio.h>
#include <stdlib.h>

#include <pulse/pulseaudio.h>

#include "audiolayer-pulse.h"
#include "../global.h"
#include "../manager.h"

static pa_context *context = NULL;
static pa_mainloop_api *mainloop_api = NULL;
static pa_sample_spec sample_spec;
static pa_channel_map channel_map;

static std::string stream_p = NULL;
static std::string stream_r = NULL;


  PulseLayer::PulseLayer(ManagerImpl* manager)
  : _urgentRingBuffer(SIZEBUF)
  , _mainSndRingBuffer(SIZEBUF)
  , _micRingBuffer(SIZEBUF)
  , _defaultVolume(PA_VOLUME_NORM)
  , playback(NULL)  
    , record(NULL)  
  , _errorMessage("")
    , _manager(manager)
{
  _sampleRate = 8000;

  _inChannel  = 1; // don't put in stereo
  _outChannel = 1; // don't put in stereo
  _echoTesting = false;
}

// Destructor
PulseLayer::~PulseLayer (void) 
{ 
  assert(mainloop_api);
  mainloop_api->quit( mainloop_api, 0 );
  // pa_stream_flush();
  // pa_stream_disconnect();
}


  void
PulseLayer::openDevice (int indexIn, int indexOut, int sampleRate, int frameSize) 
{
  _sampleRate = sampleRate;
  _frameSize = frameSize;	

  sample_spec.rate = sampleRate;
  sample_spec.format = PA_SAMPLE_S16LE; 
  sample_spec.channels = 1; 
  channel_map.channels = 1; 


  _debug(" Setting PulseLayer: device     in=%2d, out=%2d\n", indexIn, indexOut);
  _debug("                   : nb channel in=%2d, out=%2d\n", _inChannel, _outChannel);
  _debug("                   : sample rate=%5d\n", _sampleRate );

  assert(context);
  switch(pa_context_get_state(context)){
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
    case PA_CONTEXT_READY:
      pa_cvolume cv;
      assert(context && !playback && !record);
      _debug("Connection to PulseAudio server established\n");	
      playback = pa_stream_new( context, "playback stream" , &sample_spec, &channel_map);
      record = pa_stream_new( context, "capture stream" , &sample_spec, &channel_map);

      assert(playback);
      assert(record);

      // Set up the parameters required to open a (Callback)Stream:

      pa_stream_set_state_callback(playback, stream_state_callback, NULL);
      // Transferring Data - Asynchronous Mode
      pa_stream_set_write_callback(playback, audioCallback, NULL);
      pa_stream_connect_playback( playback, NULL , NULL , 0 , NULL, NULL );

      break;
    case PA_CONTEXT_TERMINATED:
      quit(0);
      break;
    case PA_CONTEXT_FAILED:
    default:
      _debug(" Error : %s" , pa_strerror(pa_context_errno(c)));
      quit(1);
  }
}

PulseLayer::stream_state_callback( void )
{
  _debug("The state of the stream changed\n");
}

  int 
PulseLayer::putMain(void* buffer, int toCopy)
{
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
}

  int 
PulseLayer::getMic(void *buffer, int toCopy)
{
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
PulseLayer::toggleEchoTesting() {
}

  int 
PulseLayer::audioCallback ()
{ 
  _debug("Audio callback: New data may be written to the stream\n");
  // pa_stream_write
  // pa_stream_peek ( to read the next fragment from the buffer ) / pa_stream_drop( to remove the data from the buffer )
}

